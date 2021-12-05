#include "ws2812.h"
#include <driver/rmt.h>
#include <driver/gpio.h>

/**
 * A NeoPixel is defined by 3 bytes ... red, green and blue.
 * Each byte is composed of 8 bits ... therefore a NeoPixel is 24 bits of data.
 * At the underlying level, 1 bit of NeoPixel data is one item (two levels)
 * This means that the number of items we need is:
 *
 * #pixels * 24
 *
 */

/**
 * Set two levels of RMT output to the Neopixel value for a "1".
 * This is:
 * a logic 1 for 0.7us
 * a logic 0 for 0.6us
 */
static void setItem1(rmt_item32_t *pItem) {
	pItem->level0    = 1;
	pItem->duration0 = 10;
	pItem->level1    = 0;
	pItem->duration1 = 6;
} // setItem1



/**
 * Set two levels of RMT output to the Neopixel value for a "0".
 * This is:
 * a logic 1 for 0.35us
 * a logic 0 for 0.8us
 */
static void setItem0(rmt_item32_t *pItem) {
	pItem->level0    = 1;
	pItem->duration0 = 4;
	pItem->level1    = 0;
	pItem->duration1 = 8;
} // setItem0


/**
 * Add an RMT terminator into the RMT data.
 */
static void setTerminator(rmt_item32_t *pItem) {
	pItem->level0    = 0;
	pItem->duration0 = 0;
	pItem->level1    = 0;
	pItem->duration1 = 0;
} // setTerminator


static rgbVal pixels[PIXEL_COUNT];
static rmt_item32_t items[PIXEL_COUNT * 24 + 1];

rgbVal* ws2812_init() {
  gpio_set_direction(PIXEL_PIN, GPIO_MODE_OUTPUT);

	rmt_config_t config;
	config.rmt_mode                  = RMT_MODE_TX;
	config.channel                   = RMT_CHANNEL_0;
	config.gpio_num                  = PIXEL_PIN;
	config.mem_block_num             = 8;
	config.clk_div                   = 8;
	config.tx_config.loop_en         = 0;
	config.tx_config.carrier_en      = 0;
	config.tx_config.idle_output_en  = 1;
	config.tx_config.idle_level      = (rmt_idle_level_t)0;
	config.tx_config.carrier_freq_hz = 10000;
	config.tx_config.carrier_level   = (rmt_carrier_level_t)1;
	config.tx_config.carrier_duty_percent = 50;

	ESP_ERROR_CHECK(rmt_config(&config));
	ESP_ERROR_CHECK(rmt_driver_install(0, 0, 0));

  return pixels;
}

void ws2812_show() {
  rmt_item32_t* item = items;

  for(int i = 0; i < PIXEL_COUNT; i++) {
    uint32_t pixel = rgbValToInt(pixels[i]);
    uint32_t mask = 1 << 23;

    for(int j = 0; j < 24; j++) {
      if((pixel & mask) == 0) {
        setItem0(item);
      } else {
        setItem1(item);
      }

      item++;
      mask = mask >> 1;
    }
  }

  setTerminator(item);
  ESP_ERROR_CHECK(rmt_write_items(0, items, PIXEL_COUNT * 24, 1 /* wait till done */));
}
