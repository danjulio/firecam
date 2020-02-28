/*
 * ADC Task
 *
 * Periodically updates operating state measured by the ADC.  Detects shutdown conditions
 * (power button long-press and critical battery) and notifies the application task.
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
#include "adc_task.h"
#include "app_task.h"
#include "adc_utilities.h"
#include "sys_utilities.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include <stdbool.h>
#include <stdint.h>



//
// ADC Task variables
//

static const char* TAG = "adc_task";

// Power button power-off counter - power-off signaled when it counts down to zero
// while power button is pressed
static int poweroff_count;



//
// ADC Task API
//
void adc_task()
{
	batt_status_t batt_status;
	bool btn_pressed;
	uint32_t notification_value;

	ESP_LOGI(TAG, "Start task");
	
	poweroff_count = ADC_TASK_PWROFF_PRESS_MSEC / ADC_TASK_SAMPLE_MSEC;
	
	while (1) {
		// This task runs every ADC_TASK_SAMPLE_MSEC mSec
		vTaskDelay(pdMS_TO_TICKS(ADC_TASK_SAMPLE_MSEC));
		
		// Update ADC values and get values we're interested in
		adc_update();
		adc_get_batt(&batt_status);
		btn_pressed = adc_button_pressed();
		
		// Determine if we have conditions to send a power-down notification to app_task
		notification_value = 0;
		
		if (batt_status.batt_state == BATT_CRIT) {
			ESP_LOGW(TAG, "Critical battery voltage");
			notification_value = APP_NOTIFY_SHUTDOWN_MASK;
		}
		
		if (btn_pressed) {
			if (--poweroff_count == 0) {
				notification_value = APP_NOTIFY_SHUTDOWN_MASK;
				poweroff_count = ADC_TASK_PWROFF_PRESS_MSEC / ADC_TASK_SAMPLE_MSEC;
			}
		} else {
			// Hold power-off counter in reset
			poweroff_count = ADC_TASK_PWROFF_PRESS_MSEC / ADC_TASK_SAMPLE_MSEC;
		}
		
		if (notification_value != 0) {
			// Notify app_task
			xTaskNotify(task_handle_app, notification_value, eSetBits);
		}
	}
}
