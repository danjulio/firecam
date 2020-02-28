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
#include <stdint.h>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_task.h"
#include "cam_task.h"
#include "ov2640.h"
#include "sys_utilities.h"
#include "system_config.h"

//
// CAM variables
//

static const char* TAG = "cam_task";



//
// CAM Task API
//

/**
 * This task drives the ArduCAM camera interface.
 */
void cam_task()
{
	int wait_count;
	uint32_t notification_value;
	
	ESP_LOGI(TAG, "Start task");
	
	// Configure the camera
	ov2640_setJPEGSize(CAM_SIZE_SPEC);
	ov2640_set_Light_Mode(Sunny);
	
	while (1) {
		// Block waiting for a request for a frame
		xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, portMAX_DELAY);

		// Take a picture;
		ov2640_capture();
		
		// Wait for the image to be captured
		wait_count = CAM_MAX_JPEG_WAIT_TIME_MSEC / CAM_JPEG_TASK_WAIT_MSEC;
		while (!ov2640_getBit(ARDUCHIP_TRIG, CAP_DONE_MASK) && (wait_count-- > 0)) {
			vTaskDelay(pdMS_TO_TICKS(CAM_JPEG_TASK_WAIT_MSEC));
		}
		if (wait_count == 0) {
			ESP_LOGE(TAG, "jpeg image not captured in time");
		}
			
		// Get the jpeg image into the shared buffer
		// Lock the SPI bus so no other task can interrupt us offloading the image
		system_lock_vspi();
		ov2640_transferJpeg(sys_cam_buffer.cam_bufferP, &sys_cam_buffer.cam_buffer_len);
		system_unlock_vspi();
		
		if (sys_cam_buffer.cam_buffer_len == 0) {
			ESP_LOGE(TAG, "Could not get jpeg image");
			// Let app_task know we failed to update the buffer
			xTaskNotify(task_handle_app, APP_NOTIFY_CAM_FAIL_MASK, eSetBits);
		} else {
			// Let app_task know we've updated the buffer
			xTaskNotify(task_handle_app, APP_NOTIFY_CAM_FRAME_MASK, eSetBits);
			//ESP_LOGI(TAG, "image size = %d", sys_cam_buffer.cam_buffer_len);
		}
	}
}