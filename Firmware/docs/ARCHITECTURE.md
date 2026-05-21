# Architecture Documentation

## Overview

The firmware is a small task-based ESP-IDF application for a lamp controller. It reads saved settings from NVS, drives a WS2812 LED strip, watches a tilt switch, uses a GPIO interrupt to debounce the switch, and starts network services when WiFi is available. It also enables light sleep to reduce power use when idle.

## Main Parts

- `settings_init()` loads lamp and WiFi settings from NVS.
- `ws2812_task()` drives the LED strip on GPIO 3.
- `switch_task()` reads the tilt switch on GPIO 1 using a GPIO interrupt and software debounce.
- `main_logic_task()` decides lamp state from switch events and commands.
- `wifi_init()` starts AP mode or STA mode depending on saved SSID.
- `http_server_start()` starts the web interface in both modes.
- `udp_start()` and `mqtt_start()` run only in STA mode.

## Startup Flow

1. Configure light sleep.
2. Load settings from NVS.
3. Start the command queue.
4. Start LED and switch tasks.
5. Start main logic.
6. Initialize WiFi.
7. Start HTTP server.
8. If WiFi is connected, start UDP and MQTT.

## Data Flow

- The tilt switch feeds events into the main logic.
- Main logic updates lamp state and LED colors.
- Settings are read from NVS and written back when changed.
- Network services share the same state through the command queue and settings store.

## Concurrency

The application uses FreeRTOS tasks instead of a single loop.

| Task | Purpose |
|------|---------|
| `ws2812_task` | LED output |
| `switch_task` | Tilt switch input |
| `main_logic_task` | Lamp behavior |

## Notes

- GPIO 3 is used for WS2812 timing, so it is handled by the RMT peripheral.
- GPIO 1 uses a GPIO interrupt, pull-down, and software debounce.
- Light sleep is enabled after boot to reduce power use.
- NVS contains both lamp settings and WiFi settings.

## See Also

- [COMPONENTS.md](COMPONENTS.md) - Component summary
- [CONFIGURATION.md](CONFIGURATION.md) - Stored settings
- [BUILD_AND_FLASH.md](BUILD_AND_FLASH.md) - Build and flash steps
