/*
 * Cmd Task
 *
 * Implement the command processing module including management of the WiFi interface.
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
#include "app_task.h"
#include "cmd_task.h"
#include "json_utilities.h"
#include "lepton_utilities.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "time_utilities.h"
#include "wifi_utilities.h"
#include "system_config.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

//
// CMD Task variables
//
static const char* TAG = "cmd_task";

// Command processor
static bool response_expected;
static bool response_available;
static bool response_was_image;
static char* response_buffer;
static uint32_t response_length;

// Main receive buffer for incoming packets
static char rx_circular_buffer[CMD_MAX_TCP_RX_BUFFER_LEN];
static int rx_circular_push_index;
static int rx_circular_pop_index;

// json command string buffer
static char json_cmd_string[JSON_MAX_CMD_TEXT_LEN];


//
// CMD Task Forward Declarations for internal functions
//
static void init_command_processor();
static void process_rx_data(char* data, int len);
static void process_rx_packet();
static void cmd_task_handle_notifications();
static int in_buffer(char c);



//
// CMD Task API
//
void cmd_task()
{
	char rx_buffer[128];
    char addr_str[16];
    int byte_offset;
    int count;
    int err;
    int flag;
    int len;
    int listen_sock;
    int sock;
    struct sockaddr_in destAddr;
    struct sockaddr_in sourceAddr;
    uint32_t addrLen;
    
	ESP_LOGI(TAG, "Start task");
	
	// Loop to setup socket, wait for connection, handle connection.  Terminates
	// when client disconnects
	
	// Wait until WiFi is connected
	if (!wifi_is_connected()) {
		vTaskDelay(pdMS_TO_TICKS(500));
	}

	// Config IPV4
    destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(CMD_PORT);
    inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);
        
    // socket - bind - listen - accept
    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        goto error;
    }
    ESP_LOGI(TAG, "Socket created");

	flag = 1;
  	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    err = bind(listen_sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
         goto error;
    }
    ESP_LOGI(TAG, "Socket bound");
    
	while (1) {
		init_command_processor();
			
        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(TAG, "Error occured during listen: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket listening");
		
        addrLen = sizeof(sourceAddr);
        sock = accept(listen_sock, (struct sockaddr *)&sourceAddr, &addrLen);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket accepted");
		
        // Handle communication with client
        while (1) {
        	len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occured during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
            // Connection closed
            else if (len == 0) {
                ESP_LOGI(TAG, "Connection closed");
                break;
            }
            // Data received
            else {
            	// Initiates handling of commands if one is found
            	process_rx_data(rx_buffer, len);
            	
            	// Wait if we're expecting to send back a response in case another
            	// task has to come up with it (e.g. an image).  Commands that we
            	// handle always have an immediate response ready
            	while (response_expected) {
            		if (response_available) {
            			// Write our response to the socket
            			byte_offset = 0;
						while (byte_offset < response_length) {
							len = response_length - byte_offset;
							if (len > CMD_MAX_TX_PKT_LEN) len = CMD_MAX_TX_PKT_LEN;
							err = send(sock, &response_buffer[byte_offset], len, 0);
							                 
							if (err < 0) {
								ESP_LOGE(TAG, "Error in socket send: errno %d", errno);
								break;
							}
							byte_offset += err;
						}
						
						if (response_was_image) {
							// Notify app_task we're done with the shared buffer
							xTaskNotify(task_handle_app, APP_NOTIFY_CMD_DONE_MASK, eSetBits);
						}
						response_expected = false;
            		} else {
            			// Look for notifications for a while indicating a response
            			count = CMD_RESPONSE_WAIT_COUNT_INIT;
            			while (count-- > 0) {
            				vTaskDelay(pdMS_TO_TICKS(CMD_RESPONSE_WAIT_TASK_SLEEP_MSEC));
            				
            				cmd_task_handle_notifications();
            				if (response_available) {
            					count = 0;
            				}
            			}
            			
            			// Bail if we didn't see a response in time
            			if (!response_available) {
            				ESP_LOGW(TAG, "Didn't get response in time - dropping command");
            				response_expected = false;
            				if (response_was_image) {
								xTaskNotify(task_handle_app, APP_NOTIFY_CMD_DONE_MASK, eSetBits);
							}
            			}
            		}
            	}
            }
        }
        
        // Close this session
        if (sock != -1) {
            ESP_LOGI(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
	}

error:
	ESP_LOGI(TAG, "Something went seriously wrong with our networking handling - bailing");
	vTaskDelete(NULL);
}



//
// CMD Task internal functions
//


/**
 * Initialize variables associated with receiving and processing commands
 */
static void init_command_processor()
{
	response_expected = false;
	response_available = false;
	response_was_image = false;
	response_length = 0;
	
	rx_circular_push_index = 0;
	rx_circular_pop_index = 0;
}


/**
 * Push received data into our circular buffer and see if we can find a complete json
 * string to process.
 */
static void process_rx_data(char* data, int len)
{
	int begin, end, i;
	
	// Push the received data into the circular buffer
	while (len-- > 0) {
		rx_circular_buffer[rx_circular_push_index] = *data++;
		if (++rx_circular_push_index >= CMD_MAX_TCP_RX_BUFFER_LEN) rx_circular_push_index = 0;
	}
	
	// See if we can find an entire json string
	end = in_buffer(CMD_JSON_STRING_STOP);
	if (end >= 0) {
		// Found end of packet, look for beginning
		begin = in_buffer(CMD_JSON_STRING_START);
		if (begin >= 0) {
			// Found packet - copy it, without delimiters to json_cmd_string
			//
			// Skip past start
			while (rx_circular_pop_index != begin) {
				if (++rx_circular_pop_index >= CMD_MAX_TCP_RX_BUFFER_LEN) rx_circular_pop_index = 0;
			}
			
			// Copy up to end
			i = 0;
			while ((rx_circular_pop_index != end) && (i < CMD_MAX_TCP_RX_BUFFER_LEN)) {
				if (i < JSON_MAX_CMD_TEXT_LEN) {
					json_cmd_string[i] = rx_circular_buffer[rx_circular_pop_index];
				}
				i++;
				if (++rx_circular_pop_index >= CMD_MAX_TCP_RX_BUFFER_LEN) rx_circular_pop_index = 0;
			}
			json_cmd_string[i] = 0;               // Make sure this is a null-terminated string
			
			// Skip past end
			if (++rx_circular_pop_index >= CMD_MAX_TCP_RX_BUFFER_LEN) rx_circular_pop_index = 0;
			
			if (i < JSON_MAX_CMD_TEXT_LEN+1) {
				// Process json command string
				process_rx_packet();
			}
		} else {
			// Unexpected end without start - skip it
			while (rx_circular_pop_index != end) {
				if (++rx_circular_pop_index >= CMD_MAX_TCP_RX_BUFFER_LEN) rx_circular_pop_index = 0;
			}
		}
	}
}


static void process_rx_packet()
{
	char ap_ssid[PS_SSID_MAX_LEN+1];
	char sta_ssid[PS_SSID_MAX_LEN+1];
	char ap_pw[PS_PW_MAX_LEN+1];
	char sta_pw[PS_PW_MAX_LEN+1];
	cJSON* json_obj;
	cJSON* cmd_args;
	gui_state_t new_gui_st;
	int cmd;
	tmElements_t te;
	wifi_info_t new_wifi_info;
	
	response_expected = false; // Will be set true if necessary
	response_available = false;
		
	// Create a json object to parse
	json_obj = json_get_cmd_object(json_cmd_string);
	if (json_obj != NULL) {
		if (json_parse_cmd(json_obj, &cmd, &cmd_args)) {
			switch (cmd) {
				case CMD_GET_STATUS:
					response_buffer = json_get_status(&response_length);
					ESP_LOGI(TAG, "cmd " CMD_GET_STATUS_S);
					if (response_length != 0) {
						response_expected = true;
						response_available = true;
						response_was_image = false;
					}
					break;
					
				case CMD_GET_IMAGE:
					ESP_LOGI(TAG, "cmd " CMD_GET_IMAGE_S);
					response_buffer = sys_cmd_response_buffer.bufferP;
					response_expected = true;
					response_available = false;
					xTaskNotify(task_handle_app, APP_NOTIFY_CMD_REQ_MASK, eSetBits);
					break;
					
				case CMD_SET_TIME:					
					ESP_LOGI(TAG, "cmd " CMD_SET_TIME_S);
					if (json_parse_set_time(cmd_args, &te)) {
						time_set(te);
					}
					break;
				
				case CMD_GET_WIFI:
					response_buffer = json_get_wifi(&response_length);
					ESP_LOGI(TAG, "cmd " CMD_GET_WIFI_S);
					if (response_length != 0) {
						response_expected = true;
						response_available = true;
						response_was_image = false;
					}
					break;
					
				case CMD_SET_WIFI:
					new_wifi_info.ap_ssid = ap_ssid;
					new_wifi_info.sta_ssid = sta_ssid;
					new_wifi_info.ap_pw = ap_pw;
					new_wifi_info.sta_pw = sta_pw;
					ESP_LOGI(TAG, "cmd " CMD_SET_WIFI_S);
					if (json_parse_set_wifi(cmd_args, &new_wifi_info)) {
						ps_set_wifi_info(&new_wifi_info);
						xTaskNotify(task_handle_app, APP_NOTIFY_NEW_WIFI_MASK, eSetBits);
					}
					break;
				
				case CMD_GET_CONFIG:
					response_buffer = json_get_config(&response_length);
					ESP_LOGI(TAG, "cmd " CMD_GET_CONFIG_S);
					if (response_length != 0) {
						response_expected = true;
						response_available = true;
						response_was_image = false;
					}
					break;
					
				case CMD_SET_CONFIG:
					ESP_LOGI(TAG, "cmd " CMD_SET_CONFIG_S);
					if (json_parse_set_config(cmd_args, &new_gui_st)) {
						// Look for changed items that require updating other modules
						if (new_gui_st.gain_mode != gui_st.gain_mode) {
							lepton_gain_mode(new_gui_st.gain_mode);
						}
						gui_st = new_gui_st;
						ps_set_gui_state(&gui_st);
						xTaskNotify(task_handle_app, APP_NOTIFY_RECORD_PARM_UPD_MASK, eSetBits);
					}
					break;
				
				case CMD_RECORD_ON:
					ESP_LOGI(TAG, "cmd " CMD_RECORD_ON_S);
					xTaskNotify(task_handle_app, APP_NOTIFY_START_RECORD_MASK, eSetBits);
					break;
				
				case CMD_RECORD_OFF:
					ESP_LOGI(TAG, "cmd " CMD_RECORD_OFF_S);
					xTaskNotify(task_handle_app, APP_NOTIFY_STOP_RECORD_MASK, eSetBits);
					break;
					
				case CMD_POWEROFF:
					ESP_LOGI(TAG, "cmd " CMD_POWEROFF_S);
					xTaskNotify(task_handle_app, APP_NOTIFY_SHUTDOWN_MASK, eSetBits);
					break;
				
				default:
					ESP_LOGE(TAG, "Unknown command in json string: %s", json_cmd_string);
			}
		} else {
			ESP_LOGE(TAG, "Unknown type of json string: %s", json_cmd_string);
		}
		
		json_free_cmd(json_obj);
	} else {
		ESP_LOGE(TAG, "Couldn't convert json string: %s", json_cmd_string);
	}
}


/**
 * Process notifications from other tasks
 */
static void cmd_task_handle_notifications()
{
	uint32_t notification_value;
	
	notification_value = 0;
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, CMD_NOTIFY_IMAGE_MASK)) {
			response_available = true;
			response_length = sys_cmd_response_buffer.length;
			response_was_image = true;
		}
	}
}


/**
 * Look for c in the rx_circular_buffer and return its location if found, -1 otherwise
 */
static int in_buffer(char c)
{
	int i;
	
	i = rx_circular_pop_index;
	while (i != rx_circular_push_index) {
		if (c == rx_circular_buffer[i]) {
			return i;
		} else {
			if (i++ >= CMD_MAX_TCP_RX_BUFFER_LEN) i = 0;
		}
	}
	
	return -1;
}
