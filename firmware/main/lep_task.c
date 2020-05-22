/*
 * Lepton Task
 *
 * Contains functions to initialize the Lepton and then sampling images from it,
 * making those available to other tasks through a shared buffer and event
 * interface.  This task should be run on the "PRO" core (0) while other application
 * tasks run on the "APP" core (1).
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
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_task.h"
#include "lep_task.h"
#include "cci.h"
#include "vospi.h"
#include "sys_utilities.h"
#include "system_config.h"


//
// LEP Task constants
//



//
// LEP Task variables
//
static const char* TAG = "lep_task";

// Lepton Vsync Interrupt handling
static volatile int64_t vsyncDetectedUsec;



//
// LEP Task API
//

/**
 * This task drives the Lepton camera interface.
 */
void lep_task()
{
	bool done;
	uint32_t notification_value;
	uint32_t vsync_count;
	
	ESP_LOGI(TAG, "Start task");
  
	while (1) {
		// Block waiting for a notification that the app_task wants an image
		xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, portMAX_DELAY);
		
		done = false;
		vsync_count = 0;
		while (!done) {
			// Then spin waiting for vsync to be asserted
			while (gpio_get_level(LEP_VSYNC_IO) == 0) {
				vTaskDelay(pdMS_TO_TICKS(9));
			};
			vsyncDetectedUsec = esp_timer_get_time();
			
			// Attempt to process a segment
			if (vospi_transfer_segment(vsyncDetectedUsec)) {				
				// Copy the frame to the shared buffer
				vospi_get_frame(&sys_lep_buffer);
				
				// Let app_task know we've updated the buffer
				xTaskNotify(task_handle_app, APP_NOTIFY_LEP_FRAME_MASK, eSetBits);		
				done = true;
			} else {
				// We should see a valid frame every 12 vsync interrupts (one frame period).
				// However, since we're resynchronizing with the VoSPI stream and our task
				// may be interrupted by other tasks, we give the lepton extra frame periods
				// to start correctly streaming data.  We may still fail when the lepton runs
				// a FFC since that takes a long time.
				if (++vsync_count == 36) {
					// Let app_task know we failed to update the buffer
					ESP_LOGE(TAG, "Could not get lepton image");
					xTaskNotify(task_handle_app, APP_NOTIFY_LEP_FAIL_MASK, eSetBits);
					done = true;
				}
			}
		}
	}
}

