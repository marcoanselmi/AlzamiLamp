idf_component_register(
    SRCS "main_logic.c"
    INCLUDE_DIRS "."
    REQUIRES ws2812 switch wifi ntp_time lamp_cmd lamp_udp esp_pm lamp_settings lamp_http
    )  