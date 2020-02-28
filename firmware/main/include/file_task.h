/*
 * File Task
 *
 * Handle the SD Card and manage writing files for app_task.  Allows file writing time
 * to vary (it increases as the number of files or directories in a directory has to be
 * traversed before creating a new item).
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
#ifndef FILE_TASK_H
#define FILE_TASK_H

#include <stdint.h>


//
// File Task Constants
//

// File Task notifications
#define FILE_NOTIFY_START_RECORDING_MASK 0x00000001
#define FILE_NOTIFY_STOP_RECORDING_MASK  0x00000002
#define FILE_NOTIFY_NEW_IMAGE_MASK       0x00000004

// Maximum file write size - maximum bytes to write through the system call so that
// we don't put too large a pressure on the stack or heap
#define MAX_FILE_WRITE_LEN               4096

// Period between checks for card present state.
#define FILE_CARD_CHECK_PERIOD_MSEC      2000



//
// File Task API
//
void file_task();


#endif /* FILE_TASK_H */