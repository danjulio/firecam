/*
 * Set Poweroff GUI screen related functions, callbacks and event handlers
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
#include "gui_screen_poweroff.h"
#include "gui_task.h"
#include "esp_system.h"
#include "lv_conf.h"

//
// Poweroff GUI Screen variables
//

// LVGL objects
static lv_obj_t* poweroff_screen;
static lv_obj_t* lbl_poweroff;

static lv_style_t lbl_poweroff_set_new_style;

// Screen state
static bool poweroff_screen_active;


//
// Poweroff GUI Screen API
//
lv_obj_t* gui_screen_poweroff_create()
{
	// Screen
	poweroff_screen = lv_obj_create(NULL, NULL);
	lv_obj_set_size(poweroff_screen, LV_HOR_RES_MAX, LV_VER_RES_MAX);
	lv_obj_set_style(poweroff_screen, &lv_style_plain_color);
	
	// Text
	lbl_poweroff = lv_label_create(poweroff_screen, NULL);
	lv_obj_set_pos(lbl_poweroff, 100, 100);
	lv_obj_set_width(lbl_poweroff, 100);
	lv_label_set_align(lbl_poweroff, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(lbl_poweroff, "Power Off!");
	
	// Modify the Text to use a larger font
	const lv_style_t* lbl_poweroff_set_style = lv_label_get_style(lbl_poweroff, LV_LABEL_STYLE_MAIN);
	lv_style_copy(&lbl_poweroff_set_new_style, lbl_poweroff_set_style);
	lbl_poweroff_set_new_style.text.font = &lv_font_roboto_28;
	lv_label_set_style(lbl_poweroff, LV_LABEL_STYLE_MAIN, &lbl_poweroff_set_new_style);
	poweroff_screen_active = false;
	
	return poweroff_screen;
}


void gui_screen_poweroff_set_active(bool en)
{
	// Nothing to do for this screen
	poweroff_screen_active = en;
}
