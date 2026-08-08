idf_component_register(
    SRCS "weld_processor.c"
    INCLUDE_DIRS "."
    REQUIRES
        lcd_st7789
        esp_tinyusb
        esp_timer
        freertos
        weld_cloud
        weld_inference
        weld_parser
        webserver
        esp_http_server
        esp_http_client
        mbedtls
        nvs_flash
        ota
)
