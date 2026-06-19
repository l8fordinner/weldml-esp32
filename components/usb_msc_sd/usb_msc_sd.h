#pragma once

#include "esp_err.h"

typedef struct {
    int miso_pin;
    int mosi_pin;
    int clk_pin;
    int cs_pin;
} usb_msc_sd_config_t;

/*
 * Initialize SD SPI on SPI3_HOST, mount the card, and install TinyUSB with
 * CDC + MSC composite.  After this call the host sees the SD card as a USB
 * mass storage device; the CDC interface is available for future use.
 * Must be called after the LCD SPI (SPI2_HOST) is already initialized.
 */
esp_err_t usb_msc_sd_init(const usb_msc_sd_config_t *cfg);
