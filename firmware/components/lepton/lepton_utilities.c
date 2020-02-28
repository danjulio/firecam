/*
 * Lepton related utilities
 *
 * Contains functions to initialize the Lepton.
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
#include "lepton_utilities.h"
#include "cci.h"
#include "i2c.h"
#include "esp_system.h"
#include "esp_log.h"



//
// Lepton Utilities variables
//
static const char* TAG = "lepton_utilities";



//
// Lepton Utilities API
//

bool lepton_init()
{
	uint32_t rsp;
  
  	// Attempt to ping the Lepton to validate communication
  	// Ff this is successful, we assume further communication will be successful
  	rsp = cci_run_ping();
  	if (rsp != 0) {
  		ESP_LOGE(TAG, "Lepton communication failed (%d)", rsp);
  		return false;
	}
	
	// Configure Radiometry for TLinear enabled
	cci_set_radiometry_enable_state(CCI_RADIOMETRY_ENABLED);
	rsp = cci_get_radiometry_enable_state();
	ESP_LOGI(TAG, "Lepton Radiometry = %d", rsp);
  
	while (rsp == 0) {
		ESP_LOGI(TAG, "Retry Set Lepton Radiometry");
		cci_set_radiometry_enable_state(CCI_RADIOMETRY_ENABLED);
		rsp = cci_get_radiometry_enable_state();
		ESP_LOGI(TAG, "Lepton Radiometry = %d", rsp);
	}

	cci_set_radiometry_tlinear_enable_state(CCI_RADIOMETRY_TLINEAR_ENABLED);
	rsp = cci_get_radiometry_tlinear_enable_state();
	ESP_LOGI(TAG, "Lepton Radiometry TLinear = %d", rsp);

	// Disable AGC
	cci_set_agc_enable_state(CCI_AGC_DISABLED);
	rsp = cci_get_agc_enable_state();
	ESP_LOGI(TAG, "Lepton AGC = %d", rsp);
  
	// Finally enable VSYNC on Lepton GPIO3
	cci_set_gpio_mode(LEP_OEM_GPIO_MODE_VSYNC);
	rsp = cci_get_gpio_mode();
	ESP_LOGI(TAG, "Lepton GPIO Mode = %d", rsp);
	
	return true;
}

