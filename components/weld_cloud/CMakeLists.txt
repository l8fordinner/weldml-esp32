idf_component_register(
    SRCS "weld_cloud.c"
    INCLUDE_DIRS "."
    REQUIRES weld_parser weld_inference
)
