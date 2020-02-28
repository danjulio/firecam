/*
 * File related utilities
 *
 * Contains functions to initialize the sdmmc interface, detect and format SD Cards,
 * create directories and write image files.
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
#ifndef FILE_UTILITIES_H
#define FILE_UTILITIES_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>



//
// File Utilities Constants
//
#define DEF_SD_CARD_LABEL "FIRECAM"

// Number of image files per subdirectory
#define FILES_PER_SUBDIRECTORY 100


//
// File Utilities API
//
bool file_init_sdmmc_driver();
bool file_check_card_still_present();
bool file_get_card_present();
bool file_check_card_inserted();
bool file_init_card();
bool file_reinit_card();
bool file_mount_sdcard();
char* file_get_session_directory_name();
bool file_create_directory(char* dir_name);
char* file_get_session_file_name(uint16_t seq_num);
bool file_open_image_write_file(char* dir_name, uint16_t seq_num, FILE** fp);
void file_close_file(FILE* fp);
void file_unmount_sdcard();



#endif /* FILE_UTILITIES_H */