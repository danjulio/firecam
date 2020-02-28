/*
 * App Task
 *
 * Implement the application logic for firecam.  The program's maestro. 
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
#ifndef APP_TASK_H
#define APP_TASK_H

#include <stdbool.h>
#include <stdint.h>



//
// App Task constants
//

// App Task notifications
#define APP_NOTIFY_SHUTDOWN_MASK        0x00000001
#define APP_NOTIFY_NEW_WIFI_MASK        0x00000002
#define APP_NOTIFY_SDCARD_PRESENT_MASK  0x00000004
#define APP_NOTIFY_SDCARD_MISSING_MASK  0x00000008
#define APP_NOTIFY_RECORD_BTN_MASK      0x00000010
#define APP_NOTIFY_START_RECORD_MASK    0x00000020
#define APP_NOTIFY_STOP_RECORD_MASK     0x00000040
#define APP_NOTIFY_RECORD_START_MASK    0x00000100
#define APP_NOTIFY_RECORD_NOSTART_MASK  0x00000200
#define APP_NOTIFY_RECORD_FAIL_MASK     0x00000400
#define APP_NOTIFY_RECORD_IMG_DONE_MASK 0x00000800
#define APP_NOTIFY_CAM_FRAME_MASK       0x00001000
#define APP_NOTIFY_CAM_FAIL_MASK        0x00002000
#define APP_NOTIFY_LEP_FRAME_MASK       0x00004000
#define APP_NOTIFY_LEP_FAIL_MASK        0x00008000
#define APP_NOTIFY_GUI_CAM_DONE_MASK    0x00010000
#define APP_NOTIFY_GUI_LEP_DONE_MASK    0x00020000
#define APP_NOTIFY_CMD_REQ_MASK         0x00040000
#define APP_NOTIFY_CMD_DONE_MASK        0x00080000



//
// App Task API
//
void app_task();
bool app_task_get_recording();
 
#endif /* APP_TASK_H */