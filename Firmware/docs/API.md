# API Reference

## Overview

This file lists the small set of public entry points used by the firmware. Most features are driven by tasks and settings rather than a large API surface.

## LED Control

**Header**: `components/ws2812/ws2812.h`

- `ws2812_task(void *args)` - LED driver task
- `ws2812_set_all_color(rgb_color_t color)` - Set every LED to one color
- `ws2812_set_led_chain(ws2812_led_chain_t chain)` - Set each LED individually

### Data Types

- `rgb_color_t` - simple RGB color
- `ws2812_led_chain_t` - LED buffer with colors and active flags
- `WS2812_NUM_LEDS` - LED count, currently 18

## Switch Input

**Header**: `components/switch/switch.h`

- `switch_task(void *args)` - Reads the tilt switch
- `switch_get_event(void)` - Returns the latest switch event/state

## WiFi

**Header**: `components/wifi/wifi.h`

- `wifi_init(void)` - Starts AP mode or STA mode
- `wifi_is_connected(void)` - True when STA is connected
- `wifi_is_ap(void)` - True when running as AP
- `wifi_wait_for_connection(void)` - Waits for STA result

## Settings

**Header**: `components/lamp_settings/lamp_settings.h`

- `settings_init(void)` - Loads lamp and WiFi settings from NVS
- `lamp_settings_get(key, out)` / `lamp_settings_set(key, value)`
- `wifi_settings_get(key, out)` / `wifi_settings_set(key, value)`

### Data Types and Keys

- `setting_value_t` - tagged value used for NVS settings
- `setting_type_t` - value type selector
- `LAMP_NVS_NAMESPACE` - `lamp_settings`
- `WIFI_NVS_NAMESPACE` - `wifi_settings`

Common keys:
- Lamp: `on_color`, `off_color_enabled`, `off_color`
- WiFi: `ssid`, `password`, `ip_static`, `udp_en`, `udp_port`, `mqtt_en`, `mqtt_broker`, `mqtt_topic`

## Commands

**Header**: `components/lamp_cmd/lamp_cmd.h`

- `lamp_cmd_queue_init(void)` - Creates the shared command queue
- `lamp_cmd_enqueue(const lamp_cmd_t *cmd)` - Sends a command
- `lamp_cmd_dequeue(uint32_t timeout_ms)` - Receives a command
- `lamp_cmd_parse_json(...)` - Parses JSON command payloads

### Command Types

- `LAMP_CMD_ON`, `LAMP_CMD_OFF`
- `LAMP_CMD_SET_ON_COLOR`, `LAMP_CMD_SET_OFF_COLOR`
- `LAMP_CMD_SET_SETTING`
- `LAMP_CMD_RESTART`

## Network Services

**Headers**:

- `components/lamp_http/lamp_http.h` - `http_server_start()`
- `components/lamp_udp/lamp_udp.h` - `udp_start()`
- `components/lamp_mqtt/lamp_mqtt.h` - `mqtt_start()`, `mqtt_publish_status()`

These services are started after WiFi comes up. HTTP works in AP and STA mode; UDP and MQTT run only in STA mode.

## Time and Logic

**Headers**:

- `components/ntp_time/ntp_time.h` - `sync_time()`, `get_time()`
- `components/main_logic/main_logic.h` - `main_logic_task()`, `is_lamp_on()`

## See Also

- [COMPONENTS.md](COMPONENTS.md) - Component summary
- [CONFIGURATION.md](CONFIGURATION.md) - Stored settings
- [ARCHITECTURE.md](ARCHITECTURE.md) - Startup and data flow
