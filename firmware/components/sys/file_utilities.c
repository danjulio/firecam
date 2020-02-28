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
#include "file_utilities.h"
#include "time_utilities.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ff.h"
#include "vfs_fat_internal.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_defs.h"
#include "diskio.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>



//
// File Utilities internal constants
//
//

// Directory and file string lengths (including null)
//
// Directory names are "session_YY_MM_DD_HH_MM_SS"
#define DIR_NAME_LEN    32
// Sub-directory names are "group_XXXX"
#define SUBDIR_NAME_LEN 16
// File names are "img_XXXXX.json"
#define FILE_NAME_LEN   16



//
// File Utilities internal variables
//
static const char* TAG = "file_utilities";

static const char base_path[] = "/sdcard";
static sdmmc_host_t host_driver = SDMMC_HOST_DEFAULT();               // 4-bit, 20 MHz
static sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT(); // No CD, WP
static sdmmc_card_t sd_card;

static bool card_present = false;


// Options for mounting the filesystem.
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
};

static FATFS *fat_fs;     // Pointer to the filesystem object

// Static allocations for directory and file names
static char session_dir_name[DIR_NAME_LEN];
static char session_subdir_name[SUBDIR_NAME_LEN];
static char session_file_name[FILE_NAME_LEN];

// Last created sub-directory
static int cur_sub_directory_num = -1;   



//
// File Utilities Forward Declarations for internal functions
//
char* file_get_subdir_name(int seq_num);
bool file_create_subdirectory(char* dir_name, char* subdir_name);

// References to internal SDMMC driver functions used to probe the SD Card for
// insertion and removal events
esp_err_t sdmmc_send_cmd_send_scr(sdmmc_card_t* card, sdmmc_scr_t *out_scr);
esp_err_t sdmmc_fix_host_flags(sdmmc_card_t* card);
esp_err_t sdmmc_io_reset(sdmmc_card_t* card);
esp_err_t sdmmc_send_cmd_go_idle_state(sdmmc_card_t* card);
esp_err_t sdmmc_init_sd_if_cond(sdmmc_card_t* card);



//
// File Utilities API
//

/**
 * Connect the SDMCC driver to FATFS and initialize the host driver.  Designed to be
 * called once during startup.
 */
bool file_init_sdmmc_driver()
{
	esp_err_t ret;
		
	// Initialize the driver
	ret = host_driver.init();
	if (ret != ESP_OK) {
		ESP_LOGI(TAG, "Could not initialize SDMMC driver (%d)", ret);
		return false;
	}
	
	// Configure the SD Slot
	ret = sdmmc_host_init_slot(host_driver.slot, &slot_config);
	if (ret != ESP_OK) {
		ESP_LOGI(TAG, "Could not initialize SD Slot (%d)", ret);
		return false;
	}

	// Register FATFS with the VFS component
	// Since we only have one drive, use the default drive number (0)
	ret = esp_vfs_fat_register(base_path, "", mount_config.max_files, &fat_fs);
	if (ret != ESP_OK) {
		ESP_LOGI(TAG, "Could not register FATFS (%d)", ret);
		return false;
	}
	
	// Register FATFS with our card (which will point to the driver when initialized)
 	ff_diskio_register_sdmmc(0, &sd_card);
	
	card_present = false;
	
	return true;
}


/**
 * Check if the card is present.  If we think it is already been detected then attempt
 * to read the SD Configuration Register from it to verify it is still present.
 */
bool file_check_card_still_present()
{
	esp_err_t ret;;
	sdmmc_scr_t scr_tmp;

	// Turn off error logging temporarily because a component in the SDMMC driver will
	// issue an error message about a timeout if the card is missing and we don't want the
	// log file cluttered up with those.
	esp_log_level_set("sdmmc_req", ESP_LOG_NONE);
	ret = sdmmc_send_cmd_send_scr(&sd_card, &scr_tmp);
	esp_log_level_set("sdmmc_req", ESP_LOG_INFO);
	if (ret != ESP_OK) {
		card_present = false;
		return false;
	}

	card_present = true;
	return true;
}


/**
 * Getter for card_present variable for use by other tasks.
 *  Note: We aren't protecting this with a mutex since it should be atomically accessed.
 */
bool file_get_card_present()
{
	return card_present;
}


/**
 * Check if an SD Card has been re-installed by probing it and seeing if a SD memory
 * card is there using basic initialization commands.
 */
bool file_check_card_inserted()
{
	sdmmc_card_t* card = &sd_card;
	memset(card, 0, sizeof(*card));
    memcpy(&card->host, &host_driver, sizeof(host_driver));

    // Fix up the host flags for the MMC driver
    if (sdmmc_fix_host_flags(card) != ESP_OK) {
    	goto fail;
    }
    
    // Turn off error logging temporarily so we don't see timeouts while the card
    // isn't there
    esp_log_level_set("sdmmc_req", ESP_LOG_NONE);
	
    // Reset SDIO (CMD52, RES) before re-initializing IO (CMD5).
    if (sdmmc_io_reset(card) != ESP_OK) {
    	goto fail;
    }
	
    // GO_IDLE_STATE (CMD0) command resets the card
    if (sdmmc_send_cmd_go_idle_state(card) != ESP_OK) {
    	goto fail;
    }
    
    // SEND_IF_COND (CMD8) command is used to identify SDHC/SDXC cards.
    if (sdmmc_init_sd_if_cond(card) != ESP_OK) {
    	goto fail;
    }
	
	// Make sure we found a SDHC/SDXC card
	if ((card->ocr & SD_OCR_SDHC_CAP) != SD_OCR_SDHC_CAP) {
		goto fail;
	}
  
  	// Success!!!
  	esp_log_level_set("sdmmc_req", ESP_LOG_INFO);
	card_present = true;
    return true;

fail:
	esp_log_level_set("sdmmc_req", ESP_LOG_INFO);
	card_present = false;
    return false;
}


/**
 * Probe and initialize the SD card.
 */
bool file_init_card()
{
	if (sdmmc_card_init(&host_driver, &sd_card) != ESP_OK) {
 		return false;
 	}
 	
 	card_present = true;
 	return true;
}


bool file_reinit_card()
{
	card_present = false;
	
	// Reconfigure the SD Slot (necessary before initializing card)
	if (sdmmc_host_init_slot(host_driver.slot, &slot_config) != ESP_OK) {
		ESP_LOGE(TAG, "Could not re-initialize SD Slot");
		return false;
	}
	if (sdmmc_card_init(&host_driver, &sd_card) != ESP_OK) {
		ESP_LOGE(TAG, "Could not re-initialize SD Card");
		return false;
	}
	
	card_present = true;
	return true;
}


/**
 * Attempt to mount the SD Card
 */
bool file_mount_sdcard()
{
	FRESULT ret;
	const size_t workbuf_size = 4096;
	void* workbuf = NULL;
	
	// Attempt to mount the default drive immediately to verify it's still present
	ret = f_mount(fat_fs, "", 1);
	if (ret == FR_NO_FILESYSTEM) {
		// Card mounted but we have to put a filesystem on it

		// Malloc a workbuf
		workbuf = malloc(workbuf_size);
		if (workbuf == NULL) {
			ESP_LOGE(TAG, "Could not allocate work buffer for sd card format");
			card_present = false;
			return false;
		}
		
		// Partition into one partion
		DWORD plist[] = {100, 0, 0, 0};
		ESP_LOGI(TAG, "partitioning card");
		ret = f_fdisk(0, plist, workbuf);
		if (ret != FR_OK) {
			free(workbuf);
			ESP_LOGE(TAG, "Could not partition sd card");
			card_present = false;
			return false;
		}
		
		// Format the partition
		size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
                sd_card.csd.sector_size,
                mount_config.allocation_unit_size);
        ESP_LOGI(TAG, "formatting card, allocation unit size=%d", alloc_unit_size);
        ret = f_mkfs("", FM_ANY, alloc_unit_size, workbuf, workbuf_size);
        if (ret != FR_OK) {
        	free(workbuf);
        	ESP_LOGE(TAG, "Could not format sd card");
        	card_present = false;
        	return false;
        }
        
        // Need to set FF_USE_LABEL in ESP IDF components fatfs/src/ffconf.h to use this
        // Name it
        //f_setlabel(DEF_SD_CARD_LABEL);
        
        free(workbuf);
        
        // Attempt to mount the new filesystem
        ret = f_mount(fat_fs, "", 1);
        if (ret != FR_OK) {
        	ESP_LOGE(TAG, "Could not mount sd card (%d)", ret);
        	card_present = false;
        	return false;
        }
	} else if (ret != FR_OK) {
		ESP_LOGE(TAG, "Could not mount sd card (%d)", ret);
 		card_present = false;
		return false;
	}

	return true;
}


/**
 * Create, using the current date and time, a directory name in our local variable and
 * return a pointer to it.
 */
char* file_get_session_directory_name()
{
	char short_time[20];
	tmElements_t te;
	
	time_get(&te);
	time_get_short_string(te, short_time);
	sprintf(session_dir_name, "session_%s", short_time);
	
	return session_dir_name;
}


/**
 * Create a directory for writing images files during a recording session.  Do
 * nothing if the directory already exists.
 */
bool file_create_directory(char* dir_name)
{
	FRESULT ret;
	
	// Setup for creating the subdirectories
	cur_sub_directory_num = -1;

	// Check if the directory already exists
	ret = f_stat(dir_name, NULL);
	if (ret == FR_NO_FILE) {
		// Create the directory
		ret = f_mkdir(dir_name);
		if (ret != FR_OK) {
			ESP_LOGE(TAG, "Could not create directory %s (%d)", dir_name, ret);
			return false;
		}
	} else if (ret != FR_OK) {
		// Something went wrong
		ESP_LOGE(TAG, "Could not stat directory %s (%d)", dir_name, ret);
		return false;
		
	} else {
		// Directory exists already - no need to do anything
		return true;
	}
	return true;
}


/**
 * Create an image file name in our local variable and return a pointer to it.
 */
char* file_get_session_file_name(uint16_t seq_num)
{
	sprintf(session_file_name, "img_%05d.json", seq_num);
	
	return session_file_name;
}


/**
 * Open a file for writing an image to return a file pointer to it
 */
bool file_open_image_write_file(char* dir_name, uint16_t seq_num, FILE** fp)
{
	char* subdir_name;
	char* file_name;
	char full_name[sizeof(base_path) + DIR_NAME_LEN + SUBDIR_NAME_LEN + FILE_NAME_LEN + 3];
	int file_group_num;
	int dir_name_len;
	
	// Make sure the subdirectory exists for this file
	file_group_num = seq_num / FILES_PER_SUBDIRECTORY;
	subdir_name = file_get_subdir_name(file_group_num);
	if (file_group_num != cur_sub_directory_num) {
		if (file_create_subdirectory(dir_name, subdir_name)) {
			cur_sub_directory_num = file_group_num;
		} else {
			ESP_LOGE(TAG, "Could not create subdirectory %s", subdir_name);
			return false;
		}
	}
	
	// Fill a buffer with the full file name
	dir_name_len = strlen(dir_name);
	if (dir_name_len == 0) {
		ESP_LOGE(TAG, "No directory specified for file open");
		return false;
	}
	file_name = file_get_session_file_name(seq_num);
	sprintf(full_name, "%s/%s/%s/%s", base_path, dir_name, subdir_name, file_name);

	// Attempt to open the file
	*fp = fopen(full_name, "w");
	if (*fp == NULL) {
		ESP_LOGE(TAG, "Could not open %s", full_name);
		return false;
	}
	
	return true;
}


/**
 * Close a file
 */
void file_close_file(FILE* fp)
{
	//close(fp);
	fclose(fp);
}


/**
 * Unmount the sd card
 */
void file_unmount_sdcard()
{
	f_mount(0, "", 0);
}



//
// File Utilities internal functions
//

/**
 * Create an image file name in our local variable and return a pointer to it.
 */
char* file_get_subdir_name(int subdir_num)
{
	sprintf(session_subdir_name, "group_%04d", subdir_num);
	
	return session_subdir_name;
}


/**
 * Create a subdirectory
 */
bool file_create_subdirectory(char* dir_name, char* subdir_name)
{
	char dir_path[DIR_NAME_LEN + SUBDIR_NAME_LEN + 2];
	FRESULT ret;
	
	// Create the full path name
	sprintf(dir_path, "%s/%s", dir_name, subdir_name);
	
	// Check if the directory already exists
	ret = f_stat(dir_path, NULL);
	if (ret == FR_NO_FILE) {
		// Create the directory
		ret = f_mkdir(dir_path);
		if (ret != FR_OK) {
			ESP_LOGE(TAG, "Could not create directory %s (%d)", dir_path, ret);
			return false;
		}
	} else if (ret != FR_OK) {
		// Something went wrong
		ESP_LOGE(TAG, "Could not stat directory %s (%d)", dir_path, ret);
		return false;
		
	} else {
		// Directory exists already - no need to do anything
		return true;
	}
	return true;
}
