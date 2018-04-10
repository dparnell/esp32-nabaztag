#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdint.h>

#define PIXEL_COUNT 22
#define PIXEL_PIN GPIO_NUM_22
#define PIXELS_RGB

typedef union {
  struct __attribute__ ((packed)) {
    uint8_t r, g, b;
  };
  uint32_t num;
} rgbVal;

extern rgbVal* ws2812_init();
extern void ws2812_show();

inline rgbVal makeRGBVal(uint8_t r, uint8_t g, uint8_t b)
{
  rgbVal v;

  v.r = r;
  v.g = g;
  v.b = b;
  return v;
}

#ifdef PIXELS_RGB
inline uint32_t rgbValToInt(rgbVal v) {
  return ((uint32_t)v.r << 16) | ((uint32_t)v.g << 8) | (uint32_t)v.b;
}
#else
#ifdef PIXELS_GBR
inline uint32_t rgbValToInt(rgbVal v) {
  return ((uint32_t)v.g << 16) | ((uint32_t)v.b << 8) | (uint32_t)v.r;
}
#else
#ifdef PIXELS_BGR
inline uint32_t rgbValToInt(rgbVal v) {
  return ((uint32_t)v.b << 16) | ((uint32_t)v.g << 8) | (uint32_t)v.r;
}
#else
#error Pixel RGB order not set
#endif
#endif
#endif

#endif /* WS2812_DRIVER_H */
