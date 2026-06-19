/*
 * WeldML ESP32 — Stage 3: screen color test
 *
 * Validates LCD hardware: white (processing state) for 2 s, then green
 * (GOOD result).  No WiFi, no NVS, no USB MSC.  LCD driver only.
 *
 * Board: Waveshare ESP32-S3-LCD-1.47 (see boards/waveshare-esp32-s3-lcd-147/board.h)
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "lcd_st7789.h"
#include "board.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "WeldML ESP32 starting — IDF v%s", esp_get_idf_version());

    lcd_st7789_config_t lcd_cfg = {
        .mosi_pin = BOARD_LCD_MOSI_PIN,
        .clk_pin  = BOARD_LCD_CLK_PIN,
        .cs_pin   = BOARD_LCD_CS_PIN,
        .dc_pin   = BOARD_LCD_DC_PIN,
        .rst_pin  = BOARD_LCD_RST_PIN,
        .bl_pin   = BOARD_LCD_BL_PIN,
        .width    = 172,
        .height   = 320,
        /* x_gap=34: 172-wide panel centred in 240-column ST7789 controller.
         * Unverified — adjust if image appears shifted horizontally. */
        .x_gap    = 34,
        .y_gap    = 0,
    };

    ESP_ERROR_CHECK(lcd_st7789_init(&lcd_cfg));

    /* White = processing/busy state */
    ESP_LOGI(TAG, "LCD: white");
    ESP_ERROR_CHECK(lcd_st7789_fill(LCD_COLOR_WHITE));
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Green = GOOD weld result */
    ESP_LOGI(TAG, "LCD: green — screen color test complete");
    ESP_ERROR_CHECK(lcd_st7789_fill(LCD_COLOR_GREEN));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
