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

// Uncomment to enable task to attempt to continuously read frames from the Lepton
// by servicing every Vsync Interrupt.  Comment to enable task to read one frame
// only when requested via a notification.  Continuously reading frames seems to fail
// from too-high interrupt->task latency when the system is busy with other tasks.
//#define LEP_READ_CONTINUOUS



//
// LEP Task variables
//
static const char* TAG = "lep_task";

// Lepton Vsync Interrupt handling
static volatile int64_t vsyncDetectedUsec;

#ifdef LEP_READ_CONTINUOUS
static uint32_t vsyncInterrupts = 0;
static uint32_t processedVsyncInterrupts = 0;
#endif



//
// LEP Task forward declarations for internal functions
//
#ifdef LEP_READ_CONTINUOUS
int init_gpio_interrupt();
static void gpio_isr_handler();
#endif
static float lep_kelvin_2_C(uint32_t k);



//
// LEP Task API
//

/**
 * This task drives the Lepton camera interface.
 */
void lep_task()
{
	uint32_t notification_value;
	uint32_t lep_value;
#ifndef LEP_READ_CONTINUOUS
	bool done;
	int vsync_count;
#endif
	
	ESP_LOGI(TAG, "Start task");

#ifdef LEP_READ_CONTINUOUS
	// Initialize the GPIO Interrupt
	init_gpio_interrupt();
#endif
  
	while (1) {
#ifdef LEP_READ_CONTINUOUS
		// Block waiting for a notification from the VSYNC ISR to process a segment
		//  outstandingVsyncInterrupts is incremented by the ISR
		xTaskNotifyWait(0x00, 0x00, &vsyncInterrupts, portMAX_DELAY);

		// Attempt to process a segment
		if (vospi_transfer_segment(vsyncDetectedUsec)) {
			// Got a complete frame from the Lepton

			// Check to see if app_task wants a copy of our latest frame
			if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
				if ((notification_value && LEP_NOTIFY_GET_FRAME_MASK) == LEP_NOTIFY_GET_FRAME_MASK) {
					// Get current camera information
					lep_value = cci_get_aux_temp();
					sys_lep_buffer.lep_aux_temp = lep_kelvin_2_C(lep_value);
					lep_value = cci_get_fpa_temp();
					sys_lep_buffer.lep_fpa_temp = lep_kelvin_2_C(lep_value);
					lep_value = cci_get_gain_mode();
					sys_lep_buffer.lep_gain_mode =  lep_value;
				
					// Copy the frame to the shared buffer
					vospi_get_frame(sys_lep_bufferP);
					
					// Let app_task know we've updated the buffer
					xTaskNotify(task_handle_app, APP_NOTIFY_LEP_FRAME_MASK, eSetBits);
				}
			}
		}
    
		if (++processedVsyncInterrupts < vsyncInterrupts) {
			//ESP_LOGW(TAG, "missed interrupt, count = %d", vsyncInterrupts - processedVsyncInterrupts);
			processedVsyncInterrupts = vsyncInterrupts;
		}
#else
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
				// Get current camera information
				lep_value = cci_get_aux_temp();
				sys_lep_buffer.lep_aux_temp = lep_kelvin_2_C(lep_value);
				lep_value = cci_get_fpa_temp();
				sys_lep_buffer.lep_fpa_temp = lep_kelvin_2_C(lep_value);
				lep_value = cci_get_gain_mode();
				sys_lep_buffer.lep_gain_mode =  lep_value;
				
				// Copy the frame to the shared buffer
				vospi_get_frame(sys_lep_buffer.lep_bufferP);
				
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
#endif
	}
}



//
// LEP Task forward declarations for internal functions
//

#ifdef LEP_READ_CONTINUOUS
/**
 * Configure the VSYNC GPIO input
 */
int init_gpio_interrupt()
{
	esp_err_t ret;
  
	// Setup the ISR
	ret = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
	ESP_ERROR_CHECK(ret);
	ret = gpio_set_intr_type(LEP_VSYNC_IO, GPIO_INTR_POSEDGE);
	ESP_ERROR_CHECK(ret);
	ret = gpio_isr_handler_add(LEP_VSYNC_IO, gpio_isr_handler, NULL);
	ESP_ERROR_CHECK(ret);
  
	return ESP_OK;
}


/**
 * VSYNC Interrupt Service Routine
 */
static void gpio_isr_handler()
{
	// Note when we saw this interrupt
	vsyncDetectedUsec = esp_timer_get_time();
  
	// Unblock our task
	xTaskNotifyFromISR(task_handle_lep, 0, eIncrement, NULL);
}
#endif


/**
 * Convert a temperature reading from the lepton (in units of K * 100) to C
 */
static float lep_kelvin_2_C(uint32_t k)
{
	return ((float) k / 100) - 273.15;
}

