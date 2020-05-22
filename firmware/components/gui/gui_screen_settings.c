/*
 * Settings GUI screen related functions, callbacks and event handlers
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
#include "gui_screen_settings.h"
#include "app_task.h"
#include "gui_task.h"
#include "cci.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "palettes.h"
#include "gui_utilities.h"
#include "lepton_utilities.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "wifi_utilities.h"



// Gain Drop-down list
static const char* dd_gain_mode_list = SYS_GAIN_DD_STRING;



//
// Settings GUI Screen variables
//

// LVGL objects
static lv_obj_t* settings_screen;
static lv_obj_t* lbl_settings_title;
static lv_obj_t* lbl_ip_addr;
static lv_obj_t* btn_settings_save;
static lv_obj_t* btn_settings_save_label;
static lv_obj_t* btn_settings_exit;
static lv_obj_t* btn_settings_exit_label;
static lv_obj_t* lbl_rec_cam_select;
static lv_obj_t* cb_en_arducam;
static lv_obj_t* cb_en_lepton;
static lv_obj_t* btn_set_network;
static lv_obj_t* btn_set_network_label;
static lv_obj_t* btn_set_wifi;
static lv_obj_t* btn_set_wifi_label;
static lv_obj_t* btn_set_time;
static lv_obj_t* btn_set_time_label;
static lv_obj_t* dd_rec_interval;
static lv_obj_t* dd_rec_interval_label;
static lv_obj_t* dd_gain_mode;
static lv_obj_t* dd_gain_mode_label;
static lv_obj_t* dd_palette;
static lv_obj_t* dd_palette_label;

// Screen state
static bool settings_screen_active;

// Displayed object state to reduce redraws
static bool prev_wifi_ip_valid;
static uint8_t prev_disp_ip_addr[4];

// Statically allocated for lv_label_set_static_text = "XXX:XXX:XXX:XXX" + null
static char ip_string[16];

// String created for recording intervals drop-down
static char* dd_rec_interval_list;

// String created for palette drop-down
static char* dd_palette_list;

// Local copy of gui_state used for local updating and check upon save
static gui_state_t local_gui_st;



//
// Settings GUI Screen internal function forward declarations
//
static void initialize_screen_values();
static void settings_screen_update_ip_addr();
static void btn_exit_callback(lv_obj_t * btn, lv_event_t event);
static void btn_save_callback(lv_obj_t * btn, lv_event_t event);
static void arducam_en_cb_callback(lv_obj_t * cb, lv_event_t event);
static void lepton_en_cb_callback(lv_obj_t * cb, lv_event_t event);
static void btn_set_network_callback(lv_obj_t * btn, lv_event_t event);
static void btn_set_time_callback(lv_obj_t * btn, lv_event_t event);
static void btn_set_wifi_callback(lv_obj_t * btn, lv_event_t event);
static void dd_rec_interval_callback(lv_obj_t * dd, lv_event_t event);
static void dd_gain_mode_callback(lv_obj_t * dd, lv_event_t event);
static void dd_palette_callback(lv_obj_t * dd, lv_event_t event);

static void add_dd_rec_interval_entries();
static void add_dd_palette_entries();



//
// Settings GUI Screen API
//

/**
 * Create the settings screen, its graphical objects and link necessary callbacks
 */
lv_obj_t* gui_screen_settings_create()
{
	settings_screen = lv_obj_create(NULL, NULL);
	lv_obj_set_size(settings_screen, LV_HOR_RES_MAX, LV_VER_RES_MAX);
	lv_obj_set_style(settings_screen, &lv_style_plain_color);
	
	// Create the graphical elements for this screen
	//
	// Screen Title
	lbl_settings_title = lv_label_create(settings_screen, NULL);
	lv_obj_set_pos(lbl_settings_title, 100, 5);
	lv_obj_set_width(lbl_settings_title, 120);
	lv_label_set_align(lbl_settings_title, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(lbl_settings_title, "Camera Settings");
	
	// Save & exit button
	btn_settings_save = lv_btn_create(settings_screen, NULL);
	lv_obj_set_pos(btn_settings_save, 5, 5);
	lv_obj_set_size(btn_settings_save, 40, 35);
	lv_obj_set_event_cb(btn_settings_save, btn_save_callback);
	btn_settings_save_label = lv_label_create(btn_settings_save, NULL);
	lv_label_set_static_text(btn_settings_save_label, LV_SYMBOL_OK);
	
	// Exit button
	btn_settings_exit = lv_btn_create(settings_screen, NULL);
	lv_obj_set_pos(btn_settings_exit, 275, 5);
	lv_obj_set_size(btn_settings_exit, 40, 35);
	lv_obj_set_event_cb(btn_settings_exit, btn_exit_callback);
	btn_settings_exit_label = lv_label_create(btn_settings_exit, NULL);
	lv_label_set_static_text(btn_settings_exit_label, LV_SYMBOL_CLOSE);
	
	// Recording camera enables
	lbl_rec_cam_select = lv_label_create(settings_screen, NULL);
	lv_obj_set_pos(lbl_rec_cam_select, 15, 115);
	lv_obj_set_width(lbl_rec_cam_select, 120);
	lv_label_set_static_text(lbl_rec_cam_select, "Record Enable:");
	
	cb_en_arducam = lv_cb_create(settings_screen, NULL);
	lv_obj_set_pos(cb_en_arducam, 120, 110);
	lv_obj_set_width(cb_en_arducam, 40);
	lv_cb_set_static_text(cb_en_arducam, "ArduCAM");
	lv_obj_set_event_cb(cb_en_arducam, arducam_en_cb_callback);
	
	cb_en_lepton = lv_cb_create(settings_screen, NULL);
	lv_obj_set_pos(cb_en_lepton, 220, 110);
	lv_obj_set_width(cb_en_lepton, 40);
	lv_cb_set_static_text(cb_en_lepton, "Lepton");
	lv_obj_set_event_cb(cb_en_lepton, lepton_en_cb_callback);
	
	// Buttons for other settings screens
	btn_set_network = lv_btn_create(settings_screen, NULL);
	lv_obj_set_pos(btn_set_network, 15, 160);
	lv_obj_set_size(btn_set_network, 90, 40);
	lv_obj_set_event_cb(btn_set_network, btn_set_network_callback);
	btn_set_network_label = lv_label_create(btn_set_network, NULL);
	lv_label_set_static_text(btn_set_network_label, "Network");
	
	btn_set_wifi = lv_btn_create(settings_screen, NULL);
	lv_obj_set_pos(btn_set_wifi, 115, 160);
	lv_obj_set_size(btn_set_wifi, 90, 40);
	lv_obj_set_event_cb(btn_set_wifi, btn_set_wifi_callback);
	btn_set_wifi_label = lv_label_create(btn_set_wifi, NULL);
	lv_label_set_static_text(btn_set_wifi_label, "WiFi");
	
	btn_set_time = lv_btn_create(settings_screen, NULL);
	lv_obj_set_pos(btn_set_time, 215, 160);
	lv_obj_set_size(btn_set_time, 90, 40);
	lv_obj_set_event_cb(btn_set_time, btn_set_time_callback);
	btn_set_time_label = lv_label_create(btn_set_time, NULL);
	lv_label_set_static_text(btn_set_time_label, "Clock");
	
	// Camera IP address
	lbl_ip_addr = lv_label_create(settings_screen, NULL);
	lv_obj_set_pos(lbl_ip_addr, 15, 210);
	lv_obj_set_width(lbl_ip_addr, 100);
	lv_label_set_align(lbl_ip_addr, LV_LABEL_ALIGN_CENTER);
	
	// Local controls
	dd_rec_interval_label = lv_label_create(settings_screen, NULL);
	lv_obj_set_pos(dd_rec_interval_label, 15, 50);
	lv_obj_set_width(dd_rec_interval_label, 90);
	lv_label_set_align(dd_rec_interval_label, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(dd_rec_interval_label, "Rec Interval");
	
	dd_gain_mode_label = lv_label_create(settings_screen, NULL);
	lv_obj_set_pos(dd_gain_mode_label, 115, 50);
	lv_obj_set_width(dd_gain_mode_label, 90);
	lv_label_set_align(dd_gain_mode_label, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(dd_gain_mode_label, "Gain Mode");
	
	dd_palette_label = lv_label_create(settings_screen, NULL);
	lv_obj_set_pos(dd_palette_label, 215, 50);
	lv_obj_set_width(dd_palette_label, 90);
	lv_label_set_align(dd_palette_label, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(dd_palette_label, "Palette");
	
	// Drop-down lists are last so they can correctly draw over controls below them
	dd_rec_interval = lv_ddlist_create(settings_screen, NULL);
	lv_obj_set_pos(dd_rec_interval, 15, 70);
	lv_ddlist_set_fix_width(dd_rec_interval, 90);
	lv_ddlist_set_sb_mode(dd_rec_interval, LV_SB_MODE_AUTO);
	lv_obj_set_event_cb(dd_rec_interval, dd_rec_interval_callback);
	
	dd_gain_mode = lv_ddlist_create(settings_screen, NULL);
	lv_obj_set_pos(dd_gain_mode, 115, 70);
	lv_ddlist_set_fix_width(dd_gain_mode, 90);
	lv_ddlist_set_sb_mode(dd_gain_mode, LV_SB_MODE_AUTO);
	lv_obj_set_event_cb(dd_gain_mode, dd_gain_mode_callback);
	lv_ddlist_set_options(dd_gain_mode, dd_gain_mode_list);
	
	dd_palette = lv_ddlist_create(settings_screen, NULL);
	lv_obj_set_pos(dd_palette, 215, 70);
	lv_ddlist_set_fix_width(dd_palette, 90);
	lv_ddlist_set_sb_mode(dd_palette, LV_SB_MODE_AUTO);
	lv_obj_set_event_cb(dd_palette, dd_palette_callback);
	
	settings_screen_active = false;
	
	initialize_screen_values();
	
	return settings_screen;
}


/**
 * Initialize the settings screen's dynamic values
 */
void gui_screen_settings_active(bool en)
{
	settings_screen_active = en;
	
	if (en) {
		// Make a copy of the system's current gui state
		local_gui_st = gui_st;
		
		// Update controls as necessary
		if (local_gui_st.record_interval_index != lv_ddlist_get_selected(dd_rec_interval)) {
			lv_ddlist_set_selected(dd_rec_interval, local_gui_st.record_interval_index);
		}
		
		if (local_gui_st.gain_mode != lv_ddlist_get_selected(dd_gain_mode)) {
			lv_ddlist_set_selected(dd_gain_mode, local_gui_st.gain_mode);
		}
		
		if (local_gui_st.palette_index != lv_ddlist_get_selected(dd_palette)) {
			lv_ddlist_set_selected(dd_palette, local_gui_st.palette_index);
		}
		
		if (local_gui_st.rec_arducam_enable != lv_cb_is_checked(cb_en_arducam)) {
			lv_cb_set_checked(cb_en_arducam, local_gui_st.rec_arducam_enable);
		}
		
		if (local_gui_st.rec_lepton_enable != lv_cb_is_checked(cb_en_lepton)) {
			lv_cb_set_checked(cb_en_lepton, local_gui_st.rec_lepton_enable);
		}
	}
}


/**
 * LVGL (sub)task to periodically update IP address values on the screen
 */
void gui_screen_settings_update_task(lv_task_t * task)
{
	if (settings_screen_active) {
		settings_screen_update_ip_addr();
	}
}



//
// Settings GUI Screen internal functions
//

static void initialize_screen_values()
{
	// Initialize displayed on-screen values
	lv_cb_set_checked(cb_en_arducam, gui_st.rec_arducam_enable);
	lv_cb_set_checked(cb_en_lepton, gui_st.rec_lepton_enable);
	
	add_dd_rec_interval_entries();
	lv_ddlist_set_selected(dd_rec_interval, gui_st.record_interval_index);
	
	lv_ddlist_set_selected(dd_gain_mode, gui_st.gain_mode);
	
	add_dd_palette_entries();
	lv_ddlist_set_selected(dd_palette, gui_st.palette_index);
	
	ip_string[0] = 0;
	prev_wifi_ip_valid = false;
	for (int i=0; i<4; i++) prev_disp_ip_addr[i] = 0;
	lv_label_set_static_text(lbl_ip_addr, ip_string);
}


static void settings_screen_update_ip_addr()
{
	bool wifi_ip_valid;
	bool wifi_ip_changed = false;
	int i;
	wifi_info_t* wifi_info;
	
	wifi_info = wifi_get_info();
	
	// Determine display state
	if ((wifi_info->flags & WIFI_INFO_FLAG_CLIENT_MODE) != 0) {
		// Client (sta) mode - display IP if connected
		wifi_ip_valid = ((wifi_info->flags & WIFI_INFO_FLAG_CONNECTED) != 0);
		
	} else {
		// AP mode - display IP if enabled
		wifi_ip_valid = ((wifi_info->flags & WIFI_INFO_FLAG_ENABLED) != 0);
	}
	
	for (i=0; i<4; i++) {
		if (wifi_info->cur_ip_addr[i] != prev_disp_ip_addr[i]) {
			wifi_ip_changed = true;
			break;
		}
	}
	
	// Update the string if anything has changed
	if ((wifi_ip_valid != prev_wifi_ip_valid) || wifi_ip_changed) {
		if (wifi_ip_valid) {
			sprintf(ip_string, "%d.%d.%d.%d", wifi_info->cur_ip_addr[3],
			                                  wifi_info->cur_ip_addr[2],
			                                  wifi_info->cur_ip_addr[1],
			                                  wifi_info->cur_ip_addr[0]);
			for (i=0; i<4; i++) {
				prev_disp_ip_addr[i] = wifi_info->cur_ip_addr[i];
			}
		} else {
			// No text
			ip_string[0] = 0;
		}
		
		lv_label_set_static_text(lbl_ip_addr, ip_string);
		
		prev_wifi_ip_valid = wifi_ip_valid;
	}
}


static void btn_save_callback(lv_obj_t * btn, lv_event_t event)
{
	bool notify_after_update = false;
	
	if (event == LV_EVENT_CLICKED) {
		// Make sure at least one camera is enabled for recording before we allow exit
		if (!(local_gui_st.rec_arducam_enable || local_gui_st.rec_lepton_enable)) {
			gui_message_box(settings_screen, "At least one camera must be enabled for recording");
		} else {
			// Look for changed items that require updating other modules
			if (local_gui_st.gain_mode != gui_st.gain_mode) {
				lepton_gain_mode(local_gui_st.gain_mode);
			}
			if ((local_gui_st.record_interval != gui_st.record_interval) ||
			    (local_gui_st.rec_arducam_enable != gui_st.rec_arducam_enable) ||
			    (local_gui_st.rec_lepton_enable != gui_st.rec_lepton_enable))
			{
				notify_after_update = true;
			}
		
			// Save the settings in persistent storage
			gui_st = local_gui_st;
			ps_set_gui_state(&gui_st);
			
			// Send notification after gui_st has been updated
			if (notify_after_update) {
				xTaskNotify(task_handle_app, APP_NOTIFY_RECORD_PARM_UPD_MASK, eSetBits);
			}
		
			gui_set_screen(GUI_SCREEN_MAIN);
		}
	}
}


static void btn_exit_callback(lv_obj_t * btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		// Exit without saving anything
		gui_set_screen(GUI_SCREEN_MAIN);
	}
}


static void arducam_en_cb_callback(lv_obj_t * cb, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		local_gui_st.rec_arducam_enable = lv_cb_is_checked(cb);
	}
}


static void lepton_en_cb_callback(lv_obj_t * cb, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		local_gui_st.rec_lepton_enable = lv_cb_is_checked(cb);
	}
}


static void btn_set_network_callback(lv_obj_t * btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		gui_set_screen(GUI_SCREEN_NETWORK);
	}
}


static void btn_set_time_callback(lv_obj_t * btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		gui_set_screen(GUI_SCREEN_TIME);
	}
}


static void btn_set_wifi_callback(lv_obj_t * btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		gui_set_screen(GUI_SCREEN_WIFI);
	}
}


static void dd_rec_interval_callback(lv_obj_t * dd, lv_event_t event)
{
	int new_sel;
	
	if (event == LV_EVENT_VALUE_CHANGED) {
		new_sel = lv_ddlist_get_selected(dd);
		local_gui_st.record_interval_index = new_sel;
		local_gui_st.record_interval = record_intervals[new_sel].interval;
	}
}


static void dd_gain_mode_callback(lv_obj_t * dd, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		local_gui_st.gain_mode = lv_ddlist_get_selected(dd);
	}
}


static void dd_palette_callback(lv_obj_t * dd, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		local_gui_st.palette_index = lv_ddlist_get_selected(dd);
	}
}


static void add_dd_rec_interval_entries()
{
	const char* cp;
	int i;
	int n;
	
	// "[Entry0]\n" + "[Entry1]\n" + ... + "[EntryN]0"
	
	// Determine how much space we need for the string
	n = 0;
	for (i=0; i<REC_INT_NUM; i++) {
		cp = record_intervals[i].name;
		n += strlen(cp) + 1;   // +1 includes "\n" or final NULL character
	}
	
	// Allocate our string
	dd_rec_interval_list = malloc(n);
	if (dd_rec_interval_list == NULL) {
		return;
	}
	
	// Add edit objects to the drop-down menu
	n = 0;
	for (i=0; i<REC_INT_NUM; i++) {
		cp = record_intervals[i].name;
		while (*cp != 0) dd_rec_interval_list[n++] = *cp++;
		if (i != (REC_INT_NUM-1)) {
			// Add trailing "\n" to all but last entry
			dd_rec_interval_list[n++] = '\n';
		}
	}
	dd_rec_interval_list[n] = 0;  // Terminate string	
	
	// Finally, add the list of items to the drop-down menu
	lv_ddlist_set_options(dd_rec_interval, dd_rec_interval_list);
}


static void add_dd_palette_entries()
{
	char* cp;
	int i;
	int n;
	
	// "[Palette0]\n" + "[Palette1]\n" + ... + "[PaletteN]0"
	
	// Determine how much space we need for the string
	n = 0;
	for (i=0; i<REC_INT_NUM; i++) {
		cp = get_palette_name(i);
		n += strlen(cp) + 1;   // +1 includes "\n" or final NULL character
	}
	
	// Allocate our string
	dd_palette_list = malloc(n);
	if (dd_palette_list == NULL) {
		return;
	}
	
	// Add edit objects to the drop-down menu
	n = 0;
	for (i=0; i<PALETTE_COUNT; i++) {
		cp = get_palette_name(i);
		while (*cp != 0) dd_palette_list[n++] = *cp++;
		if (i != (PALETTE_COUNT-1)) {
			// Add trailing "\n" to all but last entry
			dd_palette_list[n++] = '\n';
		}
	}
	dd_palette_list[n] = 0;  // Terminate string	
	
	// Finally, add the list of items to the drop-down menu
	lv_ddlist_set_options(dd_palette, dd_palette_list);
}

