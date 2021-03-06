/*
 * WiFi related utilities
 *
 * Contains functions to initialize the wifi interface, utility functions, and a set
 * of interface functions.
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
#ifndef WIFI_UTILITIES_H
#define WIFI_UTILITIES_H

#include <stdbool.h>
#include <stdint.h>

//
// WiFi Utilities Constants
//

// wifi_info_t flags
#define WIFI_INFO_FLAG_STARTUP_ENABLE 0x01
#define WIFI_INFO_FLAG_INITIALIZED    0x02
#define WIFI_INFO_FLAG_ENABLED        0x04
#define WIFI_INFO_FLAG_CONNECTED      0x08
#define WIFI_INFO_FLAG_CL_STATIC_IP   0x10
#define WIFI_INFO_FLAG_CLIENT_MODE    0x80

// Maximum attempts to reconnect to an AP in client mode
#define WIFI_MAX_RECONNECT_ATTEMPTS   5



//
// WiFi Utilities Data structures
//
typedef struct {
	char* ap_ssid;             // AP SSID is also the Camera Name
	char* sta_ssid;
	char* ap_pw;
	char* sta_pw;
	uint8_t flags;
	uint8_t ap_ip_addr[4];
	uint8_t sta_ip_addr[4];
	uint8_t cur_ip_addr[4];
} wifi_info_t;


//
// WiFi Utilities API
//
bool wifi_init();
bool wifi_reinit();
bool wifi_is_connected();
wifi_info_t* wifi_get_info();

#endif /* WIFI_UTILITIES_H */