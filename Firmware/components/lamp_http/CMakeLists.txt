idf_component_register(
    SRCS "lamp_http.c"
    INCLUDE_DIRS "."
    REQUIRES lamp_settings esp_http_server lamp_cmd wifi main_logic
)