# Alzami Lampada

A DIY smart lamp controller based on ESP32-S3 with addressable RGB LED strip and tilt-sensor orientation detection.

**Name**: "Alzami" (Italian: "lift me") — the lamp turns **ON when lifted/standing upright** and **OFF when tilted down**.

## Quick Overview

- **Firmware**: ESP-IDF C project for ESP32-S3
- **Hardware**: 18x WS2812 LEDs, tilt switch, USB-C powered
- **Core Feature**: Tilt-activated on/off — lift to turn on, tilt to turn off
- **Features**: WiFi control (AP/STA), HTTP API, optional MQTT/UDP
- **Enclosure**: 3D-printable CAD files included

## Project Structure

```
AlzamiLampada/
├── Firmware/          # ESP32-S3 firmware (read main README)
├── Hardware/          # BOM, wiring diagram, CAD files
└── README.md          # This file
```

## Getting Started

1. **Build Firmware**: See [Firmware/README.md](Firmware/README.md)
2. **Build Hardware**: See [Hardware/BOM.md](Hardware/BOM.md) and [Hardware/WIRING.md](Hardware/WIRING.md)

## License

- **Firmware**: PolyForm Noncommercial 1.0.0
- **Hardware/CAD**: Creative Commons BY-NC-SA 4.0
- **See**: [Firmware/LICENSE](Firmware/LICENSE) and [Hardware/LICENSE-CAD](Hardware/LICENSE-CAD)

Not for commercial use. Free for personal, hobby, educational use.

## Contributing

Contributions welcome! Please see [Firmware/docs/CONTRIBUTING.md](Firmware/docs/CONTRIBUTING.md).

