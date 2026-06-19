if(DEFINED ENV{BOARD})
    set(BOARD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/boards/$ENV{BOARD}")
else()
    set(BOARD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/boards/esp32-s3")
endif()

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "." "${BOARD_INCLUDE_DIR}"
    REQUIRES
        lcd_st7789
)
