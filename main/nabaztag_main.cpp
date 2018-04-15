#if defined(ARDUINO) && ARDUINO >= 100
  // No extras
#elif defined(ARDUINO) // pre-1.0
  // No extras
#elif defined(ESP_PLATFORM)
  #include "arduinoish.hpp"
#endif

#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#include "vmem.h"
#include "vloader.h"
#include "vinterp.h"
#include "properties.h"
#include "log.h"
#include "vnet.h"

extern "C" {
#include "ws2812.h"
}

static const char *TAG = "nabaztag";

#ifdef USE_SPI_MODE_SDCARD
// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define PIN_NUM_MISO GPIO_NUM_2
#define PIN_NUM_MOSI GPIO_NUM_15
#define PIN_NUM_CLK  GPIO_NUM_14
#define PIN_NUM_CS   GPIO_NUM_13
#endif //USE_SPI_MODE

void setupFilesystem() {
#ifndef USE_SPI_MODE_SDCARD
    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // To use 1-line SD mode, uncomment the following line:
    // host.flags = SDMMC_HOST_FLAG_1BIT;

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
    // Internal pull-ups are not sufficient. However, enabling internal pull-ups
    // does make a difference some boards, so we do that here.
    gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
    gpio_set_pull_mode(GPIO_NUM_12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
    gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

#else
    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = PIN_NUM_MISO;
    slot_config.gpio_mosi = PIN_NUM_MOSI;
    slot_config.gpio_sck  = PIN_NUM_CLK;
    slot_config.gpio_cs   = PIN_NUM_CS;
    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
#endif //USE_SPI_MODE

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }

    ESP_LOGI(TAG, "SD card mounted successfully. Details follow:");
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
}

void simuSetMotor(int i,int val);

#include "bc.h"

#define BUTTON_1_PIN GPIO_NUM_34
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
  return gpio_get_level(BUTTON_1_PIN) == 0;
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

  ws2812_show();

  setupFilesystem();

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

  // update the LEDs
  ws2812_show();

  esp_task_wdt_reset();
}
