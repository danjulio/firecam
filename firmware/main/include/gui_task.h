/*
 * GUI Task
 *
 * Contains functions to initialize the LittleVGL GUI system and a task
 * to evaluate its display related sub-tasks.  The GUI Task is responsible
 * for all access (updating) of the GUI managed by LittleVGL.
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
#ifndef GUI_TASK_H
#define GUI_TASK_H

#include <stdbool.h>
#include <stdint.h>


//
// GUI Task Constants
//

// Screen indicies
#define GUI_SCREEN_MAIN     0
#define GUI_SCREEN_SETTINGS 1
#define GUI_SCREEN_TIME     2
#define GUI_SCREEN_WIFI     3
#define GUI_SCREEN_NETWORK  4
#define GUI_SCREEN_POWEROFF 5
#define GUI_NUM_SCREENS     6

// GUI Task notifications
#define GUI_NOTIFY_SHUTDOWN_MASK   0x00000001
#define GUI_NOTIFY_LEP_FRAME_MASK  0x00000002
#define GUI_NOTIFY_CAM_FRAME_MASK  0x00000004
#define GUI_NOTIFY_LED_ON_MASK     0x00000010
#define GUI_NOTIFY_LED_OFF_MASK    0x00000020
#define GUI_NOTIFY_INC_REC_MASK    0x00000040
#define GUI_NOTIFY_CLR_REC_MASK    0x00000080
#define GUI_NOTIFY_MESSAGEBOX_MASK 0x00001000


//
// GUI Task API
//
void gui_task();
void gui_set_screen(int n);
 

#endif /* GUI_TASK_H */