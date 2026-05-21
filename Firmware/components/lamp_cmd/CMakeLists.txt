idf_component_register(
    SRCS "lamp_cmd.c"
    INCLUDE_DIRS "."
    REQUIRES ws2812 lamp_settings cjson
)