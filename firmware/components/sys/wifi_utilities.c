/*
 * WiFi related utilities
 *
 * Contains functions to initialize the wifi interface, utility functions, and a set
 * of interface functions.  Also includes the system event handler for use by the wifi
 * system.
 *
 * Note: Currently only 1 station is allowed to connect at a time.
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
#include "wifi_utilities.h"
#include "ps_utilities.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include <string.h>



//
// Wifi Utilities local variables
//
static const char* TAG = "wifi_utilities";

// Wifi information
static char wifi_ssid_array[PS_SSID_MAX_LEN+1];
static char wifi_pw_array[PS_PW_MAX_LEN+1];
static wifi_info_t wifi_info = {
	wifi_ssid_array,
	wifi_pw_array,
	0
};

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t wifi_event_group;



//
// WiFi Utilities Forward Declarations for internal functions
//
static bool init_esp_wifi();
static bool enable_esp_wifi_ap(bool en);
static esp_err_t sys_event_handler(void *ctx, system_event_t* event);


//
// WiFi Utilities API
//

/**
 * Power-on initialization of the WiFi system.  It is enabled based on start-up
 * information from persistent storage.  Returns false if any part of the initialization
 * fails.
 */
bool wifi_init()
{
	esp_err_t ret;
	
	// Setup the event handler
	wifi_event_group = xEventGroupCreate();
	ret = esp_event_loop_init(sys_event_handler, NULL);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not initialize event loop handler (%d)", ret);
		return false;
	}
	
	// Initialize the TCP/IP stack
	tcpip_adapter_init();
	
	// Initialize NVS
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ret = nvs_flash_erase();
		if (ret != ESP_OK) {
			ESP_LOGI(TAG, "nvs_flash_erase failed (%d)", ret);
			return false;
		}
		ret = nvs_flash_init();
	}
	if (ret != ESP_OK) {
		ESP_LOGI(TAG, "nvs_flash_init failed (%d)", ret);
		return false;
	}
	
	// Get our wifi info
	ps_get_wifi_info(&wifi_info);
	
	// Initialize the WiFi interface
	if (init_esp_wifi()) {
		wifi_info.flags |= WIFI_INFO_FLAG_INITIALIZED;
		
		// Configure the WiFi interface if enabled
		if ((wifi_info.flags & WIFI_INFO_FLAG_STARTUP_ENABLE) != 0) {
			if (enable_esp_wifi_ap(true)) {
				wifi_info.flags |= WIFI_INFO_FLAG_ENABLED;
				ESP_LOGI(TAG, "WiFi AP %s enabled", wifi_info.ssid);
			} else {
				return false;
			}
		}
	} else {
		return false;
	}
	
	return true;
}


/**
 * Re-initialize the WiFi system when information such as the SSID, password or enabe-
 * state have changed.  Returns false if anything fails.
 */
bool wifi_reinit()
{
	// Update the wifi info because we're called when it's updated
	ps_get_wifi_info(&wifi_info);
	
	if ((wifi_info.flags & WIFI_INFO_FLAG_INITIALIZED) == 0) {
		// Attempt to initialize the wifi interface again
		if (init_esp_wifi()) {
			wifi_info.flags |= WIFI_INFO_FLAG_INITIALIZED;
		} else {
			return false;
		}
	}
	
	// Shut down the old configuration
	if ((wifi_info.flags & WIFI_INFO_FLAG_ENABLED) != 0) {
		enable_esp_wifi_ap(false);
		wifi_info.flags &= ~WIFI_INFO_FLAG_ENABLED;
	}
	
	// Reconfigure the interface if enabled
	if ((wifi_info.flags & WIFI_INFO_FLAG_STARTUP_ENABLE) != 0) {
		if (enable_esp_wifi_ap(true)) {
			wifi_info.flags |= WIFI_INFO_FLAG_ENABLED;
			ESP_LOGI(TAG, "WiFi AP %s enabled", wifi_info.ssid);
		} else {
			return false;
		}
	}
	
	// Nothing should be connected now
	wifi_info.flags &= ~WIFI_INFO_FLAG_CONNECTED;
	
	return true;
}


/**
 * Return connected to client status
 */
bool wifi_is_connected()
{
	return ((wifi_info.flags & WIFI_INFO_FLAG_CONNECTED) != 0);
}


/**
 * Return current WiFi configuration and state
 */
wifi_info_t* wifi_get_info()
{
	return &wifi_info;
}


//
// WiFi Utilities internal functions
//

/**
 * Initialize the WiFi interface resources
 */
static bool init_esp_wifi()
{
	esp_err_t ret;
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	
	ret = esp_wifi_init(&cfg);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not allocate wifi resources (%d)", ret);
		return false;
	}
	
	// We don't need the NVS configuration storage for the WiFi configuration since we
	// are managing persistent storage ourselves
	ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not set RAM storage for configuration (%d)", ret);
		return false;
	}
	
	return true;
}


/**
 * Enable this device as a Soft AP
 */
static bool enable_esp_wifi_ap(bool en)
{
	esp_err_t ret;
	
	if (en) {
		// Enable the AP
		wifi_config_t wifi_config = {
        	.ap = {
            	.ssid_len = strlen(wifi_info.ssid),
            	.max_connection = 1,
            	.authmode = WIFI_AUTH_WPA_WPA2_PSK
        	},
    	};
    	strcpy((char*) wifi_config.ap.ssid, wifi_info.ssid);
    	strcpy((char*) wifi_config.ap.password, wifi_info.pw);
    	if (strlen(wifi_info.pw) == 0) {
        	wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    	}
    	
    	ret = esp_wifi_set_mode(WIFI_MODE_AP);
    	if (ret != ESP_OK) {
    		ESP_LOGE(TAG, "Could not set Soft AP mode (%d)", ret);
    		return false;
    	}
    	
    	ret = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    	if (ret != ESP_OK) {
    		ESP_LOGE(TAG, "Could not set Soft AP configuration (%d)", ret);
    		return false;
    	}
    	
    	ret = esp_wifi_start();
    	if (ret != ESP_OK) {
    		ESP_LOGE(TAG, "Could not start Soft AP (%d)", ret);
    		return false;
    	}
    	
    	return true;
	} else {
		// Disable the AP
		return (esp_wifi_stop() == ESP_OK);
	}
}


/**
 * Handle system events that we care about from the WiFi task
 */
static esp_err_t sys_event_handler(void *ctx, system_event_t* event)
{
	switch(event->event_id) {
		case SYSTEM_EVENT_AP_STACONNECTED:
			wifi_info.flags |= WIFI_INFO_FLAG_CONNECTED;
			ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
			break;
		
		case SYSTEM_EVENT_AP_STADISCONNECTED:
			wifi_info.flags &= ~WIFI_INFO_FLAG_CONNECTED;
			ESP_LOGI(TAG, "station:"MACSTR" leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
			break;
		
		default:
			break;
	}
	
	return ESP_OK;
}
