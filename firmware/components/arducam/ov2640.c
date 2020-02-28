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

/*################################# INCLUDES ##################################*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "i2c.h"
#include "ov2640regs.h"
#include "ov2640.h"
#include "system_config.h"



/*################# DATA TYPES, CONSTANTS & MACRO DEFINITIONS #################*/

/* Sensor related definitions */

#define BMP 0
#define JPEG 1
#define OV2640_ADDRESS 0x60
#define OV2640_CHIPID_HIGH 0x0A
#define OV2640_CHIPID_LOW 0x0B

/* I2C control definition */

#define I2C_ADDR_8BIT 0
#define I2C_ADDR_16BIT 1
#define I2C_REG_8BIT 0
#define I2C_REG_16BIT 1
#define I2C_DAT_8BIT 0
#define I2C_DAT_16BIT 1

/* Register initialization tables for SENSORs */

#define SENSOR_REG_TERM_8BIT                0xFF
#define SENSOR_REG_TERM_16BIT               0xFFFF
#define SENSOR_VAL_TERM_8BIT                0xFF
#define SENSOR_VAL_TERM_16BIT               0xFFFF
#define MAX_FIFO_SIZE		                0x60000

/* ArduChip registers definition */

#define RWBIT 0x80
#define ARDUCHIP_TEST1 0x00
#define ARDUCHIP_MODE 0x02
#define MCU2LCD_MODE 0x00
#define CAM2LCD_MODE 0x01
#define LCD2MCU_MODE 0x02
#define ARDUCHIP_TIM 0x03
#define ARDUCHIP_FIFO 0x04
#define FIFO_CLEAR_MASK 0x01
#define FIFO_START_MASK 0x02
#define FIFO_WRPTR_RST_MASK 0x10
#define FIFO_RDPTR_RST_MASK 0x20
#define ARDUCHIP_GPIO 0x06
#define GPIO_RESET_MASK 0x01
#define GPIO_PWDN_MASK 0x02
#define GPIO_PWREN_MASK	0x04
#define BURST_FIFO_READ	0x3C
#define SINGLE_FIFO_READ 0x3D
#define ARDUCHIP_REV 0x40
#define VER_LOW_MASK 0x0F
#define VER_HIGH_MASK 0xF0
#define VSYNC_MASK 0x01
#define SHUTTER_MASK 0x02

#define FIFO_SIZE1 0x42
#define FIFO_SIZE2 0x43
#define FIFO_SIZE3 0x44


static const char* TAG = "ov2640";

// SPI Interface
static spi_device_handle_t spi;

// Pointer to an allocated array to store data read from the camera (DMA capable)
static uint8_t* camBuf;

// Forward declarations for private functions
static void ov2640_i2c_delay();


/*######################## PUBLIC FUNCTION BODIES #############################*/

/* I2C Write 8bit address, 8bit data */
uint8_t ov2640_wrSensorReg8_8(uint8_t regID, uint8_t regDat) {
	// Make sure we meet the OV2640's Tbuf parameter (1.3 uSec min I2C to I2C cycle delay)
	ov2640_i2c_delay();
	
	// Write the register address and value
	uint8_t write_buf[2] = {
		regID,
		regDat
	};

	i2c_lock();
	if (i2c_master_write_slave(OV2640_I2C_ADDR, write_buf, sizeof(write_buf)) != ESP_OK) {
		i2c_unlock();
    	ESP_LOGE(TAG, "ov2640_wrSensorReg8_8 failed: register 0x%02x, value 0x%02x", regID, regDat);
    	return 0;
	};
	i2c_unlock();

  return 1;
}

/* I2C Read 8bit address, 8bit data */
uint8_t ov2640_rdSensorReg8_8(uint8_t regID, uint8_t* regDat) {
	uint8_t buf;
	
	// Make sure we meet the OV2640's Tbuf parameter (1.3 uSec min I2C to I2C cycle delay)
	ov2640_i2c_delay();
	
	// Send register address
	buf = regID;
	i2c_lock();
	if (i2c_master_write_slave(OV2640_I2C_ADDR, &buf, sizeof(buf)) != ESP_OK) {
		i2c_unlock();
    	ESP_LOGE(TAG, "ov2640_rdSensorReg8_8 write failed to register 0x%02x", regID);
    	return 0;
	};
	i2c_unlock();

	ov2640_i2c_delay();
	
	// Read data
	i2c_lock();
	if (i2c_master_read_slave(OV2640_I2C_ADDR, &buf, sizeof(buf)) != ESP_OK) {
		i2c_unlock();
    	ESP_LOGE(TAG, "ov2640_rdSensorReg8_8 read failed");
    	return 0;
  	}
  	i2c_unlock();

	*regDat = buf;

	return (1);
}

/* I2C Array Write 8bit address, 8bit data */
int ov2640_wrSensorRegs8_8(const struct sensor_reg* reglist) {
	uint8_t reg_addr = 0;
	uint8_t reg_val = 0;
	const struct sensor_reg *next = reglist;
	
	while ((reg_addr != 0xff) | (reg_val != 0xff))
	{
		reg_addr = next->reg;
		reg_val = next->val;
		ov2640_wrSensorReg8_8(reg_addr, reg_val);
		next++;
	}

	return 1;
}

/* Single byte SPI write operation */
void ov2640_busWrite(uint8_t address, uint8_t value) {
	spi_transaction_t t;
	esp_err_t ret;
	
	// Setup our SPI transaction
	memset(&t, 0, sizeof(t));
	t.flags = SPI_TRANS_USE_TXDATA;
	t.cmd = address;                  // SPI IF will send upper 8-bits during command phase
	t.length = 8;                     // 1 byte sent during data phase
	t.tx_data[0] = value;             // Directly include data
	t.rx_buffer = NULL;
	
	ret = spi_device_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);
}

/* Single byte SPI read operation */
uint8_t ov2640_busRead(uint8_t address) {
	spi_transaction_t t;
	esp_err_t ret;
	
	// Setup our SPI transaction
	memset(&t, 0, sizeof(t));
	t.flags = SPI_TRANS_USE_RXDATA;
	t.cmd = address;                  // SPI IF will send upper 8-bits during command phase
	t.rxlength = 8;                   // 1 byte read during data phase
	t.tx_buffer = NULL;
	
	ret = spi_device_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);

	//Return value
	return t.rx_data[0];
}

/* Burst SPI read operation to our buffer */
void ov2640_burstBusRead(uint16_t length) {
	spi_transaction_t t;
	esp_err_t ret;
	
	// Setup our SPI transaction
	memset(&t, 0, sizeof(t));
	t.cmd = BURST_FIFO_READ;          // SPI IF will send upper 8-bits during command phase
	t.rxlength = length * 8;          // length byte read during data phase
	t.tx_buffer = NULL;
	t.rx_buffer = camBuf;             // Receive into our DMA-capable buffer
	
	// Run the SPI transaction using the interrupt function so other tasks can run during the transfer
	ret = spi_device_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);
}

/* Read ArduChip internal registers */
uint8_t ov2640_readReg(uint8_t addr) {
	uint8_t data;
	data = ov2640_busRead(addr & 0x7F);
	return data;
}

/* Write ArduChip internal registers */
void ov2640_writeReg(uint8_t addr, uint8_t data) {
	ov2640_busWrite(addr | 0x80, data);
}

/* Init the camera */
/*   returns 1 for success, 0 for failure */
int ov2640_init(void) {
	uint8_t vid, pid;
	uint8_t rtnVal;
	esp_err_t espRtnVal;

	// SPI Device
	spi_device_interface_config_t devcfg = {
    	.command_bits = 8,
    	.address_bits = 0,
    	.clock_speed_hz = CAM_SPI_FREQ_HZ,
    	.input_delay_ns = 25,
    	.mode = 0,
    	.spics_io_num = CAM_CSN_IO,
    	.queue_size = 1,
    	.flags = SPI_DEVICE_HALFDUPLEX,
    	.cs_ena_pretrans = 2
	};
    if ((espRtnVal=spi_bus_add_device(VSPI_HOST, &devcfg, &spi)) != ESP_OK) {
    	ESP_LOGE(TAG, "Failed to add camera spi device");
    	return 0;
    }

    // Allocate our DMA-capable SPI buffer
    camBuf = (uint8_t*) heap_caps_malloc(CAM_MAX_SPI_PKT, MALLOC_CAP_DMA);
    if (camBuf == NULL) {
    	ESP_LOGE(TAG, "Failed to allocate camera DMA buffer");
        return 0;
    }

	//Test SPI connection first
	ov2640_writeReg(ARDUCHIP_TEST1, 0x55);
	rtnVal = ov2640_readReg(ARDUCHIP_TEST1);
	if (rtnVal != 0x55) {
		ESP_LOGE(TAG, "SPI Test read failed with 0x%02x", rtnVal);
    	return 0;
	}
	
	//Reset CPLD per https://www.arducam.com/docs/spi-cameras-for-arduino/faq/
	ov2640_writeReg(0x07, 0x80);
	vTaskDelay(pdMS_TO_TICKS(100));
	ov2640_writeReg(0x07, 0x00);
	vTaskDelay(pdMS_TO_TICKS(100));

	//Test I2C connection second
	ov2640_wrSensorReg8_8(0xff, 0x01);
	ov2640_rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
	ov2640_rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
	if ((vid != 0x26) || ((pid != 0x42) && (pid != 0x41))) {
		ESP_LOGE(TAG, "I2C Test read failed with vid = 0x%02x, pid = 0x%02x", vid, pid);
		return 0;
	}

	//Init registers
	ov2640_wrSensorReg8_8(0xff, 0x01);
	ov2640_wrSensorReg8_8(0x12, 0x80);

	//Wait some time
	vTaskDelay(pdMS_TO_TICKS(100));

	//Set format to JPEG
	ov2640_wrSensorRegs8_8(OV2640_JPEG_INIT);
	ov2640_wrSensorRegs8_8(OV2640_YUV422);
	ov2640_wrSensorRegs8_8(OV2640_JPEG);
	ov2640_wrSensorReg8_8(0xff, 0x01);
	ov2640_wrSensorReg8_8(0x15, 0x00);
	ov2640_wrSensorRegs8_8(OV2640_320x240_JPEG);
	
	//Set camera bus mode
	ov2640_setMode(MCU2LCD_MODE);

	//Return status
	return 1;
}

/* Set the format to JPEG or BMP */
void ov2640_setFormat(uint8_t fmt) {
	//BMP
	if (fmt == BMP) {
		ov2640_wrSensorRegs8_8(OV2640_QVGA);
	}
	//JPEG
	else {
		ov2640_wrSensorRegs8_8(OV2640_JPEG_INIT);
		ov2640_wrSensorRegs8_8(OV2640_YUV422);
		ov2640_wrSensorRegs8_8(OV2640_JPEG);
		ov2640_wrSensorReg8_8(0xff, 0x01);
		ov2640_wrSensorReg8_8(0x15, 0x00);
		ov2640_wrSensorRegs8_8(OV2640_320x240_JPEG);
		ov2640_wrSensorReg8_8(0xff, 0x00); //???
		ov2640_wrSensorReg8_8(0x44, 0x32); //???
	}
}

/* Set the JPEG pixel size of the image */
void ov2640_setJPEGSize(uint8_t size) {
	switch (size)
	{
	case OV2640_160x120:
		ov2640_wrSensorRegs8_8(OV2640_160x120_JPEG);
		break;
	case OV2640_176x144:
		ov2640_wrSensorRegs8_8(OV2640_176x144_JPEG);
		break;
	case OV2640_320x240:
		ov2640_wrSensorRegs8_8(OV2640_320x240_JPEG);
		break;
	case OV2640_352x288:
		ov2640_wrSensorRegs8_8(OV2640_352x288_JPEG);
		break;
	case OV2640_640x480:
		ov2640_wrSensorRegs8_8(OV2640_640x480_JPEG);
		break;
	case OV2640_800x600:
		ov2640_wrSensorRegs8_8(OV2640_800x600_JPEG);
		break;
	case OV2640_1024x768:
		ov2640_wrSensorRegs8_8(OV2640_1024x768_JPEG);
		break;
	case OV2640_1280x1024:
		ov2640_wrSensorRegs8_8(OV2640_1280x1024_JPEG);
		break;
	case OV2640_1600x1200:
		ov2640_wrSensorRegs8_8(OV2640_1600x1200_JPEG);
		break;
	default:
		ov2640_wrSensorRegs8_8(OV2640_320x240_JPEG);
		break;
	}
}

/* Set Light Mode */
void ov2640_set_Light_Mode(uint8_t Light_Mode) {
	switch(Light_Mode)
	{	
	case Auto:
		ov2640_wrSensorReg8_8(0xff, 0x00);
		ov2640_wrSensorReg8_8(0xc7, 0x00); //AWB on
		break;
	case Sunny:
		ov2640_wrSensorReg8_8(0xff, 0x00);
		ov2640_wrSensorReg8_8(0xc7, 0x40); //AWB off
		ov2640_wrSensorReg8_8(0xcc, 0x5e);
		ov2640_wrSensorReg8_8(0xcd, 0x41);
		ov2640_wrSensorReg8_8(0xce, 0x54);
		break;
	case Cloudy:
		ov2640_wrSensorReg8_8(0xff, 0x00);
		ov2640_wrSensorReg8_8(0xc7, 0x40); //AWB off
		ov2640_wrSensorReg8_8(0xcc, 0x65);
		ov2640_wrSensorReg8_8(0xcd, 0x41);
		ov2640_wrSensorReg8_8(0xce, 0x4f);  
		break;
	case Office:
		ov2640_wrSensorReg8_8(0xff, 0x00);
		ov2640_wrSensorReg8_8(0xc7, 0x40); //AWB off
		ov2640_wrSensorReg8_8(0xcc, 0x52);
		ov2640_wrSensorReg8_8(0xcd, 0x41);
		ov2640_wrSensorReg8_8(0xce, 0x66);
		break;
	case Home:
		ov2640_wrSensorReg8_8(0xff, 0x00);
		ov2640_wrSensorReg8_8(0xc7, 0x40); //AWB off
		ov2640_wrSensorReg8_8(0xcc, 0x42);
		ov2640_wrSensorReg8_8(0xcd, 0x3f);
		ov2640_wrSensorReg8_8(0xce, 0x71);
		break;
	default :
		ov2640_wrSensorReg8_8(0xff, 0x00);
		ov2640_wrSensorReg8_8(0xc7, 0x00); //AWB on
		break; 
	}	
}

/* Read Write FIFO length */
uint32_t ov2640_readFifoLength(void) {
	uint32_t len1, len2, len3, length;
	len1 = ov2640_readReg(FIFO_SIZE1);
	len2 = ov2640_readReg(FIFO_SIZE2);
	len3 = ov2640_readReg(FIFO_SIZE3) & 0x07;
	length = ((len3 << 16) | (len2 << 8) | len1);
	return length;
}

/* Set corresponding bit */
void ov2640_setBit(uint8_t addr, uint8_t bit) {
	uint8_t temp;
	temp = ov2640_readReg(addr);
	ov2640_writeReg(addr, temp | bit);
}

/* Clear corresponding bit */
void ov2640_clearBit(uint8_t addr, uint8_t bit) {
	uint8_t temp;
	temp = ov2640_readReg(addr);
	ov2640_writeReg(addr, temp & (~bit));
}

/* Get corresponding bit status */
uint8_t ov2640_getBit(uint8_t addr, uint8_t bit) {
	uint8_t temp;
	temp = ov2640_readReg(addr);
	temp = temp & bit;
	return temp;
}

/* Set ArduCAM working mode */
void ov2640_setMode(uint8_t mode) {
	switch (mode)
	{
		//MCU2LCD_MODE: MCU writes the LCD screen GRAM
	case MCU2LCD_MODE:
		ov2640_writeReg(ARDUCHIP_MODE, MCU2LCD_MODE);
		break;
		//CAM2LCD_MODE: Camera takes control of the LCD screen
	case CAM2LCD_MODE:
		ov2640_writeReg(ARDUCHIP_MODE, CAM2LCD_MODE);
		break;
		//LCD2MCU_MODE: MCU read the LCD screen GRAM
	case LCD2MCU_MODE:
		ov2640_writeReg(ARDUCHIP_MODE, LCD2MCU_MODE);
		break;
		//Default is MCU2LCD_MODE
	default:
		ov2640_writeReg(ARDUCHIP_MODE, MCU2LCD_MODE);
		break;
	}
}

/* Reset the FIFO pointers to zero */
void ov2640_flushFifo(void) {
	ov2640_writeReg(ARDUCHIP_FIFO, FIFO_RDPTR_RST_MASK | FIFO_WRPTR_RST_MASK);
}

/* Send capture command */
void ov2640_startCapture(void) {
	ov2640_writeReg(ARDUCHIP_FIFO, FIFO_START_MASK);
}

/* Clear FIFO Complete flag */
void ov2640_clearFifoFlag(void) {
	ov2640_writeReg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}

/* Read FIFO single */
uint8_t ov2640_readFifo(void) {
	uint8_t data;
	data = ov2640_busRead(SINGLE_FIFO_READ);
	return data;
}

/* Send the capture command to the camera */
void ov2640_capture(void) {
	//Flush the FIFO
	ov2640_flushFifo();
	//Clear the capture done flag
	ov2640_clearFifoFlag();
	//Start capture
	ov2640_startCapture();
}

/* Transfer data from the Arducam frame buffer */
void ov2640_transferJpeg(uint8_t* camData, uint32_t* length) {
	uint32_t read_length;           // Data length in SPI buffer
	uint32_t total_read_length = 0; // Length of data read so far
	uint32_t image_length;          // Length of image reported by camera
	uint32_t jpeg_length = 0;       // Length of jpeg image found in camera data
	uint8_t temp = 0, temp_last;    // Used to determine start and stop of jpeg data
	bool saw_header = false;        // Set when we find the start of a jpeg image
	bool found_image = false;       // Set when a valid jpeg image is found
	int i;

	// Get the image length
	image_length = ov2640_readFifoLength();
	//ESP_LOGI(TAG, "image length = %d", image_length);
	
	// Handle illegal cases
	if ((image_length == 0) || (image_length >= MAX_FIFO_SIZE)) {
		*length = 0;
		ESP_LOGI(TAG, "Unexpected camera read fifo length %d", image_length);
		return;
	}
	if (image_length > CAM_MAX_JPG_LEN) {
		*length = 0;
		ESP_LOGI(TAG, "Camera read fifo length %d is too large", image_length);
		return;
	}
	
	// Process image data
	while ((total_read_length < image_length) && !found_image) {
		// Read data from the camera
		if (total_read_length < (image_length - CAM_MAX_SPI_PKT)) {
			read_length = CAM_MAX_SPI_PKT;
		} else {
			read_length = image_length - total_read_length;
		}
		ov2640_burstBusRead(read_length);

		// Process read data
		for (i=0; i<read_length; i++) {
			temp_last = temp;
			temp = camBuf[i];

			if (saw_header) {
				// Processing jpeg data
				camData[jpeg_length] = temp;
				jpeg_length++;
				
				if ((temp == 0xD9) && (temp_last == 0xFF)) {
					// End of jpeg image
					found_image = true;
					break;
				}
			} else if ((temp == 0xD8) && (temp_last == 0xFF)) {
				// Start of jpeg image
				saw_header = true;
							
				camData[0] = temp_last;
				camData[1] = temp;
				jpeg_length = 2;

			}
		}
		total_read_length += read_length;

	}
	
	if (found_image) {
		*length = jpeg_length;
	} else {
		*length = 0;
	}
}

void ov2640_transferRaw(uint8_t * camData, uint32_t length) {
	uint32_t read_length;
	uint32_t total_read_length = 0;
	
	if (length > MAX_FIFO_SIZE) {
		length = MAX_FIFO_SIZE;
	}
	
	while (total_read_length < length) {
		// Read data from the camera
		if (total_read_length < (length - CAM_MAX_SPI_PKT)) {
			read_length = CAM_MAX_SPI_PKT;
		} else {
			read_length = length - total_read_length;
		}
		ov2640_burstBusRead(read_length);
		memcpy(&camData[total_read_length], camBuf, read_length);
		total_read_length += read_length;
	}
}


/*######################## PRIVATE FUNCTION BODIES #############################*/


/**
 * Delay for at least 1.3 uSec (on a 240 MHz CPU)
 *
 */
static void ov2640_i2c_delay()
{
	uint16_t c = 312;  // 1.3 x 240
	
	while (c--) asm volatile ("nop");
}