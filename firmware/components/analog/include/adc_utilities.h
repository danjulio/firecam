/*
 * ADC related utilities
 *
 * Contains thread-safe functions to interpret ADC values for the ADC task
 * and an evaluation call to read ADC values into internal data structures 
 * for processing.
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
#ifndef ADC_UTILITIES_H
#define ADC_UTILITIES_H

#include <stdbool.h>
#include <stdint.h>

//
// ADC Utilities constants
//

// Averaging sample counts
#define NUM_BATT_SAMPLES      16
#define NUM_TEMP_SAMPLES      16
#define NUM_STAT_SAMPLES      8

// Multipliers accounting for the external resistor divider arrays on the ADC inputs
#define BATT_ADC_MULT         5.02
#define BTN_ADC_MULT          2.5
#define STAT1_ADC_MULT        2.5
#define STAT2_ADC_MULT        2.5

// Power Button sense threshold
//   1. The button sense divider is connected to V+ (based on battery voltage or USB voltage)
//      when the button is pressed.  This voltage should always be at least the minimum
//      operating battery voltage - 100 mV (estimated max loss through MCP73871 charger).
//      Estimate: (3.4 - 0.1) / BTN_ADC_MULT = 1.32V
//   2. The button sense divider is connected to the PWR_HOLD IO pin through a schottky
//      diode.  Assuming a minimum drop of about 180 mV across the diode and a high level
//      on PWR_EN of 3.3V then this voltage should be about 3.12 volts / BTN_ADC_MULT =
//      1.25V max.
#define PWR_BTN_THRESHOLD     1.3

// Charger STAT1 threshold
//  1. The STAT1 sense divider is connected to 3.3V when STAT1 is not asserted.
//     3.3V / STAT1_ADC_MULT = 1.32V.
//  2. The STAT1 sense divider sense signal is pulled low through a schottky diode (~0.2V) 
//     when STAT1 is asserted.  Measured at around 0.4V.
#define STAT1_THRESHOLD       1.0

// Charger STAT2 threshold
//  1. The STAT2 sense divider is connected to 3.3V when STAT2 is not asserted.
//     3.3V / STAT1_ADC_MULT = 1.32V.
//  2. The STAT2 sense divider sense signal is pulled low by the MCP73871 OC output when
//     STAT2 is asserted.  Measured at around ~0.2V.
#define STAT2_THRESHOLD       0.8

// Battery state-of-charge curve
//   Based on 0.2C discharge rate on https://www.richtek.com/battery-management/en/designing-liion.html
//   This isn't particularly accurate...
#define BATT_75_THRESHOLD    3.9
#define BATT_50_THRESHOLD    3.72
#define BATT_25_THRESHOLD    3.66
#define BATT_0_THRESHOLD     3.6
#define BATT_CRIT_THRESHOLD  3.4


//
// Battery status data structures
//
enum BATT_STATE_t {
	BATT_100,
	BATT_75,
	BATT_50,
	BATT_25,
	BATT_0,
	BATT_CRIT
};

enum CHARGE_STATE_t {
	CHARGE_OFF,
	CHARGE_ON,
	CHARGE_FAULT
};

typedef struct {
	float batt_voltage;
	enum BATT_STATE_t batt_state;
	enum CHARGE_STATE_t charge_state;
} batt_status_t;



//
// ADC Utilities API
//
bool adc_init();
void adc_update();
void adc_get_batt(batt_status_t* bs);
float adc_get_temp();
bool adc_button_pressed();

#endif /* ADC_UTILITIES_H */