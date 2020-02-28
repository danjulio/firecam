/*
 * Shared utility functions for GUI screens
 *
 * Copyright 2020 Dan Julio
 *
 * This file is part of firecam.
 *
 * firecam is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * firecam is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with firecam.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#ifndef GUI_UTILITY_H
#define GUI_UTILITY_H

#include "lvgl/lvgl.h"

//
// GUI Utilities Constants
//

// Maximum preset message box string length
#define GUI_MSG_BOX_MAX_LEN 128


//
// GUI Utilities API
//
void gui_message_box(lv_obj_t* parent, const char* msg);
void gui_preset_message_box_string(const char* msg);
void gui_preset_message_box(lv_obj_t* parent);

#endif /* GUI_UTILITY_H */