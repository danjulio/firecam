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
#ifndef VOSPI_H
#define VOSPI_H

#include <stdint.h>


//
// VoSPI Constants
//

// LEP_FRAME_USEC is the per-frame period from the Lepton (interrupt rate) 
#define LEP_FRAME_USEC 9450
// LEP_MAX_FRAME_XFER_WAIT_USEC specifies the maximum time we should wait in
// vospi_transfer_segment() to read a valid frame.  It should be LEP_FRAME_USEC -
// (maximum ISR latency + transfer_packet() code path overhead)
// than LEP_FRAME_USEC -  maximum ISR latency)
#define LEP_MAX_FRAME_XFER_WAIT_USEC 9250

#define LEP_WIDTH      160
#define LEP_HEIGHT     120
#define LEP_NUM_PIXELS (LEP_WIDTH*LEP_HEIGHT)
#define LEP_PKT_LENGTH 164

/* Lepton frame error return */
enum LeptonReadError {
  NONE, DISCARD, SEGMENT_ERROR, ROW_ERROR, SEGMENT_INVALID
};



//
// VoSPI API
//
int vospi_init();
bool vospi_transfer_segment(uint64_t vsyncDetectedUsec);
void vospi_get_frame(uint16_t* bufP);

#endif /* VOSPI_H */
