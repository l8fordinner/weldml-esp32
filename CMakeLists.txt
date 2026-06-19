cmake_minimum_required(VERSION 3.16)

# Include board-specific sdkconfig.defaults if BOARD is set
# Usage: idf.py -DBOARD=esp32 build
if(DEFINED ENV{BOARD})
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;boards/$ENV{BOARD}/sdkconfig.defaults")
else()
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-base-template)

# Build a SPIFFS image from the web/ directory and flash it with idf.py flash.
# The partition label "spiffs" must match partitions.csv.
spiffs_create_partition_image(spiffs ${CMAKE_CURRENT_LIST_DIR}/web FLASH_IN_PROJECT)
