/*
*   This file is part of Anemone3DS
*   Copyright (C) 2016-2018 Contributors in CONTRIBUTORS.md
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

#ifndef MENU_H
#define MENU_H

#include "common.h"
#include "entry.h"
#include "draw.h"

enum MenuType {
    MODE_THEMES = 0,
    MODE_SPLASHES,
    MODE_BADGES,

    MODES_AMOUNT
};

enum MenuActionReturn {
    RETURN_NONE = 0,

    RETURN_CHANGE_TO_PREVIOUS_MODE,
    RETURN_CHANGE_TO_NEXT_MODE,

    RETURN_CHANGE_TO_THEME_MODE,
    RETURN_CHANGE_TO_SPLASH_MODE,
    RETURN_CHANGE_TO_BADGE_MODE,

    RETURN_CHANGE_TO_BROWSER_MODE,
    RETURN_CHANGE_TO_LIST_MODE,

    RETURN_MOVE_SLEEP,

    MENU_ACTION_AMOUNT
};

enum SortType {
    SORT_NAME,
    SORT_AUTHOR,
    SORT_FILENAME,

    SORTS_AMOUNT
};

enum RemoteSortType {
    SORT_NEWEST,
    SORT_DOWNLOAD_COUNT,
    SORT_LIKE_COUNT,
};

union Instructions {
    InstructionType array[8];
    struct separate {
        InstructionType a, b, x, y, up, left, down, right;
    };
};

using KeysActions = std::vector<std::pair<u32, std::function<MenuActionReturn()>>>;

extern Instructions normal_actions_instructions;

class MenuBase {
    public:
        virtual void draw() = 0;
        virtual void calculate_new_scroll() = 0;
        bool in_preview();
        MenuActionReturn exit_mode_controls();
        MenuActionReturn load_preview();
        MenuActionReturn exit_preview();

        std::stack<const KeysActions*> current_actions_down;
        std::stack<const KeysActions*> current_actions_held;

        bool in_instructions = false;
        bool should_scroll = false;

        u32 background_color;

        void toggle_instructions_mode();
        void draw_instructions();
        MenuActionReturn set_instruction_screen_to_left();
        MenuActionReturn set_instruction_screen_to_right();

    protected:
        MenuBase(const std::string& loading_path, int icon_size, u32 background_color);
        std::stack<const Instructions*> instructions_stack;
        void change_selected_entry(int delta);
        std::string path;
        int icon_size;

        size_t scroll = 0;
        size_t new_scroll = 0;
        size_t selected_entry = 0;
        size_t previous_selected_entry = 0;
        int change;
        bool instruction_screen_right;

        std::unique_ptr<PreviewImage> preview;
        std::unique_ptr<PreviewImage> previous_preview;
        Entry* selected_entry_for_previous_preview = nullptr;

        std::vector<std::unique_ptr<Entry>> entries;
};

class Menu : public MenuBase {
    public:
        void scroll_icons(const Handle* scroll_ready_to_draw_event);
        bool needs_thread();
        void draw();
        void calculate_new_scroll();
        MenuActionReturn sort(SortType sort_type);

        MenuActionReturn select_previous_entry();
        MenuActionReturn select_previous_page();
        MenuActionReturn select_next_entry();
        MenuActionReturn select_next_page();

        MenuActionReturn select_previous_entry_fast();
        MenuActionReturn select_previous_page_fast();
        MenuActionReturn select_next_entry_fast();
        MenuActionReturn select_next_page_fast();

        MenuActionReturn change_to_previous_mode();
        MenuActionReturn change_to_next_mode();

        MenuActionReturn change_to_extra_mode();
        MenuActionReturn change_to_qr_scanner();
        MenuActionReturn handle_touch();

    protected:
        Menu(const std::string& loading_path, size_t icons_per_screen, TextID mode_indicator_id, TextID previous_mode_indicator_id, TextID next_mode_indicator_id, int icon_size, u32 background_color, bool badge_menu = false);
        void load_icons();

        size_t icons_per_screen;
        std::vector<std::unique_ptr<EntryIcon>> icons;

        TextID mode_indicator_id;
        TextID previous_mode_indicator_id;
        TextID next_mode_indicator_id;
};

// Screw badges, get them from QRs
class RemoteMenu : public MenuBase {
    public:
        void draw();
        void calculate_new_scroll();

    protected:
        RemoteMenu();
        void load_icons();
        std::array<std::array<std::unique_ptr<EntryIcon>, 4>, 6> icons;
};

#endif
