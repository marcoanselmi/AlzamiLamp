idf_component_register(
    SRCS "lamp_settings.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash ws2812
)