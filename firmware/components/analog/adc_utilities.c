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
#include "adc_utilities.h"
#include "adc128d818.h"
#include "system_config.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <math.h>



//
// ADC Utilities private constants
//

// ADC valid channels
#define ADC_CH_DIS_MASK       0xC0 /* Enable channels 0-5 */
#define ADC_NUM_VALID_CH      6

// ADC register offsets for system voltages
#define ADC_CH_BTN_REG        (ADC_CH_BASE_REG + 0)
#define ADC_CH_STAT2_REG      (ADC_CH_BASE_REG + 1)
#define ADC_CH_BATT_REG       (ADC_CH_BASE_REG + 2)
#define ADC_CH_PWREN_REG      (ADC_CH_BASE_REG + 3)
#define ADC_CH_STAT1_REG      (ADC_CH_BASE_REG + 4)
#define ADC_CH_T_REG          (ADC_CH_BASE_REG + 5)

// cur_adc_vals indexes
#define ADC_CUR_BTN_I         0
#define ADC_CUR_STAT2_I       1
#define ADC_CUR_BATT_I        2
#define ADC_CUR_PWREN_I       3
#define ADC_CUR_STAT1_I       4
#define ADC_CUR_T_I           5

// External voltage reference value
#define ADC_EXT_VREF_V        2.048

// Uncomment to use the LM36 temperature sensor instead of the LMT86
#define ADC_USE_LM36

// Uncomment for diagnostic printing
//#define ADC_DEBUG


//
// ADC Utilities Variables
//

static const char* TAG = "time_utilities";

// Averaged state values and associated access mutexes
static batt_status_t batt_status;
static SemaphoreHandle_t batt_status_mutex;

static float temp_value;
static SemaphoreHandle_t temp_value_mutex;

static bool power_button_pressed;
static SemaphoreHandle_t power_button_mutex;


// Averaging arrays
static uint16_t batt_average_array[NUM_BATT_SAMPLES];
static int batt_average_index;

static uint16_t temp_average_array[NUM_TEMP_SAMPLES];
static int temp_average_index;

static uint16_t stat1_average_array[NUM_STAT_SAMPLES];
static uint16_t stat2_average_array[NUM_STAT_SAMPLES];
static int stat_average_index;

// Power button state
static bool power_button_cur;         // Current button reading
static bool power_button_prev;        // Previous button reading

// ADC read array
static uint16_t cur_adc_vals[ADC_NUM_VALID_CH];

#ifdef ADC_DEBUG
float temp_v;
#endif

//
// ADC Utilities Forward Declarations for internal functions
//
void adc_read_channels();
bool adc_val_greater_than_threshold(uint16_t adc_val, float threshold);
uint16_t compute_average(uint16_t* buf, int n);
void update_battery_info();
void update_temp_info();
void update_button_info();
float adc_2_volts(uint16_t adc_val);
float adc_2_temp(uint16_t adc_val);



//
// ADC Utilities API
//

/**
 * Initialize the ADC, enabling appropriate channels and configuring continuous sample
 * mode.
 */
bool adc_init()
{
	bool ready = false;
	uint8_t u8;
	
	// Create our mutexes
	batt_status_mutex = xSemaphoreCreateMutex();
	temp_value_mutex = xSemaphoreCreateMutex();
	power_button_mutex = xSemaphoreCreateMutex();
	
	// Make sure the ADC isn't still in its power-up phase
	// Note: this is the only place we check the return value from the adc access
	//       functions.  If this works then we assume I2C is good.
	while (!ready) {
		if (!adc_read_byte(ADC_BUSY_REG, &u8)) {
			ESP_LOGE(TAG, "init_adc BUSY_REG poll failed");
			return false;
		}
		ready = (u8 & ADC_PWRUP_BUSY_MASK) == 0;
	}
	
	// Verify we can communicate with the ADC
	u8 = ~ADC_MANUF_ID;
	adc_read_byte(ADC_MANUF_ID_REG, &u8);
	if (u8 != ADC_MANUF_ID) {
		ESP_LOGE(TAG, "Read ADC MANUF_ID_REG failed.  Got %2x  Exp %2x", u8, ADC_MANUF_ID);
		return false;
	}
	u8 = ~ADC_REV_ID;
	adc_read_byte(ADC_REV_ID_REG, &u8);
	if (u8 != ADC_REV_ID) {
		ESP_LOGE(TAG, "Read ADC REV_ID_REG failed.  Got %2x  Exp %2x", u8, ADC_REV_ID);
		return false;
	}
	
	// Initialize the ADC for continuous scanning
	adc_write_byte(ADC_CFG_REG, 0x00);  /* Disable ADC for configuration */
	adc_write_byte(ADC_CONV_REG, ADC_CONV_EN);
	adc_write_byte(ADC_CH_DIS_REG, ADC_CH_DIS_MASK);
	adc_write_byte(ADC_ACFG_REG, ADC_ACFG_EXT_REF_MASK | ADC_ACFG_MODE1_MASK);
	adc_write_byte(ADC_CFG_REG, ADC_CFG_START_MASK);  /* Enable ADC after configuration */
	
	// Wait to allow it to make an initial set of measurements
	vTaskDelay(pdMS_TO_TICKS(100));
	
	// Read active ADC channels
	adc_read_channels();
	
	// Initialize our averaging arrays
	for (u8=0; u8<NUM_BATT_SAMPLES; u8++) {
		batt_average_array[u8] = cur_adc_vals[ADC_CUR_BATT_I];
	}
	batt_average_index = 0;
	for (u8=0; u8<NUM_TEMP_SAMPLES; u8++) {
		temp_average_array[u8] = cur_adc_vals[ADC_CUR_T_I];
	}
	temp_average_index = 0;
	for (u8=0; u8<NUM_STAT_SAMPLES; u8++) {
		stat1_average_array[u8] = cur_adc_vals[ADC_CUR_STAT1_I];
		stat2_average_array[u8] = cur_adc_vals[ADC_CUR_STAT2_I];
	}
	stat_average_index = 0;
	
	// Assume power button is depressed from startup here
	power_button_prev = true;
	
	// Compute the initial system values
	update_battery_info();
	update_temp_info();
	update_button_info();
	
	return true;
}


/**
 * Read values from the ADC and update the internal values.
 * This function should be called at intervals greater than ADC_NUM_VALID_CH * 12.2 mSec
 * to allow the ADC's continuous mode to sample all enabled inputs.
 */
void adc_update()
{
	// Get current data from the ADC
	adc_read_channels();
	
	// Push the latest value into our averaging arrays
	batt_average_array[batt_average_index++] = cur_adc_vals[ADC_CUR_BATT_I];
	if (batt_average_index >= NUM_BATT_SAMPLES) batt_average_index = 0;
	
	temp_average_array[temp_average_index++] = cur_adc_vals[ADC_CUR_T_I];
	if (temp_average_index >= NUM_TEMP_SAMPLES) temp_average_index = 0;
	
	stat1_average_array[stat_average_index] = cur_adc_vals[ADC_CUR_STAT1_I];
	stat2_average_array[stat_average_index++] = cur_adc_vals[ADC_CUR_STAT2_I];
	if (stat_average_index >= NUM_STAT_SAMPLES) stat_average_index = 0;
	
	// Update system values
	update_battery_info();
	update_temp_info();
	update_button_info();
	
#ifdef ADC_DEBUG
	static int count = 0;
	if ((++count % 15) == 0) {
		printf("bv = %1.2f bs = %d cs = %d t = %2.2f (%1.3f) btn = %d\n", 
			   batt_status.batt_voltage, batt_status.batt_state, batt_status.charge_state,
			   temp_value, temp_v, power_button_pressed);
	}
#endif
}


/**
 * Get the current battery voltage and charge status
 */
void adc_get_batt(batt_status_t* bs)
{
	xSemaphoreTake(batt_status_mutex, portMAX_DELAY);
	bs->batt_voltage = batt_status.batt_voltage;
	bs->batt_state = batt_status.batt_state;
	bs->charge_state = batt_status.charge_state;
	xSemaphoreGive(batt_status_mutex);
}


/**
 * Get the current temp sensor value in degrees C
 */
float adc_get_temp()
{
	float f;
	
	xSemaphoreTake(temp_value_mutex, portMAX_DELAY);
	f = temp_value;
	xSemaphoreGive(temp_value_mutex);
	return f;
}


/**
 * Return the status of the power button
 */
bool adc_button_pressed()
{
	bool b;
	
	xSemaphoreTake(power_button_mutex, portMAX_DELAY);
	b = power_button_pressed;
	xSemaphoreGive(power_button_mutex);
	return b;
}



//
// ADC Utilities internal functions
//

/**
 * Read active ADC channels into a local array
 */
void adc_read_channels()
{
	int i;
	
	for (i=0; i<ADC_NUM_VALID_CH; i++) {
		adc_read_word(ADC_CH_BASE_REG + i, &cur_adc_vals[i]);
	}
}


/**
 * Convert an ADC value to voltage and compare against a voltage threshold
 */
bool adc_val_greater_than_threshold(uint16_t adc_val, float threshold)
{
	float v;
	
	v = adc_2_volts(adc_val);
	return (v >= threshold);
}


/**
 * Compute a rounded average
 */
uint16_t compute_average(uint16_t* buf, int n)
{
	int i;
	uint16_t avg;
	uint32_t sum = 0;
	
	// Sum values
	for (i=0; i<n; i++) sum += *(buf + i);
	
	// Compute the integer portion of the average
	avg = sum / n;
	
	// Round up if necessary
	if ((sum % n) >= (n / 2)) avg = avg + 1;
	
	return avg;
}


/**
 * Compute the current battery voltage average and update our battery info data structure
 */
void update_battery_info()
{
	bool s1, s2;
	float bv;
	uint16_t avg_adc_val;
	enum BATT_STATE_t bs;
	enum CHARGE_STATE_t cs;
	
	// Compute the battery voltage
	avg_adc_val = compute_average(batt_average_array, NUM_BATT_SAMPLES);
	bv = adc_2_volts(avg_adc_val) * BATT_ADC_MULT;
	
	// Set the battery state
	if (bv <= BATT_CRIT_THRESHOLD) bs = BATT_CRIT;
	else if (bv <= BATT_0_THRESHOLD) bs = BATT_0;
	else if (bv <= BATT_25_THRESHOLD) bs = BATT_25;
	else if (bv <= BATT_50_THRESHOLD) bs = BATT_50;
	else if (bv <= BATT_75_THRESHOLD) bs = BATT_75;
	else bs = BATT_100;
	
	// Compute the charger status flags
	avg_adc_val = compute_average(stat1_average_array, NUM_STAT_SAMPLES);
	s1 = adc_val_greater_than_threshold(avg_adc_val, STAT1_THRESHOLD);
	avg_adc_val = compute_average(stat2_average_array, NUM_STAT_SAMPLES);
	s2 = adc_val_greater_than_threshold(avg_adc_val, STAT2_THRESHOLD);
	
	// Convert the flags to charge state
	//   From the MCP73871 spec, Table 5-1 (simplified without PG)
	//      Charge Cycle State       STAT1    STAT2
	//      ----------------------------------------
	//      Not Charging               H        H
	//      Charging                   L        H
	//      Fault                      L        L
	//      Charge Complete            H        L  (treat this as Not Charging)
	if (!s1) {
		if (s2) {
			cs = CHARGE_ON;
		} else {
			cs = CHARGE_FAULT;
		}
	} else {
		cs = CHARGE_OFF;
	}
	
	// Finally, atomically update our value
	xSemaphoreTake(batt_status_mutex, portMAX_DELAY);
	batt_status.batt_voltage = bv;
	batt_status.batt_state = bs;
	batt_status.charge_state = cs;
	xSemaphoreGive(batt_status_mutex);
}


void update_temp_info()
{
	float t;
	uint16_t avg_adc_val;
	
	// Compute the temperature
	avg_adc_val = compute_average(temp_average_array, NUM_TEMP_SAMPLES);
#ifdef ADC_DEBUG
	temp_v = adc_2_volts(avg_adc_val);
#endif
	t = adc_2_temp(avg_adc_val);
	
	// Atomically update our value
	xSemaphoreTake(temp_value_mutex, portMAX_DELAY);
	temp_value = t;
	xSemaphoreGive(temp_value_mutex);
}


/**
 * Compute the power button pressed state and update our value
 */
void update_button_info()
{
	// Compute current pressed state
	power_button_cur = adc_val_greater_than_threshold(cur_adc_vals[ADC_CUR_BTN_I], PWR_BTN_THRESHOLD);
	
	// Atomically update the value
	xSemaphoreTake(power_button_mutex, portMAX_DELAY);
	power_button_pressed = power_button_cur && power_button_prev;
	xSemaphoreGive(power_button_mutex);
	
	// Save the current for next time
	power_button_prev = power_button_cur;
}


/**
 * Convert a 12-bit ADC value to the voltage at the ADC input pin
 */
float adc_2_volts(uint16_t adc_val)
{
	return ((ADC_EXT_VREF_V * adc_val) / 4095.0);
}



#ifdef ADC_USE_LM36
/**
 * Convert a 12-bit ADC value to degrees C according to the conversion specification
 * for the LM36 temperature sensor
 */
float adc_2_temp(uint16_t adc_val)
{
	float t;
	float mv;
	
	mv = (double) adc_2_volts(adc_val) * 1000.0;
	
	// LM36 offset at 0C = 500mV; scale factor = 10mV/C
	t = (mv - 500.0) / 10.0;
	
	return t;
}
#else
/**
 * Convert a 12-bit ADC value to degrees C according to the conversion specification 
 * for the LMT86 thermistor
 */
float adc_2_temp(uint16_t adc_val)
{
	double t;
	double mv;
	
	// Implementation of parabolic curve fit from LMT86 datasheet, equation 2 (page 10)
	mv = (double) adc_2_volts(adc_val) * 1000.0;
	
	t = sqrt(pow(-10.888, 2) + (4 * 0.00347 * (1777.3 - mv)));
	t = ((10.888 - t) / (2 * -0.00347)) + 30.0;
	
	return (float) t;	
}
#endif
