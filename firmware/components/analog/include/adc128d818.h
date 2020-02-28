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
#ifndef ADC128D818_H
#define ADC128D818_H

#include <stdbool.h>
#include <stdint.h>


//
// ADC constants
//

// I2C 7-bit address
#define ADC_I2C_ADDR          0x1D

// ADC Registers
#define ADC_CFG_REG           0x00
#define ADC_CFG_START_MASK    0x01
#define ADC_CFG_INT_EN_MASK   0x02
#define ADC_CFG_INT_CLR_MASK  0x08
#define ADC_CFG_INIT_MASK     0x80

#define ADC_CONV_REG          0x07
#define ADC_CONV_EN           0x01
#define ADC_CONV_LP           0x00

#define ADC_CH_DIS_REG        0x08

#define ADC_ONE_SHOT_REG      0x09
#define ADC_ONE_SHOT_TRG_MASK 0x01

#define ADC_SHUTDOWN_REG      0x0A
#define ADC_SHUTDOWN_EN_MASK  0x01

#define ADC_ACFG_REG          0x0B
#define ADC_ACFG_EXT_REF_MASK 0x01
#define ADC_ACFG_MODE0_MASK   0x00
#define ADC_ACFG_MODE1_MASK   0x02
#define ADC_ACFG_MODE2_MASK   0x04
#define ADC_ACFG_MODE3_MASK   0x06

#define ADC_BUSY_REG          0x0C
#define ADC_CONV_BUSY_MASK    0x01
#define ADC_PWRUP_BUSY_MASK   0x02

#define ADC_CH_BASE_REG       0x20

#define ADC_LIM_BASE_REG      0x2A

#define ADC_MANUF_ID_REG      0x3E
#define ADC_MANUF_ID          0x01

#define ADC_REV_ID_REG        0x3F
#define ADC_REV_ID            0x09

// Internal voltage reference value
#define ADC_INT_VREF_V        2.56



//
// ADC API
//
bool adc_write_byte(uint8_t reg_addr, uint8_t reg_data);
bool adc_read_byte(uint8_t reg_addr, uint8_t* reg_data);
bool adc_read_word(uint8_t reg_addr, uint16_t* reg_data);


#endif /* ADC128D818_H */