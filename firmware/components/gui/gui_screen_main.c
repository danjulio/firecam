/*
 * Main GUI screen related functions, callbacks and event handlers
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
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gui_screen_main.h"
#include "app_task.h"
#include "gui_task.h"
#include "adc_utilities.h"
#include "file_utilities.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "time_utilities.h"
#include "wifi_utilities.h"
#include "lv_conf.h"
#include "vospi.h"
#include "palettes.h"
#include "render_jpg.h"
#include <math.h>
#include <stdio.h>
#include <string.h>



//
// Main GUI Screen constants
//

// Uncomment to debug image display timing
//#define GUI_DEBUG_IMG


//
// Main GUI Screen variables
//
static const char* TAG = "gui_main";

// LVGL objects
static lv_obj_t* main_screen;
static lv_obj_t* lbl_title_version;
static lv_obj_t* lbl_sdcard_status;
static lv_obj_t* lbl_batt_status;
static lv_obj_t* lbl_ssid;
static lv_obj_t* lbl_time_date;
static lv_obj_t* lbl_lens_temp;
static lv_img_dsc_t arducam_img_dsc;
static lv_obj_t* img_arducam;
static lv_img_dsc_t lepton_img_dsc;
static lv_obj_t* img_lepton;
static lv_obj_t* btn_record;
static lv_obj_t* btn_record_label;
static lv_obj_t* led_record;
static lv_obj_t* lbl_record_image_num;
static lv_obj_t* btn_settings;
static lv_obj_t* btn_settings_label;
static lv_obj_t* btn_poweroff;
static lv_obj_t* btn_poweroff_label;

// Screen state
static bool main_screen_active;

// Displayed object state to reduce redraws
static char prev_ssid[PS_SSID_MAX_LEN];
static uint8_t prev_flags;
static batt_status_t prev_bs;
static bool prev_sdcard_present;
static int prev_temp;
static uint16_t prev_record_count;



//
// Main GUI Screen internal function forward declarations
//
static char* main_screen_get_name_version();
static void main_screen_initialize_dynamic_values();
static void main_screen_update_wifi();
static void main_screen_update_sdcard();
static void main_screen_update_batt();
static void main_screen_update_time();
static void main_screen_update_temp();
static void btn_record_callback(lv_obj_t * btn, lv_event_t event);
static void btn_settings_callback(lv_obj_t * btn, lv_event_t event);
static void btn_poweroff_callback(lv_obj_t * btn, lv_event_t event);



//
// Main GUI Screen API
//

/**
 * Create the main screen, its graphical objects and link necessary callbacks
 */
lv_obj_t* gui_screen_main_create()
{
	main_screen = lv_obj_create(NULL, NULL);
	lv_obj_set_size(main_screen, LV_HOR_RES_MAX, LV_VER_RES_MAX);
	lv_obj_set_style(main_screen, &lv_style_plain_color);
	
	// Create the graphical elements for this screen
	//
	// Line 1
	lbl_title_version = lv_label_create(main_screen, NULL);
	lv_obj_set_pos(lbl_title_version, 5, 2);
	lv_obj_set_width(lbl_title_version, 150);
	lv_label_set_static_text(lbl_title_version, main_screen_get_name_version());
	
	lbl_ssid = lv_label_create(main_screen, NULL);
	lv_obj_set_pos(lbl_ssid, 120, 2);
	lv_obj_set_width(lbl_ssid, 80);
	lv_label_set_recolor(lbl_ssid, true);
	
	lbl_sdcard_status = lv_label_create(main_screen, NULL);
	lv_obj_set_pos(lbl_sdcard_status, 250, 2);
	lv_obj_set_width(lbl_sdcard_status, 30);
	lv_label_set_align(lbl_sdcard_status, LV_LABEL_ALIGN_CENTER);
	
	lbl_batt_status = lv_label_create(main_screen, NULL);
	lv_obj_set_pos(lbl_batt_status, 280, 2);
	lv_obj_set_width(lbl_batt_status, 50);
	lv_label_set_align(lbl_batt_status, LV_LABEL_ALIGN_RIGHT);
	
	// Line 2
	lbl_time_date = lv_label_create(main_screen, NULL);
	lv_obj_set_pos(lbl_time_date, 5, 22);
	lv_obj_set_width(lbl_time_date, 80);
	lv_label_set_align(lbl_time_date, LV_LABEL_ALIGN_LEFT);
	
	lbl_lens_temp = lv_label_create(main_screen, NULL);
	lv_obj_set_pos(lbl_lens_temp, 280, 22);
	lv_obj_set_width(lbl_lens_temp, 50);
	lv_label_set_align(lbl_lens_temp, LV_LABEL_ALIGN_RIGHT);
	
	// Arducam image data structure
	arducam_img_dsc.header.always_zero = 0;
	arducam_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
	arducam_img_dsc.header.w = CAM_IMG_WIDTH;
	arducam_img_dsc.header.h = CAM_IMG_HEIGHT;
	arducam_img_dsc.data_size = CAM_IMG_WIDTH * CAM_IMG_HEIGHT * 2;
	arducam_img_dsc.data = (uint8_t*) gui_cam_bufferP;
	
	// Arducam Image Area
	img_arducam = lv_img_create(main_screen, NULL);
	lv_img_set_src(img_arducam, &arducam_img_dsc);
	lv_obj_set_pos(img_arducam, 0, 40);
	
	// Lepton image data structure
	lepton_img_dsc.header.always_zero = 0;
	lepton_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
	lepton_img_dsc.header.w = LEP_IMG_WIDTH;
	lepton_img_dsc.header.h = LEP_IMG_HEIGHT;
	lepton_img_dsc.data_size = LEP_IMG_WIDTH * LEP_IMG_HEIGHT * 2;
	lepton_img_dsc.data = (uint8_t*) gui_lep_bufferP;

	// Lepton Image Area
	img_lepton = lv_img_create(main_screen, NULL);
	lv_img_set_src(img_lepton, &lepton_img_dsc);
	lv_obj_set_pos(img_lepton, 160, 40);
	
	// Button Area
	btn_record = lv_btn_create(main_screen, NULL);
	lv_obj_set_pos(btn_record, 20, 165);
	lv_obj_set_size(btn_record, 100, 70);
	lv_obj_set_event_cb(btn_record, btn_record_callback);
	btn_record_label = lv_label_create(btn_record, NULL);
	lv_label_set_static_text(btn_record_label, "REC");
	
	// Create a style for the LED
	static lv_style_t style_led;
	lv_style_copy(&style_led, &lv_style_pretty_color);
	style_led.body.radius = LV_RADIUS_CIRCLE;
	style_led.body.main_color = LV_COLOR_MAKE(0xb5, 0x0f, 0x04);
	style_led.body.grad_color = LV_COLOR_MAKE(0x50, 0x07, 0x02);
	style_led.body.border.color = LV_COLOR_MAKE(0xfa, 0x0f, 0x00);
	style_led.body.border.width = 3;
	style_led.body.border.opa = LV_OPA_30;
	style_led.body.shadow.color = LV_COLOR_MAKE(0xb5, 0x0f, 0x04);
	style_led.body.shadow.width = 5;
	
	led_record = lv_led_create(main_screen, NULL);
	lv_obj_set_style(led_record, &style_led);
	lv_obj_set_pos(led_record, 145, 165);
	lv_obj_set_size(led_record, 30, 30);
	lv_led_off(led_record);
	
	lbl_record_image_num = lv_label_create(main_screen, NULL);
	lv_obj_set_pos(lbl_record_image_num, 140, 210);
	lv_obj_set_width(lbl_record_image_num, 60);
	lv_label_set_align(lbl_record_image_num, LV_LABEL_ALIGN_RIGHT);
	
	btn_settings = lv_btn_create(main_screen, NULL);
	lv_obj_set_pos(btn_settings, 205, 180);
	lv_obj_set_size(btn_settings, 40, 40);
	lv_obj_set_event_cb(btn_settings, btn_settings_callback);
	btn_settings_label = lv_label_create(btn_settings, NULL);
	lv_label_set_static_text(btn_settings_label, LV_SYMBOL_SETTINGS);
	
	btn_poweroff = lv_btn_create(main_screen, NULL);
	lv_obj_set_pos(btn_poweroff, 265, 180);
	lv_obj_set_size(btn_poweroff, 40, 40);
	lv_obj_set_event_cb(btn_poweroff, btn_poweroff_callback);
	btn_poweroff_label = lv_label_create(btn_poweroff, NULL);
	lv_label_set_static_text(btn_poweroff_label, LV_SYMBOL_POWER);

	main_screen_active = false;
	
	// Force immediate draw of status lines (so we don't get the default "Text" in some labels)
	main_screen_initialize_dynamic_values();
	
	return main_screen;
}


/**
 * Tell this screen if it is newly active or not
 * (activating a screen (re)initializes its display)
 */
void gui_screen_main_set_active(bool en)
{
	main_screen_active = en;
	
	if (en) {
		main_screen_initialize_dynamic_values();
	}
}


/**
 * LVGL (sub)task to periodically update status label values on the screen
 */
void gui_screen_main_status_update_task(lv_task_t * task)
{
	if (main_screen_active) {
		main_screen_update_wifi();
		main_screen_update_sdcard();
		main_screen_update_batt();
		main_screen_update_time();
		main_screen_update_temp();
	}
}


/**
 * Update the ArduCAM display.  Convert the jpeg image to our image size and
 * force LittleVGL to update the image area
 */
void gui_screen_main_update_cam_image()
{
	// Attempt to convert the jpeg image from the shared buffer into a bitmap in our
	// display buffer
	if (render_jpeg_image((uint8_t*) gui_cam_bufferP, sys_cam_buffer.cam_bufferP,
	    sys_cam_buffer.cam_buffer_len, CAM_JPEG_WIDTH, CAM_IMG_WIDTH) == 1) {
	    
		// Invalidate the object to force it to redraw from the buffer
		lv_obj_invalidate(img_arducam);
		
#ifdef GUI_DEBUG_IMG
		ESP_LOGI(TAG, "render cam");
#endif
	}
}


/**
 * Update the lepton display.  Perform a simple linearization of the raw lepton data and
 * scale it to 8-bit and write pseudo-color pixels to the gui lepton display buffer
 * and force LittleVGL to update the image area.
 */
void gui_screen_main_update_lep_image()
{
	uint32_t t32;
	uint32_t diff;
	uint16_t* ptr = sys_lep_buffer.lep_bufferP;
	uint16_t* ptr2 = gui_lep_bufferP;
	uint8_t t8;
	
	// Copy the source buffer to the destination buffer
	//  - Scale each source value to an 8-bit intensity value
	//  - Convert the intensity value to a byte-swapped RGB565 pixel to store
	ptr = sys_lep_buffer.lep_bufferP;
	diff = sys_lep_buffer.lep_max_val - sys_lep_buffer.lep_min_val;
	
	while (ptr < (sys_lep_buffer.lep_bufferP + LEP_NUM_PIXELS)) {
		t32 = ((uint32_t)(*ptr++ - sys_lep_buffer.lep_min_val) * 255) / diff;
		t8 = (t32 > 255) ? 255 : (uint8_t) t32;
		*ptr2++ = PALLETTE_LOOKUP(t8);
	}

	// Finally invalidate the object to force it to redraw from the buffer
	lv_obj_invalidate(img_lepton);
		
#ifdef GUI_DEBUG_IMG
		ESP_LOGI(TAG, "render lep");
#endif
}


/**
 * Update the recording LED state
 */
void gui_screen_main_update_rec_led(bool en)
{
	if(en) {
		lv_led_on(led_record);
	} else {
		lv_led_off(led_record);
	}
}


/**
 * Update the recording count
 */
void gui_screen_main_update_rec_count(uint16_t c)
{
	static char temp_buf[6];  // Statically allocated for lv_label_set_static_text

	if (c != prev_record_count) {
		sprintf(temp_buf, "%5d", c);
		lv_label_set_static_text(lbl_record_image_num, temp_buf);
		prev_record_count = c;
	}
}



//
// Main GUI Screen internal functions
//

/**
 * Return the pointer to a string containing the program name and version
 * Note: Although this function can support the full 32-character version string from the
 * ESP-IDF data structures, in reality we don't have room for that so the developer needs
 * to stick with short version strings like "V1.0".
 */
static char* main_screen_get_name_version()
{
	static char name_version[42]; // Statically allocated for lv_label_set_static_text
	const esp_app_desc_t* app_desc;
	
	app_desc = esp_ota_get_app_description();
	sprintf(name_version, "FireCAM v%s", app_desc->version);
	
	return name_version;
}


/**
 * Initialize the main screen's dynamic values
 */
static void main_screen_initialize_dynamic_values()
{
	static int prev_palette_index = -1;
	
	// Force updates
	prev_ssid[0] = 0;
	prev_flags = 0;
	prev_sdcard_present = !file_get_card_present();
	prev_bs.batt_state = BATT_CRIT;         /* Pick values hopefully we won't have */
	prev_bs.charge_state = CHARGE_FAULT;
	prev_temp = 99999;
	prev_record_count = 1;
	
	main_screen_update_wifi();
	main_screen_update_sdcard();
	main_screen_update_batt();
	main_screen_update_time();
	main_screen_update_temp();
	gui_screen_main_update_rec_count(0);
	
	if (gui_st.palette_index != prev_palette_index) {
		prev_palette_index = gui_st.palette_index;
		set_palette(gui_st.palette_index);
	}
	
}


/**
 * Update the Wifi status
 */
static void main_screen_update_wifi()
{
	bool ssid_different;
	bool sta_mode;
	
	// Statically allocated for lv_label_set_static_text = "<ssid> #CCCCCC <symbol_3>#" + null
	static char wifi_label[PS_SSID_MAX_LEN + 14];
	
	wifi_info_t* wifi_info;
	
	wifi_info = wifi_get_info();
	
	sta_mode = ((wifi_info->flags & WIFI_INFO_FLAG_CLIENT_MODE) != 0);
	
	if (sta_mode) {
		ssid_different = (strcmp(prev_ssid, wifi_info->sta_ssid) != 0);
	} else {
		ssid_different = (strcmp(prev_ssid, wifi_info->ap_ssid) != 0);
	}
	
	if (ssid_different || (prev_flags != wifi_info->flags)) {
	    
		// Update the label with the SSID and optional WiFi Icon to indicate active/connected
		memset(wifi_label, 0, sizeof(wifi_label));
		if (sta_mode) {
			// Client Mode: No icon if interface disabled, dim icon if enabled but not
			// successfully connected to an AP, bright icon if connected to an AP
			if ((wifi_info->flags & WIFI_INFO_FLAG_ENABLED) != 0) {
				if ((wifi_info->flags & WIFI_INFO_FLAG_CONNECTED) != 0) {
					sprintf(wifi_label, "%s #FFFFFF %s#", wifi_info->sta_ssid, LV_SYMBOL_WIFI);
				} else {
					sprintf(wifi_label, "%s #B0B0B0 %s#", wifi_info->sta_ssid, LV_SYMBOL_WIFI);
				}
			} else {
				strcpy(wifi_label, wifi_info->sta_ssid);
			}
		} else {
			// AP Mode: Show WiFi Icon when we have successfully enabled the interface,
			// otherwise show no icon.
			if ((wifi_info->flags & WIFI_INFO_FLAG_ENABLED) != 0) {
				sprintf(wifi_label, "%s #FFFFFF %s#", wifi_info->ap_ssid, LV_SYMBOL_WIFI);
			} else {
				strcpy(wifi_label, wifi_info->ap_ssid);
			}
		}
		
		lv_label_set_static_text(lbl_ssid, wifi_label);
		
		if (sta_mode) {
			strcpy(prev_ssid, wifi_info->sta_ssid);
		} else {
			strcpy(prev_ssid, wifi_info->ap_ssid);
		}
		prev_flags = wifi_info->flags;
	}
}


/**
 * Update SD Card present display
 */
static void main_screen_update_sdcard()
{
	bool present;
	static char sdcard_buf[4];   // Statically allocated for lv_label_set_static_text
	
	present = file_get_card_present();
	
	if (present != prev_sdcard_present) {
		if (present) {
			sprintf(sdcard_buf, LV_SYMBOL_SD_CARD);
		} else {
			sprintf(sdcard_buf, "   ");
		}
		lv_label_set_static_text(lbl_sdcard_status, sdcard_buf);
		prev_sdcard_present = present;
	}
}


/**
 * Update battery display - battery condition and charge/fault status
 */
static void main_screen_update_batt()
{
	static char batt_buf[8];  // Statically allocated for lv_label_set_static_text
	batt_status_t bs;
	
	adc_get_batt(&bs);
	
	if ((bs.batt_state != prev_bs.batt_state) || (bs.charge_state != prev_bs.charge_state)) {
		// Set battery charge condition icon
		switch (bs.batt_state) {
			case BATT_100:
				strcpy(&batt_buf[0], LV_SYMBOL_BATTERY_FULL);
				break;
			case BATT_75:
				strcpy(&batt_buf[0], LV_SYMBOL_BATTERY_3);
				break;
			case BATT_50:
				strcpy(&batt_buf[0], LV_SYMBOL_BATTERY_2);
				break;
			case BATT_25:
				strcpy(&batt_buf[0], LV_SYMBOL_BATTERY_1);
				break;
			default:
				strcpy(&batt_buf[0], LV_SYMBOL_BATTERY_EMPTY);
				break;
		}
	
		// Space between
		batt_buf[3] = ' ';
	
		// Set charge/fault icon
		switch (bs.charge_state) {
			case CHARGE_OFF:
				strcpy(&batt_buf[4], "   ");
				break;
			case CHARGE_ON:
				strcpy(&batt_buf[4], LV_SYMBOL_CHARGE);
				break;
			default:
				strcpy(&batt_buf[4], LV_SYMBOL_WARNING);
				break;
		}
	
		// Null terminator
		batt_buf[7] = 0;
	
		lv_label_set_static_text(lbl_batt_status, batt_buf);
	
		prev_bs.batt_voltage = bs.batt_voltage;
		prev_bs.batt_state = bs.batt_state;
		prev_bs.charge_state = bs.charge_state;
	}
}


static void main_screen_update_time()
{
	static char time_buf[26];  // Statically allocated for lv_label_set_static_text
	tmElements_t tm;
	
	time_get(&tm);
	time_get_disp_string(tm, time_buf);
	lv_label_set_static_text(lbl_time_date, time_buf);
}


static void main_screen_update_temp()
{
	static char temp_buf[8];  // Statically allocated for lv_label_set_static_text
	float t;
	int it;
	
	t = adc_get_temp();
	it = round(t);
	
	if (it != prev_temp) {
		sprintf(temp_buf, "%2d C", it);
		lv_label_set_static_text(lbl_lens_temp, temp_buf);
		prev_temp = it;
	}
}


static void btn_record_callback(lv_obj_t * btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		// Let the app_task know the button was pressed since it handles the system mode
		xTaskNotify(task_handle_app, APP_NOTIFY_RECORD_BTN_MASK, eSetBits);
	}
}


static void btn_settings_callback(lv_obj_t * btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		gui_set_screen(GUI_SCREEN_SETTINGS);
	}
}


static void btn_poweroff_callback(lv_obj_t * btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		// Notify app_task to power down
		xTaskNotify(task_handle_app, APP_NOTIFY_SHUTDOWN_MASK, eSetBits);
	}
}
