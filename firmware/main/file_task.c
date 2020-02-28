/*
 * File Task
 *
 * Handle the SD Card and manage writing files for app_task.  Allows file writing time
 * to vary (it increases as the number of files or directories in a directory has to be
 * traversed before creating a new item).
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
#include "file_task.h"
#include "app_task.h"
#include "file_utilities.h"
#include "sys_utilities.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"


//
// File Task private constants
//
#define FILE_EVAL_MSEC 50



//
// File Task private variables
//
static const char* TAG = "file_task";

// Counter used to probe card for presence 
static int card_check_count = FILE_CARD_CHECK_PERIOD_MSEC / FILE_EVAL_MSEC;
static bool recording;
static char* rec_dir_name;
static uint16_t rec_seq_num = 0;


//
// File Task Forward Declarations for internal functions
//
static void handle_notifications();
static void update_card_present_info();
static bool setup_recording_session();
static bool write_image_file();


//
// File Task API
//
void file_task()
{
	ESP_LOGI(TAG, "Start task");
	
	// Try to initialize a SD Card to see if one is there.
	if (file_init_card()) {
		ESP_LOGI(TAG, "SD Card found");
		
		// Notify app_task
		xTaskNotify(task_handle_app, APP_NOTIFY_SDCARD_PRESENT_MASK, eSetBits);
		
		// Mount it briefly to force a format if necessary, then unmount it.
		if (file_mount_sdcard()) {
			file_unmount_sdcard();
		}
	} else {
		xTaskNotify(task_handle_app, APP_NOTIFY_SDCARD_MISSING_MASK, eSetBits);
		ESP_LOGI(TAG, "No SD Card found");
	}
	
	// Loop handling notifications and file operation requests
	while (1) {
		handle_notifications();
		update_card_present_info();
	
		vTaskDelay(pdMS_TO_TICKS(FILE_EVAL_MSEC));
	}
}



//
// File Task internal functions
//

/**
 * Process notifications from other tasks
 */
static void handle_notifications()
{
	uint32_t notification_value;
	
	notification_value = 0;
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
	
		if (Notification(notification_value, FILE_NOTIFY_START_RECORDING_MASK)) {
			if (setup_recording_session()) {
				xTaskNotify(task_handle_app, APP_NOTIFY_RECORD_START_MASK, eSetBits);
			} else {
				xTaskNotify(task_handle_app, APP_NOTIFY_RECORD_NOSTART_MASK, eSetBits);
			}
		}
		
		if (Notification(notification_value, FILE_NOTIFY_NEW_IMAGE_MASK)) {
			if (write_image_file()) {
				xTaskNotify(task_handle_app, APP_NOTIFY_RECORD_IMG_DONE_MASK, eSetBits);
			} else {
				xTaskNotify(task_handle_app, APP_NOTIFY_RECORD_FAIL_MASK, eSetBits);
			}		  
		}
		
		if (Notification(notification_value, FILE_NOTIFY_STOP_RECORDING_MASK)) {
			recording = false;
			rec_seq_num = 0;
			file_unmount_sdcard();
			ESP_LOGI(TAG, "End recording session");
		}
	}
}


/**
 * Handle card insertion/removal detection.  Initialize the a new card.  Update the
 * card present status available from file_utilities and notify the app_task of changes.
 */
static void update_card_present_info()
{
	if (--card_check_count == 0) {
		if (!recording) {
			if (file_get_card_present()) {
				// Make sure it's still there
				if (!file_check_card_still_present()) {
					xTaskNotify(task_handle_app, APP_NOTIFY_SDCARD_MISSING_MASK, eSetBits);
					ESP_LOGI(TAG, "SD Card detected removed");
				}
			} else {
				// See if one has shown up
				if (file_check_card_inserted()) {
					// See if we can initialize it
					if (file_reinit_card()) {
						xTaskNotify(task_handle_app, APP_NOTIFY_SDCARD_PRESENT_MASK, eSetBits);
						ESP_LOGI(TAG, "SD Card detected inserted");
					}
				}
			}
		}
		
		card_check_count = FILE_CARD_CHECK_PERIOD_MSEC / FILE_EVAL_MSEC;
	}
}


/**
 * Setup a recording session - initialize (if necessary), mount the sd card and create
 * the session directory.
 */
static bool setup_recording_session()
{
	if (file_get_card_present()) {
		if (file_mount_sdcard()) {
			rec_dir_name = file_get_session_directory_name();
			if (file_create_directory(rec_dir_name)) {
				recording = true;
				rec_seq_num = 1;
				ESP_LOGI(TAG, "Start recording session: %s", rec_dir_name);
				return true;
			} else {
				ESP_LOGE(TAG, "Could not create session directory");
			}
		} else {
			ESP_LOGE(TAG, "Could not mount the SD Card");
		}
	} else {
		ESP_LOGE(TAG, "Attempted to start recording when SD Card not present - internal logic error");
	}
	
	return false;
}


/**
 * Create and write out an image file from the shared image buffer
 */
static bool write_image_file()
{
	bool err = false;
	char* image_json_text;
	FILE* fp;
	int write_ret;
	int len;
	uint32_t image_json_len;
	uint32_t byte_offset;
	
	image_json_text = sys_image_file_buffer.bufferP;
	image_json_len = sys_image_file_buffer.length;
	
	if (file_open_image_write_file(rec_dir_name, rec_seq_num, &fp)) {
		byte_offset = 0;
		while (byte_offset < image_json_len) {
			// Determine maximum bytes to write
			len = image_json_len - byte_offset;
			if (len > MAX_FILE_WRITE_LEN) len = MAX_FILE_WRITE_LEN;
			
			write_ret = fwrite(&image_json_text[byte_offset], 1, len, fp);
			if (write_ret < 0) {
				ESP_LOGE(TAG, "Error in file write - %d", write_ret);
				err = true;
				break;
			}
			byte_offset += write_ret;
		}
	
		file_close_file(fp);
		rec_seq_num++;
	} else {
		ESP_LOGE(TAG, "Could not open file for writing");
		err = true;
	}
	
	return !err;
}

