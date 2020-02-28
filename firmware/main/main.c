/*
 * FireCAM Main
 *
 * FireCAM is a timelapse camera system designed to capture both visual and radiometric
 * images and store them to a local SD Card.  It is designed to run on the FireCAM
 * system designed by danjuliodesigns, LLC.  It includes support for the following
 * peripherals:
 *   1. ESP32 WROVER-B with at least 4 MB SPI Flash and 4 MB SPI RAM
 *   2. ArduCAM Mini-2MP Plus jpeg camera
 *   3. FLiR Lepton 3.5 thermal imaging camera
 *   4. 320x240 Pixel 16-bit LCD controlled by an ILI9341 display controller
 *   5. Resistive touchpad controlled by a XPT2046 controller
 *   6. Battery-backed DS3232 Realtime clock with SRAM
 *   7. TI ADC128D818 8-channel 12-bit ADC with external 2.048 volt precision reference
 *      - External LM36 temperature sensor
 *   8. Micro-SD card
 *
 * This firmware provides the following high-level functionality:
 *   1. Record Mode to store image metadata, jpeg visual image, 16-bit radiometric raw
 *      data as a json object serialized as a text file on the SD Card.
 *   2. GUI displaying current camera images and controls for operating the camera and
 *      setting various operating parameters.
 *   3. Remote access via a socket exposed on the ESP32 WiFi interface for querying the
 *      camera and setting record mode and operating parameters.  Packets are encoded
 *      a textually serialized json objects.
 *   4. Soft-power pushbutton control, battery voltage detection and automatic low-battery
 *      shutdown.
 *   5. Time/Date and parameter storage in an externally battery-backed RTC.
 *   6. Operational and error logging to USB Serial interface.
 *   7. Auto-restart on camera crash and automatic restart of recording.
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
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "adc_task.h"
#include "app_task.h"
#include "cam_task.h"
#include "cmd_task.h"
#include "file_task.h"
#include "gui_task.h"
#include "lep_task.h"
#include "mon_task.h"
#include "system_config.h"
#include "sys_utilities.h"



static const char* TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "FireCAM startup");
    
    // Initialize the ESP32 IO pins, set PWR_EN to keep us powered up and initialize
    // the shared SPI and I2C drivers
    if (!system_esp_io_init()) {
    	ESP_LOGE(TAG, "FireCAM ESP32 init failed - shutting off");
    	system_shutoff();
    }
    
    // After the IO has been set, holding power on, delay for > 950 mSec to allow
    // the both the ArduCAM and Lepton to finish booting so the ArduCAM is accessible
    // and the Lepton doesn't get confused by I2C traffic to other peripherals.
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Initialize the camera's peripheral devices: RTC, ADC, Arducam, Lepton
    if (!system_peripheral_init()) {
    	ESP_LOGE(TAG, "FireCAM Peripheral init failed - shutting off");
    	system_shutoff();
    }
    
    // Pre-allocate big buffers
    if (!system_buffer_init()) {
    	ESP_LOGE(TAG, "FireCAM memory allocate failed - shutting off");
    	system_shutoff();
    }
    
    // Initialized: Start tasks
    xTaskCreatePinnedToCore(&adc_task,  "adc_task",  2048, NULL, 1, &task_handle_adc,  1);
    xTaskCreatePinnedToCore(&cam_task,  "cam_task",  2048, NULL, 2, &task_handle_cam,  1);
    xTaskCreatePinnedToCore(&cmd_task,  "cmd_task",  3072, NULL, 1, &task_handle_cmd,  0);
    xTaskCreatePinnedToCore(&file_task, "file_task", 3072, NULL, 1, &task_handle_file, 1);
    xTaskCreatePinnedToCore(&gui_task,  "gui_task",  3072, NULL, 1, &task_handle_gui,  1);
    xTaskCreatePinnedToCore(&lep_task,  "lep_task",  2048, NULL, 2, &task_handle_lep,  0);
    xTaskCreatePinnedToCore(&app_task,  "app_task",  3072, NULL, 1, &task_handle_app,  1);
#ifdef INCLUDE_SYS_MON
	xTaskCreatePinnedToCore(&mon_task,  "mon_task",  2048, NULL, 1, &task_handle_mon,  1);
#endif
}
