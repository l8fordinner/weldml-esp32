idf_component_register(
    SRCS "mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES
        mqtt
        nvs_flash
        esp_event
)
# "mqtt" in REQUIRES refers to the ESP-IDF built-in esp-mqtt component.
# This component (app_mqtt) wraps it with a simpler API.
