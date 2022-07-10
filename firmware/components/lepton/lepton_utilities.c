/*
 * Lepton related utilities
 *
 * Contains utility and access functions for the Lepton.
 *
 * Note: I noticed that on occasion, the first time some commands run on the lepton
 * will fail either silently or with an error response code.  The access routines in
 * this module attempt to detect and retry the commands if necessary.
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
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "vospi.h"
#include "system_config.h"



//
// Lepton Utilities variables
//
static const char* TAG = "lepton_utilities";



//
// Lepton Utilities API
//

bool lepton_init()
{
	cc_gain_mode_t gain_mode;
	gui_state_t gui_state;
	uint32_t rsp;
  
  	// Attempt to ping the Lepton to validate communication
  	// If this is successful, we assume further communication will be successful
  	rsp = cci_run_ping();
  	if (rsp != 0) {
  		ESP_LOGE(TAG, "Lepton communication failed (%d)", rsp);
  		return false;
	}
	
	// Configure Radiometry for TLinear enabled, auto-resolution
	cci_set_radiometry_enable_state(CCI_RADIOMETRY_ENABLED);
	rsp = cci_get_radiometry_enable_state();
	ESP_LOGI(TAG, "Lepton Radiometry = %d", rsp);
	if (rsp != CCI_RADIOMETRY_ENABLED) {
		// Make one more effort
		vTaskDelay(pdMS_TO_TICKS(10));
		ESP_LOGI(TAG, "Retry Set Lepton Radiometry");
		cci_set_radiometry_enable_state(CCI_RADIOMETRY_ENABLED);
		rsp = cci_get_radiometry_enable_state();
		ESP_LOGI(TAG, "Lepton Radiometry = %d", rsp);
		if (rsp != CCI_RADIOMETRY_ENABLED) {
			ESP_LOGE(TAG, "Lepton communication failed (%d)", rsp);
			return false;
		}
	}

	cci_set_radiometry_tlinear_enable_state(CCI_RADIOMETRY_TLINEAR_ENABLED);
	rsp = cci_get_radiometry_tlinear_enable_state();
	ESP_LOGI(TAG, "Lepton Radiometry TLinear = %d", rsp);
	if (rsp != CCI_RADIOMETRY_TLINEAR_ENABLED) {
		ESP_LOGE(TAG, "Lepton communication failed (%d)", rsp);
  		return false;
	}
	
	cci_set_radiometry_tlinear_auto_res(CCI_RADIOMETRY_AUTO_RES_ENABLED);
	rsp = cci_get_radiometry_tlinear_auto_res();
	ESP_LOGI(TAG, "Lepton Radiometry Auto Resolution = %d", rsp);
	if (rsp != CCI_RADIOMETRY_AUTO_RES_ENABLED) {
		ESP_LOGE(TAG, "Lepton communication failed (%d)", rsp);
  		return false;
	}
	
	// Disable AGC
	cci_set_agc_enable_state(CCI_AGC_DISABLED);
	rsp = cci_get_agc_enable_state();
	ESP_LOGI(TAG, "Lepton AGC = %d", rsp);
	if (rsp != CCI_AGC_DISABLED) {
		ESP_LOGE(TAG, "Lepton communication failed (%d)", rsp);
  		return false;
	}
	
	// Enable telemetry
	cci_set_telemetry_enable_state(CCI_TELEMETRY_ENABLED);
	rsp = cci_get_telemetry_enable_state();
	ESP_LOGI(TAG, "Lepton Telemetry = %d", rsp);
	if (rsp != CCI_TELEMETRY_ENABLED) {
		ESP_LOGE(TAG, "Lepton communication failed (%d)", rsp);
  		return false;
	}
	vospi_include_telem(true);
	
	// Set gain mode from persistent storage
	ps_get_gui_state(&gui_state);
	switch (gui_state.gain_mode) {
		case SYS_GAIN_HIGH:
			gain_mode = LEP_SYS_GAIN_MODE_HIGH;
			break;
		case SYS_GAIN_LOW:
			gain_mode = LEP_SYS_GAIN_MODE_LOW;
			break;
		default:
			gain_mode = LEP_SYS_GAIN_MODE_AUTO;
	}
	cci_set_gain_mode(gain_mode);
	rsp = cci_get_gain_mode();
	ESP_LOGI(TAG, "Lepton Gain Mode = %d", rsp);
	if (rsp != (uint32_t) gain_mode) {
		ESP_LOGE(TAG, "Lepton communication failed (%d)", rsp);
  		return false;
	}
  
	// Finally enable VSYNC on Lepton GPIO3
	cci_set_gpio_mode(LEP_OEM_GPIO_MODE_VSYNC);
	rsp = cci_get_gpio_mode();
	ESP_LOGI(TAG, "Lepton GPIO Mode = %d", rsp);
	if (rsp != LEP_OEM_GPIO_MODE_VSYNC) {
		ESP_LOGE(TAG, "Lepton communication failed (%d)", rsp);
  		return false;
	}
	
	return true;
}


bool lepton_check_reset_state()
{
	cc_gain_mode_t gain_mode;
	gui_state_t gui_state;
	uint32_t rsp;
  
  	// Attempt to ping the Lepton to validate communication
  	// If this is successful, we assume further communication will be successful
  	rsp = cci_run_ping();
  	if (rsp != 0) {
  		ESP_LOGE(TAG, "Lepton ping failed (%d)", rsp);
  		return false;
	}
	
	// Check Radiometry for TLinear enabled, auto-resolution
	rsp = cci_get_radiometry_enable_state();
	if (rsp != CCI_RADIOMETRY_ENABLED) {
		ESP_LOGE(TAG, "Reset Lepton Radiometry");
		cci_set_radiometry_enable_state(CCI_RADIOMETRY_ENABLED);
		rsp = cci_get_radiometry_enable_state();
		if (rsp != CCI_RADIOMETRY_ENABLED) {
			ESP_LOGE(TAG, "Reset Lepton Radiometry failed");
			return false;
		}
	}
	
	rsp = cci_get_radiometry_tlinear_enable_state();
	if (rsp != CCI_RADIOMETRY_TLINEAR_ENABLED) {
		ESP_LOGE(TAG, "Reset Lepton Radiometry TLinear");
		cci_set_radiometry_tlinear_enable_state(CCI_RADIOMETRY_TLINEAR_ENABLED);
  		rsp = cci_get_radiometry_tlinear_enable_state();
  		if (rsp != CCI_RADIOMETRY_TLINEAR_ENABLED) {
  			ESP_LOGE(TAG, "Reset Lepton Radiometry TLinear failed");
			return false;
  		}
	}
	
	rsp = cci_get_radiometry_tlinear_auto_res();
	if (rsp != CCI_RADIOMETRY_AUTO_RES_ENABLED) {
		ESP_LOGE(TAG, "Reset Lepton Radiometry Auto Resolution");
		cci_set_radiometry_tlinear_auto_res(CCI_RADIOMETRY_AUTO_RES_ENABLED);
		rsp = cci_get_radiometry_tlinear_auto_res();
		if (rsp != CCI_RADIOMETRY_AUTO_RES_ENABLED) {
			ESP_LOGE(TAG, "Reset Lepton Radiometry Auto Resolution failed");
			return false;
		}
	}
	
	// Check AGC disabled
	rsp = cci_get_agc_enable_state();
	if (rsp != CCI_AGC_DISABLED) {
		ESP_LOGE(TAG, "Reset Lepton AGC");
		cci_set_agc_enable_state(CCI_AGC_DISABLED);
		rsp = cci_get_agc_enable_state();
		if (rsp != CCI_AGC_DISABLED) {
			ESP_LOGE(TAG, "Reset Lepton AGC failed");
			return false;
		}
	}
	
	// Check telemetry enabled
	rsp = cci_get_telemetry_enable_state();
	if (rsp != CCI_TELEMETRY_ENABLED) {
		ESP_LOGE(TAG, "Reset Lepton Telemetry enable");
		cci_set_telemetry_enable_state(CCI_TELEMETRY_ENABLED);
		if (rsp != CCI_TELEMETRY_ENABLED) {
			ESP_LOGE(TAG, "Reset Lepton Telemetry enable failed");
			return false;
		}
	}
	
	// Check gain mode matches persistent storage
	ps_get_gui_state(&gui_state);
	switch (gui_state.gain_mode) {
		case SYS_GAIN_HIGH:
			gain_mode = LEP_SYS_GAIN_MODE_HIGH;
			break;
		case SYS_GAIN_LOW:
			gain_mode = LEP_SYS_GAIN_MODE_LOW;
			break;
		default:
			gain_mode = LEP_SYS_GAIN_MODE_AUTO;
	}
	rsp = cci_get_gain_mode();
	if (rsp != (uint32_t) gain_mode) {
		ESP_LOGE(TAG, "Reset Lepton Gain Mode");
		cci_set_gain_mode(gain_mode);
		rsp = cci_get_gain_mode();
		if (rsp != (uint32_t) gain_mode) {
			ESP_LOGE(TAG, "Reset Lepton Gain Mode failed");
			return false;
		}
	}
	
	// Make sure VSYNC is enabled on Lepton GPIO3
	rsp = cci_get_gpio_mode();
	if (rsp != LEP_OEM_GPIO_MODE_VSYNC) {
		ESP_LOGE(TAG, "Reset Lepton GPIO Mode");
		cci_set_gpio_mode(LEP_OEM_GPIO_MODE_VSYNC);
		rsp = cci_get_gpio_mode();
		if (rsp != LEP_OEM_GPIO_MODE_VSYNC) {
			ESP_LOGE(TAG, "Reset Lepton GPIO Mode failed");
			return false;
		}
	}
	
	return true;
}


void lepton_agc(bool en)
{
	if (en) {
		cci_set_radiometry_enable_state(CCI_RADIOMETRY_DISABLED);
		cci_set_agc_enable_state(CCI_AGC_ENABLED);
	} else {
		cci_set_radiometry_enable_state(CCI_RADIOMETRY_ENABLED);
		cci_set_radiometry_tlinear_enable_state(CCI_RADIOMETRY_TLINEAR_ENABLED);
		cci_set_agc_enable_state(CCI_AGC_DISABLED);
	}
}


void lepton_ffc()
{
	cci_run_ffc();
}


void lepton_gain_mode(uint8_t mode)
{
	cc_gain_mode_t gain_mode;
	
	switch (mode) {
		case SYS_GAIN_HIGH:
			gain_mode = LEP_SYS_GAIN_MODE_HIGH;
			break;
		case SYS_GAIN_LOW:
			gain_mode = LEP_SYS_GAIN_MODE_LOW;
			break;
		default:
			gain_mode = LEP_SYS_GAIN_MODE_AUTO;
	}
	cci_set_gain_mode(gain_mode);
}


void lepton_spotmeter(uint16_t r1, uint16_t c1, uint16_t r2, uint16_t c2)
{
	cci_set_radiometry_spotmeter(r1, c1, r2, c2);
}


void lepton_emissivity(uint16_t e)
{
	cci_rad_flux_linear_params_t set_flux_values;
	
	// Scale percentage e into Lepton scene emissivity values (1-100% -> 82-8192)
	if (e < 1) e = 1;
	if (e > 100) e = 100;
	set_flux_values.sceneEmissivity = e * 8192 / 100;
	
	// Set default (no lens) values for the remaining parameters
	set_flux_values.TBkgK      = 29515;
	set_flux_values.tauWindow  = 8192;
	set_flux_values.TWindowK   = 29515;
	set_flux_values.tauAtm     = 8192;
	set_flux_values.TAtmK      = 29515;
	set_flux_values.reflWindow = 0;
	set_flux_values.TReflK     = 29515;
	
	cci_set_radiometry_flux_linear_params(&set_flux_values);
}


uint32_t lepton_get_tel_status(uint16_t* tel_buf)
{
	return (tel_buf[LEP_TEL_STATUS_HIGH] << 16) | tel_buf[LEP_TEL_STATUS_LOW];
}


/**
 * Convert a temperature reading from the lepton (in units of K * 100) to C
 */
float lepton_kelvin_to_C(uint16_t k, float lep_res)
{
	return (((float) k) * lep_res) - 273.15;
}
