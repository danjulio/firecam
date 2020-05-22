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
#ifndef CMD_TASK_H
#define CMD_TASK_H

#include <stdint.h>



//
// CMD Task Constants
//

// Commands
#define CMD_GET_STATUS 0
#define CMD_GET_IMAGE  1
#define CMD_SET_TIME   2
#define CMD_GET_WIFI   3
#define CMD_SET_WIFI   4
#define CMD_GET_CONFIG 5
#define CMD_SET_CONFIG 6
#define CMD_RECORD_ON  7
#define CMD_RECORD_OFF 8
#define CMD_POWEROFF   9
#define CMD_UNKNOWN    10
#define CMD_NUM        10

// Command strings
#define CMD_GET_STATUS_S "get_status"
#define CMD_GET_IMAGE_S  "get_image"
#define CMD_SET_TIME_S   "set_time"
#define CMD_GET_WIFI_S   "get_wifi"
#define CMD_SET_WIFI_S   "set_wifi"
#define CMD_GET_CONFIG_S "get_config"
#define CMD_SET_CONFIG_S "set_config"
#define CMD_RECORD_ON_S  "record_on"
#define CMD_RECORD_OFF_S "record_off"
#define CMD_POWEROFF_S   "poweroff"


// Maximum wait period for the system to come up with a response to send back
#define CMD_RESPONSE_MAX_WAIT_MSEC        1500
#define CMD_RESPONSE_WAIT_TASK_SLEEP_MSEC 100
#define CMD_RESPONSE_WAIT_COUNT_INIT      (CMD_RESPONSE_MAX_WAIT_MSEC / CMD_RESPONSE_WAIT_TASK_SLEEP_MSEC)

// Delimiters used to wrap json strings sent over the network
#define CMD_JSON_STRING_START 0x02
#define CMD_JSON_STRING_STOP  0x03

// Maximum send packet size
#define CMD_MAX_TX_PKT_LEN    1024

// App Task notifications
#define CMD_NOTIFY_IMAGE_MASK 0x00000001


//
// CMD Task API
//
void cmd_task();

#endif /* CMD_TASK_H */