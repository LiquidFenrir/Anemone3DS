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

#include "themes.h"
#include "unicode.h"
#include "fs.h"
#include "draw.h"

#define BODY_CACHE_SIZE 0x150000
#define BGM_MAX_SIZE 0x337000

static Result install_theme_internal(Entry_List_s themes, int installmode)
{
    Result res = 0;
    char* music = NULL;
    u32 music_size = 0;
    u32 shuffle_music_sizes[MAX_SHUFFLE_THEMES] = {0};
    char* body = NULL;
    u32 body_size = 0;
    u32 shuffle_body_sizes[MAX_SHUFFLE_THEMES] = {0};

    if(installmode & THEME_INSTALL_SHUFFLE)
    {
        if(themes.shuffle_count == 0)
        {
            DEBUG("no themes selected for shuffle\n");
            return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_COMMON, RD_INVALID_SELECTION);
        }

        if(themes.shuffle_count > MAX_SHUFFLE_THEMES)
        {
            DEBUG("too many themes selected for shuffle\n");
            return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_COMMON, RD_INVALID_SELECTION);
        }

        int shuffle_count = 0;
        Handle body_cache_handle;

        if(installmode & THEME_INSTALL_BODY)
        {
            remake_file("/BodyCache_rd.bin", ArchiveThemeExt, BODY_CACHE_SIZE * MAX_SHUFFLE_THEMES);
            FSUSER_OpenFile(&body_cache_handle, ArchiveThemeExt, fsMakePath(PATH_ASCII, "/BodyCache_rd.bin"), FS_OPEN_WRITE, 0);
        }

        for(int i = 0; i < themes.entries_count; i++)
        {
            Entry_s * current_theme = &themes.entries[i];

            if(current_theme->in_shuffle)
            {
                if(installmode & THEME_INSTALL_BODY)
                {
                    body_size = load_data("/body_LZ.bin", *current_theme, &body);
                    if(body_size == 0)
                    {
                        free(body);
                        DEBUG("body not found\n");
                        throw_error("No body_LZ.bin found - is this a theme?", ERROR_LEVEL_WARNING);
                        return MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, RD_NOT_FOUND);
                    }

                    shuffle_body_sizes[shuffle_count] = body_size;
                    FSFILE_Write(body_cache_handle, NULL, BODY_CACHE_SIZE * shuffle_count, body, body_size, FS_WRITE_FLUSH);
                    free(body);
                }

                if(installmode & THEME_INSTALL_BGM)
                {
                    char bgm_cache_path[26] = {0};
                    sprintf(bgm_cache_path, "/BgmCache_%.2i.bin", shuffle_count);
                    music_size = load_data("/bgm.bcstm", *current_theme, &music);

                    if(music_size > BGM_MAX_SIZE)
                    {
                        free(music);
                        DEBUG("bgm too big\n");
                        return MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, RD_TOO_LARGE);
                    }

                    shuffle_music_sizes[shuffle_count] = music_size;
                    remake_file(bgm_cache_path, ArchiveThemeExt, BGM_MAX_SIZE);
                    buf_to_file(music_size, bgm_cache_path, ArchiveThemeExt, music);
                    free(music);
                }

                shuffle_count++;
            }
        }

        if(installmode & THEME_INSTALL_BGM)
        {
            for(int i = shuffle_count; i < MAX_SHUFFLE_THEMES; i++)
            {
                char bgm_cache_path[26] = {0};
                sprintf(bgm_cache_path, "/BgmCache_%.2i.bin", i);
                remake_file(bgm_cache_path, ArchiveThemeExt, BGM_MAX_SIZE);
            }
        }

        if(installmode & THEME_INSTALL_BODY)
        {
            FSFILE_Close(body_cache_handle);
        }
    }
    else
    {
        Entry_s current_theme = themes.entries[themes.selected_entry];

        if(installmode & THEME_INSTALL_BODY)
        {
            body_size = load_data("/body_LZ.bin", current_theme, &body);
            if(body_size == 0)
            {
                free(body);
                DEBUG("body not found\n");
                throw_error("No body_LZ.bin found - is this a theme?", ERROR_LEVEL_WARNING);
                return MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, RD_NOT_FOUND);
            }

            res = buf_to_file(body_size, "/BodyCache.bin", ArchiveThemeExt, body); // Write body data to file
            free(body);

            if(R_FAILED(res)) return res;
        }

        if(installmode & THEME_INSTALL_BGM)
        {
            music_size = load_data("/bgm.bcstm", current_theme, &music);
            if(music_size > BGM_MAX_SIZE)
            {
                free(music);
                DEBUG("bgm too big\n");
                return MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, RD_TOO_LARGE);
            }

            res = buf_to_file(music_size, "/BgmCache.bin", ArchiveThemeExt, music);
            free(music);

            if(R_FAILED(res)) return res;
        }
    }

     //----------------------------------------
    char* thememanage_buf = NULL;
    file_to_buf(fsMakePath(PATH_ASCII, "/ThemeManage.bin"), ArchiveThemeExt, &thememanage_buf);
    ThemeManage_bin_s * theme_manage = (ThemeManage_bin_s *)thememanage_buf;

    theme_manage->unk1 = 1;
    theme_manage->unk2 = 0;

    if(installmode & THEME_INSTALL_SHUFFLE)
    {
        theme_manage->music_size = 0;
        theme_manage->body_size = 0;

        for(int i = 0; i < MAX_SHUFFLE_THEMES; i++)
        {
            theme_manage->shuffle_body_sizes[i] = shuffle_body_sizes[i];
            theme_manage->shuffle_music_sizes[i] = shuffle_music_sizes[i];
        }
    }
    else
    {
        if(installmode & THEME_INSTALL_BGM)
            theme_manage->music_size = music_size;
        if(installmode & THEME_INSTALL_BODY)
            theme_manage->body_size = body_size;
    }

    theme_manage->unk3 = 0xFF;
    theme_manage->unk4 = 1;
    theme_manage->dlc_theme_content_index = 0xFF;
    theme_manage->use_theme_cache = 0x0200;

    res = buf_to_file(0x800, "/ThemeManage.bin", ArchiveThemeExt, thememanage_buf);
    free(thememanage_buf);
    if(R_FAILED(res)) return res;
    //----------------------------------------

    //----------------------------------------
    char* savedata_buf = NULL;
    u32 savedata_size = file_to_buf(fsMakePath(PATH_ASCII, "/SaveData.dat"), ArchiveHomeExt, &savedata_buf);
    SaveData_dat_s* savedata = (SaveData_dat_s*)savedata_buf;

    memset(&savedata->theme_entry, 0, sizeof(ThemeEntry_s));
    savedata->theme_entry.type = 3;
    savedata->theme_entry.index = 0xff;

    savedata->shuffle = (installmode & THEME_INSTALL_SHUFFLE);
    if(installmode & THEME_INSTALL_SHUFFLE)
    {
        memset(savedata->shuffle_themes, 0, sizeof(ThemeEntry_s)*MAX_SHUFFLE_THEMES);
        for(int i = 0; i < themes.shuffle_count; i++)
        {
            savedata->shuffle_themes[i].type = 3;
            savedata->shuffle_themes[i].index = i;
        }
    }

    res = buf_to_file(savedata_size, "/SaveData.dat", ArchiveHomeExt, savedata_buf);
    free(savedata_buf);
    if(R_FAILED(res)) return res;
    //----------------------------------------

    return 0;
}

inline Result theme_install(Entry_s theme)
{
    Entry_List_s list = {0};
    list.entries_count = 1;
    list.entries = &theme;
    list.selected_entry = 0;
    return install_theme_internal(list, THEME_INSTALL_BODY | THEME_INSTALL_BGM);
}

inline Result bgm_install(Entry_s theme)
{
    Entry_List_s list = {0};
    list.entries_count = 1;
    list.entries = &theme;
    list.selected_entry = 0;
    return install_theme_internal(list, THEME_INSTALL_BGM);
}

inline Result no_bgm_install(Entry_s theme)
{
    Entry_List_s list = {0};
    list.entries_count = 1;
    list.entries = &theme;
    list.selected_entry = 0;
    return install_theme_internal(list, THEME_INSTALL_BODY);
}

inline Result shuffle_install(Entry_List_s themes)
{
    return install_theme_internal(themes, THEME_INSTALL_SHUFFLE | THEME_INSTALL_BODY | THEME_INSTALL_BGM);
}

Result dump_installed_theme(void)
{
    char* savedata_buf = NULL;
    u32 savedata_size = file_to_buf(fsMakePath(PATH_ASCII, "/SaveData.dat"), ArchiveHomeExt, &savedata_buf);
    if(!savedata_size) return -1;
    SaveData_dat_s* savedata = (SaveData_dat_s*)savedata_buf;
    bool shuffle = savedata->shuffle;
    free(savedata_buf);

    char* thememanage_buf = NULL;
    u32 theme_manage_size = file_to_buf(fsMakePath(PATH_ASCII, "/ThemeManage.bin"), ArchiveThemeExt, &thememanage_buf);
    if(!theme_manage_size) return -1;
    ThemeManage_bin_s * theme_manage = (ThemeManage_bin_s *)thememanage_buf;

    u32 single_body_size = theme_manage->body_size;
    u32 single_bgm_size = theme_manage->body_size;
    u32 shuffle_body_sizes[MAX_SHUFFLE_THEMES] = {0};
    u32 shuffle_music_sizes[MAX_SHUFFLE_THEMES] = {0};
    memcpy(shuffle_body_sizes, theme_manage->shuffle_body_sizes, sizeof(u32)*MAX_SHUFFLE_THEMES);
    memcpy(shuffle_music_sizes, theme_manage->shuffle_music_sizes, sizeof(u32)*MAX_SHUFFLE_THEMES);
    free(thememanage_buf);

    Result res = 0;

    Handle body_cache_handle;
    if(shuffle)
        FSUSER_OpenFile(&body_cache_handle, ArchiveThemeExt, fsMakePath(PATH_ASCII, "/BodyCache_rd.bin"), FS_OPEN_READ, 0);

    for(int i = 0; i < MAX_SHUFFLE_THEMES; i++)
    {
        char * folder_path = NULL;
        char * smdh_path = NULL;
        char * smdh_desc = NULL;

        smdh_desc = calloc(0xFF, sizeof(char));
        if(smdh_desc == NULL) goto end;

        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        const char * day_notifier[] = {"th","st","nd","rd","th","th","th","th","th","th",};
        char format[0xFF] = {0};
        sprintf(format, "Dumped at %%T on %%d%s %%b %%Y", day_notifier[tm.tm_mday % 10]);
        strftime(smdh_desc, 0xFF, format, &tm);

        sprintf(format, "%sDumped on %%4.4i-%%2.2i-%%2.2i at %%2.2i-%%2.2i-%%2.2i", main_paths[MODE_THEMES]);
        folder_path = calloc(0xFF, sizeof(char));
        snprintf(folder_path, 0xFF, format, tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        if(shuffle)
            snprintf(folder_path, 0xFF, "%s_%i", folder_path, i);

        DEBUG("path: %s\n", folder_path);

        res = FSUSER_CreateDirectory(ArchiveSD, fsMakePath(PATH_ASCII, folder_path), 0);
        if(R_FAILED(res))
        {
            throw_error("Failed to create the folder to\nhold the dumped theme.", ERROR_LEVEL_WARNING);
            goto end;
        }

        /*
        Gettin the body_lz and bgm.bcstm here
        */
        if(shuffle)
        {
            u32 body_size = shuffle_body_sizes[i];
            if(body_size)
            {
                char * body = NULL;
                char * body_path = NULL;
                res = (Result)asprintf(&body_path, "%s/body_LZ.bin", folder_path);
                if(res == -1) goto end;

                body = calloc(body_size, sizeof(char));
                FSFILE_Read(body_cache_handle, NULL, BODY_CACHE_SIZE * i, (u8*)body, body_size);

                remake_file(body_path, ArchiveSD, body_size);
                buf_to_file(body_size, body_path, ArchiveSD, body);

                free(body);
                free(body_path);
            }

            u32 bgm_size = shuffle_music_sizes[i];
            if(bgm_size)
            {
                char * bgm = NULL;
                char * bgm_path = NULL;
                res = (Result)asprintf(&bgm_path, "%s/bgm.bcstm", folder_path);
                if(res == -1) goto end;

                char installed_bgm_path[26] = {0};
                sprintf(installed_bgm_path, "/BgmCache.bin_%.2i", i);

                file_to_buf(fsMakePath(PATH_ASCII, "/BgmCache.bin"), ArchiveThemeExt, &bgm);

                remake_file(bgm_path, ArchiveSD, bgm_size);
                buf_to_file(bgm_size, bgm_path, ArchiveSD, bgm);

                free(bgm_path);
                free(bgm);
            }
        }
        else
        {
            char * body = NULL;
            char * body_path = NULL;
            res = (Result)asprintf(&body_path, "%s/BodyCache.bin", folder_path);
            if(res == -1) goto end;

            u32 body_size = file_to_buf(fsMakePath(PATH_ASCII, "/BodyCache.bin"), ArchiveThemeExt, &body);
            if(body_size == single_body_size)
            {
                remake_file(body_path, ArchiveSD, body_size);
                buf_to_file(body_size, body_path, ArchiveSD, body);
            }
            free(body);

            char * bgm = NULL;
            char * bgm_path = NULL;
            res = (Result)asprintf(&bgm_path, "%s/BgmCache.bin", folder_path);
            if(res == -1) goto end;

            u32 bgm_size = file_to_buf(fsMakePath(PATH_ASCII, "/BgmCache.bin"), ArchiveThemeExt, &bgm);
            if(bgm_size == single_bgm_size)
            {
                remake_file(bgm_path, ArchiveSD, bgm_size);
                buf_to_file(bgm_size, bgm_path, ArchiveSD, bgm);
            }
            free(bgm);
        }


        res = (Result)asprintf(&smdh_path, "%s/info.smdh", folder_path);
        if(res == -1) goto end;

        Icon_s smdh = {0};
        struacat(smdh.name, "Dumped theme");
        struacat(smdh.author, "Unkown author");
        struacat(smdh.desc, smdh_desc);

        u32 color = RGB565(rand() % 255, rand() % 255, rand() % 255);
        for(int i = 0; i < 48*48; i++)
        {
            smdh.big_icon[i] = color;
        }

        remake_file(smdh_path, ArchiveSD, sizeof(Icon_s));
        buf_to_file(sizeof(Icon_s), smdh_path, ArchiveSD, (char*)&smdh);

        res = 0;

        end:
        free(folder_path);
        free(smdh_path);
        free(smdh_desc);

        if(!shuffle)
            break;
    }

    FSFILE_Close(body_cache_handle);

    return res;
}

void themes_check_installed(void * void_arg)
{
    Thread_Arg_s * arg = (Thread_Arg_s *)void_arg;
    Entry_List_s * list = (Entry_List_s *)arg->thread_arg;
    if(list == NULL || list->entries == NULL) return;

    #ifndef CITRA_MODE
    char* savedata_buf = NULL;
    u32 savedata_size = file_to_buf(fsMakePath(PATH_ASCII, "/SaveData.dat"), ArchiveHomeExt, &savedata_buf);
    if(!savedata_size) return;
    SaveData_dat_s* savedata = (SaveData_dat_s*)savedata_buf;
    bool shuffle = savedata->shuffle;
    free(savedata_buf);

    #define HASH_SIZE_BYTES 256/8
    u8 body_hash[MAX_SHUFFLE_THEMES][HASH_SIZE_BYTES];
    memset(body_hash, 0, MAX_SHUFFLE_THEMES*HASH_SIZE_BYTES);

    char* thememanage_buf = NULL;
    u32 theme_manage_size = file_to_buf(fsMakePath(PATH_ASCII, "/ThemeManage.bin"), ArchiveThemeExt, &thememanage_buf);
    if(!theme_manage_size) return;
    ThemeManage_bin_s * theme_manage = (ThemeManage_bin_s *)thememanage_buf;

    u32 single_body_size = theme_manage->body_size;
    u32 shuffle_body_sizes[MAX_SHUFFLE_THEMES] = {0};
    memcpy(shuffle_body_sizes, theme_manage->shuffle_body_sizes, sizeof(u32)*MAX_SHUFFLE_THEMES);
    free(thememanage_buf);

    if(shuffle)
    {
        char * body_buf = NULL;
        u32 body_cache_size = file_to_buf(fsMakePath(PATH_ASCII, "/BodyCache_rd.bin"), ArchiveThemeExt, &body_buf);
        if(!body_cache_size) return;

        for(int i = 0; i < MAX_SHUFFLE_THEMES; i++)
        {
            FSUSER_UpdateSha256Context(body_buf + BODY_CACHE_SIZE*i, shuffle_body_sizes[i], body_hash[i]);
        }

        free(body_buf);
    }
    else
    {
        char * body_buf = NULL;
        u32 body_size = file_to_buf(fsMakePath(PATH_ASCII, "/BodyCache.bin"), ArchiveThemeExt, &body_buf);
        if(!body_size) return;

        u8 * hash = body_hash[0];
        FSUSER_UpdateSha256Context(body_buf, single_body_size, hash);
        free(body_buf);
    }

    int total_installed = 0;
    for(int i = 0; i < list->entries_count && total_installed < MAX_SHUFFLE_THEMES && arg->run_thread; i++)
    {
        Entry_s * theme = &list->entries[i];
        char * theme_body = NULL;
        u32 theme_body_size = load_data("/body_LZ.bin", *theme, &theme_body);
        if(!theme_body_size) return;

        u8 theme_body_hash[HASH_SIZE_BYTES];
        FSUSER_UpdateSha256Context(theme_body, theme_body_size, theme_body_hash);
        free(theme_body);

        for(int j = 0; j < MAX_SHUFFLE_THEMES; j++)
        {
            if(!memcmp(body_hash[j], theme_body_hash, HASH_SIZE_BYTES))
            {
                theme->installed = true;
                total_installed++;
                if(!shuffle) break; //only need to check the first if the installed theme inst shuffle
            }
        }
    }
    #endif
}
