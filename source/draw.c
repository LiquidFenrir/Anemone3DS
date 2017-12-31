/*
*   This file is part of Anemone3DS
*   Copyright (C) 2016-2017 Alex Taber ("astronautlevel"), Dawid Eckert ("daedreth")
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

#include "draw.h"
#include "unicode.h"
#include "colors.h"

#include "pp2d/pp2d/pp2d.h"

#include <time.h>

void init_screens(void)
{
    pp2d_init();

    pp2d_set_screen_color(GFX_TOP, COLOR_BACKGROUND);
    pp2d_set_screen_color(GFX_BOTTOM, COLOR_BACKGROUND);

    pp2d_load_texture_png(TEXTURE_ARROW, "romfs:/arrow.png");
    pp2d_load_texture_png(TEXTURE_SHUFFLE, "romfs:/shuffle.png");
    pp2d_load_texture_png(TEXTURE_INSTALLED, "romfs:/installed.png");
    pp2d_load_texture_png(TEXTURE_RELOAD, "romfs:/reload.png");
    pp2d_load_texture_png(TEXTURE_PREVIEW_ICON, "romfs:/preview.png");
    pp2d_load_texture_png(TEXTURE_DOWNLOAD, "romfs:/download.png");
    pp2d_load_texture_png(TEXTURE_BATTERY_0, "romfs:/battery0.png");
    pp2d_load_texture_png(TEXTURE_BATTERY_1, "romfs:/battery1.png");
    pp2d_load_texture_png(TEXTURE_BATTERY_2, "romfs:/battery2.png");
    pp2d_load_texture_png(TEXTURE_BATTERY_3, "romfs:/battery3.png");
    pp2d_load_texture_png(TEXTURE_BATTERY_4, "romfs:/battery4.png");
    pp2d_load_texture_png(TEXTURE_BATTERY_5, "romfs:/battery5.png");
    pp2d_load_texture_png(TEXTURE_BATTERY_CHARGE, "romfs:/charging.png");
    pp2d_load_texture_png(TEXTURE_SELECT_BUTTON, "romfs:/select.png");
    pp2d_load_texture_png(TEXTURE_START_BUTTON, "romfs:/start.png");
}

void exit_screens(void)
{
    pp2d_exit();
}

void draw_base_interface(void)
{
    pp2d_begin_draw(GFX_TOP, GFX_LEFT);
    pp2d_draw_rectangle(0, 0, 400, 23, COLOR_ACCENT);

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    pp2d_draw_textf(7, 2, 0.6, 0.6, COLOR_WHITE, "%.2i", tm.tm_hour);
    pp2d_draw_text(28, 1, 0.6, 0.6, COLOR_WHITE, (tm.tm_sec % 2 == 1) ? ":" : " ");
    pp2d_draw_textf(34, 2, 0.6, 0.6, COLOR_WHITE, "%.2i", tm.tm_min);

    #ifndef CITRA_MODE
    u8 battery_charging = 0;
    PTMU_GetBatteryChargeState(&battery_charging);
    u8 battery_status = 0;
    PTMU_GetBatteryLevel(&battery_status);
    pp2d_draw_texture(TEXTURE_BATTERY_0 + battery_status, 357, 2);

    if(battery_charging)
        pp2d_draw_texture(TEXTURE_BATTERY_CHARGE, 357, 2);
    #endif

    pp2d_draw_on(GFX_BOTTOM, GFX_LEFT);
    pp2d_draw_rectangle(0, 0, 320, 24, COLOR_ACCENT);
    pp2d_draw_rectangle(0, 216, 320, 24, COLOR_ACCENT);
    pp2d_draw_text(7, 219, 0.6, 0.6, COLOR_WHITE, VERSION);

    pp2d_draw_on(GFX_TOP, GFX_LEFT);
}

static void draw_text_center(gfxScreen_t target, float y, float scaleX, float scaleY, u32 color, const char* text)
{
    char * _text = strdup(text);
    float prevY = y;
    int offset = 0;
    while(true)
    {
        char *nline = strchr(_text+offset, '\n');
        int nlinepos = 0;
        if(nline != NULL)
        {
            nlinepos = nline-_text;
            _text[nlinepos] = '\0';
        }
        pp2d_draw_text_center(target, prevY, scaleX, scaleY, color, _text+offset);
        if(nline == NULL) break;
        else
        {
            prevY += pp2d_get_text_height(_text+offset, scaleX, scaleY);
            _text[nlinepos] = '\n';
            offset = nlinepos+1;
        }
    }
    free(_text);
}

void throw_error(char* error, ErrorLevel level)
{
    switch(level)
    {
        case ERROR_LEVEL_ERROR:
            while(aptMainLoop())
            {
                hidScanInput();
                u32 kDown = hidKeysDown();
                draw_base_interface();
                draw_text_center(GFX_TOP, 100, 0.6, 0.6, COLOR_RED, error);
                pp2d_draw_wtext_center(GFX_TOP, 150, 0.6, 0.6, COLOR_WHITE, L"Press \uE000 to shut down.");
                pp2d_end_draw();
                if(kDown & KEY_A) break;
            }
            break;
        case ERROR_LEVEL_WARNING:
            while(aptMainLoop())
            {
                hidScanInput();
                u32 kDown = hidKeysDown();
                draw_base_interface();
                draw_text_center(GFX_TOP, 100, 0.6, 0.6, COLOR_YELLOW, error);
                pp2d_draw_wtext_center(GFX_TOP, 150, 0.6, 0.6, COLOR_WHITE, L"Press \uE000 to continue.");
                pp2d_end_draw();
                if(kDown & KEY_A) break;
            }
            break;
    }
}

bool draw_confirm(const char* conf_msg, Entry_List_s* list)
{
    while(aptMainLoop())
    {
        Button_s * controls = NULL;
        draw_interface(list, controls);
        pp2d_draw_on(GFX_TOP, GFX_LEFT);
        draw_text_center(GFX_TOP, BUTTONS_Y_LINE_1, 0.7, 0.7, COLOR_YELLOW, conf_msg);
        pp2d_draw_wtext_center(GFX_TOP, BUTTONS_Y_LINE_3, 0.6, 0.6, COLOR_WHITE, L"\uE000 Yes   \uE001 No");
        pp2d_end_draw();

        hidScanInput();
        u32 kDown = hidKeysDown();
        if(kDown & KEY_A) return true;
        if(kDown & KEY_B) return false;
    }

    return false;
}

void draw_preview(int preview_offset)
{
    pp2d_begin_draw(GFX_TOP, GFX_LEFT);
    pp2d_draw_texture_part(TEXTURE_PREVIEW, 0, 0, preview_offset, 0, 400, 240);
    pp2d_draw_on(GFX_BOTTOM, GFX_LEFT);
    pp2d_draw_texture_part(TEXTURE_PREVIEW, 0, 0, 40 + preview_offset, 240, 320, 240);
}

void draw_install(InstallType type)
{
    draw_base_interface();
    switch(type)
    {
        case INSTALL_LOADING_THEMES:
            pp2d_draw_text_center(GFX_TOP, 120, 0.8, 0.8, COLOR_WHITE, "Loading themes, please wait...");
            break;
        case INSTALL_LOADING_SPLASHES:
            pp2d_draw_text_center(GFX_TOP, 120, 0.8, 0.8, COLOR_WHITE, "Loading splashes, please wait...");
            break;
        case INSTALL_LOADING_ICONS:
            pp2d_draw_text_center(GFX_TOP, 120, 0.8, 0.8, COLOR_WHITE, "Loading icons, please wait...");
            break;
        case INSTALL_SINGLE:
            pp2d_draw_text_center(GFX_TOP, 120, 0.8, 0.8, COLOR_WHITE, "Installing a single theme...");
            break;
        case INSTALL_SHUFFLE:
            pp2d_draw_text_center(GFX_TOP, 120, 0.8, 0.8, COLOR_WHITE, "Installing shuffle themes...");
            break;
        case INSTALL_BGM:
            pp2d_draw_text_center(GFX_TOP, 120, 0.8, 0.8, COLOR_WHITE, "Installing BGM-only theme...");
            break;
        case INSTALL_DOWNLOAD:
            pp2d_draw_text_center(GFX_TOP, 120, 0.8, 0.8, COLOR_WHITE, "Downloading...");
            break;
        case INSTALL_SPLASH:
            pp2d_draw_text_center(GFX_TOP, 120, 0.8, 0.8, COLOR_WHITE, "Installing a splash...");
            break;
        case INSTALL_SPLASH_DELETE:
            pp2d_draw_text_center(GFX_TOP, 120, 0.8, 0.8, COLOR_WHITE, "Deleting installed splash...");
            break;
        case INSTALL_ENTRY_DELETE:
            pp2d_draw_text_center(GFX_TOP, 120, 0.8, 0.8, COLOR_WHITE, "Deleting from SD...");
            break;
        case INSTALL_NO_BGM:
            pp2d_draw_text_center(GFX_TOP, 120, 0.8, 0.8, COLOR_WHITE, "Installing theme without BGM...");
            break;
        default:
            break;
    }
    pp2d_end_draw();
}

static void draw_controls(Button_s * controls)
{
    pp2d_draw_on(GFX_TOP, GFX_LEFT);

    if(controls == NULL) return;

    for(int i = 0; i < BUTTONS_AMOUNT; i++)
    {
        Button_s button = controls[i];
        if(button.button_info != NULL)
        {
            if(button.y_pos == BUTTONS_Y_LINE_4)
            {
                if(button.x_pos == BUTTONS_X_LEFT)
                    pp2d_draw_texture(TEXTURE_START_BUTTON, BUTTONS_X_LEFT-10, BUTTONS_Y_LINE_4 + 3);
                if(button.x_pos == BUTTONS_X_RIGHT)
                    pp2d_draw_texture(TEXTURE_SELECT_BUTTON, BUTTONS_X_RIGHT-10, BUTTONS_Y_LINE_4 + 3);

                pp2d_draw_wtext(button.x_pos+26, button.y_pos, 0.6, 0.6, COLOR_WHITE, button.button_info);
            }
            else
            {
                pp2d_draw_wtext(button.x_pos, button.y_pos, 0.6, 0.6, COLOR_WHITE, button.button_info);
            }
        }
    }
}

void draw_interface(Entry_List_s* list, Button_s * controls)
{
    draw_base_interface();
    EntryMode current_mode = list->mode;

    const char* mode_string[MODE_AMOUNT] = {
        "Theme mode",
        "Splash mode",
    };

    pp2d_draw_text_center(GFX_TOP, 4, 0.5, 0.5, COLOR_WHITE, mode_string[current_mode]);

    if(list->entries == NULL)
    {
        const char* mode_found_string[MODE_AMOUNT] = {
            "No themes found",
            "No splashes found",
        };
        pp2d_draw_text_center(GFX_TOP, 80, 0.7, 0.7, COLOR_YELLOW, mode_found_string[current_mode]);
        pp2d_draw_text_center(GFX_TOP, 110, 0.7, 0.7, COLOR_YELLOW, "Press \uE005 to download from QR");
        const char* mode_switch_string[MODE_AMOUNT] = {
            "Or \uE004 to switch to splashes",
            "Or \uE004 to switch to themes",
        };
        pp2d_draw_text_center(GFX_TOP, 140, 0.7, 0.7, COLOR_YELLOW, mode_switch_string[current_mode]);
        pp2d_draw_text_center(GFX_TOP, 170, 0.7, 0.7, COLOR_YELLOW, "Or        to quit");
        pp2d_texture_select(TEXTURE_START_BUTTON, 162, 173);
        pp2d_texture_blend(COLOR_YELLOW);
        pp2d_texture_scale(1.25, 1.4);
        pp2d_texture_draw();
        return;
    }

    draw_controls(controls);

    int selected_entry = list->selected_entry;
    Entry_s current_entry = list->entries[selected_entry];

    wchar_t title[0x41] = {0};
    utf16_to_utf32((u32*)title, current_entry.name, 0x40);
    pp2d_draw_wtext_wrap(20, 30, 0.7, 0.7, COLOR_WHITE, 380, title);

    wchar_t author[0x41] = {0};
    utf16_to_utf32((u32*)author, current_entry.author, 0x40);
    pp2d_draw_text(20, 50, 0.5, 0.5, COLOR_WHITE, "By: ");
    pp2d_draw_wtext_wrap(44, 50, 0.5, 0.5, COLOR_WHITE, 380, author);

    wchar_t description[0x81] = {0};
    utf16_to_utf32((u32*)description, current_entry.desc, 0x80);
    pp2d_draw_wtext_wrap(20, 65, 0.5, 0.5, COLOR_WHITE, 363, description);

    pp2d_draw_on(GFX_BOTTOM, GFX_LEFT);

    switch(current_mode)
    {
        case MODE_THEMES:
            pp2d_draw_textf(7, 3, 0.6, 0.6, list->shuffle_count <= 10 ? COLOR_WHITE : COLOR_RED, "Shuffle: %i/10", list->shuffle_count);
            pp2d_draw_texture_blend(TEXTURE_SHUFFLE, 320-120, 0, COLOR_WHITE);
            break;
        default:
            break;
    }

    pp2d_draw_texture_blend(TEXTURE_RELOAD, 320-96, 0, COLOR_WHITE);
    pp2d_draw_texture_blend(TEXTURE_PREVIEW_ICON, 320-72, 0, COLOR_WHITE);
    pp2d_draw_texture_blend(TEXTURE_DOWNLOAD, 320-48, 0, COLOR_WHITE);
    pp2d_draw_textf(320-24+2.5, -3, 1, 1, COLOR_WHITE, "%c", *mode_string[!list->mode]);


    // Show arrows if there are themes out of bounds
    //----------------------------------------------------------------
    if(list->scroll > 0)
        pp2d_draw_texture(TEXTURE_ARROW, 155, 6);
    if(list->scroll + ENTRIES_PER_SCREEN < list->entries_count)
        pp2d_draw_texture_flip(TEXTURE_ARROW, 155, 224, VERTICAL);

    for(int i = list->scroll; i < (ENTRIES_PER_SCREEN + list->scroll); i++)
    {
        if(i >= list->entries_count) break;

        current_entry = list->entries[i];

        wchar_t name[0x41] = {0};
        utf16_to_utf32((u32*)name, current_entry.name, 0x40);

        int vertical_offset = 48 * (i - list->scroll);
        u32 font_color = COLOR_WHITE;

        if(i == list->selected_entry)
        {
            font_color = COLOR_BLACK;
            pp2d_draw_rectangle(0, 24 + vertical_offset, 320, 48, COLOR_CURSOR);
        }
        pp2d_draw_wtext(54, 40 + vertical_offset, 0.55, 0.55, font_color, name);
        if(!current_entry.placeholder_color)
        {
            ssize_t id = 0;
            if(list->entries_count > ICONS_OFFSET_AMOUNT*ENTRIES_PER_SCREEN)
                id = list->icons_ids[ICONS_VISIBLE][i - list->scroll];
            else
                id = ((size_t *)list->icons_ids)[i];
            pp2d_draw_texture(id, 0, 24 + vertical_offset);
        }
        else
            pp2d_draw_rectangle(0, 24 + vertical_offset, 48, 48, current_entry.placeholder_color);

        if(current_entry.in_shuffle)
            pp2d_draw_texture_blend(TEXTURE_SHUFFLE, 320-24-4, 24 + vertical_offset, font_color);
        if(current_entry.installed)
            pp2d_draw_texture_blend(TEXTURE_INSTALLED, 320-24-4, 24 + 22 + vertical_offset, font_color);
    }

    char entries_count_str[0x20] = {0};
    sprintf(entries_count_str, "/%i", list->entries_count);
    float x = 316;
    x -= pp2d_get_text_width(entries_count_str, 0.6, 0.6);
    pp2d_draw_text(x, 219, 0.6, 0.6, COLOR_WHITE, entries_count_str);

    char selected_entry_str[0x20] = {0};
    sprintf(selected_entry_str, "%i", selected_entry + 1);
    x -= pp2d_get_text_width(selected_entry_str, 0.6, 0.6);
    pp2d_draw_text(x, 219, 0.6, 0.6, COLOR_WHITE, selected_entry_str);

    pp2d_draw_text(176, 219, 0.6, 0.6, COLOR_WHITE, list->entries_count < 1000 ? "Selected:" : "Sel.:");
}
