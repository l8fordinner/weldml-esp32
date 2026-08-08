idf_component_register(
    SRCS "ota.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_https_ota
        app_update
        nvs_flash
        esp_event
        freertos
        esp_http_client
        mbedtls
        esp_app_format
        ota_policy
)
