/**
 * @file XPT2046.h
 *
 * Official lvgl esp32 port, ported to firecam by Dan Julio
 */

#ifndef XPT2046_H
#define XPT2046_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <stdint.h>
#include <stdbool.h>
#include "lvgl/lvgl.h"
#include "system_config.h"

/*********************
 *      DEFINES
 *********************/
#define XPT2046_IRQ         TS_IRQ_IO
#define XPT2046_CS          TS_CSN_IO

#define XPT2046_AVG         4
#define XPT2046_X_MIN       LVGL_TOUCH_X_MIN
#define XPT2046_Y_MIN       LVGL_TOUCH_Y_MIN
#define XPT2046_X_MAX       LVGL_TOUCH_X_MAX
#define XPT2046_Y_MAX       LVGL_TOUCH_Y_MAX
#ifdef LVGL_TOUCH_INVERT_X
#define XPT2046_X_INV       1
#endif
#ifdef LVGL_TOUCH_INVERT_Y
#define XPT2046_Y_INV       1
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void xpt2046_init(void);
bool xpt2046_read(lv_indev_drv_t * drv, lv_indev_data_t * data);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* XPT2046_H */
