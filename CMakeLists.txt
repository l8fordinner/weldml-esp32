cmake_minimum_required(VERSION 3.16)

# Include board-specific sdkconfig.defaults if BOARD is set.
# Usage: BOARD=waveshare-esp32-s3-lcd-147 idf.py build
if(DEFINED ENV{BOARD})
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;boards/$ENV{BOARD}/sdkconfig.defaults")
else()
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(weldml-esp32)
