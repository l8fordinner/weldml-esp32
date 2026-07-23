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

add_executable(test_weld_inference
    test_weld_inference.c
    ../../components/weld_inference/weld_inference.c
)
target_include_directories(test_weld_inference PRIVATE
    ../../components/weld_parser
    ../../components/weld_inference
)
target_compile_definitions(test_weld_inference PRIVATE
    GOLDEN_CSV="${CMAKE_CURRENT_LIST_DIR}/../../model_exports/esp32_port/golden_vectors/golden_vectors.csv"
)
target_link_libraries(test_weld_inference PRIVATE m)

enable_testing()
add_test(NAME weld_parser_features COMMAND test_weld_parser_features)
add_test(NAME weld_inference COMMAND test_weld_inference)
