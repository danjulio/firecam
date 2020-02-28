/*
 * OV2640 - Driver for the Arducam camera module
 *
 * Based on DIY-Thermocam Firmware, ported by Dan Julio
 *
 * Copyright by Max Ritter and Dan Julio
 *
 * http://www.diy-thermocam.net
 * https://github.com/maxritter/DIY-Thermocam
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
#ifndef OV2640_H
#define OV2640_H

#include <stdint.h>
#include "ov2640regs.h"

/*################# HARDWARE INTERFACE CONFIGURTION ##################*/

/* I2C constants */
#define OV2640_I2C_ADDR 0x30
#define I2C_MASTER_SDA_IO 22
#define I2C_MASTER_SCL_IO 23
#define I2C_MASTER_NUM 1
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TX_BUF_LEN 0
#define I2C_MASTER_RX_BUF_LEN 0

#define ACK_CHECK_EN 0x1
#define ACK_CHECK_DIS 0x0
#define ACK_VAL 0x0
#define NACK_VAL 0x1 

/* SPI constants */
#define CAM_SPI_HOST VSPI_HOST
#define CAM_DMA_CH 1
#define CAM_SPI_HZ 4000000
#define CAM_PIN_NUM_MOSI 33
#define CAM_PIN_NUM_MISO 34
#define CAM_PIN_NUM_SCLK 25
#define CAM_PIN_NUM_CS   27
#define CAM_MAX_SPI_PKT  1024


/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

#define OV2640_160x120 		0
#define OV2640_176x144 		1
#define OV2640_320x240 		2
#define OV2640_352x288 		3
#define OV2640_640x480		4
#define OV2640_800x600 		5
#define OV2640_1024x768		6
#define OV2640_1280x1024	7
#define OV2640_1600x1200	8

#define CAP_DONE_MASK 0x08
#define ARDUCHIP_TRIG 0x41

//Light Mode
#define Auto                 0
#define Sunny                1
#define Cloudy               2
#define Office               3
#define Home                 4


/*########################## PUBLIC PROCEDURES ################################*/

void ov2640_busWrite(uint8_t address, uint8_t value);
uint8_t ov2640_busRead(uint8_t address);
void ov2640_burstBusRead(uint16_t length);
void ov2640_capture(void);
void ov2640_clearBit(uint8_t addr, uint8_t bit);
void ov2640_clearFifoFlag(void);
void ov2640_flushFifo(void);
uint8_t ov2640_getBit(uint8_t addr, uint8_t bit);
int ov2640_init(void);
uint8_t ov2640_rdSensorReg8_8(uint8_t regID, uint8_t* regDat);
uint32_t ov2640_readFifoLength(void);
uint8_t ov2640_readFifo(void);
uint8_t ov2640_readReg(uint8_t addr);
void ov2640_setBit(uint8_t addr, uint8_t bit);
void ov2640_setFormat(uint8_t fmt);
void ov2640_setJPEGSize(uint8_t size);
void ov2640_set_Light_Mode(uint8_t Light_Mode);
void ov2640_setMode(uint8_t mode);
void ov2640_startCapture(void);
void ov2640_transferJpeg(uint8_t * camData, uint32_t* length);
void ov2640_transferRaw(uint8_t * camData, uint32_t length);
void ov2640_writeReg(uint8_t addr, uint8_t data);
uint8_t ov2640_wrSensorReg8_8(uint8_t regID, uint8_t regDat);
int ov2640_wrSensorRegs8_8(const struct sensor_reg* reglist);

#endif /* OV2640_H */
