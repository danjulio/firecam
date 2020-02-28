/*
 * System Configuration File
 *
 * Contains system definition and configurable items.
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
#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "esp_system.h"
#include "ov2640.h"


// ======================================================================================
// System debug
//

// Undefine to include the system monitoring task (included only for debugging/tuning)
//#define INCLUDE_SYS_MON



// ======================================================================================
// System hardware definitions
//

//
// IO Pins
//   Lepton uses HSPI (no MOSI)
//   ArduCam, LCD and TS use VSPI
//
#define TS_CSN_IO         5
#define LCD_CSN_IO        18
#define HSPI_SCK_IO       19
#define LEP_CSN_IO        21
#define I2C_MASTER_SDA_IO 22
#define I2C_MASTER_SCL_IO 23
#define VSPI_SCK_IO       25
#define PWR_HOLD_IO       26
#define CAM_CSN_IO        27
#define LCD_DC_IO         32
#define VSPI_MOSI_IO      33
#define VSPI_MISO_IO      34
#define TS_IRQ_IO         35
#define LEP_VSYNC_IO      36
#define HSPI_MISO_IO      39


//
// Hardware Configuration
//

// I2C
#define I2C_MASTER_NUM     1
#define I2C_MASTER_FREQ_HZ 100000

// SPI
#define LEP_SPI_HOST    HSPI_HOST
#define CAM_SPI_HOST    VSPI_HOST
#define LCD_SPI_HOST    VSPI_HOST
#define TS_SPI_HOST     VSPI_HOST
#define HSPI_DMA_NUM    1
#define VSPI_DMA_NUM    2
#define LEP_SPI_FREQ_HZ 16000000
#define LCD_SPI_FREQ_HZ 16000000
#define CAM_SPI_FREQ_HZ  4000000
#define TS_SPI_FREQ_HZ   2000000


// ======================================================================================
// System configuration
//

// Little VGL buffer update size
#define LVGL_DISP_BUF_SIZE (320 * 40)

// Little VGL Touchpanel configuration (a bit of a hack - it should be calibrated)
#define LVGL_TOUCH_X_MIN    360
#define LVGL_TOUCH_Y_MIN    270
#define LVGL_TOUCH_X_MAX    3900
#define LVGL_TOUCH_Y_MAX    3800
#define LVGL_TOUCH_INVERT_X 1
#define LVGL_TOUCH_INVERT_Y 1

// Little VGL evaluation rate (mSec)
#define LVGL_EVAL_MSEC      10


// ArduCAM image resolution
//  Undefine for 640x480 resolution, otherwise 320x240
//  Note: CAM_RES_HIGH also sets the jpeg decoder scale factor in render_jpg.c
#define CAM_RES_HIGH

#ifdef CAM_RES_HIGH
#define CAM_SIZE_SPEC       OV2640_640x480
#define CAM_JPEG_WIDTH      640
#else
#define CAM_SIZE_SPEC       OV2640_320x240
#define CAM_JPEG_WIDTH      320
#endif


// ArduCAM max jpg image size (based on CAM_SIZE_SPEC)
#ifdef CAM_RES_HIGH
#define CAM_MAX_JPG_LEN     65536
#else
#define CAM_MAX_JPG_LEN     32768
#endif

// Combined image (ArduCAM + Lepton + Metadata) json object text size
// Based on the following items:
//   1. Base64 encoded ArduCAM maximum image size: CAM_MAX_JPG_LEN*4 / 3
//   2. Base64 encoded Lepton image size: (160x120x2)*4 / 3
//   3. Metadata text size: 2048
//   4. Json object overhead (child names, formatting characters, NLs): 256
// Manually calculate this and round to 4-byte boundary
#ifdef CAM_RES_HIGH
#define JSON_MAX_IMAGE_TEXT_LEN (1024 * 160)
#else
#define JSON_MAX_IMAGE_TEXT_LEN (1024 * 128)
#endif

// Max command response json object text size
#define JSON_MAX_RSP_TEXT_LEN   1024

// Maximum incoming command json string length (large enough for longest command)
#define JSON_MAX_CMD_TEXT_LEN   256

// Maximum TCP/IP Socket receiver buffer
//  This should be large enough for the maximum number of command received at a time
//  (probably 2 is ok)
#define CMD_MAX_TCP_RX_BUFFER_LEN 1024

// TCP/IP listening port
#define CMD_PORT 5001





#endif // SYSTEM_CONFIG_H