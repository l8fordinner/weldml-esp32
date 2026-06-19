idf_component_register(
    SRCS "webserver.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_http_server
        nvs_flash
        esp_wifi
        esp_netif
        spiffs
        esp_timer
)
