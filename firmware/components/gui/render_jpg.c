/*
 * TJpgDec wrapper functions for decoding jpeg images.
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
#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "render_jpg.h"
#include "tjpgd.h"


static const char* TAG = "render_jpg";


// tjpgd Decompressor structure
typedef struct {
	uint8_t* jpic;	   // Pointer to jpeg image
	uint16_t jsize;	   // Jpeg image length (bytes)
	uint16_t joffset;  // Current offset reading from jpeg image
	uint16_t fwidth;   // Frame buffer width
	uint8_t* fbuf;	   // Pointer to frame buffer
} IODEV;


// tjpgd work area
static void* tjpgd_work;



//
// Internal Function Forward Declarations
//
static unsigned int tjpgd_input(JDEC* jd, unsigned char* buff, unsigned int nbyte);
static unsigned int tjpgd_output(JDEC* jd, void* bitmap, JRECT* rect);



//
// JPG Render API
//

/*
 * Initialize the rendering engine
 *   returns 1 for success, 0 for failure
 */
int render_init()
{
	// Get memory for the tjpgd decompressor
	tjpgd_work = malloc(TJPGD_WORK_BUF_LEN);
	
	if (tjpgd_work == NULL) {
		ESP_LOGE(TAG, "Could not allocate work area");
		return 0;
	}
	
	return 1;
}


/**
 * Decompress a jpeg image to our frame buffer
 *   returns 1 for success, 0 for failure
 */
int render_jpeg_image(uint8_t* fb, uint8_t* jpeg, uint32_t jpeg_length, uint16_t src_width, uint16_t dst_width)
{
	JDEC jdec;		  /* Decompression object */
	JRESULT res;	  /* Result code of TJpgDec API */
	IODEV devid;	  /* User defined device identifier */
	uint8_t scale;    /* Scale Factor: 0=1:1, 1=2:1, 2=4:1, 3=8:1 */
	
	// Prepare to decompress
	devid.jpic = jpeg;
	devid.jsize = jpeg_length;
	devid.joffset = 0;
	devid.fwidth = dst_width;
	devid.fbuf = fb;
	res = jd_prepare(&jdec, tjpgd_input, tjpgd_work, TJPGD_WORK_BUF_LEN, &devid);
	if (res != JDR_OK) {
		ESP_LOGE(TAG, "jd_prepare failed with %d", res);
		return 0;
	}
	
	// Compute scale factor
	if (src_width / dst_width == 1) scale = 0;
	else if (src_width / dst_width == 2) scale = 1;
	else if (src_width / dst_width == 4) scale = 2;
	else scale = 3;

	// Decompress
	res = jd_decomp(&jdec, tjpgd_output, scale);
	if (res != JDR_OK) {
		ESP_LOGE(TAG, "jd_decomp failed with %d", res);
		return 0;
	}
	
	return 1;
}


//
// Internal Routines
//
 
/**
 * TJpgDec input function - returns data to the compressor from the jpeg image buffer
 */
static unsigned int tjpgd_input(JDEC* jd, unsigned char* buff, unsigned int nbyte)
{
	IODEV * dev = (IODEV *)jd->device;
	
	nbyte = (unsigned int)dev->jsize - dev->joffset > nbyte ?
		nbyte : dev->jsize - dev->joffset;
	if (buff)
		memcpy(buff, dev->jpic + dev->joffset, nbyte);
	dev->joffset += nbyte;
	return nbyte;
}


/**
 * TjpgDec output function - updates the image buffer
 */
static unsigned int tjpgd_output(JDEC* jd, void* bitmap, JRECT* rect)
{
	IODEV *dev = (IODEV*)jd->device;
	uint8_t *src, *dst;
	uint16_t y, bws, bwd;
	
	/* Copy the decompressed RGB rectanglar to the frame buffer (assuming RGB565 cfg) */
	src = (uint8_t*)bitmap;
	dst = dev->fbuf + 2 * (rect->top * dev->fwidth + rect->left);  /* Left-top of destination rectangular */
	bws = 2 * (rect->right - rect->left + 1);	  /* Width of source rectangular [byte] */
	bwd = 2 * dev->fwidth;						 /* Width of frame buffer [byte] */
	for (y = rect->top; y <= rect->bottom; y++) {
		memcpy(dst, src, bws);   /* Copy a line */
		src += bws; dst += bwd;  /* Next line */
	}

	return 1;	/* Continue to decompress */
}
