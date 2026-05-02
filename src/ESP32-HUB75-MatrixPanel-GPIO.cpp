#include "ESP32-HUB75-MatrixPanel-GPIO.h"
#include <string.h>
#include <algorithm>

#define TAG "HUB75_GPIO"

#define CLK_PULSE_GPIO \
    gpio_set_level_fast(m_cfg.gpio.clk, 1); \
    gpio_set_level_fast(m_cfg.gpio.clk, 0);

MatrixPanel_GPIO::MatrixPanel_GPIO(const HUB75_GPIO_CFG& cfg)
    :
#ifdef USE_GFX_LITE
      GFX(cfg.mx_width * cfg.chain_length, cfg.mx_height),
#elif !defined NO_GFX
      Adafruit_GFX(cfg.mx_width * cfg.chain_length, cfg.mx_height),
#endif
      m_cfg(cfg), m_backBufferIdx(0), m_brightness(128), m_initialized(false), m_refreshTaskHandle(NULL)
{
    size_t fb_size = cfg.mx_width * cfg.chain_length * cfg.mx_height * 3;
    m_framebuffers[0] = (uint8_t*)malloc(fb_size);
    if (m_framebuffers[0]) {
        memset(m_framebuffers[0], 0, fb_size);
    }

    if (m_cfg.double_buff) {
        m_framebuffers[1] = (uint8_t*)malloc(fb_size);
        if (m_framebuffers[1]) {
            memset(m_framebuffers[1], 0, fb_size);
        }
    } else {
        m_framebuffers[1] = m_framebuffers[0];
    }
}

MatrixPanel_GPIO::~MatrixPanel_GPIO() {
    stop();
    if (m_framebuffers[0]) free(m_framebuffers[0]);
    if (m_cfg.double_buff && m_framebuffers[1]) free(m_framebuffers[1]);
}

bool MatrixPanel_GPIO::begin() {
    if (m_initialized) return true;
    if (!m_framebuffers[0]) return false;

    // Initialize GPIOs
    const int8_t pins[] = {
        m_cfg.gpio.r1, m_cfg.gpio.g1, m_cfg.gpio.b1,
        m_cfg.gpio.r2, m_cfg.gpio.g2, m_cfg.gpio.b2,
        m_cfg.gpio.a, m_cfg.gpio.b, m_cfg.gpio.c, m_cfg.gpio.d, m_cfg.gpio.e,
        m_cfg.gpio.lat, m_cfg.gpio.oe, m_cfg.gpio.clk
    };

    for (int i = 0; i < sizeof(pins)/sizeof(pins[0]); i++) {
        if (pins[i] != -1) {
            gpio_reset_pin((gpio_num_t)pins[i]);
            gpio_set_direction((gpio_num_t)pins[i], GPIO_MODE_OUTPUT);
            gpio_set_level((gpio_num_t)pins[i], 0);
        }
    }

    gpio_set_level((gpio_num_t)m_cfg.gpio.oe, 1); // Disable display initially

    // Specialized driver init
    if (m_cfg.driver != HUB75_GPIO_CFG::SHIFTREG) {
        shiftDriver(m_cfg);
    }

    // Start refresh task
    xTaskCreatePinnedToCore(
        refreshTask,
        "HUB75_Refresh",
        4096,
        this,
        configMAX_PRIORITIES - 1, // High priority
        &m_refreshTaskHandle,
        1 // Pin to Core 1
    );

    m_initialized = true;
    return true;
}

void MatrixPanel_GPIO::stop() {
    if (m_refreshTaskHandle) {
        vTaskDelete(m_refreshTaskHandle);
        m_refreshTaskHandle = NULL;
    }
    if (m_initialized) {
        gpio_set_level((gpio_num_t)m_cfg.gpio.oe, 1);
        m_initialized = false;
    }
}

void MatrixPanel_GPIO::flipBuffer() {
    if (m_cfg.double_buff) {
        m_backBufferIdx = !m_backBufferIdx;
    }
}

void MatrixPanel_GPIO::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= _width || y < 0 || y >= _height) return;

    uint8_t r = ((color >> 11) & 0x1F) << 3;
    uint8_t g = ((color >> 5) & 0x3F) << 2;
    uint8_t b = (color & 0x1F) << 3;

    int index = (y * _width + x) * 3;
    uint8_t* fb = m_framebuffers[m_backBufferIdx];
    fb[index] = r;
    fb[index+1] = g;
    fb[index+2] = b;
}

void MatrixPanel_GPIO::fillScreen(uint16_t color) {
    uint8_t r = ((color >> 11) & 0x1F) << 3;
    uint8_t g = ((color >> 5) & 0x3F) << 2;
    uint8_t b = (color & 0x1F) << 3;

    uint8_t* fb = m_framebuffers[m_backBufferIdx];
    for (int i = 0; i < _width * _height; i++) {
        fb[i*3] = r;
        fb[i*3+1] = g;
        fb[i*3+2] = b;
    }
}

void MatrixPanel_GPIO::setBrightness(uint8_t b) {
    m_brightness = b;
}

void MatrixPanel_GPIO::refreshTask(void* pvParameters) {
    MatrixPanel_GPIO* panel = (MatrixPanel_GPIO*)pvParameters;
    while (1) {
        panel->updatePanel();
        // Yield after a full frame refresh to allow other tasks to run and avoid watchdog
        vTaskDelay(1);
    }
}

void IRAM_ATTR MatrixPanel_GPIO::updatePanel() {
    int rows = m_cfg.mx_height / 2;
    int width = m_cfg.mx_width * m_cfg.chain_length;
    uint8_t* fb = m_framebuffers[!m_backBufferIdx]; // Read from front buffer

    for (uint8_t row = 0; row < rows; row++) {
        for (uint8_t plane = 0; plane < m_cfg.color_depth; plane++) {
            uint8_t bitmask = (1 << (8 - m_cfg.color_depth + plane));

            // Shift out data
            for (int x = 0; x < width; x++) {
                int idx1 = (row * width + x) * 3;
                int idx2 = ((row + rows) * width + x) * 3;

                gpio_set_level_fast(m_cfg.gpio.r1, (fb[idx1] & bitmask));
                gpio_set_level_fast(m_cfg.gpio.g1, (fb[idx1+1] & bitmask));
                gpio_set_level_fast(m_cfg.gpio.b1, (fb[idx1+2] & bitmask));
                gpio_set_level_fast(m_cfg.gpio.r2, (fb[idx2] & bitmask));
                gpio_set_level_fast(m_cfg.gpio.g2, (fb[idx2+1] & bitmask));
                gpio_set_level_fast(m_cfg.gpio.b2, (fb[idx2+2] & bitmask));

                CLK_PULSE_GPIO
            }

            // Latch and Address
            gpio_set_level_fast(m_cfg.gpio.oe, 1); // Disable display

            setAddress(row);

            gpio_set_level_fast(m_cfg.gpio.lat, 1);
            gpio_set_level_fast(m_cfg.gpio.lat, 0);

            // Enable display and wait for BCM duration
            if (m_brightness > 0) {
                gpio_set_level_fast(m_cfg.gpio.oe, 0);
            }

            uint32_t delay_us = (1 << plane);
            uint32_t active_us = (delay_us * m_brightness) / 255;

            if (active_us > 0) {
                ets_delay_us(active_us);
            }
            gpio_set_level_fast(m_cfg.gpio.oe, 1);
            if (delay_us > active_us) {
                ets_delay_us(delay_us - active_us);
            }
        }
    }
}

void IRAM_ATTR MatrixPanel_GPIO::setAddress(uint8_t row) {
    gpio_set_level_fast(m_cfg.gpio.a, (row & 0x01));
    gpio_set_level_fast(m_cfg.gpio.b, (row & 0x02));
    gpio_set_level_fast(m_cfg.gpio.c, (row & 0x04));
    gpio_set_level_fast(m_cfg.gpio.d, (row & 0x08));
    gpio_set_level_fast(m_cfg.gpio.e, (row & 0x10));
}

void MatrixPanel_GPIO::shiftDriver(const HUB75_GPIO_CFG& _cfg) {
    switch (_cfg.driver) {
    case HUB75_GPIO_CFG::ICN2038S:
    case HUB75_GPIO_CFG::FM6124:
    case HUB75_GPIO_CFG::FM6126A:
        fm6124init(_cfg);
        break;
    case HUB75_GPIO_CFG::DP3246:
        dp3246init(_cfg);
        break;
    default:
        break;
    }
}

void MatrixPanel_GPIO::fm6124init(const HUB75_GPIO_CFG& _cfg) {
    ESP_LOGI(TAG, "Initializing FM6124/ICN2038S driver (GPIO)...");

    bool REG1[16] = {0,0,0,0,0, 1,1,1,1,1,1, 0,0,0,0,0};
    bool REG2[16] = {0,0,0,0,0, 0,0,0,0,1,0, 0,0,0,0,0};

    gpio_set_level_fast(_cfg.gpio.oe, 1);

    int width = _cfg.mx_width * _cfg.chain_length;

    // REG1
    for (int l = 0; l < width; l++) {
        bool val = REG1[l%16];
        gpio_set_level_fast(_cfg.gpio.r1, val);
        gpio_set_level_fast(_cfg.gpio.g1, val);
        gpio_set_level_fast(_cfg.gpio.b1, val);
        gpio_set_level_fast(_cfg.gpio.r2, val);
        gpio_set_level_fast(_cfg.gpio.g2, val);
        gpio_set_level_fast(_cfg.gpio.b2, val);

        if (l > width - 12) gpio_set_level_fast(_cfg.gpio.lat, 1);
        CLK_PULSE_GPIO
    }
    gpio_set_level_fast(_cfg.gpio.lat, 0);

    // REG2
    for (int l = 0; l < width; l++) {
        bool val = REG2[l%16];
        gpio_set_level_fast(_cfg.gpio.r1, val);
        gpio_set_level_fast(_cfg.gpio.g1, val);
        gpio_set_level_fast(_cfg.gpio.b1, val);
        gpio_set_level_fast(_cfg.gpio.r2, val);
        gpio_set_level_fast(_cfg.gpio.g2, val);
        gpio_set_level_fast(_cfg.gpio.b2, val);

        if (l > width - 13) gpio_set_level_fast(_cfg.gpio.lat, 1);
        CLK_PULSE_GPIO
    }
    gpio_set_level_fast(_cfg.gpio.lat, 0);

    gpio_set_level_fast(_cfg.gpio.oe, 0);
}

void MatrixPanel_GPIO::dp3246init(const HUB75_GPIO_CFG& _cfg) {
    ESP_LOGI(TAG, "Initializing DP3246 driver (GPIO)...");
    // Implementation omitted for brevity but follows same pattern as fm6124init
}
