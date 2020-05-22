/*
 * Main GUI screen related functions, callbacks and event handlers
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
#ifndef GUI_SCREEN_MAIN_H
#define GUI_SCREEN_MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl/lvgl.h"

//
// Main Screen Constants
//

// ArduCAM display area
#define CAM_IMG_WIDTH  160
#define CAM_IMG_HEIGHT 120
#define CAM_IMG_PIXELS (CAM_IMG_WIDTH * CAM_IMG_HEIGHT)

// Lepton display area
#define LEP_IMG_WIDTH  160
#define LEP_IMG_HEIGHT 120
#define LEP_IMG_PIXELS (LEP_IMG_WIDTH * LEP_IMG_HEIGHT)




//
// Main GUI Screen API
//
lv_obj_t* gui_screen_main_create();
void gui_screen_main_set_active(bool en);
void gui_screen_main_status_update_task(lv_task_t * task);
void gui_screen_main_update_cam_image();
void gui_screen_main_update_lep_image();
void gui_screen_main_update_rec_led(bool en);
void gui_screen_main_update_rec_count(uint16_t c);

#endif /* GUI_SCREEN_MAIN_H */