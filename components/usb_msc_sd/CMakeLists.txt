idf_component_register(
    SRCS "usb_msc_sd.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        sdmmc
        esp_tinyusb
)
