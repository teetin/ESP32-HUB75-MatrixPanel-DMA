# LVGL Integration Example

This example demonstrates how to use the ESP32-HUB75-MatrixPanel-I2S-DMA library as a display driver for [LVGL](https://lvgl.io/).

## How to use

1. Ensure you have the `lvgl` component in your project.
2. Select `LVGL` in the `ESP32 HUB75 Configuration > GFX Library` menu in `menuconfig`.
3. Use `matrix_display->drawLVGLBitmap()` in your display flush callback.

## Configuration

The example is configured for a 64x64 panel by default. Adjust `PANEL_RES_X`, `PANEL_RES_Y` in `main.cpp` as needed.
