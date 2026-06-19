idf_component_register(
    SRCS "zigbee.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
)

# Zigbee is only supported on ESP32-H2 and ESP32-C6 natively.
# The Kconfig flag CONFIG_BOARD_HAS_ZIGBEE guards inclusion at the app level.
#
# To enable the real Zigbee stack, add the ESP Zigbee SDK dependency:
#   idf.py add-dependency "espressif/esp-zigbee-sdk"
# Then add esp_zigbee_core back to REQUIRES above.
# The stub compiles without it so the base template builds on all targets.
