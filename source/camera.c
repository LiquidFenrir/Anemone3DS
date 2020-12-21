/*
*   This file is part of Anemone3DS
*   Copyright (C) 2016-Present Contributors in CONTRIBUTORS.md
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include "camera.h"

#include "quirc/quirc.h"

#include "draw.h"
#include "fs.h"
#include "loading.h"
#include "remote.h"

#include <archive.h>
#include <archive_entry.h>

typedef struct {
    u16* camera_buffer;

    Handle event_stop;
    Thread cam_thread, ui_thread;

    LightEvent event_cam_info, event_ui_info;

    CondVar cond;
    LightLock mut;
    u32 num_readers_active;
    bool writer_waiting;
    bool writer_active;

    bool any_update;

    struct quirc* context;
} qr_data;

static void start_read(qr_data *data)
{
    LightLock_Lock(&data->mut);
    while(data->writer_waiting || data->writer_active)
    {
        CondVar_Wait(&data->cond, &data->mut);
    }

    AtomicIncrement(&data->num_readers_active);
    LightLock_Unlock(&data->mut);
}
static void stop_read(qr_data *data)
{
    LightLock_Lock(&data->mut);
    AtomicDecrement(&data->num_readers_active);
    if(data->num_readers_active == 0)
    {
        CondVar_Signal(&data->cond);
    }
    LightLock_Unlock(&data->mut);
}
static void start_write(qr_data *data)
{
    LightLock_Lock(&data->mut);
    data->writer_waiting = true;

    while(data->num_readers_active)
    {
        CondVar_Wait(&data->cond, &data->mut);
    }

    data->writer_waiting = false;
    data->writer_active = true;

    LightLock_Unlock(&data->mut);
}
static void stop_write(qr_data *data)
{
    LightLock_Lock(&data->mut);

    data->writer_active = false;
    CondVar_Broadcast(&data->cond);

    LightLock_Unlock(&data->mut);
}

static void capture_cam_thread(void *arg)
{
    qr_data *data = (qr_data *) arg;

    Handle cam_events[3] = {0};
    cam_events[0] = data->event_stop;

    u32 transferUnit;
    const u32 bufsz = 400 * 240 * sizeof(u16);
    u16 *buffer = linearAlloc(bufsz);

    camInit();
    CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A);
    CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A);
    CAMU_SetFrameRate(SELECT_OUT1, FRAME_RATE_30);
    CAMU_SetNoiseFilter(SELECT_OUT1, true);
    CAMU_SetAutoExposure(SELECT_OUT1, true);
    CAMU_SetAutoWhiteBalance(SELECT_OUT1, true);
    CAMU_Activate(SELECT_OUT1);
    CAMU_GetBufferErrorInterruptEvent(&cam_events[2], PORT_CAM1);
    CAMU_SetTrimming(PORT_CAM1, false);
    CAMU_GetMaxBytes(&transferUnit, 400, 240);
    CAMU_SetTransferBytes(PORT_CAM1, transferUnit, 400, 240);
    CAMU_ClearBuffer(PORT_CAM1);
    CAMU_SetReceiving(&cam_events[1], buffer, PORT_CAM1, bufsz, transferUnit);
    CAMU_StartCapture(PORT_CAM1);

    bool cancel = false;

    while (!cancel)
    {
        s32 index = 0;
        svcWaitSynchronizationN(&index, cam_events, 3, false, U64_MAX);
        switch(index) {
            case 0:
                DEBUG("Cancel event received\n");
                cancel = true;
                break;
            case 1:
                svcCloseHandle(cam_events[1]);
                cam_events[1] = 0;

                start_write(data);
                memcpy(data->camera_buffer, buffer, bufsz);
                data->any_update = true;
                stop_write(data);

                CAMU_SetReceiving(&cam_events[1], buffer, PORT_CAM1, bufsz, transferUnit);
                break;
            case 2:
                svcCloseHandle(cam_events[1]);
                cam_events[1] = 0;

                CAMU_ClearBuffer(PORT_CAM1);
                CAMU_SetReceiving(&cam_events[1], buffer, PORT_CAM1, bufsz, transferUnit);
                CAMU_StartCapture(PORT_CAM1);
                break;
            default:
                break;
        }
    }

    CAMU_StopCapture(PORT_CAM1);

    bool busy = false;
    while(R_SUCCEEDED(CAMU_IsBusy(&busy, PORT_CAM1)) && busy) {
        svcSleepThread(1000000);
    }

    CAMU_ClearBuffer(PORT_CAM1);
    CAMU_Activate(SELECT_NONE);
    camExit();

    linearFree(buffer);
    for(int i = 1; i < 3; i++)
    {
        if(cam_events[i] != 0) {
            svcCloseHandle(cam_events[i]);
            cam_events[i] = 0;
        }
    }

    LightEvent_Signal(&data->event_cam_info);
}

static void update_ui(void *arg)
{
    qr_data* data = (qr_data*) arg;
    C3D_Tex tex;

    static const Tex3DS_SubTexture subt3x = { 400, 240, 0.0f, 1.0f, 400.0f/512.0f, 1.0f - (240.0f/256.0f) };
    C3D_TexInit(&tex, 512, 256, GPU_RGB565);

    C3D_TexSetFilter(&tex, GPU_LINEAR, GPU_LINEAR);

    while(svcWaitSynchronization(data->event_stop, 2 * 1000 * 1000ULL) == 0x09401BFE) // timeout of 2ms occured, still have 14 for copy and render
    {
        // Untiled texture loading code adapted from FBI
        start_read(data);
        if(data->any_update)
        {
            for(u32 y = 0; y < 240; y++) {
                const u32 srcPos = y * 400;
                for(u32 x = 0; x < 400; x++) {
                    const u32 dstPos = ((((y >> 3) * (512 >> 3) + (x >> 3)) << 6) + ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3)));

                    ((u16*)tex.data)[dstPos] = data->camera_buffer[srcPos + x];
                }
            }
            data->any_update = false;
        }
        stop_read(data);

        start_frame();

        draw_base_interface();

        C2D_DrawImageAt((C2D_Image){ &tex, &subt3x }, 0.0f, 0.0f, 0.4f, NULL, 1.0f, 1.0f);

        set_screen(render_info->bottom_screen);

        C2D_Text exit_text;
        C2D_TextParse(&exit_text, render_info->dynamic_text, "Press any key to close the scanner");
        float height = 0.0f;
        C2D_TextGetDimensions(&exit_text, 0.5f, 0.5f, NULL, &height);

        C2D_DrawText(&exit_text, C2D_WithColor | C2D_AlignCenter, 0.0f, (240.0f - height)/2.0f, 0.0f, 0.5f, 0.5f, render_info->colors.foreground);
    
        end_frame();
    }

    C3D_TexDelete(&tex);
    LightEvent_Signal(&data->event_ui_info);
}

static bool start_capture_cam(qr_data *data) 
{
    if((data->cam_thread = threadCreate(capture_cam_thread, data, 0x1000, 0x1A, 1, false)) == NULL)
    {
        throw_error("Capture cam thread creation failed\nPlease report this to the developers", ERROR_LEVEL_ERROR);
        LightEvent_Signal(&data->event_cam_info);
        LightEvent_Signal(&data->event_ui_info);
        return false;
    }
    if((data->ui_thread = threadCreate(update_ui, data, 0x1000, 0x1A, 1, false)) == NULL)
    {
        LightEvent_Signal(&data->event_ui_info);
        return false;
    }
    return true;
}

static bool update_qr(qr_data *data, struct quirc_data* scan_data)
{
    int w;
    int h;
    u8 *image = (u8*) quirc_begin(data->context, &w, &h);

    start_read(data);
    for (int y = 0; y < h; y++) {
        const int actual_y = y * w;
        for (int x = 0; x < w; x++) {
            const int actual_off = actual_y + x;
            const u16 px = data->camera_buffer[actual_off];
            image[actual_off] = (u8)(((((px >> 11) & 0x1F) << 3) + (((px >> 5) & 0x3F) << 2) + ((px & 0x1F) << 3)) / 3);
        }
    }
    stop_read(data);

    quirc_end(data->context);
    if(quirc_count(data->context) > 0)
    {
        struct quirc_code code;
        quirc_extract(data->context, 0, &code);
        if (!quirc_decode(&code, scan_data))
        {
            return true;
        }
    }

    return false;
}

static void start_qr(qr_data *data)
{
    svcCreateEvent(&data->event_stop, RESET_STICKY);
    LightEvent_Init(&data->event_cam_info, RESET_STICKY);
    LightEvent_Init(&data->event_ui_info, RESET_STICKY);
    LightLock_Init(&data->mut);
    CondVar_Init(&data->cond);

    data->cam_thread = NULL;
    data->ui_thread = NULL;
    data->any_update = false;

    data->context = quirc_new();
    quirc_resize(data->context, 400, 240);
    data->camera_buffer = calloc(1, 400 * 240 * sizeof(u16));
}
static void exit_qr(qr_data *data)
{
    svcSignalEvent(data->event_stop);

    LightEvent_Wait(&data->event_ui_info);
    LightEvent_Clear(&data->event_ui_info);
    if(data->ui_thread != NULL)
    {
        threadJoin(data->ui_thread, U64_MAX);
        threadFree(data->ui_thread);
        data->ui_thread = NULL;
    }

    LightEvent_Wait(&data->event_cam_info);
    LightEvent_Clear(&data->event_cam_info);
    if(data->cam_thread != NULL)
    {
        threadJoin(data->cam_thread, U64_MAX);
        threadFree(data->cam_thread);
        data->cam_thread = NULL;
    }

    free(data->camera_buffer);
    data->camera_buffer = NULL;
    svcCloseHandle(data->event_stop);
    data->event_stop = 0;
}

void qr_scanner(Application_t* app)
{
    qr_data data;

    memset(&data, 0, sizeof(data));

    start_qr(&data);

    struct quirc_data* scan_data = calloc(1, sizeof(struct quirc_data)); 
    const bool ready = start_capture_cam(&data);
    bool finished = !ready;

    while(!finished)
    {
        hidScanInput();
        if (hidKeysDown()) break;

        finished = update_qr(&data, scan_data);
        svcSleepThread(50 * 1000 * 1000ULL); // only scan every 50ms
    }

    exit_qr(&data);

    if(finished && ready)
    {
        Buffer_t zip_buf = empty_buffer();
        char * filename = NULL;
        Result res = http_get((const char*)scan_data->payload, &filename, &zip_buf, "Downloading...");

        if(res == 0 && zip_buf.size != 0)
        {
            draw_loading("Checking downloaded zip...");

            struct archive *a = archive_read_new();
            archive_read_support_format_zip(a);

            int r = archive_read_open_memory(a, zip_buf.data, zip_buf.size);

            if(r == ARCHIVE_OK)
            {
                AppplicationMode mode = MODES_AMOUNT;
                struct archive_entry *entry;
                while(archive_read_next_header(a, &entry) == ARCHIVE_OK)
                {
                    const char* name = archive_entry_pathname(entry);
                    if(!strcasecmp(name, "body_LZ.bin"))
                    {
                        mode = MODE_THEMES;
                        break;
                    }
                    else if(!strcasecmp(name, "splash.bin"))
                    {
                        mode = MODE_SPLASHES;
                        break;
                    }
                    else if(!strcasecmp(name, "splashbottom.bin"))
                    {
                        mode = MODE_SPLASHES;
                        break;
                    }
                }

                if(mode != MODES_AMOUNT)
                {
                    char* path_to_file = calloc(1, ENTRY_PATH_SIZE + 1);
                    const int len = sprintf(path_to_file, "%s%s", app->lists[mode].loading_path, filename);
                    char * extension = strrchr(path_to_file, '.');
                    if (extension == NULL || strcmp(extension, ".zip"))
                    {
                        strcat(path_to_file + len, ".zip");
                    }

                    FS_Path zip_path = fsMakePath(PATH_ASCII, path_to_file);
                    remake_file(zip_path, ArchiveSD, zip_buf.size);
                    buf_to_file(zip_path, ArchiveSD, &zip_buf);

                    // TODO: Queue entry addition work atom (waited on)
                    // for now:
                    free(path_to_file);
                }
                else
                {
                    throw_error("ZIP downloaded is neither\na splash nor a theme.", ERROR_LEVEL_WARNING);
                }
            }
            else
            {
                throw_error("File downloaded isn't a ZIP.", ERROR_LEVEL_WARNING);
            }

            archive_read_free(a);
        }
        else
        {
            throw_error("Download failed.", ERROR_LEVEL_WARNING);
        }

        free_buffer(&zip_buf);
        free(filename);
    }

    free(scan_data);
    quirc_destroy(data.context);
}
