#if defined(ARDUINO) && ARDUINO >= 100
  // No extras
#elif defined(ARDUINO) // pre-1.0
  // No extras
#elif defined(ESP_PLATFORM)
  #include "arduinoish.hpp"
#endif

#include "esp_system.h"
#include "esp_task_wdt.h"

#include "vmem.h"
#include "vloader.h"
#include "vinterp.h"
#include "properties.h"
#include "log.h"
#include "vnet.h"

#include "audio.h"
#include "spi.h"

extern "C" {
#include "ws2812.h"
}

// static const char *TAG = "nabaztag";

void simuSetMotor(int i,int val);

#include "bc.h"

#define BUTTON_1_PIN GPIO_NUM_5
#define BUTTON_2_PIN GPIO_NUM_35

#ifdef BRIGHT
#define ADJUST_BRIGHTNESS(v) (v)
#else

// this is calculated as follows: WS2801 = [int(pow(float(i) / 255.0, 2.5) * 32.0) for i in range(256)]
const uint8_t gamma8[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4,
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7,
  7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9,
  9, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11,
  11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13,
  14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 16, 16,
  16, 16, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18,
  19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21,
  22, 22, 22, 22, 23, 23, 23, 23, 24, 24, 24, 24,
  25, 25, 25, 26, 26, 26, 26, 27, 27, 27, 28, 28,
  28, 28, 29, 29, 29, 30, 30, 30, 31, 31, 31, 32 };
#define ADJUST_BRIGHTNESS(v) (gamma8[v])
#endif

static rgbVal* pixels = NULL;

void simuSetLed(int i,int val) {
  if(i < PIXEL_COUNT) {
    pixels[i] = makeRGBVal(ADJUST_BRIGHTNESS((val>>16)&255), ADJUST_BRIGHTNESS((val>>8)&255), ADJUST_BRIGHTNESS((val)&255));
  }
}

int getButton() {
  return gpio_get_level(BUTTON_1_PIN) != 0;
}

SemaphoreHandle_t xSemaphore = NULL;

int lockInterp() {
  return xSemaphoreTake( xSemaphore, ( TickType_t ) 10000 ) ;
}

void unlockInterp() {
  xSemaphoreGive( xSemaphore );
}

void setup() {
  Serial.begin(115200);

  vSemaphoreCreateBinary( xSemaphore );
  gpio_set_direction(BUTTON_1_PIN, GPIO_MODE_INPUT);

  netInit();
  pixels = ws2812_init();
  for(int i=0; i < PIXEL_COUNT; i++) {
    pixels[i] = makeRGBVal(0, 0, 0);
  }

  init_spi();
  //Init Audio
  init_vlsi();

  ws2812_show();

  loaderInit((char*)dumpbc);

  lockInterp();
  VPUSH(INTTOVAL(0));
  interpGo();
  VDROP();
  unlockInterp();
}

void loop() {
  lockInterp();
	VPUSH(VCALLSTACKGET(sys_start, SYS_CBLOOP));
	if (VSTACKGET(0)!=NIL) interpGo();
	VDROP();
  unlockInterp();

  play_check(0);
  rec_check();

  // update the LEDs
  ws2812_show();

  vTaskDelay(50 / portTICK_PERIOD_MS);
  esp_task_wdt_reset();
}
