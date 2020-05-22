/*
 * Persistent Storage Module
 *
 * Manage the persistent storage kept in the RTC chip RAM and provide access
 * routines to it.
 *
 * NOTE: It is assumed that only task will access persistent storage at a time.
 * This is done to eliminate the need for mutex protection, that could cause a 
 * dead-lock with another process also accessing a device via I2C.
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
#ifndef PS_UTILITIES_H
#define PS_UTILITIES_H

#include "sys_utilities.h"
#include "wifi_utilities.h"
#include <stdbool.h>
#include <stdint.h>



//
// PS Utilities Constants
//

// Base part of the default SSID/Camera name - the last 4 nibbles of the ESP32's
// mac address are appended as ASCII characters
#define PS_DEFAULT_AP_SSID "firecam-"

// Field lengths
#define PS_SSID_MAX_LEN     32
#define PS_PW_MAX_LEN       32
#define PS_PALETTE_NAME_LEN 16
#define PS_REC_INTERVAL_LEN 2



//
// PS Utilities API
//
void ps_init();
void ps_get_wifi_info(wifi_info_t* info);
void ps_set_wifi_info(const wifi_info_t* info);
bool ps_get_rec_enable();
void ps_set_rec_enable(bool en);
void ps_get_gui_state(gui_state_t* state);
void ps_set_gui_state(const gui_state_t* state);

#endif /* PS_UTILITIES_H */