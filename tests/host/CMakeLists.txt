cmake_minimum_required(VERSION 3.16)
project(weldml_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_weld_parser_features
    test_weld_parser_features.c
    ../../components/weld_parser/weld_parser.c
)
target_include_directories(test_weld_parser_features PRIVATE
    ../../components/weld_parser
)
target_link_libraries(test_weld_parser_features PRIVATE m)

enable_testing()
add_test(NAME weld_parser_features COMMAND test_weld_parser_features)
