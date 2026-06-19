idf_component_register(
    SRCS "ble_provision.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
)

# This component is only valid on chips with BLE.
# The stub compiles without BT/provisioning dependencies so the base template
# builds on all targets.
#
# To enable real BLE provisioning, add back to REQUIRES:
#   bt wifi_provisioning protocomm esp_wifi
# and implement ble_provision.c following:
#   $IDF_PATH/examples/provisioning/wifi_prov_mgr
