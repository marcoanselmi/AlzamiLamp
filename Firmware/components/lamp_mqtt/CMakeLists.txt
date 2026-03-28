idf_component_register(
    SRCS "lamp_mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES mqtt cjson lamp_cmd
)