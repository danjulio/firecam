/*
 * ADC128D818 ADC Module
 *
 * Provides access to the ADC128D818 8-channel 12-bit ADC chip.
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
#include "adc128d818.h"
#include "esp_system.h"
#include "esp_log.h"
#include "i2c.h"


//
// ADC Variables
//
static const char* TAG = "ADC128D818";


//
// ADC API
//

/**
 * Write an 8-bit ADC register
 */
bool adc_write_byte(uint8_t reg_addr, uint8_t reg_data)
{
	uint8_t buf[2];
	bool ret = true;
	
	// Write the register address
	buf[0] = reg_addr;
	buf[1] = reg_data;
	
	i2c_lock();
	if (i2c_master_write_slave(ADC_I2C_ADDR, buf, 2) != ESP_OK) {
		ESP_LOGE(TAG, "failed to write %02x = %02x", reg_addr, reg_data);
		ret = false;
	}
	i2c_unlock();
	
	return ret;
}


/**
 * Read an 8-bit ADC register
 */
bool adc_read_byte(uint8_t reg_addr, uint8_t* reg_data)
{
	uint8_t buf = 0;
	
	// Write the register address
	buf = reg_addr;
	
	i2c_lock();
	if (i2c_master_write_slave(ADC_I2C_ADDR, &buf, 1) != ESP_OK) {
		i2c_unlock();
		ESP_LOGE(TAG, "failed to write address register %02x", reg_addr);
		return false;
	}

	// Read
	if (i2c_master_read_slave(ADC_I2C_ADDR, &buf, 1) != ESP_OK) {
		i2c_unlock();
		ESP_LOGE(TAG, "failed to read from byte register %02x", reg_addr);
		return false;
	}
	i2c_unlock();

	*reg_data = buf;
	return true;
}


/**
 * Read a 16-bit ADC register - data is returned in the low 12-bits
 */
bool adc_read_word(uint8_t reg_addr, uint16_t* reg_data)
{
	uint8_t buf[2] = {0, 0};
	
	// Write the register address
	buf[0] = reg_addr;
	
	i2c_lock();
	if (i2c_master_write_slave(ADC_I2C_ADDR, &buf[0], 1) != ESP_OK) {
		i2c_unlock();
		ESP_LOGE(TAG, "failed to write address register %02x", reg_addr);
		return false;
	}

	// Read
	if (i2c_master_read_slave(ADC_I2C_ADDR, buf, 2) != ESP_OK) {
		i2c_unlock();
		ESP_LOGE(TAG, "failed to read from word register %02x", reg_addr);
		return false;
	}
	i2c_unlock();

	*reg_data = ((buf[0] << 8) | buf[1]) >> 4; /* I dunno why, but TI put the 12-bits in the top */
	return true;
}
