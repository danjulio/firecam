/*
 * System related utilities
 *
 * Contains functions to initialize the system, other utility functions and a set
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
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "adc_utilities.h"
#include "file_utilities.h"
#include "json_utilities.h"
#include "lepton_utilities.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "time_utilities.h"
#include "wifi_utilities.h"
#include "gui_screen_main.h"
#include "i2c.h"
#include "ov2640.h"
#include "render_jpg.h"
#include "vospi.h"



//
// System Utilities variables
//
static const char* TAG = "sys";


//
// Task handle externs for use by tasks to communicate with each other
//
TaskHandle_t task_handle_adc;
TaskHandle_t task_handle_app;
TaskHandle_t task_handle_cam;
TaskHandle_t task_handle_cmd;
TaskHandle_t task_handle_file;
TaskHandle_t task_handle_gui;
TaskHandle_t task_handle_lep;
#ifdef INCLUDE_SYS_MON
TaskHandle_t task_handle_mon;
#endif


//
// Global buffer pointers for memory allocated in the external SPIRAM
//

// Shared memory data structures
cam_buffer_t sys_cam_buffer;   // Loaded by cam_task with jpeg data for other tasks
lep_buffer_t sys_lep_buffer;   // Loaded by lep_task for other task
json_image_string_t sys_image_file_buffer;   // Loaded by app_task with image data for file_task
json_image_string_t sys_cmd_response_buffer; // Loaded by app_task with image data for cmd_task

// Big buffers
uint16_t* gui_cam_bufferP;    // Loaded by gui_task for its own use
uint16_t* gui_lep_bufferP;    // Loaded by gui_task for its own use

// Designed to lock VSPI for multiple uninterruptible SPI transactions by one task
static SemaphoreHandle_t vspi_mutex;



//
// System Utilities API
//

/**
 * Initialize the ESP32 GPIO and internal peripherals
 */
bool system_esp_io_init()
{
	ESP_LOGI(TAG, "ESP32 Peripheral Initialization");
	
	// First thing to do is set PWR_HOLD to keep system powered after power button released
	gpio_set_direction(PWR_HOLD_IO, GPIO_MODE_OUTPUT);
	gpio_set_level(PWR_HOLD_IO, 1);
	
	// Configure other GPIO pins
	gpio_set_direction(CAM_CSN_IO, GPIO_MODE_OUTPUT);
	gpio_set_level(CAM_CSN_IO, 1);
	gpio_set_direction(LCD_CSN_IO, GPIO_MODE_OUTPUT);
	gpio_set_level(LCD_CSN_IO, 1);
	gpio_set_direction(LEP_CSN_IO, GPIO_MODE_OUTPUT);
	gpio_set_level(LEP_CSN_IO, 1);
	gpio_set_direction(TS_CSN_IO, GPIO_MODE_OUTPUT);
	gpio_set_level(TS_CSN_IO, 1);
	gpio_set_direction(LCD_DC_IO, GPIO_MODE_OUTPUT);
	gpio_set_level(LCD_DC_IO, 0);
	gpio_set_direction(TS_IRQ_IO, GPIO_MODE_INPUT);
	gpio_set_direction(LEP_VSYNC_IO, GPIO_MODE_INPUT);
	
	// Handle the special case where the DS3232 RTC I2C interface is confused.
	// Per its data sheet, we manually toggle SCL until SDA is seen high.
	// Do this before handing the pins over the I2C interface during its initialization.
	
	
	// Attempt to initialize the I2C Master
	if (i2c_master_init() != ESP_OK) {
		ESP_LOGE(TAG, "I2C Master initialization failed");
		return false;
	}
	
	// Attempt to initialize the HSPI Master (used by the Lepton)
	spi_bus_config_t hspi_buscfg = {
		.miso_io_num=HSPI_MISO_IO,
		.mosi_io_num=-1,
		.sclk_io_num=HSPI_SCK_IO,
		.max_transfer_sz=LEP_PKT_LENGTH,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1
	};
	if (spi_bus_initialize(HSPI_HOST, &hspi_buscfg, HSPI_DMA_NUM) != ESP_OK) {
		ESP_LOGE(TAG, "HSPI Master initialization failed");
		return false;
	}
	
	// Attempt to initialize the VSPI Master (used by the ArduCAM, LCD and Touchscreen)
	// Maximum transfer size is set by the LVGL display buffer (that is larger than
	// packets transferred for the ArduCAM or Touchscreen).
	spi_bus_config_t vspi_buscfg = {
		.miso_io_num=VSPI_MISO_IO,
		.mosi_io_num=VSPI_MOSI_IO,
		.sclk_io_num=VSPI_SCK_IO,
		.max_transfer_sz=LVGL_DISP_BUF_SIZE * 2,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1
	};
	if (spi_bus_initialize(VSPI_HOST, &vspi_buscfg, VSPI_DMA_NUM) != ESP_OK) {
		ESP_LOGE(TAG, "VSPI Master initialization failed");
		return false;
	}
	
	return true;
}


/**
 * Initialize the board-level peripheral subsystems
 */
bool system_peripheral_init()
{
	ESP_LOGI(TAG, "System Peripheral Initialization");
	
	time_init();
	ps_init();
	
	if (!adc_init()) {
		ESP_LOGE(TAG, "ADC subsystem initialization failed");
		return false;
	}
	
	if (ov2640_init() == 0) {
		ESP_LOGE(TAG, "Arducam ov2640 initialization failed");
		return false;
	}

	if (!lepton_init()) {
		ESP_LOGE(TAG, "Lepton initialization failed");
		return false;
	}
	if (vospi_init() != ESP_OK) {
		ESP_LOGE(TAG, "Lepton VoSPI initialization failed");
		return false;
	}
	
	if (!file_init_sdmmc_driver()) {
		ESP_LOGE(TAG, "SD Card driver initialization failed");
		return false;
	}
	
	if (!wifi_init()) {
		ESP_LOGE(TAG, "WiFi initialization failed");
		return false;
	}
	
	return true;
}


/**
 * Allocate shared buffers for use by tasks for image data
 */
bool system_buffer_init()
{
	uint16_t* ptr;
	
	ESP_LOGI(TAG, "Buffer Allocation");
	
	// Allocate the ArduCAM jpeg image buffer in the external RAM
	sys_cam_buffer.cam_bufferP = heap_caps_malloc(CAM_MAX_JPG_LEN, MALLOC_CAP_SPIRAM);
	if (sys_cam_buffer.cam_bufferP == NULL) {
		ESP_LOGE(TAG, "malloc ArduCAM shared buffer failed");
		return false;
	}
	
	// Allocate the buffer used by the gui to display images from the ArduCAM
	gui_cam_bufferP = heap_caps_malloc(CAM_IMG_PIXELS*2, MALLOC_CAP_SPIRAM);
	if (gui_cam_bufferP == NULL) {
		ESP_LOGE(TAG, "malloc ArduCAM gui buffer failed");
		return false;
	}
	
	// Clear ArduCAM display buffer (to color black)
	ptr = gui_cam_bufferP;
	while (ptr < (gui_cam_bufferP + CAM_IMG_PIXELS)) {
		*ptr++ = 0;
	}
	
	// Allocate the lepton frame buffer in the external RAM
	sys_lep_buffer.lep_bufferP = heap_caps_malloc(LEP_NUM_PIXELS*2, MALLOC_CAP_SPIRAM);
	if (sys_lep_buffer.lep_bufferP == NULL) {
		ESP_LOGE(TAG, "malloc lepton shared buffer failed");
		return false;
	}
	
	// Allocate the buffer used by the gui to display images from the lepton
	gui_lep_bufferP = heap_caps_malloc(LEP_IMG_PIXELS*2, MALLOC_CAP_SPIRAM);
	if (gui_lep_bufferP == NULL) {
		ESP_LOGE(TAG, "malloc lepton gui buffer failed");
		return false;
	}
	// Clear lepton display buffer (to color black)
	ptr = gui_lep_bufferP;
	while (ptr < (gui_lep_bufferP + LEP_IMG_PIXELS)) {
		*ptr++ = 0;
	}
	
	// Allocate the work area for the jpeg decompressor
	if (!render_init()) {
		ESP_LOGE(TAG, "initialize jpeg decompressor failed");
		return false;
	}
	
	// Allocate the json buffers
	if (!json_init()) {
		ESP_LOGE(TAG, "malloc json buffers failed");
		return false;
	}
	
	// Allocate the shared json image text buffers in the external RAM
	sys_image_file_buffer.bufferP = heap_caps_malloc(JSON_MAX_IMAGE_TEXT_LEN, MALLOC_CAP_SPIRAM);
	if (sys_image_file_buffer.bufferP == NULL) {
		ESP_LOGE(TAG, "malloc shared json image text file buffer failed");
		return false;
	}
	sys_cmd_response_buffer.bufferP = heap_caps_malloc(JSON_MAX_IMAGE_TEXT_LEN, MALLOC_CAP_SPIRAM);
	if (sys_cmd_response_buffer.bufferP == NULL) {
		ESP_LOGE(TAG, "malloc shared json image text command buffer failed");
		return false;
	}
	
	// Create the VSPI SPI Bus mutex
	vspi_mutex = xSemaphoreCreateMutex();
	if (vspi_mutex == NULL) {
		ESP_LOGE(TAG, "could not create vspi_mutex");
		return false;
	}
	
	return true;
}


/**
 * Shut the system off
 */
void system_shutoff()
{
	ESP_LOGI(TAG, "shutdown");
	
	// Delay for final logging
	vTaskDelay(pdMS_TO_TICKS(10));
	
	// Commit Seppuku
	gpio_set_level(PWR_HOLD_IO, 0);
}


/**
 * Lock the VSPI SPI bus
 */
void system_lock_vspi()
{
	xSemaphoreTake(vspi_mutex, portMAX_DELAY);
}


/**
 * Unlock the VSPI SPI bus
 */
void system_unlock_vspi()
{
	xSemaphoreGive(vspi_mutex);
}
