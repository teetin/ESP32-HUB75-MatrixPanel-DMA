#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "esp_log.h"

#define TAG "LVGL_HUB75"

#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

MatrixPanel_I2S_DMA *matrix_display = nullptr;

/* LVGL display flush callback */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    int32_t w = (area->x2 - area->x1 + 1);
    int32_t h = (area->y2 - area->y1 + 1);

    matrix_display->drawLVGLBitmap(area->x1, area->y1, w, h, (const void *)color_p);

    lv_disp_flush_ready(disp_drv);
}

/* Tick interface for LVGL */
static void lv_tick_task(void *arg) {
    lv_tick_inc(1);
}

extern "C" void app_main() {
    /* Initialize HUB75 Panel */
    HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
    matrix_display = new MatrixPanel_I2S_DMA(mxconfig);
    matrix_display->begin();
    matrix_display->setBrightness8(128);
    matrix_display->clearScreen();

    /* Initialize LVGL */
    lv_init();

    /* Create a display buffer */
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf[PANEL_RES_X * 10]; // Buffer for 10 lines
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, PANEL_RES_X * 10);

    /* Register the display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = PANEL_RES_X * PANEL_CHAIN;
    disp_drv.ver_res = PANEL_RES_Y;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Tick timer */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
    esp_timer_start_periodic(periodic_timer, 1000); // 1ms

    /* Create simple UI */
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello HUB75!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    ESP_LOGI(TAG, "LVGL initialized");

    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
