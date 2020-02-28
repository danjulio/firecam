/**
 * @file tp_spi.c
 *
 * Official lvgl esp32 port, ported to firecam by Dan Julio
 */

/*********************
 *      INCLUDES
 *********************/
#include "tp_spi.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <string.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/


/**********************
 *  STATIC PROTOTYPES
 **********************/
static spi_device_handle_t spi;

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void tp_spi_init(void)
{

	esp_err_t ret;

	spi_device_interface_config_t devcfg={
		.clock_speed_hz=TP_SPI_FREQ_HZ,
		.mode=0,                       //SPI mode 0
		.spics_io_num=-1,              //CS pin
		.queue_size=1,
		.pre_cb=NULL,
		.post_cb=NULL,
	};

	//Attach the LCD to the SPI bus
	ret=spi_bus_add_device(TP_SPI_HOST, &devcfg, &spi);
	assert(ret==ESP_OK);
}

uint8_t tp_spi_xchg(uint8_t data_send)
{
    uint8_t data_recv = 0;
    
    spi_transaction_t t = {
        .length = 8, // length is in bits
        .tx_buffer = &data_send,
        .rx_buffer = &data_recv
    };

    spi_device_queue_trans(spi, &t, portMAX_DELAY);

    spi_transaction_t * rt;
    spi_device_get_trans_result(spi, &rt, portMAX_DELAY);

    return data_recv;
}


/**********************
 *   STATIC FUNCTIONS
 **********************/
