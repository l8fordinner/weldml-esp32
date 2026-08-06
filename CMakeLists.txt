cmake_minimum_required(VERSION 3.16)

# Board selection: prefer -D BOARD=<name> (cached across re-runs) over env var.
# Usage: idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build
#   or:  BOARD=waveshare-esp32-s3-lcd-147 idf.py build
if(DEFINED BOARD)
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;boards/${BOARD}/sdkconfig.defaults")
elseif(DEFINED ENV{BOARD})
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;boards/$ENV{BOARD}/sdkconfig.defaults")
else()
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(weldml-esp32)

# Package web/*.html, style.css, app.js into the "spiffs" partition webserver.c
# mounts at /web. Without this the partition is blank and every static-file
# request 404s even though the httpd server itself is up.
spiffs_create_partition_image(spiffs ${CMAKE_SOURCE_DIR}/web FLASH_IN_PROJECT)
