/*
 * JSON related utilities
 *
 * Contains functions to generate json text objects and parse text objects into the
 * json objects used by firecam.  Uses the cjson library.  Image data is formatted
 * using Base64 encoding.
 *
 * This module uses two pre-allocated buffers for the json text objects.  One for image
 * data (that can be stored as a file or sent to the host) and one for smaller responses
 * to the host.
 *
 * Be sure to read the requirements about freeing allocated buffers or objects in
 * the function description.  Or BOOM.
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
#include "adc_utilities.h"
#include "json_utilities.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "system_config.h"
#include "time_utilities.h"
#include "wifi_utilities.h"
#include "app_task.h"
#include "cmd_task.h"
#include "vospi.h"
#include "crypto/base64.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include <string.h>



//
// JSON Utilities variables
//
static const char* TAG = "json_utilities";

static char* json_image_text;       // Loaded for combined image data
static char* json_response_text;    // Loaded for response data

static unsigned char* base64_jpeg_data;
static unsigned char* base64_lep_data;



//
// JSON Utilities Forward Declarations for internal functions
//
bool json_add_cam_image_object(cJSON* parent);
void json_free_cam_base64_image();
bool json_add_lep_image_object(cJSON* parent);
void json_free_lep_base64_image();
bool json_add_metadata_object(cJSON* parent, int seq_num, bool inc_lep);



//
// JSON Utilities API
//

/**
 * Pre-allocate buffers
 */
bool json_init()
{
	// Get memory for the json text output strings
	json_image_text = heap_caps_malloc(JSON_MAX_IMAGE_TEXT_LEN, MALLOC_CAP_SPIRAM);
	if (json_image_text == NULL) {
		ESP_LOGE(TAG, "Could not allocate json_image_text buffer");
		return false;
	}
	
	json_response_text = heap_caps_malloc(JSON_MAX_RSP_TEXT_LEN, MALLOC_CAP_SPIRAM);
	if (json_response_text == NULL) {
		ESP_LOGE(TAG, "Could not allocate json_response_text buffer");
		return false;
	}
	
	return true;
}


/**
 * Create a json command object from a string, returns NULL if it fails.  The object
 * will need to be freed using json_free_cmd when it is no longer necessary.
 */
cJSON* json_get_cmd_object(char* json_string)
{
	return cJSON_Parse(json_string);
}


/**
 * Return a formatted json string in our pre-allocated json text image buffer containing
 * up to three json objects.  *len is non-zero for a successful operation.
 *   - Base64 encoded jpeg image from the ArduCAM if has_cam is true
 *   - Base64 encoded raw image from the Lepton if has_lep is true
 *   - Image meta-data
 *
 * This function handles its own memory management.
 */
char* json_get_image_file_string(int seq_num, bool has_cam, bool has_lep, uint32_t* len)
{
	bool success;
	cJSON* root;
	
	*len = 0;
	root = cJSON_CreateObject();
	if (root == NULL) return NULL;
	
	// Construct the json object
	success = json_add_metadata_object(root, seq_num, has_lep);
	if (success) {
		if (has_cam) {
			success = json_add_cam_image_object(root);
		}
		if (success) {
			if (has_lep) {
				success = json_add_lep_image_object(root);
				if (!success && has_cam) {
					// We have to free the cam_image that was already allocated
					json_free_cam_base64_image();
				}
			}
		}
	}
	
	// Pretty-print the object to our buffer
	if (success) {
		if (cJSON_PrintPreallocated(root, json_image_text, JSON_MAX_IMAGE_TEXT_LEN, true) == 0) {
			*len = 0;
		} else {
			*len = strlen(json_image_text);
		}
		
		// Free the base-64 converted image strings
		if (has_cam) {
			json_free_cam_base64_image();
		}
		if (has_lep) {
			json_free_lep_base64_image();
		}
	} else {
		ESP_LOGE(TAG, "failed to create json image text");
	}
	
	cJSON_Delete(root);
	
	return json_image_text;
}


/**
 * Return a formatted json string containing the system status in response to the
 * get_status command.  Include the delimitors since this string will be sent via
 * the socket interface.
 */
char* json_get_status(uint32_t* len)
{
	char buf[80];
	cJSON* root;
	cJSON* status;
	wifi_info_t* wifi_info;
	const esp_app_desc_t* app_desc;
	tmElements_t te;
	batt_status_t batt;
	
	// Get system information
	app_desc = esp_ota_get_app_description();	
	time_get(&te);
	adc_get_batt(&batt);
	
	// Create and add to the metadata object
	*len = 0;
	root=cJSON_CreateObject();
	if (root == NULL) return NULL;
	
	cJSON_AddItemToObject(root, "status", status=cJSON_CreateObject());
	
	wifi_info = wifi_get_info();
	cJSON_AddStringToObject(status, "Camera", wifi_info->ssid);
	
	cJSON_AddStringToObject(status, "Version", app_desc->version);
	
	cJSON_AddNumberToObject(status, "Recording", (const double) app_task_get_recording());
	
	sprintf(buf, "%d:%02d:%02d", te.Hour, te.Minute, te.Second);
	cJSON_AddStringToObject(status, "Time", buf);
	sprintf(buf, "%d/%d/%02d", te.Month, te.Day, te.Year-30); // Year starts at 1970
	cJSON_AddStringToObject(status, "Date", buf);
	
	cJSON_AddNumberToObject(status, "Battery", (const double) batt.batt_voltage);
	
	switch (batt.charge_state) {
		case CHARGE_OFF:
			strcpy(buf, "OFF");
			break;
		case CHARGE_ON:
			strcpy(buf, "ON");
			break;
		case CHARGE_FAULT:
			strcpy(buf, "FAULT");
			break;
	}
	cJSON_AddStringToObject(status, "Charge", buf);
	
	// Tightly print the object into our buffer with delimitors
	json_response_text[0] = CMD_JSON_STRING_START;
	if (cJSON_PrintPreallocated(root, &json_response_text[1], JSON_MAX_RSP_TEXT_LEN, false) == 0) {
		*len = 0;
	} else {
		*len = strlen(json_response_text);
		json_response_text[*len] = CMD_JSON_STRING_STOP;
		json_response_text[*len+1] = 0;
		*len += 1;
	}
	
	cJSON_Delete(root);
	
	return json_response_text;
}


/**
 * Parse a top level command object, returning the command number and a pointer to 
 * a json object containing "args".  The pointer is set to NULL if there are no args.
 */
bool json_parse_cmd(cJSON* cmd_obj, int* cmd, cJSON** cmd_args)
{
	 cJSON *cmd_type = cJSON_GetObjectItem(cmd_obj, "cmd");
	 char* cmd_name;
	 
	 if (cmd_type != NULL) {
	 	cmd_name = cJSON_GetStringValue(cmd_type);

	 	if (cmd_name != NULL) {
	 		if (strcmp(cmd_name, "get_status") == 0) {
	 			*cmd = CMD_GET_STATUS;
	 		} else if (strcmp(cmd_name, "get_image") == 0) {
	 			*cmd = CMD_GET_IMAGE;
	 		} else if (strcmp(cmd_name, "set_time") == 0) {
	 			*cmd = CMD_SET_TIME;
	 		} else if (strcmp(cmd_name, "set_wifi") == 0) {
	 			*cmd = CMD_SET_WIFI;
	 		} else if (strcmp(cmd_name, "record_on") == 0) {
	 			*cmd = CMD_RECORD_ON;
	 		} else if (strcmp(cmd_name, "record_off") == 0) {
	 			*cmd = CMD_RECORD_OFF;
	 		} else if (strcmp(cmd_name, "poweroff") == 0) {
	 			*cmd = CMD_POWEROFF;
	 		} else {
	 			*cmd = CMD_UNKNOWN;
	 		}
	 		
	 		*cmd_args = cJSON_GetObjectItem(cmd_obj, "args");
	 		
	 		return true;
	 	}
	 }

	 return false;
}


/**
 * Fill in a tmElements object with arguments from a set_time command
 */
bool json_parse_set_time(cJSON* cmd_args, tmElements_t* te)
{
	int item_count = 0;
	
	if (cmd_args != NULL) {
		if (cJSON_HasObjectItem(cmd_args, "sec")) {
			te->Second = cJSON_GetObjectItem(cmd_args, "sec")->valueint; // 0 - 59
			item_count++;
		}
		
		if (cJSON_HasObjectItem(cmd_args, "min")) {
			te->Minute = cJSON_GetObjectItem(cmd_args, "min")->valueint; // 0 - 59
			item_count++;
		}
		
		if (cJSON_HasObjectItem(cmd_args, "hour")) {
			te->Hour   = cJSON_GetObjectItem(cmd_args, "hour")->valueint; // 0 - 23
			item_count++;
		}
		
		if (cJSON_HasObjectItem(cmd_args, "dow")) {
			te->Wday   = cJSON_GetObjectItem(cmd_args, "dow")->valueint; // 1 - 7
			item_count++;
		}
		
		if (cJSON_HasObjectItem(cmd_args, "day")) {
			te->Day    = cJSON_GetObjectItem(cmd_args, "day")->valueint; // 1 - 31
			item_count++;
		}
		
		if (cJSON_HasObjectItem(cmd_args, "mon")) {
			te->Month  = cJSON_GetObjectItem(cmd_args, "mon")->valueint; // 1 - 12
			item_count++;
		}
		
		if (cJSON_HasObjectItem(cmd_args, "year")) {
			te->Year   = cJSON_GetObjectItem(cmd_args, "year")->valueint; // offset from 1970
			item_count++;
		}
		
		return (item_count == 7);
	}
	
	return false;
}


/**
 * Fill in a wifi_info_t object with arguments from a set_wifi command
 */
bool json_parse_set_wifi(cJSON* cmd_args, wifi_info_t* wifi_info)
{
	char* s;
	int count = 0;
	
	if (cmd_args != NULL) {
		if (cJSON_HasObjectItem(cmd_args, "ssid")) {
			s = cJSON_GetObjectItem(cmd_args, "ssid")->valuestring;
			if (strlen(s) <= PS_SSID_MAX_LEN) {
				strcpy(wifi_info->ssid, s);
				count++;
			}
		}
		
		if (cJSON_HasObjectItem(cmd_args, "pw")) {
			s = cJSON_GetObjectItem(cmd_args, "pw")->valuestring;
			if (strlen(s) <= PS_PW_MAX_LEN) {
				strcpy(wifi_info->pw, s);
				count++;
			}
		}
		
		if (cJSON_HasObjectItem(cmd_args, "flags")) {
			wifi_info->flags = (uint8_t) cJSON_GetObjectItem(cmd_args, "flags")->valueint;
			count++;
		}
		
		return (count == 3);
	}
	
	return false;
}


/**
 * Free the json command object
 */
void json_free_cmd(cJSON* cmd)
{
	if (cmd != NULL) cJSON_Delete(cmd);
}



//
// JSON Utilities internal functions
//

/**
 * Add a child object containing base64 encoded jpeg image from the shared buffer
 *
 * Note: The encoded image string is held in an array that must be freed with
 * json_free_cam_image() after the json object is converted to a string.
 */
bool json_add_cam_image_object(cJSON* parent)
{
	size_t base64_obj_len;
	
	// Base-64 encode the camera data
	base64_jpeg_data = base64_encode((const unsigned char *) sys_cam_buffer.cam_bufferP,
	                                 sys_cam_buffer.cam_buffer_len, &base64_obj_len);
	
	// Add the encoded data as a reference since we're managing the buffer
	if (base64_obj_len != 0) {
		cJSON_AddItemToObject(parent, "jpeg", cJSON_CreateStringReference((char*) base64_jpeg_data));
	} else {
		ESP_LOGE(TAG, "failed to create jpeg image base64 text");
		return false;
	}
	
	return true;
}


/**
 * Free the base64-encoded ArduCAM image.  Call this routine after printing the 
 * image json object.
 */
void json_free_cam_base64_image()
{
	free(base64_jpeg_data);
}


/**
 * Add a child object containing base64 encoded jpeg image from the shared buffer
 *
 * Note: The encoded image string is held in an array that must be freed with
 * json_free_cam_image() after the json object is converted to a string.
 */
bool json_add_lep_image_object(cJSON* parent)
{
	size_t base64_obj_len;
	
	// Base-64 encode the camera data
	base64_lep_data = base64_encode((const unsigned char *) sys_lep_buffer.lep_bufferP,
	                                 LEP_NUM_PIXELS*2, &base64_obj_len);
	
	// Add the encoded data as a reference since we're managing the buffer
	if (base64_obj_len != 0) {
		cJSON_AddItemToObject(parent, "radiometric", cJSON_CreateStringReference((char*) base64_lep_data));
	} else {
		ESP_LOGE(TAG, "failed to create lepton image base64 text");
		return false;
	}
	
	return true;
}


/**
 * Free the base64-encoded Lepton image.  Call this routine after printing the 
 * image json object.
 */
void json_free_lep_base64_image()
{
	free(base64_lep_data);
}


/**
 * Add a child object containing image metadata to the parent.  Data related to the
 * Lepton is not included if inc_lep is not set.
 */
bool json_add_metadata_object(cJSON* parent, int seq_num, bool inc_lep)
{
	char buf[80];
	cJSON* meta;
	wifi_info_t* wifi_info;
	const esp_app_desc_t* app_desc;
	tmElements_t te;
	batt_status_t batt;
	
	// Get system information
	app_desc = esp_ota_get_app_description();
	time_get(&te);
	adc_get_batt(&batt);
	
	// Create and add to the metadata object
	cJSON_AddItemToObject(parent, "metadata", meta=cJSON_CreateObject());
	
	wifi_info = wifi_get_info();
	cJSON_AddStringToObject(meta, "Camera", wifi_info->ssid);
	
	cJSON_AddStringToObject(meta, "Version", app_desc->version);
	
	cJSON_AddNumberToObject(meta, "Sequence Number", (const double) seq_num);
	
	sprintf(buf, "%d:%02d:%02d", te.Hour, te.Minute, te.Second);
	cJSON_AddStringToObject(meta, "Time", buf);
	sprintf(buf, "%d/%d/%02d", te.Month, te.Day, te.Year-30);  // Year starts at 1970
	cJSON_AddStringToObject(meta, "Date", buf);
	
	cJSON_AddNumberToObject(meta, "Battery", (const double) batt.batt_voltage);
	switch (batt.charge_state) {
		case CHARGE_OFF:
			strcpy(buf, "OFF");
			break;
		case CHARGE_ON:
			strcpy(buf, "ON");
			break;
		case CHARGE_FAULT:
			strcpy(buf, "FAULT");
			break;
	}
	cJSON_AddStringToObject(meta, "Charge", buf);
	
	if (inc_lep) {
		cJSON_AddNumberToObject(meta, "FPA Temp", (const double) sys_lep_buffer.lep_fpa_temp);
		cJSON_AddNumberToObject(meta, "AUX Temp", (const double) sys_lep_buffer.lep_aux_temp);
		cJSON_AddNumberToObject(meta, "Lens Temp", (const double) adc_get_temp());
		switch (sys_lep_buffer.lep_gain_mode) {
			case 0:
				strcpy(buf, "HIGH");
				break;
			case 1:
				strcpy(buf, "LOW");
				break;
			default:
				strcpy(buf, "AUTO");
		}
		cJSON_AddStringToObject(meta, "Lepton Gain Mode", buf);
	}
	
	return true;
}
