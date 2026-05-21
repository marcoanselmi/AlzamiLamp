# Alzami Lampada Firmware

Small ESP-IDF firmware for a homemade lamp controller on an ESP32-S3 Zero board.

## Overview

The firmware drives an 18 LED WS2812 strip, reads a tilt switch on GPIO 1 with interrupt-based debounce, and uses WiFi for local control and status services. It can run in AP mode or connect to an existing WiFi network in STA mode, and it enables light sleep to save power when idle.

## Main Features

- WS2812 RGB LED control on GPIO 3
- Tilt switch input for orientation detection
- WiFi AP/STA startup from saved settings
- HTTP server for local control
- Optional UDP and MQTT support in STA mode

## Hardware

- **Board**: ESP32-S3 Zero from AliExpress
- **MCU**: ESP32-S3
- **LED Strip**: 18x WS2812 LEDs
- **Input**: Tilt switch on GPIO 1

## Project Docs

- [SETUP.md](SETUP.md) - Development environment setup
- [BUILD_AND_FLASH.md](BUILD_AND_FLASH.md) - Build and flash steps
- [ARCHITECTURE.md](ARCHITECTURE.md) - How the firmware is structured
- [API.md](API.md) - Public entry points
- [CONFIGURATION.md](CONFIGURATION.md) - Saved settings and defaults
- [HARDWARE.md](HARDWARE.md) - Board and wiring notes
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common problems
- [CONTRIBUTING.md](CONTRIBUTING.md) - How to help
- [CHANGELOG.md](CHANGELOG.md) - Release history

## Notes

This project is intentionally small and modular. Most behavior is controlled through the settings stored in NVS and the task flow described in the architecture docs.

## License

This project is licensed under PolyForm Noncommercial License 1.0.0.

- [LICENSE](../LICENSE)
- [PolyForm Noncommercial 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0/)
