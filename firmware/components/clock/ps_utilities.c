/*
 * Persistent Storage Module
 *
 * Manage the persistent storage kept in the RTC chip RAM and provide access
 * routines to it.
 *
 * NOTE: It is assumed that only one task will access persistent storage at a time.
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
#include "ps_utilities.h"
#include "system_config.h"
#include "ds3232.h"
#include "esp_system.h"
#include "esp_log.h"
#include "palettes.h"
#include <stdbool.h>
#include <string.h>


//
// PS Utilities internal constants
//

// "Magic Word" constants
#define PS_MAGIC_WORD_0 0x12
#define PS_MAGIC_WORD_1 0x34

// Layout version - to allow future firmware versions to change the layout without
// losing data
#define PS_LAYOUT_VERSION 2

// Memory Array indicies
//   String regions include an extra byte for a null terminator

// Original version 1 contents
#define PS_MAGIC_WORD_0_ADDR   0
#define PS_MAGIC_WORD_1_ADDR   1
#define PS_LAYOUT_VERSION_ADDR 2
#define PS_REC_EN_ADDR         3
#define PS_WIFI_EN_ADDR        4
#define PS_WIFI_AP_SSID_ADDR   5
#define PS_WIFI_AP_PW_ADDR     (PS_WIFI_AP_SSID_ADDR + PS_SSID_MAX_LEN + 1)

// Version 2 additions
#define PS_WIFI_STA_SSID_ADDR  (PS_WIFI_AP_PW_ADDR + PS_PW_MAX_LEN + 1)
#define PS_WIFI_STA_PW_ADDR    (PS_WIFI_STA_SSID_ADDR + PS_SSID_MAX_LEN + 1)
#define PS_WIFI_AP_IP_ADDR     (PS_WIFI_STA_PW_ADDR + PS_PW_MAX_LEN + 1)
#define PS_WIFI_STA_IP_ADDR    (PS_WIFI_AP_IP_ADDR + 4)
#define PS_REC_ARD_EN_ADDR     (PS_WIFI_STA_IP_ADDR + 4)
#define PS_REC_LEP_EN_ADDR     (PS_REC_ARD_EN_ADDR + 1)
#define PS_GAIN_MODE_ADDR      (PS_REC_LEP_EN_ADDR + 1)
#define PS_PALETTE_NAME_ADDR   (PS_GAIN_MODE_ADDR + 1)
#define PS_REC_INTERVAL_ADDR   (PS_PALETTE_NAME_ADDR + PS_PALETTE_NAME_LEN + 1)

#define PS_LAST_VALID_ADDR     (PS_REC_INTERVAL_ADDR + PS_REC_INTERVAL_LEN)
#define PS_CHECKSUM_ADDR       (SRAM_SIZE - 1)

// Update region lengths
#define PS_REC_EN_UPD_LEN      1
#define PS_WIFI_UPD_LEN        (PS_REC_ARD_EN_ADDR - PS_WIFI_EN_ADDR)
#define PS_GUI_UPD_LEN         (PS_LAST_VALID_ADDR - PS_REC_ARD_EN_ADDR)

// Stored Wifi Flags bitmask
#define PS_WIFI_FLAG_MASK      (WIFI_INFO_FLAG_STARTUP_ENABLE | WIFI_INFO_FLAG_CL_STATIC_IP | WIFI_INFO_FLAG_CLIENT_MODE)


enum ps_update_types_t {
	FULL,                      // Update all bytes in the external SRAM
	WIFI,                      // Update wifi-related and checksum
	REC,                       // Update record enable and checksum
	GUI                        // Update GUI state related and checksum
};



//
// PS Utilities Internal variables
//
static const char* TAG = "ps_utilities";

// Our local copy for reading
static uint8_t ps_shadow_buffer[SRAM_SIZE];



//
// PS Utilities Forward Declarations for internal functions
//
static bool ps_read_array();
static bool ps_write_array(enum ps_update_types_t t);
static void ps_init_array(bool upgrade);
static void ps_store_string(char* s, uint8_t start, uint8_t max_len);
static bool ps_valid_magic_word();
static uint8_t ps_compute_checksum();
static int ps_write_bytes_to_rtc(uint8_t start_addr, uint8_t* data, uint8_t data_len);
static char ps_nibble_to_ascii(uint8_t n);



//
// PS Utilities API
//

/**
 * Initialize persistent storage
 *   - Load our local buffer
 *   - Initialize it and the RTC SRAM with valid data if necessary
 *   - Upgrade version 1 layout if necessary
 */
void ps_init()
{
	// Get the persistent data from the battery-backed RTC chip
	if (!ps_read_array()) {
		ESP_LOGE(TAG, "Failed to read persistent data from RTC SRAM");
	}
	
	// Check if it is initialized with valid data, initialize if not
	if (!ps_valid_magic_word() || (ps_compute_checksum() != ps_shadow_buffer[PS_CHECKSUM_ADDR])) {
		ESP_LOGI(TAG, "Initialize persistent storage with default values");
		ps_init_array(false);
		if (!ps_write_array(FULL)) {
			ESP_LOGE(TAG, "Failed to write persistent data to RTC SRAM");
		}
	} else if (ps_shadow_buffer[PS_LAYOUT_VERSION_ADDR] == 1) {
		ESP_LOGI(TAG, "Upgrading persistent storage from version 1");
		ps_init_array(true);
	}
}


/**
 * Get WiFi info from persistent storage.
 * Note: the string buffers in the wifi data structure passed in must have enough room
 *       for the maximum sized string
 */
void ps_get_wifi_info(wifi_info_t* info)
{
	int i;
	
	strcpy(info->ap_ssid, (const char*) &ps_shadow_buffer[PS_WIFI_AP_SSID_ADDR]);
	strcpy(info->ap_pw, (const char*) &ps_shadow_buffer[PS_WIFI_AP_PW_ADDR]);
	strcpy(info->sta_ssid, (const char*) &ps_shadow_buffer[PS_WIFI_STA_SSID_ADDR]);
	strcpy(info->sta_pw, (const char*) &ps_shadow_buffer[PS_WIFI_STA_PW_ADDR]);
	
	info->flags = ps_shadow_buffer[PS_WIFI_EN_ADDR] & PS_WIFI_FLAG_MASK;
	
	for (i=0; i<4; i++) {
		info->ap_ip_addr[i] = ps_shadow_buffer[PS_WIFI_AP_IP_ADDR + i];
		info->sta_ip_addr[i] = ps_shadow_buffer[PS_WIFI_STA_IP_ADDR + i];
	}
}


/**
 * Store Wifi info into persistent storage (both the local buffer and RTC SRAM)
 */
void ps_set_wifi_info(const wifi_info_t* info)
{
	int i;
	
	ps_store_string(info->ap_ssid, PS_WIFI_AP_SSID_ADDR, PS_SSID_MAX_LEN);
	ps_store_string(info->ap_pw, PS_WIFI_AP_PW_ADDR, PS_PW_MAX_LEN);
	ps_store_string(info->sta_ssid, PS_WIFI_STA_SSID_ADDR, PS_SSID_MAX_LEN);
	ps_store_string(info->sta_pw, PS_WIFI_STA_PW_ADDR, PS_PW_MAX_LEN);
	ps_shadow_buffer[PS_WIFI_EN_ADDR] = info->flags & PS_WIFI_FLAG_MASK;
	ps_shadow_buffer[PS_CHECKSUM_ADDR] = ps_compute_checksum();
	for (i=0; i<4; i++) {
		ps_shadow_buffer[PS_WIFI_AP_IP_ADDR + i] = info->ap_ip_addr[i];
		ps_shadow_buffer[PS_WIFI_STA_IP_ADDR + i] = info->sta_ip_addr[i];
	}
	ps_shadow_buffer[PS_CHECKSUM_ADDR] = ps_compute_checksum();
	if (!ps_write_array(WIFI)) {
		ESP_LOGE(TAG, "Failed to write WiFi data to RTC SRAM");
	}
}


/**
 * Get recording mode
 */
bool ps_get_rec_enable()
{
	if (ps_shadow_buffer[PS_REC_EN_ADDR] != 0) {
		return true;
	} else {
		return false;
	}
}


/**
 * Store recording mode into persistent storage (both the local buffer and RTC SRAM)
 */
void ps_set_rec_enable(bool en)
{
	ps_shadow_buffer[PS_REC_EN_ADDR] = en ? 1 : 0;
	ps_shadow_buffer[PS_CHECKSUM_ADDR] = ps_compute_checksum();
	if (!ps_write_array(REC)) {
		ESP_LOGE(TAG, "Failed to write record enable to RTC SRAM");
	}
}


/**
 * Get GUI Camera state
 */
void ps_get_gui_state(gui_state_t* state)
{
	bool repair_mem = false;
	
	state->rec_arducam_enable = ps_shadow_buffer[PS_REC_ARD_EN_ADDR] != 0 ? true : false;
	state->rec_lepton_enable = ps_shadow_buffer[PS_REC_LEP_EN_ADDR] != 0 ? true : false;
	
	state->gain_mode = ps_shadow_buffer[PS_GAIN_MODE_ADDR];
	
	state->record_interval = (ps_shadow_buffer[PS_REC_INTERVAL_ADDR] << 8) |
	                         ps_shadow_buffer[PS_REC_INTERVAL_ADDR + 1];
	state->record_interval_index = system_get_rec_interval_index(state->record_interval);
	if (state->record_interval_index < 0) {
		// Fix illegal entry
		state->record_interval_index = 0;
		state->record_interval = record_intervals[state->record_interval_index].interval;
		ps_shadow_buffer[PS_REC_INTERVAL_ADDR] = state->record_interval >> 8;
		ps_shadow_buffer[PS_REC_INTERVAL_ADDR + 1] = state->record_interval & 0xFF;
		repair_mem = true;
		ESP_LOGE(TAG, "reset record_interval to legal value");
	}
	
	state->palette_index = get_palette_by_name((const char*) &ps_shadow_buffer[PS_PALETTE_NAME_ADDR]);
	if (state->palette_index < 0) {
		state->palette_index = 0;
		ps_store_string(get_palette_name(state->palette_index), PS_PALETTE_NAME_ADDR, PS_PALETTE_NAME_LEN);
		repair_mem = true;
		ESP_LOGE(TAG, "reset palette to legal value");
	}
	
	if (repair_mem) {
		ps_shadow_buffer[PS_CHECKSUM_ADDR] = ps_compute_checksum();
		if (!ps_write_array(GUI)) {
			ESP_LOGE(TAG, "Failed to write GUI state to RTC SRAM");
		}
	}
}

 
/**
 * Store GUI Camera state into persistent storage (both the local buffer and RTC SRAM)
 */
void ps_set_gui_state(const gui_state_t* state)
{
	ps_shadow_buffer[PS_REC_ARD_EN_ADDR] = state->rec_arducam_enable ? 1 : 0;
	ps_shadow_buffer[PS_REC_LEP_EN_ADDR] = state->rec_lepton_enable ? 1 : 0;
	ps_shadow_buffer[PS_GAIN_MODE_ADDR] = state->gain_mode;
	ps_shadow_buffer[PS_REC_INTERVAL_ADDR] = state->record_interval >> 8;
	ps_shadow_buffer[PS_REC_INTERVAL_ADDR + 1] = state->record_interval & 0xFF;
	ps_store_string(get_palette_name(state->palette_index), PS_PALETTE_NAME_ADDR, PS_PALETTE_NAME_LEN);
	ps_shadow_buffer[PS_CHECKSUM_ADDR] = ps_compute_checksum();
	if (!ps_write_array(GUI)) {
		ESP_LOGE(TAG, "Failed to write GUI state to RTC SRAM");
	}
}



//
// PS Utilities internal functions
//

/**
 * Load our local buffer from the RTC SRAM
 */
static bool ps_read_array()
{
	return (read_rtc_bytes(SRAM_START_ADDR, ps_shadow_buffer, SRAM_SIZE) == 0);
}


/**
 * Write parts (to reduce locked I2C time) or the full local buffer to RTC SRAM
 */
static bool ps_write_array(enum ps_update_types_t t)
{
	bool ret = false;
	
	switch(t) {
	case FULL:
		ret = (ps_write_bytes_to_rtc(SRAM_START_ADDR, ps_shadow_buffer, SRAM_SIZE) == 0);
		break;
	
	case WIFI:
		if (ps_write_bytes_to_rtc(SRAM_START_ADDR + PS_WIFI_EN_ADDR,
		                          &ps_shadow_buffer[PS_WIFI_EN_ADDR],
		                          PS_WIFI_UPD_LEN) == 0)
		{
			ret = (write_rtc_byte(SRAM_START_ADDR + PS_CHECKSUM_ADDR,
			                       ps_shadow_buffer[PS_CHECKSUM_ADDR]) == 0);
		} else {
			ret = false;
		}
		break;
	
	case REC:
		if (write_rtc_byte(SRAM_START_ADDR + PS_REC_EN_ADDR,
		                    ps_shadow_buffer[PS_REC_EN_ADDR]) == 0)
		{
			ret = (write_rtc_byte(SRAM_START_ADDR + PS_CHECKSUM_ADDR,
			                       ps_shadow_buffer[PS_CHECKSUM_ADDR]) == 0);
		} else {
			ret = false;
		}
		break;
	
	case GUI:
		if (ps_write_bytes_to_rtc(SRAM_START_ADDR + PS_REC_ARD_EN_ADDR,
		                          &ps_shadow_buffer[PS_REC_ARD_EN_ADDR],
		                          PS_GUI_UPD_LEN) == 0)
		{
			ret = (write_rtc_byte(SRAM_START_ADDR + PS_CHECKSUM_ADDR,
			                       ps_shadow_buffer[PS_CHECKSUM_ADDR]) == 0);
		} else {
			ret = false;
		}
		break;
	}
	
	return ret;
}


/**
 * Initialize our local array with default values.
 *   Upgrade from version 1 layout if requested
 */
static void ps_init_array(bool upgrade)
{
	uint8_t sys_mac_addr[6];
	char def_ssid[PS_SSID_MAX_LEN + 1];
	
	if (!upgrade) {
		// Initialize the full array
		
		// Zero buffer
		memset(ps_shadow_buffer, 0, SRAM_SIZE);
	
		// Get the system's default MAC address and add 1 to match the "Soft AP" mode
		// (see "Miscellaneous System APIs" in the ESP-IDF documentation)
		esp_efuse_mac_get_default(sys_mac_addr);
		sys_mac_addr[5] = sys_mac_addr[5] + 1;
	
		// Construct our default SSID/Camera name
		sprintf(def_ssid, "%s%c%c%c%c", PS_DEFAULT_AP_SSID,
		        ps_nibble_to_ascii(sys_mac_addr[4] >> 4),
		        ps_nibble_to_ascii(sys_mac_addr[4]),
		        ps_nibble_to_ascii(sys_mac_addr[5] >> 4),
	 	        ps_nibble_to_ascii(sys_mac_addr[5]));
		
		// Load fields
		ps_shadow_buffer[PS_MAGIC_WORD_0_ADDR] = PS_MAGIC_WORD_0;
		ps_shadow_buffer[PS_MAGIC_WORD_1_ADDR] = PS_MAGIC_WORD_1;
		ps_shadow_buffer[PS_LAYOUT_VERSION_ADDR] = PS_LAYOUT_VERSION;
		ps_shadow_buffer[PS_REC_EN_ADDR] = 0;
		ps_shadow_buffer[PS_WIFI_EN_ADDR] = WIFI_INFO_FLAG_STARTUP_ENABLE;
		ps_store_string(def_ssid, PS_WIFI_AP_SSID_ADDR, PS_SSID_MAX_LEN);
		ps_store_string("", PS_WIFI_AP_PW_ADDR, PS_PW_MAX_LEN);
	} else {
		// Leave version 1 values but upgrade the version number
		ps_shadow_buffer[PS_LAYOUT_VERSION_ADDR] = PS_LAYOUT_VERSION;
	}
	
	// Add default values for new items in this version
	ps_store_string("", PS_WIFI_STA_SSID_ADDR, PS_SSID_MAX_LEN);
	ps_store_string("", PS_WIFI_STA_PW_ADDR, PS_PW_MAX_LEN);
	ps_shadow_buffer[PS_WIFI_AP_IP_ADDR  + 3] = 192;
	ps_shadow_buffer[PS_WIFI_AP_IP_ADDR  + 2] = 168;
	ps_shadow_buffer[PS_WIFI_AP_IP_ADDR  + 1] = 4;
	ps_shadow_buffer[PS_WIFI_AP_IP_ADDR  + 0] = 1;
	ps_shadow_buffer[PS_WIFI_STA_IP_ADDR + 3] = 192;
	ps_shadow_buffer[PS_WIFI_STA_IP_ADDR + 2] = 168;
	ps_shadow_buffer[PS_WIFI_STA_IP_ADDR + 1] = 4;
	ps_shadow_buffer[PS_WIFI_STA_IP_ADDR + 0] = 2;
	ps_shadow_buffer[PS_REC_ARD_EN_ADDR] = 1;
	ps_shadow_buffer[PS_REC_LEP_EN_ADDR] = 1;
	ps_shadow_buffer[PS_GAIN_MODE_ADDR] = (uint8_t) LEP_DEF_GAIN_MODE;
	ps_store_string("Fusion", PS_PALETTE_NAME_ADDR, PS_PALETTE_NAME_LEN);
	ps_shadow_buffer[PS_REC_INTERVAL_ADDR] = 0;
	ps_shadow_buffer[PS_REC_INTERVAL_ADDR + 1] = 1;
	
	// Finally compute and load checksum
	ps_shadow_buffer[PS_CHECKSUM_ADDR] = ps_compute_checksum();
}


/**
 * Store a string at the specified location in our local buffer making sure it does
 * not exceed the available space and is terminated with a null character.
 */
static void ps_store_string(char* s, uint8_t start, uint8_t max_len)
{
	char c;
	int i = 0;
	bool saw_s_end = false;
	
	while (i < max_len) {
		if (!saw_s_end) {
			// Copy string data
			c = *(s+i);
			ps_shadow_buffer[start + i] = c;
			if (c == 0) saw_s_end = true;
		} else {
			// Pad with nulls
			ps_shadow_buffer[start + i] = 0;
		}
		i++;
	}
}


/**
 * Return true if our local array starts with the magic word
 */
static bool ps_valid_magic_word()
{
	return ((ps_shadow_buffer[PS_MAGIC_WORD_0_ADDR] == PS_MAGIC_WORD_0) &&
	        (ps_shadow_buffer[PS_MAGIC_WORD_1_ADDR] == PS_MAGIC_WORD_1));
}


/**
 * Compute the checksum over all non-checksum bytes.  The checksum is simply their
 * summation.
 */
static uint8_t ps_compute_checksum()
{
	int i;
	uint8_t cs = 0;
	
	for (i=0; i<PS_CHECKSUM_ADDR; i++) {
		cs += ps_shadow_buffer[i];
	}
	
	return cs;
}


/**
 * Wrapper function for write_rtc_bytes
 */
static int ps_write_bytes_to_rtc(uint8_t start_addr, uint8_t* data, uint8_t data_len)
{
	int i;
	uint8_t rtc_bytes[SRAM_SIZE+1];
	
	rtc_bytes[0] = start_addr;
	for (i=1; i<=data_len; i++) {
		rtc_bytes[i] = *(data + i - 1);
	}
	
	return write_rtc_bytes(rtc_bytes, data_len+1);
}


/**
 * Return an ASCII character version of a 4-bit hexadecimal number
 */
static char ps_nibble_to_ascii(uint8_t n)
{
	n = n & 0x0F;
	
	if (n < 10) {
		return '0' + n;
	} else {
		return 'A' + n - 10;
	}
}
