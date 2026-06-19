#include "lcd_st7789.h"

#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_dev.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lcd_st7789";

#define LCD_SPI_HOST    SPI2_HOST
#define LCD_PIXEL_CLK   40000000    /* 40 MHz */
#define TILE_ROWS       16
#define LCD_MAX_WIDTH   172         /* Waveshare 1.47" panel */

static esp_lcd_panel_handle_t s_panel = NULL;
static int s_width  = 0;
static int s_height = 0;

/* Tile buffer lives in internal DRAM — DMA-safe, no PSRAM needed. */
static uint16_t s_tile[TILE_ROWS * LCD_MAX_WIDTH];

esp_err_t lcd_st7789_init(const lcd_st7789_config_t *cfg)
{
    s_width  = cfg->width;
    s_height = cfg->height;

    /* SPI bus on SPI2_HOST — LCD only; SD will use SPI3_HOST in Stage 4. */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = cfg->mosi_pin,
        .miso_io_num     = -1,
        .sclk_io_num     = cfg->clk_pin,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_MAX_WIDTH * TILE_ROWS * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    /* Panel IO — SPI mode 0, 8-bit cmd/param widths. */
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = cfg->dc_pin,
        .cs_gpio_num       = cfg->cs_pin,
        .pclk_hz           = LCD_PIXEL_CLK,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io));

    /* ST7789 panel.
     * data_endian = LITTLE: ST7789 RAMCTRL EPF bit set so the panel accepts
     * pixel data in little-endian byte order, matching the ESP32-S3 memory
     * layout for uint16_t.  This avoids per-pixel byte-swapping in software.
     * rgb_ele_order = RGB: matches the panel's sub-pixel order.
     * invert_color: ST7789 panels require INVON to display correct colors. */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = cfg->rst_pin,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian    = LCD_RGB_DATA_ENDIAN_LITTLE,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &s_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, cfg->x_gap, cfg->y_gap));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    /* Backlight — GPIO48 drives the MOSFET gate; active HIGH.
     * Enable only after DISPON to avoid showing garbage during panel init. */
    gpio_config_t bl_cfg = {
        .pin_bit_mask = (1ULL << cfg->bl_pin),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_cfg));
    gpio_set_level(cfg->bl_pin, 1);

    ESP_LOGI(TAG, "ST7789 ready — %dx%d gap(%d,%d) clk=%dMHz",
             cfg->width, cfg->height, cfg->x_gap, cfg->y_gap,
             LCD_PIXEL_CLK / 1000000);
    return ESP_OK;
}

esp_err_t lcd_st7789_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (!s_panel) {
        return ESP_ERR_INVALID_STATE;
    }

    int tile_pixels = TILE_ROWS * w;
    for (int i = 0; i < tile_pixels; i++) {
        s_tile[i] = color;
    }

    for (int row = y; row < y + h; row += TILE_ROWS) {
        int rows = ((row + TILE_ROWS) <= (y + h)) ? TILE_ROWS : (y + h - row);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, x, row, x + w, row + rows, s_tile));
    }
    return ESP_OK;
}

esp_err_t lcd_st7789_fill(uint16_t color)
{
    if (!s_panel) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Fill the tile buffer once, reuse for every tile row. */
    int tile_pixels = TILE_ROWS * s_width;
    for (int i = 0; i < tile_pixels; i++) {
        s_tile[i] = color;
    }

    for (int y = 0; y < s_height; y += TILE_ROWS) {
        int rows = (y + TILE_ROWS <= s_height) ? TILE_ROWS : (s_height - y);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, 0, y, s_width, y + rows, s_tile));
    }
    return ESP_OK;
}
