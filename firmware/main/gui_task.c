/*
 * GUI Task
 *
 * Contains functions to initialize the LittleVGL GUI system and a task
 * to evaluate its display related sub-tasks.  The GUI Task is responsible
 * for all access (updating) of the GUI managed by LittleVGL.
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
#include "gui_task.h"
#include "app_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_freertos_hooks.h"
#include "system_config.h"
#include "lvgl/lvgl.h"
#include "disp_spi.h"
#include "ili9341.h"
#include "tp_spi.h"
#include "xpt2046.h"
#include "gui_utilities.h"
#include "sys_utilities.h"
#include "gui_screen_main.h"
#include "gui_screen_network.h"
#include "gui_screen_settings.h"
#include "gui_screen_time.h"
#include "gui_screen_poweroff.h"
#include "gui_screen_wifi.h"

//
// GUI Task internal constants
//

// LVGL sub-task indicies
#define LVGL_ST_MAIN_STATUS 0
#define LVGL_ST_SETTINGS    1
#define LVGL_ST_EVENT       2
#define LVGL_ST_NUM         3


//
// GUI Task variables
//

static const char* TAG = "gui_task";

// Dual display update buffers to allow DMA/SPI transfer of one while the other is updated
static lv_color_t lvgl_disp_buf1[LVGL_DISP_BUF_SIZE];
static lv_color_t lvgl_disp_buf2[LVGL_DISP_BUF_SIZE];
static lv_disp_buf_t lvgl_disp_buf;

// Display driver
static lv_disp_drv_t lvgl_disp_drv;

// Touchscreen driver
static lv_indev_drv_t lvgl_indev_drv;

// Screen object array and current screen index
static lv_obj_t* gui_screens[GUI_NUM_SCREENS];
static int gui_cur_screen_index;

// LVGL sub-task array
static lv_task_t* lvgl_tasks[LVGL_ST_NUM];


//
// GUI Task internal function forward declarations
//
static void gui_lvgl_init();
static void gui_screen_init();
static void gui_add_subtasks();
static void gui_task_event_handler_task(lv_task_t * task);
static void IRAM_ATTR lv_tick_callback();



//
// GUI Task API
//

/**
 * GUI Task - Executes all GUI/display related activities via LittleVGL objects
 * and LittleVGL sub-tasks evaluated by lv_task_handler.  Communication with other
 * tasks is handled in this routine although various LVGL callbacks and event code
 * may call routines in other modules to get or set state.
 */
void gui_task()
{
	ESP_LOGI(TAG, "Start task");

	// Initialize
	gui_lvgl_init();
	gui_screen_init();
	gui_add_subtasks();
	
	// Set the initially displayed screen
	gui_set_screen(GUI_SCREEN_MAIN);
	
	while (1) {
		// This task runs every LVGL_EVAL_MSEC mSec
		vTaskDelay(pdMS_TO_TICKS(LVGL_EVAL_MSEC));
		lv_task_handler();
	}
}


/**
 * Set the currently displayed screen
 */
void gui_set_screen(int n)
{
	if (n < GUI_NUM_SCREENS) {
		gui_cur_screen_index = n;
		
		gui_screen_main_set_active(n == GUI_SCREEN_MAIN);
		gui_screen_settings_active(n == GUI_SCREEN_SETTINGS);
		gui_screen_time_set_active(n == GUI_SCREEN_TIME);
		gui_screen_wifi_set_active(n == GUI_SCREEN_WIFI);
		gui_screen_network_set_active(n == GUI_SCREEN_NETWORK);
		gui_screen_poweroff_set_active(n == GUI_SCREEN_POWEROFF);
		
		lv_scr_load(gui_screens[n]);
	}
}



//
// GUI Task Internal functions
//

/**
 * Initialize the LittleVGL system including initializing the LCD display and
 * Touchscreen controller.
 */
static void gui_lvgl_init()
{
	// Initialize lvgl and its hardware
	lv_init();
	disp_spi_init();
	ili9341_init();
	tp_spi_init();
	xpt2046_init();
	
	// Install the display driver
	lv_disp_buf_init(&lvgl_disp_buf, lvgl_disp_buf1, lvgl_disp_buf2, LVGL_DISP_BUF_SIZE);
	lv_disp_drv_init(&lvgl_disp_drv);
	lvgl_disp_drv.flush_cb = ili9341_flush;
	lvgl_disp_drv.buffer = &lvgl_disp_buf;
	lv_disp_drv_register(&lvgl_disp_drv);
	
	// Install the touchscreen driver
    lv_indev_drv_init(&lvgl_indev_drv);
    lvgl_indev_drv.read_cb = xpt2046_read;
    lvgl_indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&lvgl_indev_drv);
    
    // Hook LittleVGL's timebase to its CPU system tick so it can keep track of time
    esp_register_freertos_tick_hook(lv_tick_callback);
}


/**
 * Initialize the screen objects and their control callbacks
 */
static void gui_screen_init()
{
	// Initialize the screens
	gui_screens[GUI_SCREEN_MAIN] = gui_screen_main_create();
	gui_screens[GUI_SCREEN_SETTINGS] = gui_screen_settings_create();
	gui_screens[GUI_SCREEN_TIME] = gui_screen_time_create();
	gui_screens[GUI_SCREEN_WIFI] = gui_screen_wifi_create();
	gui_screens[GUI_SCREEN_NETWORK] = gui_screen_network_create();
	gui_screens[GUI_SCREEN_POWEROFF] = gui_screen_poweroff_create();
}


/**
 * Add the LittleVGL sub-tasks and specify their evaluation period
 */
static void gui_add_subtasks()
{
	// Main screen Status line update sub-task runs once per second
	lvgl_tasks[LVGL_ST_MAIN_STATUS] = lv_task_create(gui_screen_main_status_update_task,
		1000, LV_TASK_PRIO_MID, NULL);
		
	// Settings screen IP address update sub-task runs once per second
	lvgl_tasks[LVGL_ST_SETTINGS] = lv_task_create(gui_screen_settings_update_task,
		1000, LV_TASK_PRIO_LOW, NULL);
		
	// Event handler sub-task runs every 50 mSec
	lvgl_tasks[LVGL_ST_EVENT] = lv_task_create(gui_task_event_handler_task, 50,
		LV_TASK_PRIO_LOW, NULL);
}


/**
 * LittleVGL sub-task to handle events from the other tasks
 */
static void gui_task_event_handler_task(lv_task_t * task)
{
	uint32_t notification_value;
	static uint16_t image_num;     // Image number displayed on the main screen, managed here
	
	// Look for incoming notifications (clear them upon reading)
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
	
		if (Notification(notification_value, GUI_NOTIFY_SHUTDOWN_MASK)) {
			gui_set_screen(GUI_SCREEN_POWEROFF);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_CAM_FRAME_MASK)) {
			if (gui_cur_screen_index == GUI_SCREEN_MAIN) {
				// Trigger the main screen to draw the image from the buffer to the display
				gui_screen_main_update_cam_image();
			}
			// Let the app task know we're done with the buffer
			xTaskNotify(task_handle_app, APP_NOTIFY_GUI_CAM_DONE_MASK, eSetBits);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_LEP_FRAME_MASK)) {
			if (gui_cur_screen_index == GUI_SCREEN_MAIN) {
				// Trigger the main screen to draw the image from the buffer to the display
				gui_screen_main_update_lep_image();
			}
			// Let the app task know we're done with the buffer
			xTaskNotify(task_handle_app, APP_NOTIFY_GUI_LEP_DONE_MASK, eSetBits);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_LED_ON_MASK)) {
			gui_screen_main_update_rec_led(true);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_LED_OFF_MASK)) {
			gui_screen_main_update_rec_led(false);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_INC_REC_MASK)) {
			image_num++;
			gui_screen_main_update_rec_count(image_num);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_CLR_REC_MASK)) {
			image_num = 0;
			gui_screen_main_update_rec_count(image_num);
		}
		
		if (Notification(notification_value, GUI_NOTIFY_MESSAGEBOX_MASK)) {
			gui_preset_message_box(gui_screens[gui_cur_screen_index]);
		}	
	}
}


/**
 * LittleVGL timekeeping callback - hooked to the system tick timer so LVGL
 * knows how much time has gone by (used for animations, etc).
 */
static void IRAM_ATTR lv_tick_callback()
{
	lv_tick_inc(portTICK_RATE_MS);
}
