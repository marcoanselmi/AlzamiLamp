idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES ws2812 switch wifi ntp_time lamp_cmd lamp_udp lamp_mqtt esp_pm lamp_settings lamp_http main_logic
    )  