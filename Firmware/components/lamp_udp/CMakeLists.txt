idf_component_register(
    SRCS "lamp_udp.c"
    INCLUDE_DIRS "."
    REQUIRES cjson lamp_cmd lamp_settings
)