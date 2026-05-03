#ifndef HUB75_GFX_COMPAT_H
#define HUB75_GFX_COMPAT_H

#ifdef __has_include
    #if __has_include(<sdkconfig.h>)
        #include <sdkconfig.h>
    #endif
#endif

// 1. Check for GFX_Lite
#if defined(USE_GFX_LITE) || defined(CONFIG_ESP32_HUB75_USE_GFX_LITE)
    #define HUB75_USE_GFX_LITE
    #include "GFX_Lite.h"
    #define HUB75_GFX_PARENT GFX
    #define HUB75_HAS_CRGB

// 2. Check for NO_GFX
#elif defined(NO_GFX) || defined(CONFIG_ESP32_HUB75_NO_GFX)
    #define HUB75_NO_GFX

// 3. Default to Adafruit_GFX
#else
    #define HUB75_USE_ADAFRUIT_GFX
    #include "Adafruit_GFX.h"
    #define HUB75_GFX_PARENT Adafruit_GFX
#endif

#endif // HUB75_GFX_COMPAT_H
