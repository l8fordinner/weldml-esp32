/*
 * WeldML ESP32 — Stage 4: USB MSC + SD SPI + LCD
 *
 * Exposes the microSD card as a USB mass storage device so the robot/host
 * can write weld data files.  LCD shows startup state (white) then ready
 * (green) or fault (red).
 *
 * Board: Waveshare ESP32-S3-LCD-1.47 (boards/waveshare-esp32-s3-lcd-147/board.h)
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "lcd_st7789.h"
#include "usb_msc_sd.h"
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
        /* x_gap=34 centres the 172-pixel panel in ST7789 240-column GRAM.
         * mirror(true,true) in the driver corrects the 180° MADCTL rotation. */
        .x_gap    = 34,
        .y_gap    = 0,
    };
    ESP_ERROR_CHECK(lcd_st7789_init(&lcd_cfg));

    ESP_LOGI(TAG, "LCD: white — initializing USB MSC");
    ESP_ERROR_CHECK(lcd_st7789_fill(LCD_COLOR_WHITE));

    usb_msc_sd_config_t sd_cfg = {
        .miso_pin = BOARD_SD_MISO_PIN,
        .mosi_pin = BOARD_SD_MOSI_PIN,
        .clk_pin  = BOARD_SD_CLK_PIN,
        .cs_pin   = BOARD_SD_CS_PIN,
    };

    esp_err_t ret = usb_msc_sd_init(&sd_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB MSC init failed (%s) — LCD red", esp_err_to_name(ret));
        ESP_ERROR_CHECK(lcd_st7789_fill(LCD_COLOR_RED));
    } else {
        ESP_LOGI(TAG, "LCD: green — USB MSC ready");
        ESP_ERROR_CHECK(lcd_st7789_fill(LCD_COLOR_GREEN));
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
