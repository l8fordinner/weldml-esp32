#pragma once

#include "esp_err.h"
#include <stdint.h>

/* Standard RGB565 color constants */
#define LCD_COLOR_WHITE   0xFFFF
#define LCD_COLOR_BLACK   0x0000
#define LCD_COLOR_RED     0xF800
#define LCD_COLOR_GREEN   0x07E0
#define LCD_COLOR_BLUE    0x001F
#define LCD_COLOR_YELLOW  0xFFE0
#define LCD_COLOR_CYAN    0x07FF
#define LCD_COLOR_MAGENTA 0xF81F

typedef struct {
    int mosi_pin;
    int clk_pin;
    int cs_pin;
    int dc_pin;
    int rst_pin;
    int bl_pin;   /* Backlight MOSFET gate — active HIGH */
    int width;
    int height;
    int x_gap;    /* Column offset in ST7789 controller space (e.g. 34 for 172-wide on 240-col) */
    int y_gap;    /* Row offset in ST7789 controller space */
} lcd_st7789_config_t;

/* Initialize SPI bus, ST7789 panel, and backlight. */
esp_err_t lcd_st7789_init(const lcd_st7789_config_t *cfg);

/* Fill the entire display with a single RGB565 color. */
esp_err_t lcd_st7789_fill(uint16_t color_rgb565);

/* Fill a rectangle (x, y are top-left; w and h in pixels). */
esp_err_t lcd_st7789_fill_rect(int x, int y, int w, int h, uint16_t color_rgb565);
