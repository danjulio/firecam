/*
 * Lepton VoSPI Module
 *
 * Contains the functions to get frames from a Lepton 3.5 via its SPI port.
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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "system_config.h"
#include "vospi.h"



//
// VoSPI Variables
//

// Logging support
static const char* TAG = "vospi";

// SPI Interface
static spi_device_handle_t spi;
static spi_transaction_t lep_spi_trans;

// Pointer to allocated array to store one Lepton packet (DMA capable)
static uint8_t* lepPacketP;

// Lepton Frame buffer (16-bit values)
static uint16_t lepBuffer[LEP_NUM_PIXELS];

// Processing State
static int curSegment = 1;
static bool validSegmentRegion = false;




//
// VoSPI Forward Declarations for internal functions
//
static bool transfer_packet(uint8_t* line, uint8_t* seg);
static void copy_packet_to_buffer(uint8_t line);



//
// VoSPI API
//

/**
 * Initialise the VoSPI interface.
 */
int vospi_init()
{
	esp_err_t ret;
  
	spi_device_interface_config_t devcfg = {
		.command_bits = 0,
		.address_bits = 0,
		.clock_speed_hz = LEP_SPI_FREQ_HZ,
		.mode = 3,
		.spics_io_num = LEP_CSN_IO,
		.queue_size = 1,
		.flags = SPI_DEVICE_HALFDUPLEX,
		.cs_ena_pretrans = 10
	};

	if ((ret=spi_bus_add_device(LEP_SPI_HOST, &devcfg, &spi)) != ESP_OK) {
		ESP_LOGE(TAG, "failed to add lepton spi device");
	} else {
		// Allocate DMA capable memory for the lepton packet
		lepPacketP = (uint8_t*) heap_caps_malloc(LEP_PKT_LENGTH, MALLOC_CAP_DMA);
		if (lepPacketP != NULL) {
			ret = ESP_OK;
		} else {
			ESP_LOGE(TAG, "failed to allocate lepton DMA packet buffer");
			ret = ESP_FAIL;
		}
	}

	return ret;
}


/**
 * Attempt to read a complete segment from the Lepton
 *  - Data loaded into lepBuffer
 *  - Returns true when last successful segment read, false otherwise
 */
bool vospi_transfer_segment(uint64_t vsyncDetectedUsec)
{
	uint8_t line, prevLine;
	uint8_t segment;
	bool done = false;
	bool beforeValidData = true;
	bool success = false;

	prevLine = 255;

	while (!done) {
		if (transfer_packet(&line, &segment)) {
			// Saw a valid packet
			if (line == prevLine) {
			// This is garbage data since line numbers should always increment
			done = true;
			} else {
				// Check for termination or completion conditions
				if (line == 20) {
					// Check segment
					if (!validSegmentRegion) {
						// Look for start of valid segment data
						if (segment == 1) {
							beforeValidData = false;
							validSegmentRegion = true;
						}
					} else if ((segment < 2) || (segment > 4)) {
						// Hold/Reset in starting position (always collecting in segment 1 buffer locations)
						validSegmentRegion = false;  // In case it was set
						curSegment = 1;
					}
				}
        
				// Copy the data to the lepton frame buffer
				//  - beforeValidData is used to collect data before we know if the current segment (1) is valid
				//  - then we use validSegmentRegion for remaining data once we know we're seeing valid data
				if ((beforeValidData || validSegmentRegion) && (line <= 59)) {
					copy_packet_to_buffer(line);
				}
	
				if (line == 59) {
					// Saw a complete segment, move to next segment or complete frame aquisition if possible
					if (validSegmentRegion) {
						if (curSegment < 4) {
							// Setup to get next segment
							curSegment++;
						} else {
							// Got frame
							success = true;

							// Setup to get the next frame
							curSegment = 1;
							validSegmentRegion = false;
						}
					}
					done = true;
				}
			}
			prevLine = line;
		} else if ((esp_timer_get_time() - vsyncDetectedUsec) > LEP_MAX_FRAME_XFER_WAIT_USEC) {
			// Did not see a valid packet within this segment interval
      		done = true;
    	}
	}
	
  	return success;
}


/**
 * Load the shared buffer from our buffer for another task
 */
void vospi_get_frame(uint16_t* bufP)
{
	uint16_t* lptr = &lepBuffer[0];

	// Load into the c_frame
	while (lptr < &lepBuffer[LEP_NUM_PIXELS]) {
		*bufP++ = *lptr++;
	}
}



//
// VoSPI Forward Declarations for internal functions
//

/**
 * Attempt to read one packet from the lepton
 *  - Return false for discard packets
 *  - Return true otherwise
 *    - line contains the packet line number for all valid packets
 *    - seg contains the packet segment number if the line number is 20
 */
bool transfer_packet(uint8_t* line, uint8_t* seg)
{
	bool valid = false;
	esp_err_t ret;

	// *seg will be set if possible
	*seg = 0;

	// Setup our SPI transaction
	memset(&lep_spi_trans, 0, sizeof(spi_transaction_t));
	lep_spi_trans.tx_buffer = NULL;
	lep_spi_trans.rx_buffer = lepPacketP;
	lep_spi_trans.rxlength = LEP_PKT_LENGTH*8;

	/************************************************************************************/
    /* Note: queued transactions cause a panic when a task yields and I can't figure    */
    /* it out.  Disabling queuing gets rid of the panic at some performance hit.        */
    /************************************************************************************/
	// Get a packet using the interrupt method and DMA engine to free the CPU some
	//ret = spi_device_polling_transmit(spi, &lep_spi_trans);
	ret = spi_device_transmit(spi, &lep_spi_trans);
	ESP_ERROR_CHECK(ret);
  
	// Repeat as long as the frame is not valid, equals sync
	if ((*lepPacketP & 0x0F) == 0x0F) {
		valid = false;
	} else {
		*line = *(lepPacketP + 1);

		// Get segment when possible
		if (*line == 20) {
			*seg = (*lepPacketP >> 4);
		}

		valid = true;
	}

	return(valid);
}


/**
 * Copy the lepton packet to the raw lepton frame
 *   - line specifies packet line number
 */
void copy_packet_to_buffer(uint8_t line)
{
	uint8_t* lepPopPtr = lepPacketP + 4;
	uint16_t* acqPushPtr = &lepBuffer[((curSegment-1) * 30 * LEP_WIDTH) + (line * (LEP_WIDTH/2))];
	uint16_t t;

	while (lepPopPtr <= (lepPacketP + (LEP_PKT_LENGTH-1))) {
		t = *lepPopPtr++ << 8;
		t |= *lepPopPtr++;
		*acqPushPtr++ = t;
	}
}

