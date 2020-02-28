/*
 * System related utilities
 *
 * Contains functions to initialize the system, other utility functions, a set
 * of globally available handles for the various tasks (to use for task notifications).
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
#ifndef SYS_UTILITIES_H
#define SYS_UTILITIES_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_config.h"
#include <stdbool.h>
#include <stdint.h>


//
// System Utilities typedefs
//
typedef struct {
	uint32_t cam_buffer_len;
	uint8_t* cam_bufferP;
} cam_buffer_t;

typedef struct {
	float lep_aux_temp;
	float lep_fpa_temp;
	uint32_t lep_gain_mode;
	uint16_t* lep_bufferP;
} lep_buffer_t;

typedef struct {
	uint32_t length;
	char* bufferP;
} json_image_string_t;



//
// System Utilities macros
//
#define Notification(var, mask) ((var & mask) == mask)



//
// Task handle externs for use by tasks to communicate with each other
//
extern TaskHandle_t task_handle_adc;
extern TaskHandle_t task_handle_app;
extern TaskHandle_t task_handle_cam;
extern TaskHandle_t task_handle_cmd;
extern TaskHandle_t task_handle_file;
extern TaskHandle_t task_handle_gui;
extern TaskHandle_t task_handle_lep;
#ifdef INCLUDE_SYS_MON
extern TaskHandle_t task_handle_mon;
#endif

//
// Global buffer pointers for memory allocated in the external SPIRAM
//

// Shared memory data structures
extern cam_buffer_t sys_cam_buffer;   // Loaded by cam_task with jpeg data for other tasks
extern lep_buffer_t sys_lep_buffer;   // Loaded by lep_task for other tasks
extern json_image_string_t sys_image_file_buffer;   // Loaded by app_task with image data for file_task
extern json_image_string_t sys_cmd_response_buffer; // Loaded by app_task with image data for cmd_task

// Big buffers
extern uint16_t* gui_cam_bufferP;    // Loaded by gui_task for its own use
extern uint16_t* gui_lep_bufferP;    // Loaded by gui_task for its own use



//
// System Utilities API
//
bool system_esp_io_init();
bool system_peripheral_init();
bool system_buffer_init();
void system_shutoff();
void system_lock_vspi();
void system_unlock_vspi();
 
#endif /* SYS_UTILITIES_H */