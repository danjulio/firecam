/*
 * Set Time GUI screen related functions, callbacks and event handlers
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
#ifndef GUI_SCREEN_TIME_H
#define GUI_SCREEN_TIME_H

#include "lvgl/lvgl.h"

//
// Set Time GUI Screen API
//
lv_obj_t* gui_screen_time_create();
void gui_screen_time_set_active(bool en);

#endif /* GUI_SCREEN_TIME_H */