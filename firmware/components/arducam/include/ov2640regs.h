/*
 *
 * OV2640Regs - Register for the Arducam camera module
 *
 * DIY-Thermocam Firmware
 *
 * GNU General Public License v3.0
 *
 * Copyright by Max Ritter
 *
 * http://www.diy-thermocam.net
 * https://github.com/maxritter/DIY-Thermocam
 *
 */

#ifndef OV2640_REGS_H
#define OV2640_REGS_H

struct sensor_reg
{
	uint8_t reg;
	uint8_t val;
};


/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

extern const struct sensor_reg OV2640_QVGA[];
extern const struct sensor_reg OV2640_JPEG_INIT[];
extern const struct sensor_reg OV2640_YUV422[];
extern const struct sensor_reg OV2640_JPEG[];
extern const struct sensor_reg OV2640_160x120_JPEG[];
extern const struct sensor_reg OV2640_176x144_JPEG[];
extern const struct sensor_reg OV2640_320x240_JPEG[];
extern const struct sensor_reg OV2640_352x288_JPEG[];
extern const struct sensor_reg OV2640_640x480_JPEG[];
extern const struct sensor_reg OV2640_800x600_JPEG[];
extern const struct sensor_reg OV2640_1024x768_JPEG[];
extern const struct sensor_reg OV2640_1280x1024_JPEG[];
extern const struct sensor_reg OV2640_1600x1200_JPEG[];

#endif /* OV2640_REGS_H */
