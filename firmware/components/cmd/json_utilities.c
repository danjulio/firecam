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
#include "system_config.h"
#include "lepton_utilities.h"
#include "time_utilities.h"
#include "app_task.h"
#include "cmd_task.h"
#include "vospi.h"
#include "crypto/base64.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include <string.h>



//
// Command parser
//
typedef struct {
	const char* cmd_name;
	int cmd_index;
} cmd_name_t;

const cmd_name_t command_list[CMD_NUM] = {
	{CMD_GET_STATUS_S, CMD_GET_STATUS},
	{CMD_GET_IMAGE_S, CMD_GET_IMAGE},
	{CMD_GET_CONFIG_S, CMD_GET_CONFIG},
	{CMD_SET_CONFIG_S, CMD_SET_CONFIG},
	{CMD_SET_TIME_S, CMD_SET_TIME},
	{CMD_GET_WIFI_S, CMD_GET_WIFI},
	{CMD_SET_WIFI_S, CMD_SET_WIFI},
	{CMD_RECORD_ON_S, CMD_RECORD_ON},
	{CMD_RECORD_OFF_S, CMD_RECORD_OFF},
	{CMD_POWEROFF_S, CMD_POWEROFF}
};



//
// JSON Utilities variables
//
static const char* TAG = "json_utilities";

static char* json_image_text;       // Loaded for combined image data
static char* json_response_text;    // Loaded for response data

static unsigned char* base64_jpeg_data;
static unsigned char* base64_lep_data;
static unsigned char* base64_lep_telem_data;



//
// JSON Utilities Forward Declarations for internal functions
//
bool json_add_cam_image_object(cJSON* parent);
void json_free_cam_base64_image();
bool json_add_lep_image_object(cJSON* parent);
void json_free_lep_base64_image();
bool json_add_lep_telem_object(cJSON* parent);
void json_free_lep_base64_telem();
bool json_add_metadata_object(cJSON* parent, int seq_num, bool inc_lep);
int json_generate_response_string(cJSON* root);
bool json_ip_string_to_array(uint8_t* ip_array, char* ip_string);



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
				} else {
					success = json_add_lep_telem_object(root);
					if (!success) {
						// Free images that were already allocated
						if (has_cam) {
							json_free_cam_base64_image();
						}
						json_free_lep_base64_image();
					}
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
			json_free_lep_base64_telem();
		}
	} else {
		ESP_LOGE(TAG, "failed to create json image text");
	}
	
	cJSON_Delete(root);
	
	return json_image_text;
}


/**
 * Return a formatted json string containing the camera's operating parameters in
 * response to the get_config commmand.  Include the delimitors since this string
 * will be sent via the socket interface
 */
char* json_get_config(uint32_t* len)
{
	cJSON* root;
	cJSON* config;
	gui_state_t* gui_stP;
	
	// Get state
	gui_stP = system_get_gui_st();
	
	// Create and add to the config object
	root=cJSON_CreateObject();
	if (root == NULL) return NULL;
	
	cJSON_AddItemToObject(root, "config", config=cJSON_CreateObject());
	
	cJSON_AddNumberToObject(config, "arducam_enable", (const double) gui_stP->rec_arducam_enable);
	cJSON_AddNumberToObject(config, "lepton_enable", (const double) gui_stP->rec_lepton_enable);
	cJSON_AddNumberToObject(config, "gain_mode", (const double) gui_stP->gain_mode);
	cJSON_AddNumberToObject(config, "record_interval", (const double) gui_stP->record_interval);
	
	// Tightly print the object into our buffer with delimitors
	*len = json_generate_response_string(root);
	
	cJSON_Delete(root);
	
	return json_response_text;
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
	wifi_info_t* wifi_infoP;
	const esp_app_desc_t* app_desc;
	tmElements_t te;
	batt_status_t batt;
	
	// Get system information
	app_desc = esp_ota_get_app_description();	
	time_get(&te);
	adc_get_batt(&batt);
	
	// Create and add to the metadata object
	root=cJSON_CreateObject();
	if (root == NULL) return NULL;
	
	cJSON_AddItemToObject(root, "status", status=cJSON_CreateObject());
	
	wifi_infoP = wifi_get_info();
	cJSON_AddStringToObject(status, "Camera", wifi_infoP->ap_ssid);
	
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
	*len = json_generate_response_string(root);
	
	cJSON_Delete(root);
	
	return json_response_text;
}


/**
 * Return a formatted json string containing the wifi setup (minus password) in response
 * to the get_wifi command.  Include the delimitors since this string will be sent via
 * the socket interface.
 */
char* json_get_wifi(uint32_t* len)
{
	char ip_string[16];  // "XXX:XXX:XXX:XXX" + null
	cJSON* root;
	cJSON* wifi;
	wifi_info_t* wifi_infoP;
	
	// Get wifi information
	wifi_infoP = wifi_get_info();
	
	// Create and add to the metadata object
	root=cJSON_CreateObject();
	if (root == NULL) return NULL;
	
	cJSON_AddItemToObject(root, "wifi", wifi=cJSON_CreateObject());
	
	cJSON_AddStringToObject(wifi, "ap_ssid", wifi_infoP->ap_ssid);
	cJSON_AddStringToObject(wifi, "sta_ssid", wifi_infoP->sta_ssid);
	cJSON_AddNumberToObject(wifi, "flags", (const double) wifi_infoP->flags);
	
	sprintf(ip_string, "%d.%d.%d.%d", wifi_infoP->ap_ip_addr[3],
			                          wifi_infoP->ap_ip_addr[2],
			                          wifi_infoP->ap_ip_addr[1],
			                          wifi_infoP->ap_ip_addr[0]);
	cJSON_AddStringToObject(wifi, "ap_ip_addr", ip_string);
	
	sprintf(ip_string, "%d.%d.%d.%d", wifi_infoP->sta_ip_addr[3],
			                          wifi_infoP->sta_ip_addr[2],
			                          wifi_infoP->sta_ip_addr[1],
			                          wifi_infoP->sta_ip_addr[0]);
	cJSON_AddStringToObject(wifi, "sta_ip_addr", ip_string);
	
	sprintf(ip_string, "%d.%d.%d.%d", wifi_infoP->cur_ip_addr[3],
			                          wifi_infoP->cur_ip_addr[2],
			                          wifi_infoP->cur_ip_addr[1],
			                          wifi_infoP->cur_ip_addr[0]);
	cJSON_AddStringToObject(wifi, "cur_ip_addr", ip_string);
	
	// Tightly print the object into our buffer with delimitors
	*len = json_generate_response_string(root);
	
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
	 int i;
	 
	 if (cmd_type != NULL) {
	 	cmd_name = cJSON_GetStringValue(cmd_type);

	 	if (cmd_name != NULL) {
	 		*cmd = CMD_UNKNOWN;
	 		
	 		for (i=0; i<CMD_NUM; i++) {
	 			if (strcmp(cmd_name, command_list[i].cmd_name) == 0) {
	 				*cmd = command_list[i].cmd_index;
	 				break;
	 			}
	 		}
	 		
	 		*cmd_args = cJSON_GetObjectItem(cmd_obj, "args");
	 		
	 		return true;
	 	}
	 }

	 return false;
}


/**
 * Fill in a gui_st struct with arguments from a set_config command, preserving
 * unmodified elements
 */
bool json_parse_set_config(cJSON* cmd_args, gui_state_t* new_st)
{
	int item_count = 0;
	gui_state_t* gui_stP;
	
	// Get existing settings
	gui_stP = system_get_gui_st();
	
	if (cmd_args != NULL) {
		if (cJSON_HasObjectItem(cmd_args, "arducam_enable")) {
			new_st->rec_arducam_enable = cJSON_GetObjectItem(cmd_args, "arducam_enable")->valueint > 0 ? true : false;
			item_count++;
		} else {
			new_st->rec_arducam_enable = gui_stP->rec_arducam_enable;
		}
		
		if (cJSON_HasObjectItem(cmd_args, "lepton_enable")) {
			new_st->rec_lepton_enable = cJSON_GetObjectItem(cmd_args, "lepton_enable")->valueint > 0 ? true : false;
			item_count++;
		} else {
			new_st->rec_lepton_enable = gui_stP->rec_lepton_enable;
		}
		
		if (cJSON_HasObjectItem(cmd_args, "gain_mode")) {
			new_st->gain_mode = cJSON_GetObjectItem(cmd_args, "gain_mode")->valueint;
			if (new_st->gain_mode > SYS_GAIN_AUTO) {
				ESP_LOGW(TAG, "Unsupported set_config gain_mode %d", new_st->gain_mode);
				new_st->gain_mode = SYS_GAIN_AUTO;
			}
			item_count++;
		} else {
			new_st->gain_mode = gui_stP->gain_mode;
		}
		
		if (cJSON_HasObjectItem(cmd_args, "record_interval")) {
			new_st->record_interval = cJSON_GetObjectItem(cmd_args, "record_interval")->valueint;
			new_st->record_interval_index = system_get_rec_interval_index(new_st->record_interval);
			if (new_st->record_interval_index < 0) {
				ESP_LOGW(TAG, "Unsupported set_config record_interval %d", new_st->record_interval);
				// Fix illegal interval
				new_st->record_interval_index = 0;
				new_st->record_interval = record_intervals[new_st->record_interval_index].interval;
			}
			item_count++;
		} else {
			new_st->record_interval = gui_stP->record_interval;
			new_st->record_interval_index = gui_stP->record_interval_index;
		}
		
		// Copy existing palette index over
		new_st->palette_index = gui_stP->palette_index;
		
		return (item_count > 0);
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
 * Fill in a wifi_info_t object with arguments from a set_wifi command, preserving
 * unmodified elements
 */
bool json_parse_set_wifi(cJSON* cmd_args, wifi_info_t* new_wifi_info)
{
	char* s;
	int i;
	int item_count = 0;
	wifi_info_t* wifi_infoP;
	
	// Get existing settings
	wifi_infoP = wifi_get_info();
	
	if (cmd_args != NULL) {
		if (cJSON_HasObjectItem(cmd_args, "ap_ssid")) {
			s = cJSON_GetObjectItem(cmd_args, "ap_ssid")->valuestring;
			if (strlen(s) <= PS_SSID_MAX_LEN) {
				strcpy(new_wifi_info->ap_ssid, s);
				item_count++;
			} else {
				ESP_LOGE(TAG, "set_wifi ap_ssid: %s too long", s);
				return false;
			}
		} else {
			strcpy(new_wifi_info->ap_ssid, wifi_infoP->ap_ssid);
		}
		
		if (cJSON_HasObjectItem(cmd_args, "sta_ssid")) {
			s = cJSON_GetObjectItem(cmd_args, "sta_ssid")->valuestring;
			if (strlen(s) <= PS_SSID_MAX_LEN) {
				strcpy(new_wifi_info->sta_ssid, s);
				item_count++;
			} else {
				ESP_LOGE(TAG, "set_wifi sta_ssid: %s too long", s);
				return false;
			}
		} else {
			strcpy(new_wifi_info->sta_ssid, wifi_infoP->sta_ssid);
		}
		
		if (cJSON_HasObjectItem(cmd_args, "ap_pw")) {
			s = cJSON_GetObjectItem(cmd_args, "ap_pw")->valuestring;
			if (strlen(s) <= PS_PW_MAX_LEN) {
				strcpy(new_wifi_info->ap_pw, s);
				item_count++;
			} else {
				ESP_LOGE(TAG, "set_wifi ap_pw: %s too long", s);
				return false;
			}
		} else {
			strcpy(new_wifi_info->ap_pw, wifi_infoP->ap_pw);
		}
		
		if (cJSON_HasObjectItem(cmd_args, "sta_pw")) {
			s = cJSON_GetObjectItem(cmd_args, "sta_pw")->valuestring;
			if (strlen(s) <= PS_PW_MAX_LEN) {
				strcpy(new_wifi_info->sta_pw, s);
				item_count++;
			} else {
				ESP_LOGE(TAG, "set_wifi sta_pw: %s too long", s);
				return false;
			}
		} else {
			strcpy(new_wifi_info->sta_pw, wifi_infoP->sta_pw);
		}
		
		if (cJSON_HasObjectItem(cmd_args, "flags")) {
			new_wifi_info->flags = (uint8_t) cJSON_GetObjectItem(cmd_args, "flags")->valueint;
			item_count++;
		} else {
			new_wifi_info->flags = wifi_infoP->flags;
		}
		
		if (cJSON_HasObjectItem(cmd_args, "ap_ip_addr")) {
			s = cJSON_GetObjectItem(cmd_args, "ap_ip_addr")->valuestring;
			if (json_ip_string_to_array(new_wifi_info->ap_ip_addr, s)) {
				item_count++;
			} else {
				ESP_LOGE(TAG, "Illegal set_wifi ap_ip_addr: %s", s);
				return false;
			}
		} else {
			for (i=0; i<4; i++) new_wifi_info->ap_ip_addr[i] = wifi_infoP->ap_ip_addr[i];
		}
		
		if (cJSON_HasObjectItem(cmd_args, "sta_ip_addr")) {
			s = cJSON_GetObjectItem(cmd_args, "sta_ip_addr")->valuestring;
			if (json_ip_string_to_array(new_wifi_info->sta_ip_addr, s)) {
				item_count++;
			} else {
				ESP_LOGE(TAG, "Illegal set_wifi sta_ip_addr: %s", s);
				return false;
			}
		} else {
			for (i=0; i<4; i++) new_wifi_info->sta_ip_addr[i] = wifi_infoP->sta_ip_addr[i];
		}
		
		// Just copy existing address over
		for (i=0; i<4; i++) new_wifi_info->cur_ip_addr[i] = wifi_infoP->cur_ip_addr[i];
		
		return (item_count > 0);
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
 * json_free_cam_base64_image() after the json object is converted to a string.
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
 * Add a child object containing base64 encoded lepton image from the shared buffer
 *
 * Note: The encoded image string is held in an array that must be freed with
 * json_free_lep_base64_image() after the json object is converted to a string.
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
 * Add a child object containing base64 encoded lepton telemetry array from the shared buffer
 *
 * Note: The encoded telemetry string is held in an array that must be freed with
 * json_free_lep_base64_telem() after the json object is converted to a string.
 */
bool json_add_lep_telem_object(cJSON* parent)
{
	size_t base64_obj_len;
	
	// Base-64 encode the telemetry array
	base64_lep_telem_data = base64_encode((const unsigned char *) sys_lep_buffer.lep_telemP,
	                                 LEP_TEL_WORDS*2, &base64_obj_len);
	
	// Add the encoded data as a reference since we're managing the buffer
	if (base64_obj_len != 0) {
		cJSON_AddItemToObject(parent, "telemetry", cJSON_CreateStringReference((char*) base64_lep_telem_data));
	} else {
		ESP_LOGE(TAG, "failed to create lepton telemetry base64 text");
		return false;
	}
	
	return true;
}


/**
 * Free the base64-encoded Lepton telemetry string.  Call this routine after printing the 
 * telemetry json object.
 */
void json_free_lep_base64_telem()
{
	free(base64_lep_telem_data);
}


/**
 * Add a child object containing image metadata to the parent.  Data related to the
 * Lepton is not included if inc_lep is not set.
 */
bool json_add_metadata_object(cJSON* parent, int seq_num, bool inc_lep)
{
	char buf[80];
	float t;
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
	cJSON_AddStringToObject(meta, "Camera", wifi_info->ap_ssid);
	
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
		t = lepton_kelvin_to_C(sys_lep_buffer.lep_telemP[LEP_TEL_FPA_T_K100], 0.01);
		cJSON_AddNumberToObject(meta, "FPA Temp", (const double) t);
		
		t = lepton_kelvin_to_C(sys_lep_buffer.lep_telemP[LEP_TEL_HSE_T_K100], 0.01);
		cJSON_AddNumberToObject(meta, "AUX Temp", (const double) t);
		
		cJSON_AddNumberToObject(meta, "Lens Temp", (const double) adc_get_temp());
		
		if (sys_lep_buffer.lep_telemP[LEP_TEL_GAIN_MODE] == 2) {
			// Lepton is in Auto Gain mode, so get the effective value
			switch (sys_lep_buffer.lep_telemP[LEP_TEL_EFF_GAIN_MODE]) {
				case 0:
					strcpy(buf, "HIGH");
					break;
				case 1:
					strcpy(buf, "LOW");
					break;
				default:
					strcpy(buf, "UNKNOWN");
			}
		} else {
			// Lepton is in one of the manual Gain modes so just use it
			switch (sys_lep_buffer.lep_telemP[LEP_TEL_GAIN_MODE]) {
				case 0:
					strcpy(buf, "HIGH");
					break;
				case 1:
					strcpy(buf, "LOW");
					break;
				default:
					strcpy(buf, "UNKNOWN");
			}
		}
		cJSON_AddStringToObject(meta, "Lepton Gain Mode", buf);
		
		if (sys_lep_buffer.lep_telemP[LEP_TEL_TLIN_RES] == 0) {
			strcpy(buf, "0.1");
		} else {
			strcpy(buf, "0.01");
		}
		cJSON_AddStringToObject(meta, "Lepton Resolution", buf);
	}
	
	return true;
}


/**
 * Tightly print a response into a string with delimitors for transmission over the network.
 * Returns length of the string.
 */
int json_generate_response_string(cJSON* root)
{
	int len;
	
	json_response_text[0] = CMD_JSON_STRING_START;
	if (cJSON_PrintPreallocated(root, &json_response_text[1], JSON_MAX_RSP_TEXT_LEN, false) == 0) {
		len = 0;
	} else {
		len = strlen(json_response_text);
		json_response_text[len] = CMD_JSON_STRING_STOP;
		json_response_text[len+1] = 0;
		len += 1;
	}
	
	return len;
}


/**
 * Convert a string in the form of "XXX.XXX.XXX.XXX" into a 4-byte array for wifi_info_t
 */
bool json_ip_string_to_array(uint8_t* ip_array, char* ip_string)
{
	char c;;
	int i = 3;
	
	ip_array[i] = 0;
	while ((c = *ip_string++) != 0) {
		if (c == '.') {
			if (i == 0) {
				// Too many '.' characters
				return false;
			} else {
				// Setup for next byte
				ip_array[--i] = 0;
			}
		} else if ((c >= '0') && (c <= '9')) {
			// Add next numeric digit
			ip_array[i] = (ip_array[i] * 10) + (c - '0');
		} else {
			// Illegal character in string
			return false;
		}
	}
	
	return true;
}
