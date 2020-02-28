/**
 * @file lv_templ.h
 *
 * Official lvgl esp32 port, ported to firecam by Dan Julio
 */

#ifndef ILI9341_H
#define ILI9341_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>

#include "lvgl/lvgl.h"
#include "system_config.h"

/*********************
 *      DEFINES
 *********************/
#define ILI9341_DC             LCD_DC_IO

// if text/images are backwards, try setting this to 1
//#define ILI9341_INVERT_DISPLAY 1

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void ili9341_init(void);
void ili9341_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map);

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*ILI9341_H*/
