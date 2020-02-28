/*
 * Wifi Configuration GUI screen related functions, callbacks and event handlers
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
#include "gui_screen_wifi.h"
#include "app_task.h"
#include "cmd_task.h"
#include "gui_task.h"
#include "esp_system.h"
#include "gui_utilities.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "wifi_utilities.h"
#include "lvgl/lvgl.h"
#include "lv_conf.h"
#include <string.h>



//
// Set WiFi GUI Screen private constants
//

// Currently selected text area
enum selected_text_area_t {
	SET_SSID,
	SET_PW
};



//
// Set Wifi GUI Screen variables
//

// LVGL objects
static lv_obj_t* wifi_screen;
static lv_obj_t* lbl_wifi_title;
static lv_obj_t* lbl_ssid;
static lv_obj_t* ta_ssid;
static lv_obj_t* lbl_pw;
static lv_obj_t* ta_pw;
static lv_obj_t* btn_show_password;
static lv_obj_t* lbl_btn_show_password;
static lv_obj_t* cb_wifi_en;
static lv_obj_t* kbd;

// Screen WiFi information
static char wifi_ssid_array[PS_SSID_MAX_LEN+1];
static char wifi_pw_array[PS_PW_MAX_LEN+1];
static wifi_info_t wifi_info = {
	wifi_ssid_array,
	wifi_pw_array,
	0
};

// Screen state
static bool wifi_screen_active;
static enum selected_text_area_t selected_text_area_index;
static lv_obj_t* selected_text_area_lv_obj;
static int selected_text_area_max_chars;

// Static array for lbl_btn_show_password lv_label_set_static_text
static char lbl_btn_show_password_text[4];    // Room for the icon text string and a null



//
// Set WiFi GUI Screen Forward Declarations for internal functions
//
static void update_values_from_ps();
static void set_active_text_area(enum selected_text_area_t n);
static void set_show_password_icon();
static void ssid_ta_callback(lv_obj_t* ta, lv_event_t event);
static void pw_ta_callback(lv_obj_t* ta, lv_event_t event);
static void show_pw_btn_callback(lv_obj_t* lbl, lv_event_t event);
static void wifi_en_cb_callback(lv_obj_t* cb, lv_event_t event);
static void kbd_callback(lv_obj_t* kb, lv_event_t event);



//
// Set WiFi GUI Screen API
//
lv_obj_t* gui_screen_wifi_create()
{
	wifi_screen = lv_obj_create(NULL, NULL);
	lv_obj_set_size(wifi_screen, LV_HOR_RES_MAX, LV_VER_RES_MAX);
	lv_obj_set_style(wifi_screen, &lv_style_plain_color);
	
	// Create the graphical elements for this screen
	//
	// Screen Title
	lbl_wifi_title = lv_label_create(wifi_screen, NULL);
	lv_obj_set_pos(lbl_wifi_title, 10, 5);
	lv_obj_set_width(lbl_wifi_title, 100);
	lv_label_set_align(lbl_wifi_title, LV_LABEL_ALIGN_LEFT);
	lv_label_set_static_text(lbl_wifi_title, "Set WiFi Access Point");
	
	// WiFi SSID text entry area label
	lbl_ssid = lv_label_create(wifi_screen, NULL);
	lv_obj_set_pos(lbl_ssid, 10, 50);
	lv_obj_set_width(lbl_ssid, 60);
	lv_label_set_static_text(lbl_ssid, "SSID:");
	
	// WiFi SSID text entry text area
	ta_ssid = lv_ta_create(wifi_screen, NULL);
	lv_obj_set_pos(ta_ssid, 90, 45);
	lv_obj_set_width(ta_ssid, 190);
	lv_ta_set_text_align(ta_ssid, LV_LABEL_ALIGN_LEFT);
	lv_ta_set_one_line(ta_ssid, true);
	lv_ta_set_max_length(ta_ssid, PS_SSID_MAX_LEN);
	lv_obj_set_event_cb(ta_ssid, ssid_ta_callback);
	
	// WiFi Password text entry area label
	lbl_pw = lv_label_create(wifi_screen, NULL);
	lv_obj_set_pos(lbl_pw, 10, 85);
	lv_obj_set_width(lbl_pw, 60);
	lv_label_set_static_text(lbl_pw, "Password:");
	
	// WiFi Password text entry text area
	ta_pw = lv_ta_create(wifi_screen, NULL);
	lv_obj_set_pos(ta_pw, 90, 80);
	lv_obj_set_width(ta_pw, 190);
	lv_ta_set_text_align(ta_pw, LV_LABEL_ALIGN_LEFT);
	lv_ta_set_one_line(ta_pw, true);
	lv_ta_set_max_length(ta_pw, PS_PW_MAX_LEN);
	lv_ta_set_pwd_mode(ta_pw, true);
	lv_obj_set_event_cb(ta_pw, pw_ta_callback);
	
	// Show password button
	btn_show_password = lv_btn_create(wifi_screen, NULL);
	lv_obj_set_pos(btn_show_password, 285, 80);
	lv_obj_set_width(btn_show_password, 30);
	lv_obj_set_height(btn_show_password, 30);
	lv_obj_set_event_cb(btn_show_password, show_pw_btn_callback);
	
	// Show password button label
	lbl_btn_show_password = lv_label_create(btn_show_password, NULL);
	set_show_password_icon();
	lv_label_set_static_text(lbl_btn_show_password, lbl_btn_show_password_text);
	
	// WiFi Enable checkbox
	cb_wifi_en = lv_cb_create(wifi_screen, NULL);
	lv_obj_set_pos(cb_wifi_en, 230, 10);
	lv_obj_set_width(cb_wifi_en, 40);
	lv_cb_set_static_text(cb_wifi_en, "Enable");
	lv_obj_set_event_cb(cb_wifi_en, wifi_en_cb_callback);
	
	// Styles for the keyboard
    static lv_style_t rel_style, pr_style;
    
    lv_style_copy(&rel_style, &lv_style_btn_rel);
    rel_style.body.radius = 0;
    rel_style.body.border.width = 1;

    lv_style_copy(&pr_style, &lv_style_btn_pr);
    pr_style.body.radius = 0;
    pr_style.body.border.width = 1;
    
	// Keyboard
	kbd = lv_kb_create(wifi_screen, NULL);
    lv_kb_set_cursor_manage(kbd, true);
    lv_kb_set_style(kbd, LV_KB_STYLE_BG, &lv_style_transp_tight);
    lv_kb_set_style(kbd, LV_KB_STYLE_BTN_REL, &rel_style);
    lv_kb_set_style(kbd, LV_KB_STYLE_BTN_PR, &pr_style);
    lv_obj_align(kbd, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    lv_obj_set_event_cb(kbd, kbd_callback);
    
    update_values_from_ps();
    set_active_text_area(SET_SSID);
    
    wifi_screen_active = false;
		
	return wifi_screen;
}


/**
 * Initialize the time screen's dynamic values
 */
void gui_screen_wifi_set_active(bool en)
{
	wifi_screen_active = en;
	
	if (en) {
		// Get the current WiFi values into our value and update our controls
		update_values_from_ps();
	}
}



//
// Set WiFi GUI Screen internal functions
//

/**
 * Get the system values and update our controls
 */
static void update_values_from_ps()
{
	ps_get_wifi_info(&wifi_info);
	
	lv_ta_set_text(ta_ssid, wifi_info.ssid);
	lv_ta_set_text(ta_pw, wifi_info.pw);
	lv_cb_set_checked(cb_wifi_en, ((wifi_info.flags & WIFI_INFO_FLAG_STARTUP_ENABLE) != 0));
}


/**
 * Select the currently active text area, set its cursor at the end and
 * connect it to the keyboard.  Make the cursor invisible in the other text
 * area.
 */
void set_active_text_area(enum selected_text_area_t n)
{
	selected_text_area_index = n;
	
	if (n == SET_SSID) {
		selected_text_area_lv_obj = ta_ssid;
		selected_text_area_max_chars = PS_SSID_MAX_LEN;
		lv_ta_set_cursor_type(ta_ssid, LV_CURSOR_LINE);
		lv_ta_set_cursor_type(ta_pw, LV_CURSOR_LINE | LV_CURSOR_HIDDEN);
	} else {
		selected_text_area_lv_obj = ta_pw;
		selected_text_area_max_chars = PS_PW_MAX_LEN;
		lv_ta_set_cursor_type(ta_ssid, LV_CURSOR_LINE | LV_CURSOR_HIDDEN);
		lv_ta_set_cursor_type(ta_pw, LV_CURSOR_LINE);
	}
	
	lv_ta_set_cursor_pos(selected_text_area_lv_obj, LV_TA_CURSOR_LAST);
	
	lv_kb_set_ta(kbd, selected_text_area_lv_obj);
}


/**
 * Update the Show Password button label based on the current text area show password state
 */
static void set_show_password_icon()
{
	memset(lbl_btn_show_password_text, 0, sizeof(lbl_btn_show_password_text));
	
	if (lv_ta_get_pwd_mode(ta_pw)) {
		strcpy(lbl_btn_show_password_text, LV_SYMBOL_EYE_CLOSE);
	} else {
		strcpy(lbl_btn_show_password_text, LV_SYMBOL_EYE_OPEN);
	}
	// Invalidate the object to force a redraw
	lv_obj_invalidate(lbl_btn_show_password);
}



/**
 * Set SSID text area handler - select the Set SSID text area if not already selected
 */
static void ssid_ta_callback(lv_obj_t* ta, lv_event_t event)
{
	if (selected_text_area_index != SET_SSID) {
		if (event == LV_EVENT_CLICKED) {
			// User just selected us
			set_active_text_area(SET_SSID);
		}
	}
}


/**
 * Set PW text area handler - select the Set PW text area if not already selected
 */
static void pw_ta_callback(lv_obj_t* ta, lv_event_t event)
{
	if (selected_text_area_index != SET_PW) {
		if (event == LV_EVENT_CLICKED) {
			// User just selected us
			set_active_text_area(SET_PW);
		}
	}
}


/**
 * Show password button callback
 */
static void show_pw_btn_callback(lv_obj_t* lbl, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		lv_ta_set_pwd_mode(ta_pw, !lv_ta_get_pwd_mode(ta_pw));
		set_show_password_icon();
		
		// Invalidate the object to force a redraw
		lv_obj_invalidate(ta_pw);
	}
}


/**
 * Wifi Enable Checkbox handler
 */
static void wifi_en_cb_callback(lv_obj_t* cb, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		if (lv_cb_is_checked(cb)) {
			wifi_info.flags |= WIFI_INFO_FLAG_STARTUP_ENABLE;
		} else {
			wifi_info.flags &= ~WIFI_INFO_FLAG_STARTUP_ENABLE;
		}
	}
}


/**
 * Keyboard handler
 *   Process close buttons ourselves
 *   Let the active text area process all other keypresses
 */
static void kbd_callback(lv_obj_t* kb, lv_event_t event)
{
	// First look for close keys
	if (event == LV_EVENT_CANCEL) {
		gui_set_screen(GUI_SCREEN_MAIN);
	}
	
	if (event == LV_EVENT_APPLY) {
		char* ssid_ta_str = (char*) lv_ta_get_text(ta_ssid);
		char* pw_ta_str = (char*) lv_ta_get_text(ta_pw);
		
		if (strlen(ssid_ta_str) == 0) {
			gui_message_box(wifi_screen, "SSID must contain a valid string");
		} else if ((strlen(pw_ta_str) < 8) && (strlen(pw_ta_str) != 0)) {
			gui_message_box(wifi_screen, "WPA2 passwords must be at least 8 characters");
		} else {
			// Copy the text area strings to our data structure
			strcpy(wifi_info.ssid, ssid_ta_str);
			strcpy(wifi_info.pw, pw_ta_str);
			
			// Save the new WiFi configuration
			ps_set_wifi_info(&wifi_info);

			// Notify app_task of the update
			xTaskNotify(task_handle_app, APP_NOTIFY_NEW_WIFI_MASK, eSetBits);
			
			gui_set_screen(GUI_SCREEN_MAIN);
		}
	}
	
	// Then let the normal keyboard handler run (text handling in attached text area)
	lv_kb_def_event_cb(kb, event);
}

