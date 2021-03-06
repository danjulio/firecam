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
#include "palettes.h"
#include "arctic.h"
#include "double_rainbow.h"
#include "fusion.h"
#include "gray.h"
#include "ironblack.h"
#include "rainbow.h"
#include "esp_system.h"
#include "esp_log.h"
#include <string.h>


//
// Palette Variables
//
static const char* TAG = "palettes";

static palette_t palettes[PALETTE_COUNT] = {
	{
		.name = "Grayscale",
		.map_ptr = &gray_palette_map
	},
	{
		.name = "Fusion",
		.map_ptr = &fusion_palette_map
	},
	{
		.name = "Rainbow",
		.map_ptr = &rainbow_palette_map
	},
	{
		.name = "Rainbow2",
		.map_ptr = &double_rainbow_palette_map
	},
	{
		.name = "Ironblack",
		.map_ptr = &ironblack_palette_map
	},
	{
		.name = "Arctic",
		.map_ptr = &arctic_palette_map
	},
};



//
// Palette variables
//
uint16_t palette16[256];                 // Current palette for fast lookup
int cur_palette;


//
// Palette API
//
void set_palette(int n)
{
	int i;
	
	if ((n >= 0) && (n < PALETTE_COUNT)) {
		ESP_LOGI(TAG, "Loading %s color map", palettes[n].name);
		
		for (i=0; i<256; i++) {
			palette16[i] = RGB_TO_16BIT_SWAP(
				(*(palettes[n].map_ptr))[i][0],
				(*(palettes[n].map_ptr))[i][1],
				(*(palettes[n].map_ptr))[i][2]
			);
		}
		cur_palette = n;
	}
}


char* get_palette_name(int n)
{
	return palettes[n].name;
}


int get_palette_by_name(const char* name)
{
	int i;
	
	for (i=0; i<PALETTE_COUNT; i++) {
		if (strcmp(name, palettes[i].name) == 0) {
			return i;
		}
	}
	
	return -1;
}
