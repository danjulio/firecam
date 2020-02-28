/**
 * @file disp_spi.h
 *
 * Official lvgl esp32 port, ported to firecam by Dan Julio
 */

#ifndef DISP_SPI_H
#define DISP_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <stdbool.h>
#include "system_config.h"

/*********************
 *      DEFINES
 *********************/

#define DISP_SPI_HOST LCD_SPI_HOST
#define DISP_SPI_FREQ_HZ LCD_SPI_FREQ_HZ
#define DISP_SPI_CS LCD_CSN_IO


/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void disp_spi_init(void);
void disp_spi_send_data(uint8_t * data, uint16_t length);
void disp_spi_send_colors(uint8_t * data, uint16_t length);
bool disp_spi_is_busy(void);

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*DISP_SPI_H*/
