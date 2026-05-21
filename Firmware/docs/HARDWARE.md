# Hardware Documentation

## Board Overview

**ESP32-S3 Zero** (from AliExpress)

- **Microcontroller**: ESP32-S3 (Xtensa dual-core)
- **CPU Clock**: Up to 240 MHz (configured with light sleep support)
- **RAM**: 512 KB SRAM
- **Flash**: 2 MB
- **Size**: Minimal form factor (smaller than DevKit boards)
- **Cost**: ~€5-8 USD from AliExpress

## Connectors

### USB Connector

- **Type**: USB-C (for programming and serial)
- **Purpose**: Flashing firmware, serial debug output (115200 baud)
- **No capacitors on this board** - relies on USB power stability

### Pin Headers

- **GPIO Headers**: 2 rows of pins for easy access to GPIO
- **Power Pins**: 3.3V and 5V available from USB
- **GND**: Multiple ground pins

## GPIO Usage

```
GPIO  Function              Hardware
────  ────────────────────  ──────────────────────
1     Tilt Switch           Tilt switch (standness detection)
3     WS2812 LED Data      RGB LED strip (18 LEDs)
17    UART0 TX             Serial debug output
18    UART0 RX             Serial debug input
```

### Unused but Available

- GPIO 21, 22: Reserved for I2C (future expansion)
- Other GPIOs: Available for expansion

## LED Strip (WS2812B)

- **GPIO**: GPIO 3 (RMT protocol)
- **Count**: 18 addressable RGB LEDs
- **Format**: GRB (Green, Red, Blue)
- **Power**: Connected directly to 5V USB
- **Data**: Single wire on GPIO 3

## Tilt Switch (Orientation Sensor)

- **GPIO**: GPIO 1
- **Type**: Tilt switch / gravity sensor
- **Function**: Detects lamp orientation (standing vs tilted)
- **Pull-down**: Enabled in GPIO config
- **Debounce**: 300 ms
- **How it works**: Closes circuit when lamp is upright, opens when tilted

## Power

### USB Power

- **Input**: 5V from USB-C
- **No voltage regulators needed** - clean power from USB
- **Current Usage**:
  - Idle (WiFi off): ~15-20 mA
  - WiFi connected: ~80-120 mA
  - WiFi active + LEDs: ~200-500 mA
  - All LEDs white (max brightness): ~1000+ mA

### Power Supply Notes

- **Recommended**: USB 5V/2A minimum
- **For bright LEDs**: Use USB 5V/3A or higher
- **No capacitors needed** on this minimal setup - USB cable provides filtering

## Serial Connection

- **Baud Rate**: 115200 bps
- **TX Pin**: GPIO 17 (available on USB via built-in converter)
- **RX Pin**: GPIO 18 (available on USB via built-in converter)
- **Connection**: USB-C direct

## Expansion Possibilities

### I2C Bus (Reserved)

- **SDA**: GPIO 21
- **SCL**: GPIO 22
- **Use Case**: Future sensors (temperature, light, etc.)

### Additional UARTs

- **UART1**: Available on GPIO 9/10
- **UART2**: Available on GPIO 19/20
- **Use Case**: GPS, external modules

## Temperature & Environment

- **Operating**: 0°C to +40°C (typical)
- **Storage**: -20°C to +60°C
- **Humidity**: Best in dry environments
- **Location**: Avoid direct condensation

## Component List (Homemade Setup)

| Part | Source | Cost |
|------|--------|------|
| ESP32-S3 Zero | AliExpress | ~€6 |
| WS2812B Strip (18x) | AliExpress | ~€2-3 |
| Tilt Switch | AliExpress | ~€0.5 |
| USB Cable (5V/2A) | Any | ~€2-5 |
| **Total** | | ~€10-15 |

**That's it!** No capacitors, no extra components - just the board, LED strip, and tilt switch.

## Soldering/Wiring

### Minimal Connections

1. **GPIO 3** → WS2812 Data wire (green)
2. **GND** → WS2812 GND (black) + Tilt switch GND
3. **5V** → WS2812 5V (red)
4. **GPIO 1** → Tilt switch → GND (GPIO 1 pulled down in firmware)

## Schematic (Homemade)

```
USB 5V ───────────┬────────── WS2812B 5V
                  │
ESP32-S3 Zero     ├── 18 LEDs
                  │
GPIO 3 ───────────┴────────── WS2812B Data

GPIO 1 ───────────┬────────── Tilt Switch
                  │
             [TILT SW]
                  │
              ┌───┴────
              │
             GND ───────────── GND (common)

USB 5V also powers ESP32-S3 (built-in regulator to 3.3V)
```

## See Also

- [SETUP.md](SETUP.md) - Setup guide
- [BUILD_AND_FLASH.md](BUILD_AND_FLASH.md) - Build instructions
- [COMPONENTS.md](COMPONENTS.md) - Firmware components
