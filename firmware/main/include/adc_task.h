/*
 * ADC Task
 *
 * Periodically updates operating state measured by the ADC.  Detects shutdown conditions
 * (power button long-press and critical battery) and notifies the application task. 
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
#ifndef ADC_TASK_H
#define ADC_TASK_H


//
// ADC Task Constants
//

// ADC Update interval - should be longer than the time it takes the ADC
// chip to sample all inputs.
#define ADC_TASK_SAMPLE_MSEC 75

// Power-button long-press detection period - rounded to a multiple of the sample period
#define ADC_TASK_PWROFF_PRESS_MSEC 1500


//
// ADC Task API
//
void adc_task();
 
#endif /* ADC_TASK_H */