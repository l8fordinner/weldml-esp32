idf_component_register(
    SRCS "weld_processor.c"
    INCLUDE_DIRS "."
    REQUIRES
        lcd_st7789
        esp_tinyusb
        esp_timer
        freertos
)
