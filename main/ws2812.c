#include "ws2812.h"
#include <driver/rmt.h>
#include <driver/gpio.h>

#define F_CPU (CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ * 1000000U)
#define CLOCKLESS_FREQUENCY F_CPU
#define DIVIDER 2
#define F_CPU_RMT                   (  APB_CLK_FREQ )
#define RMT_CYCLES_PER_SEC          (F_CPU_RMT/DIVIDER)
#define RMT_CYCLES_PER_ESP_CYCLE    (F_CPU / RMT_CYCLES_PER_SEC)
#define ESP_TO_RMT_CYCLES(n)        ((n) / (RMT_CYCLES_PER_ESP_CYCLE))
#define C_NS(_NS) (((_NS * ((CLOCKLESS_FREQUENCY / 1000000L)) + 999)) / 1000)

#define T1 C_NS(250)
#define T2 C_NS(625)
#define T3 C_NS(375)

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
 */
static void setItem1(rmt_item32_t *pItem) {
	pItem->level0    = 1;
	pItem->duration0 = ESP_TO_RMT_CYCLES(T1+T2);
	pItem->level1    = 0;
	pItem->duration1 = ESP_TO_RMT_CYCLES(T3);
} // setItem1



/**
 * Set two levels of RMT output to the Neopixel value for a "0".
 */
static void setItem0(rmt_item32_t *pItem) {
	pItem->level0    = 1;
	pItem->duration0 = ESP_TO_RMT_CYCLES(T1);
	pItem->level1    = 0;
	pItem->duration1 = ESP_TO_RMT_CYCLES(T2+T3);
} // setItem0


/**
 * Add an RMT terminator into the RMT data.
 */
static void setTerminator(rmt_item32_t *pItem) {
	pItem->level0    = 0;
	pItem->duration0 = ESP_TO_RMT_CYCLES(T1+T2+T3);
	pItem->level1    = 0;
	pItem->duration1 = ESP_TO_RMT_CYCLES(T1+T2+T3);
} // setTerminator


static rgbVal pixels[PIXEL_COUNT];
static rmt_item32_t items[PIXEL_COUNT * 24 + 1];

rgbVal* ws2812_init() {
  gpio_set_direction(PIXEL_PIN, GPIO_MODE_OUTPUT);

	rmt_config_t config;
  config.channel = RMT_CHANNEL_0;
  config.rmt_mode = RMT_MODE_TX;
  config.gpio_num = PIXEL_PIN;
  config.mem_block_num = 1;
  config.clk_div = DIVIDER;
  config.tx_config.loop_en = false;
  config.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
  config.tx_config.carrier_en = false;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  config.tx_config.idle_output_en = true;

	ESP_ERROR_CHECK(rmt_config(&config));
	ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

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
