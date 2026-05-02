/*
 * GPIO Implementation Example for HUB75 RGB LED Matrix
 *
 * This example demonstrates how to use the MatrixPanel_GPIO class,
 * which provides a fully GPIO-based (bit-banged) implementation of the HUB75 protocol.
 *
 * This is useful for ESP32 variants that lack I2S/LCD DMA hardware (like ESP32-C3)
 * or when you want to use arbitrary GPIO pins.
 */

#include <ESP32-HUB75-MatrixPanel-GPIO.h>

// Panel configuration
const int PANEL_RES_X = 64;      // Number of pixels wide of a single panel.
const int PANEL_RES_Y = 32;      // Number of pixels tall of a single panel.
const int PANEL_CHAIN = 1;       // Number of panels chained together.

MatrixPanel_GPIO *dma_display = nullptr;

void setup() {
  Serial.begin(115200);

  // Default configuration
  HUB75_GPIO_CFG mxconfig(
    PANEL_RES_X,   // module width
    PANEL_RES_Y,   // module height
    PANEL_CHAIN    // Chain length
  );

  // Custom pins can be set here if needed
  // mxconfig.gpio.r1 = 25;
  // ...

  // Initialize the panel
  dma_display = new MatrixPanel_GPIO(mxconfig);
  if (!dma_display->begin()) {
    Serial.println("Failed to initialize panel!");
    return;
  }

  dma_display->setBrightness(128); // 0-255
  dma_display->fillScreen(dma_display->color565(0, 0, 0));
  dma_display->flipBuffer();
}

void loop() {
  // Draw some simple shapes
  dma_display->fillScreen(dma_display->color565(0, 0, 0));

  // Red rectangle
  dma_display->fillRect(5, 5, 20, 10, dma_display->color565(255, 0, 0));

  // Green circle (if Adafruit_GFX is available)
#ifndef NO_GFX
  dma_display->drawCircle(40, 16, 10, dma_display->color565(0, 255, 0));
#endif

  // Blue line
  dma_display->drawLine(0, 0, 63, 31, dma_display->color565(0, 0, 255));

  // Show the changes
  dma_display->flipBuffer();

  delay(2000);
}
