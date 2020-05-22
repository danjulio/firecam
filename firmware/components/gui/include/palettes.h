/*
 * Colormap structure for converting lepton 8-bit data to 16 bit RGB
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
#ifndef PALETTES_H
#define PALETTES_H

#include <stdint.h>


//
// Palette constants
//

// Available palettes
#define PALETTE_GRAY      0
#define PALETTE_FUSION    1
#define PALETTE_RAINBOW   2
#define PALETTE_RAINBOW2  3
#define PALETTE_IRONBLACK 4
#define PALETTE_ARCTIC    5
#define PALETTE_COUNT     6

typedef const uint8_t palette_map_t[256][3];

typedef struct {
  char name[32];
  palette_map_t* map_ptr;
} palette_t;



//
// Palette macros
//
#define PALLETTE_LOOKUP(n) palette16[n];

// Macro to convert 24-bit color to 16 bit RGB565
#define RGB_TO_16BIT(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

// Macro to convert 24-bit color to 16 bit byte-swapped RGB565 for lv_img
#define RGB_TO_16BIT_SWAP(r, g, b) (((b & 0xF8) << 5) | (r & 0xF8) | ((g & 0xE0) >> 5) | ((g & 0x1C) << 11))

//
// Palette extern variables
//
extern uint16_t palette16[256];                 // Current palette for fast lookup
extern int cur_palette;


//
// Palette API
//
void set_palette(int n);
char* get_palette_name(int n);
int get_palette_by_name(const char* name);

#endif
