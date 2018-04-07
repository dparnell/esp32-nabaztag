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
#include "linux_simunet.h"
#include "vnet.h"

extern "C" {
#include "ws2812.h"
}

//void simuSetLed(int i,int val);
void simuSetMotor(int i,int val);

#include "bc.h"

#define BUTTON_1_PIN GPIO_NUM_34
#define BUTTON_2_PIN GPIO_NUM_35
#define PIXEL_COUNT 22
#define PIXEL_PIN GPIO_NUM_22

#define ADJUST_BRIGHTNESS(v) ((v))

static rgbVal pixels[PIXEL_COUNT];

void simuSetLed(int i,int val) {
  pixels[i] = makeRGBVal(ADJUST_BRIGHTNESS((val>>16)&255), ADJUST_BRIGHTNESS((val>>8)&255), ADJUST_BRIGHTNESS((val)&255));
}

int getButton() {

  return gpio_get_level(BUTTON_1_PIN) == 0;
}

void setup() {
  Serial.begin(115200);

  gpio_set_direction(BUTTON_1_PIN, GPIO_MODE_INPUT);
  gpio_set_direction(PIXEL_PIN, GPIO_MODE_OUTPUT);

  netInit();
  for(int i=0; i < PIXEL_COUNT; i++) {
    pixels[i] = makeRGBVal(0, 0, 0);
  }
  ws2812_init(PIXEL_PIN);
  ws2812_setColors(PIXEL_COUNT, pixels);

  simunetinit();
  loaderInit((char*)dumpbc);

  VPUSH(INTTOVAL(0));
  interpGo();
  VDROP();
}

void loop() {
  checkNetworkEvents();

	VPUSH(VCALLSTACKGET(sys_start, SYS_CBLOOP));
	if (VSTACKGET(0)!=NIL) interpGo();
	VDROP();

  // update the LEDs
  ws2812_setColors(PIXEL_COUNT, pixels);

  esp_task_wdt_reset();
}
