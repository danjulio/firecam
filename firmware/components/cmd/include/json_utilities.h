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
#ifndef JSON_UTILITIES_H
#define JSON_UTILITIES_H

#include "ds3232.h"
#include "sys_utilities.h"
#include "wifi_utilities.h"
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"



//
// JSON Utilities API
//
bool json_init();
cJSON* json_get_cmd_object(char* json_string);
char* json_get_image_file_string(int seq_num, bool has_cam, bool has_lep, uint32_t* len);
char* json_get_config(uint32_t* len);
char* json_get_status(uint32_t* len);
char* json_get_wifi(uint32_t* len);
bool json_parse_cmd(cJSON* cmd_obj, int* cmd, cJSON** cmd_args);
bool json_parse_set_config(cJSON* cmd_args, gui_state_t* new_st);
bool json_parse_set_time(cJSON* cmd_args, tmElements_t* te);
bool json_parse_set_wifi(cJSON* cmd_args, wifi_info_t* new_wifi_info);
void json_free_cmd(cJSON* cmd);

#endif /* JSON_UTILITIES_H */