/*
 * Shared utility functions for GUI screens
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
#include "gui_utilities.h"
#include <string.h>


//
// GUI Utilities variables
//
static char preset_msgbox_string[GUI_MSG_BOX_MAX_LEN];

// LVGL objects
static lv_obj_t*  msg_box;

// Message box buttons
static const char* msg_box_buttons[] = {"Ok", ""};

//
// GUI Utilities Forward Declarations for internal functions
//
static void mbox_event_callback(lv_obj_t *obj, lv_event_t evt);



//
// GUI Utilities API
//

/**
 * Display a message box with OK button for dismissal
 */
void gui_message_box(lv_obj_t* parent, const char* msg)
{
	static lv_style_t modal_style;   // Message box background style
	
	// Create a full-screen background
	lv_style_copy(&modal_style, &lv_style_plain_color);
	
	// Set the background's style
	modal_style.body.main_color = modal_style.body.grad_color = LV_COLOR_BLACK;
	modal_style.body.opa = LV_OPA_50;
	
	// Create a base object for the modal background 
	lv_obj_t *obj = lv_obj_create(parent, NULL);
	lv_obj_set_style(obj, &modal_style);
	lv_obj_set_pos(obj, 0, 0);
	lv_obj_set_size(obj, LV_HOR_RES, LV_VER_RES);
	lv_obj_set_opa_scale_enable(obj, true); // Enable opacity scaling for the animation
	
	// Create the message box as a child of the modal background
	msg_box = lv_mbox_create(obj, NULL);
	lv_mbox_add_btns(msg_box, msg_box_buttons);
	lv_mbox_set_text(msg_box, msg);
	lv_obj_align(msg_box, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_event_cb(msg_box, mbox_event_callback);
	
	// Fade the message box in with an animation
	lv_anim_t a;
	lv_anim_init(&a);
	lv_anim_set_time(&a, 500, 0);
	lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
	lv_anim_set_exec_cb(&a, obj, (lv_anim_exec_xcb_t)lv_obj_set_opa_scale);
	lv_anim_create(&a);
}


/**
 * Set the string for gui_preset_message_box - this function is designed to be called
 * by another task who then sends a GUI_NOTIFY_MESSAGEBOX_MASK to gui_task to initiate
 * the message box
 */
void gui_preset_message_box_string(const char* msg)
{
	char c;
	int i = 0;
	
	// Copy up to GUI_MSG_BOX_MAX_LEN-1 characters (leaving room for null)
	while (i<GUI_MSG_BOX_MAX_LEN-1) {
		c = *(msg+i);
		preset_msgbox_string[i++] = c;
		if (c == 0) break;
	}
	preset_msgbox_string[i] = 0;
}


/**
 * Display a message box with the preset string - be sure to set the string first!!!
 */
void gui_preset_message_box(lv_obj_t* parent)
{
	gui_message_box(parent, preset_msgbox_string);
}



//
// GUI Utilities internal functions
//

/**
 * Message Box callback handling closure and deferred object deletion
 */
static void mbox_event_callback(lv_obj_t *obj, lv_event_t event)
{
	if ((event == LV_EVENT_DELETE) && (obj == msg_box)) {
		// Delete the parent modal background
		lv_obj_del_async(lv_obj_get_parent(msg_box));
		
		msg_box = NULL; // happens before object is actually deleted!

	} else if (event == LV_EVENT_VALUE_CHANGED) {
		// Button was clicked
		lv_mbox_start_auto_close(msg_box, 0);
	}
}
