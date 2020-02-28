/**
 * @file tp_spi.h
 *
 * Official lvgl esp32 port, ported to firecam by Dan Julio
 */

#ifndef TP_SPI_H
#define TP_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include "system_config.h"

/*********************
 *      DEFINES
 *********************/

#define ENABLE_TOUCH_INPUT  1

#define TP_SPI_HOST         TS_SPI_HOST
#define TP_SPI_FREQ_HZ      TS_SPI_FREQ_HZ


/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void tp_spi_init(void);
uint8_t tp_spi_xchg(uint8_t data_send);

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*TP_SPI_H*/
