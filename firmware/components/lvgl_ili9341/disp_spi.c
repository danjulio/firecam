/**
 * @file disp_spi.c
 *
 * Official lvgl esp32 port, ported to firecam by Dan Julio
 */

/*********************
 *      INCLUDES
 *********************/
#include "disp_spi.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/task.h"
#include "lvgl/lvgl.h"
#include "ili9341.h"
#include "tp_spi.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void IRAM_ATTR spi_ready (spi_transaction_t *trans);

/**********************
 *  STATIC VARIABLES
 **********************/
static spi_device_handle_t spi;
static volatile bool spi_trans_in_progress;
static volatile bool spi_color_sent;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void disp_spi_init(void)
{

    esp_err_t ret;

    spi_device_interface_config_t devcfg={
            .clock_speed_hz=DISP_SPI_FREQ_HZ,
            .mode=0,                               //SPI mode 0
            .spics_io_num=LCD_CSN_IO,              //CS pin
            .queue_size=1,
            .pre_cb=NULL,
            .post_cb=spi_ready,
            .flags = SPI_DEVICE_HALFDUPLEX
    };

    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(DISP_SPI_HOST, &devcfg, &spi);
    assert(ret==ESP_OK);

    spi_trans_in_progress = false;
}

void disp_spi_send_data(uint8_t * data, uint16_t length)
{
    if (length == 0) return;           //no need to send anything

    while (spi_trans_in_progress) {};

    spi_transaction_t t = {
        .length = length * 8, // transaction length is in bits
        .tx_buffer = data
    };

    spi_trans_in_progress = true;
    spi_color_sent = false;             //Mark the "lv_flush_ready" NOT needs to be called in "spi_ready"
    /************************************************************************************/
    /* Note: queued transactions cause a panic when this task yields and I can't figure */
    /* it out.  Disabling queuing gets rid of the panic at some performance hit.        */
    /************************************************************************************/
    //spi_device_queue_trans(spi, &t, portMAX_DELAY);
    spi_device_transmit(spi, &t);

}

void disp_spi_send_colors(uint8_t * data, uint16_t length)
{
    if (length == 0) return;           //no need to send anything

    while (spi_trans_in_progress) {};

    spi_transaction_t t = {
        .length = length * 8, // transaction length is in bits
        .tx_buffer = data
    };
    
    spi_trans_in_progress = true;
    spi_color_sent = true;              //Mark the "lv_flush_ready" needs to be called in "spi_ready"
    /************************************************************************************/
    /* Note: queued transactions cause a panic when this task yields and I can't figure */
    /* it out.  Disabling queuing gets rid of the panic at some performance hit.        */
    /************************************************************************************/
    //spi_device_queue_trans(spi, &t, portMAX_DELAY);
    spi_device_transmit(spi, &t);
}


bool disp_spi_is_busy(void)
{
    return spi_trans_in_progress;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void IRAM_ATTR spi_ready (spi_transaction_t *trans)
{
    spi_trans_in_progress = false;

    lv_disp_t * disp = lv_refr_get_disp_refreshing();
    if(spi_color_sent) lv_disp_flush_ready(&disp->driver);
}
