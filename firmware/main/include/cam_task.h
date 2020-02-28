/*
 * ArduCAM Task
 *
 * Contains functions to initialize the ArduCAM and then sampling images from it,
 * making those available to other tasks through a shared buffer and event
 * interface.
 *
 * Note: The ArduCAM shares its SPI bus with other peripherals.  Activity on the SPI
 * bus directed at other devices while we are offloading an image seems to confuse the
 * ArduCAM so although the ESP32 IDF SPI driver provides access control, we lock the SPI
 * bus during the entire image offload process.
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
#ifndef CAM_TASK_H
#define CAM_TASK_H

#include <stdint.h>



//
// CAM Task Constants
//

// Task wait time while waiting for the ArduCAM to complete a jpeg snapshot
#define CAM_JPEG_TASK_WAIT_MSEC     10

// CAM Max wait time for ArduCAM to complete a jpeg snapshot
#define CAM_MAX_JPEG_WAIT_TIME_MSEC 300

// CAM Task notifications
#define CAM_NOTIFY_GET_FRAME_MASK 0x00000001



//
// CAM Task API
//
void cam_task();

#endif /* CAM_TASK_H */