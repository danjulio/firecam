/*
 * App Task
 *
 * Implement the application logic for firecam.  The program's maestro. 
 *
 * Copyright 2020-2022 Dan Julio
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
#include "app_task.h"
#include "cam_task.h"
#include "cmd_task.h"
#include "file_task.h"
#include "gui_task.h"
#include "lep_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "file_utilities.h"
#include "gui_utilities.h"
#include "json_utilities.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "time_utilities.h"
#include "system_config.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>



//
// App Task private constants
//

#define APP_EVAL_MSEC 50

// Maximum wait period within a one second window to see both images before processing
// whatever we have.  Should be divisible by APP_EVAL_MSEC.
#define APP_MAX_WAIT_MSEC 800

// Uncomment to trace image timing
//#define APP_DEBUG_IMG


enum app_state_t {
	WAIT_TOS,
	WAIT_IMAGE
};

enum app_image_request_state_t {
	IDLE,
	REQUESTED,
	RECEIVED,
	FAILED
};



//
// App Task variables
//
static const char* TAG = "app_task";

static enum app_state_t app_state = IDLE;
static time_t app_prev_time;  // Used to detect second intervals

static enum app_image_request_state_t cam_image_request_state = IDLE;
static enum app_image_request_state_t lep_image_request_state = IDLE;

static bool cam_gui_update_pending = false;
static bool lep_gui_update_pending = false;

static bool sdcard_present = false;    // Can't start recording unless a card is present
static bool app_recording = false;
static bool file_image_send_pending = false;
static bool app_rec_arducam_en;
static bool app_rec_lepton_en;
static uint16_t app_rec_seq_num = 0;
static uint16_t app_rec_interval;      // Seconds between images when recording
static uint16_t app_rec_interval_cnt;  // Counts interval up to app_rec_interval to trigger picture

static bool cmd_requesting_image = false;
static bool cmd_image_send_pending = false;


//
// App Task Forward Declarations for internal functions
//
static void app_task_handle_notifications();
static void app_task_start_recording(bool from_gui);
static void app_task_stop_recording(bool en_restart);
static void app_process_images(bool valid_cam, bool valid_lep);


//
// App Task API
//

void app_task()
{
	int msec_count = 0;
	
	ESP_LOGI(TAG, "Start task");
	
	// Let other tasks start running first
	vTaskDelay(pdMS_TO_TICKS(100));
	
	// Get initial recording values
	app_rec_arducam_en = gui_st.rec_arducam_enable;
	app_rec_lepton_en = gui_st.rec_lepton_enable;
	app_rec_interval = gui_st.record_interval;
	app_rec_interval_cnt = 0;
	
	// If we were recording when we last powered down (e.g. crashed and rebooted) then
	// notify ourselves to start recording again immediately.
	if (ps_get_rec_enable()) {
		ESP_LOGI(TAG, "Restarting recording on powerup");
		xTaskNotify(task_handle_app, APP_NOTIFY_START_RECORD_MASK, eSetBits);
	}
	
	// app_task distributes activities over a one second interval in order to spread 
	// time-consuming activities out over time.  It evaluates on APP_EVAL_MSEC intervals.
	// While recording it prioritizes getting a file written every second, even if there
	// is not an image from one of the cameras (e.g. the lepton is performing a FFC and
	// has stalled its VoSPI pipeline).  Because of this image files may contain 2, 1 or
	// 0 images but they always have some metadata.
	//
	//   1. At the beginning of each second it requests the cameras get an image
	//      a. It will not request an image if the GUI is still displaying previous
	//         images since it shares the image buffer with the GUI.  The GUI should
	//         normally never fail to process previous images before the start of a new
	//         second.
	//   2. Up to APP_MAX_WAIT_MSEC mSec it checks to see if has received both images.
	//      If it has:
	//      a. It generates a json text file with metadata and available images.
	//      b. It writes the file if it is recording and a file operation is not already
	//         pending.
	//      c. It initiates an image response using the file data through the cmd_task if
	//         there is a pending request for one.
	//   3. At APP_MAX_WAIT_MSEC mSec it generates a json text file with metadata and any
	//      image it has received and writes the file if recording if possible and initiates
	//      an image response if one is pending.
	//   4. It initiates image updates in the GUI as they are received from the camera
	//      tasks.
	//   5. It handles other notifications as they are received.
	//
	while (1) {
		// Look for notifications to act on
		app_task_handle_notifications();
		
		switch (app_state) {
			case WAIT_TOS:
				// Look for the start of our one-second evaluation interval
				if (time_changed(NULL, &app_prev_time)) {
#ifdef APP_DEBUG_IMG
					ESP_LOGI(TAG, "TOS");
#endif
					msec_count = 0;
					app_state = WAIT_IMAGE;
	
					if (!cam_gui_update_pending) {
						// Request cam_task update the shared buffer with a new image when available
						xTaskNotify(task_handle_cam, CAM_NOTIFY_GET_FRAME_MASK, eSetBits);
						cam_image_request_state = REQUESTED;
#ifdef APP_DEBUG_IMG
						ESP_LOGI(TAG, "  Req Cam");
#endif
					} else {
						cam_image_request_state = IDLE;
					}
					if (!lep_gui_update_pending) {
						// Request lep_task update the shared buffer with a new image when available
						xTaskNotify(task_handle_lep, LEP_NOTIFY_GET_FRAME_MASK, eSetBits);
						lep_image_request_state = REQUESTED;
#ifdef APP_DEBUG_IMG
						ESP_LOGI(TAG, "  Req Lep");
#endif
					} else {
						lep_image_request_state = IDLE;
					}
				}
				break;
			
			case WAIT_IMAGE:
				if ((cam_image_request_state == RECEIVED) && (lep_image_request_state == RECEIVED)) {
				    // Handle demand for the images as soon as possible, prioritizing recording
					if ((!file_image_send_pending && app_recording) ||
						(!app_recording && !cmd_image_send_pending && cmd_requesting_image)) {

						app_process_images(true, true);
#ifdef APP_DEBUG_IMG
						ESP_LOGI(TAG, "Process image early");
#endif
					}
					app_state = WAIT_TOS;
				} else if (msec_count >= APP_MAX_WAIT_MSEC) {
					// At the end of the period, handle whatever we have
					if (app_recording || cmd_requesting_image) {
						app_process_images((cam_image_request_state == RECEIVED),
						                   (lep_image_request_state == RECEIVED));
#ifdef APP_DEBUG_IMG
						ESP_LOGI(TAG, "Process image: cam = %d, lep = %d", cam_image_request_state == RECEIVED, lep_image_request_state == RECEIVED);
#endif
					}
					app_state = WAIT_TOS;
				}
				break;
		}
	
		vTaskDelay(pdMS_TO_TICKS(APP_EVAL_MSEC));
		msec_count += APP_EVAL_MSEC;
	}
}


/**
 * Return the recording state
 */
bool app_task_get_recording()
{
	return app_recording;
}



//
// App Task internal functions
//

/**
 * Process notifications from other tasks
 */
static void app_task_handle_notifications()
{
	uint32_t notification_value;
	
	notification_value = 0;
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		//
		// SHUTDOWN
		//
		if (Notification(notification_value, APP_NOTIFY_SHUTDOWN_MASK)) {
			// Stop recording if it is in process
			if (app_recording) {
				app_task_stop_recording(false);
			}
			
			// Notify the gui_task to display the shutdown screen.  Wait a short bit
			// to give it a chance to be displayed and then shut off our power. This will
			// turn off the LCD backlight.  Note that the user may still be holding the
			// power button and keeping us alive so just spin in a loop after that waiting
			// for power to go away.
			xTaskNotify(task_handle_gui, GUI_NOTIFY_SHUTDOWN_MASK, eSetBits);
			vTaskDelay(pdMS_TO_TICKS(1500));
			system_shutoff();
			while (1) {
				vTaskDelay(pdMS_TO_TICKS(1000));
			}
		}
		
		//
		// ARDUCAM
		//
		if (Notification(notification_value, APP_NOTIFY_CAM_FRAME_MASK)) {
			// cam_task has updated the shared buffer with a new image
			cam_image_request_state = RECEIVED;
			if (!cam_gui_update_pending) {
				// Notify the GUI to update
				xTaskNotify(task_handle_gui, GUI_NOTIFY_CAM_FRAME_MASK, eSetBits);
				cam_gui_update_pending = true;
#ifdef APP_DEBUG_IMG
				ESP_LOGI(TAG, "Got cam image");
#endif
			}
		}
		
		if (Notification(notification_value, APP_NOTIFY_CAM_FAIL_MASK)) {
			// cam_task failed to get an image and update the shared buffer
			cam_image_request_state = FAILED;
		}
			
		if (Notification(notification_value, APP_NOTIFY_GUI_CAM_DONE_MASK)) {
			// GUI has consumed the shared buffer
			cam_gui_update_pending = false;
		}
		
		//
		// LEPTON
		//	
		if (Notification(notification_value, APP_NOTIFY_LEP_FRAME_MASK)) {
			// lep_task has updated the shared buffer with a new image
			lep_image_request_state = RECEIVED;
			if (!lep_gui_update_pending) {
				// Notify the GUI to update
				xTaskNotify(task_handle_gui, GUI_NOTIFY_LEP_FRAME_MASK, eSetBits);
				lep_gui_update_pending = true;
#ifdef APP_DEBUG_IMG
				ESP_LOGI(TAG, "Got lep image");
#endif
			}
		}
		
		if (Notification(notification_value, APP_NOTIFY_LEP_FAIL_MASK)) {
			// lep_task failed to get an image and update the shared buffer
			lep_image_request_state = FAILED;
		}
		
		if (Notification(notification_value, APP_NOTIFY_GUI_LEP_DONE_MASK)) {
			// GUI has consumed the shared buffer
			lep_gui_update_pending = false;
		}
		
		//
		// RECORD BUTTON CONTROL
		//
		if (Notification(notification_value, APP_NOTIFY_RECORD_BTN_MASK)) {
			// Set recording state
			if (app_recording) {
				app_task_stop_recording(false);
			} else {
				app_task_start_recording(true);
			}
		}
		
		//
		// RECORDING PARAMETERS
		//
		if (Notification(notification_value, APP_NOTIFY_RECORD_PARM_UPD_MASK)) {
			app_rec_arducam_en = gui_st.rec_arducam_enable;
			app_rec_lepton_en = gui_st.rec_lepton_enable;
			app_rec_interval = gui_st.record_interval;
		}
		
		//
		// FILE OPERATIONS
		//
		if (Notification(notification_value, APP_NOTIFY_SDCARD_PRESENT_MASK)) {
			sdcard_present = true;
		}
		
		if (Notification(notification_value, APP_NOTIFY_SDCARD_MISSING_MASK)) {
			sdcard_present = false;
		}
		
		if (Notification(notification_value, APP_NOTIFY_RECORD_START_MASK)) {
			// file_task has initiated recording
			app_recording = true;
			app_rec_seq_num = 1;
			app_rec_interval_cnt = 0;
			ps_set_rec_enable(true);
			xTaskNotify(task_handle_gui, GUI_NOTIFY_LED_ON_MASK, eSetBits);
		}
		
		if (Notification(notification_value, APP_NOTIFY_RECORD_NOSTART_MASK)) {
			// Not currently implemented
		}
		
		if (Notification(notification_value, APP_NOTIFY_RECORD_FAIL_MASK)) {
			app_task_stop_recording(true);
		}
		
		if (Notification(notification_value, APP_NOTIFY_RECORD_IMG_DONE_MASK)) {
			// file_task has consumed the image buffer
			file_image_send_pending = false;
			
			// Bump the count if we're recording (we will get a final image done
			// notification after recording is ended for the last image and we don't
			// want to increment any counters then)
			if (app_recording) {
				app_rec_seq_num++;
				xTaskNotify(task_handle_gui, GUI_NOTIFY_INC_REC_MASK, eSetBits);
			}
		}
		
		//
		// COMMAND CONTROL
		//
		if (Notification(notification_value, APP_NOTIFY_START_RECORD_MASK)) {
			// Start recording command
			app_task_start_recording(false);
		}
		
		if (Notification(notification_value, APP_NOTIFY_STOP_RECORD_MASK)) {
			// Stop recording command
			app_task_stop_recording(false);
		}
		
		if (Notification(notification_value, APP_NOTIFY_CMD_REQ_MASK)) {
			// cmd_task is requesting an image
			cmd_requesting_image = true;
		}
		
		if (Notification(notification_value, APP_NOTIFY_CMD_DONE_MASK)) {
			// cmd_task is done using the image it requested
			cmd_image_send_pending = false;
		}
		
		//
		// WIFI CONFIGURATION
		//
		if (Notification(notification_value, APP_NOTIFY_NEW_WIFI_MASK)) {
			// Reconfigure WiFi
			if (!wifi_reinit()) {
				// Let the user know we couldn't start recording
				gui_preset_message_box_string("Could not restart WiFi with the new configuration");
				xTaskNotify(task_handle_gui, GUI_NOTIFY_MESSAGEBOX_MASK, eSetBits);
			}				
		}
	}
}


static void app_task_start_recording(bool from_gui)
{
	if (!app_recording) {
		// Make sure there's an SD Card
		if (sdcard_present) {
			// Request file_task start a recording session
			xTaskNotify(task_handle_file, FILE_NOTIFY_START_RECORDING_MASK, eSetBits);
		} else {
			if (from_gui) {
				// Let the user know we couldn't start recording
				gui_preset_message_box_string("Please insert a SD Card");
				xTaskNotify(task_handle_gui, GUI_NOTIFY_MESSAGEBOX_MASK, eSetBits);
			}
		}
	}
}


static void app_task_stop_recording(bool en_restart)
{
	if (app_recording) {
		app_recording = false;
		app_rec_seq_num = 0;
		app_rec_interval_cnt = 0;
	
		xTaskNotify(task_handle_file, FILE_NOTIFY_STOP_RECORDING_MASK, eSetBits);
		xTaskNotify(task_handle_gui, GUI_NOTIFY_LED_OFF_MASK, eSetBits);
		xTaskNotify(task_handle_gui, GUI_NOTIFY_CLR_REC_MASK, eSetBits);
		
		if (!en_restart) {
			// Normal recording stop
			ps_set_rec_enable(false);
		} else {
			// Something went wrong so we reboot hoping we'll be able to start recording
			// again successfully
			ESP_LOGE(TAG, "Recording session failed - rebooting system");
			app_task_stop_recording(true);
			vTaskDelay(pdMS_TO_TICKS(10));
			esp_restart();
		}
	}
}


static void app_process_images(bool valid_cam, bool valid_lep)
{
	bool process_cam;
	bool process_lep;
	char* image_json_text;
	uint32_t image_json_len;
	
	// Determine what images to process
	process_cam = valid_cam && (!app_recording || (app_recording && app_rec_arducam_en));
	process_lep = valid_lep && (!app_recording || (app_recording && app_rec_lepton_en));
	
	// Get the image json text string
	image_json_text = json_get_image_file_string(app_rec_seq_num, process_cam, process_lep,
	                                             &image_json_len);
	
	// Send it to file_task for writing if we are recording and a send not already pending
	if (app_recording) {
		if (++app_rec_interval_cnt >= app_rec_interval) {
			if (!file_image_send_pending && app_recording) {
				// Record image
				app_rec_interval_cnt = 0;
				if (image_json_len < JSON_MAX_IMAGE_TEXT_LEN) {
					// Copy the image to the shared buffer for file_task
					memcpy(sys_image_file_buffer.bufferP, image_json_text, image_json_len);
					sys_image_file_buffer.length = image_json_len;
			
					// Notify file_task
					xTaskNotify(task_handle_file, FILE_NOTIFY_NEW_IMAGE_MASK, eSetBits);
					file_image_send_pending = true;
				} else {
					ESP_LOGE(TAG, "image_json_text (%d bytes) too large for sys_image_file_buffer", image_json_len);
				}
			}
		}
	}
	
	// Send it to cmd_task if there's a request and a send not already pending
	if (!cmd_image_send_pending && cmd_requesting_image) {
		if (image_json_len < JSON_MAX_IMAGE_TEXT_LEN-2) {
			// Copy the image to the shared buffer for cmd_task, adding delimitors
			sys_cmd_response_buffer.bufferP[0] = CMD_JSON_STRING_START;
			memcpy(&sys_cmd_response_buffer.bufferP[1], image_json_text, image_json_len);
			sys_cmd_response_buffer.bufferP[image_json_len+1] = CMD_JSON_STRING_STOP;
			sys_cmd_response_buffer.length = image_json_len + 2;
			
			// Notify cmd_task
			xTaskNotify(task_handle_cmd, CMD_NOTIFY_IMAGE_MASK, eSetBits);
			cmd_image_send_pending = true;
		} else {
			ESP_LOGE(TAG, "image_json_text (%d bytes) too large for sys_cmd_response_buffer", image_json_len);
		}
		cmd_requesting_image = false;
	}
}
