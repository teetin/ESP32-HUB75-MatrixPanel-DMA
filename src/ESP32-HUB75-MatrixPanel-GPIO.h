#ifndef _ESP32_HUB75_MATRIX_PANEL_GPIO
#define _ESP32_HUB75_MATRIX_PANEL_GPIO

#include <vector>
#include <memory>
#include <esp_err.h>
#include <esp_log.h>
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_struct.h"

#include "GFX_Compat.h"

struct HUB75_GPIO_CFG {
    enum shift_driver {
        SHIFTREG = 0,
        FM6124,
        FM6126A,
        ICN2038S,
        MBI5124,
        DP3246
    };

    struct gpio_pins {
        int8_t r1, g1, b1, r2, g2, b2, a, b, c, d, e, lat, oe, clk;
    } gpio;

    uint16_t mx_width;
    uint16_t mx_height;
    uint16_t chain_length;
    uint8_t color_depth; // bits per color channel (1-8)
    bool double_buff;
    shift_driver driver;

    HUB75_GPIO_CFG(
        uint16_t _w = 64,
        uint16_t _h = 32,
        uint16_t _chain = 1,
        gpio_pins _pins = {25, 26, 27, 14, 12, 13, 23, 19, 5, 17, -1, 4, 15, 16},
        uint8_t _depth = 8,
        bool _double_buff = true,
        shift_driver _drv = SHIFTREG
    ) : mx_width(_w), mx_height(_h), chain_length(_chain), gpio(_pins), color_depth(_depth), double_buff(_double_buff), driver(_drv) {}
};

#ifndef HUB75_NO_GFX
class MatrixPanel_GPIO : public HUB75_GFX_PARENT
#else
class MatrixPanel_GPIO
#endif
{
public:
    MatrixPanel_GPIO(const HUB75_GPIO_CFG& cfg);
    virtual ~MatrixPanel_GPIO();

    bool begin();
    void stop();

    // GFX implementation
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
    virtual void fillScreen(uint16_t color);

    void setBrightness(uint8_t b);
    void flipBuffer();

    // Colour conversion helpers
    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

private:
    HUB75_GPIO_CFG m_cfg;
    uint8_t* m_framebuffers[2];
    volatile uint8_t m_backBufferIdx;
    uint8_t m_brightness;
    bool m_initialized;
    TaskHandle_t m_refreshTaskHandle;

    static void refreshTask(void* pvParameters);
    void updatePanel();

    inline void setAddress(uint8_t row);

    // Specialized IC support
    void shiftDriver(const HUB75_GPIO_CFG& _cfg);
    void fm6124init(const HUB75_GPIO_CFG& _cfg);
    void dp3246init(const HUB75_GPIO_CFG& _cfg);

    // Fast GPIO manipulation using ESP-IDF REG_WRITE
    inline void gpio_set_level_fast(int pin, bool level) {
        if (pin < 0) return;
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
        if (pin < 32) {
            if (level) REG_WRITE(GPIO_OUT_W1TS_REG, (1 << pin));
            else REG_WRITE(GPIO_OUT_W1TC_REG, (1 << pin));
        } else {
            if (level) REG_WRITE(GPIO_OUT1_W1TS_REG, (1 << (pin - 32)));
            else REG_WRITE(GPIO_OUT1_W1TC_REG, (1 << (pin - 32)));
        }
#else
        // For C3, H2 etc which may have different GPIO structure or only 32 pins
        if (level) REG_WRITE(GPIO_OUT_W1TS_REG, (1 << pin));
        else REG_WRITE(GPIO_OUT_W1TC_REG, (1 << pin));
#endif
    }
};

#endif
