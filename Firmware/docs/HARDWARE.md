# Hardware Documentation

**Board**: ESP32-S3 Zero from AliExpress

For full hardware assembly and wiring details, see:
- [Hardware BOM](../../Hardware/BOM.md)
- [Hardware Wiring & Connections](../../Hardware/docs/WIRING.md)

## USB Connection

The USB-C connection on the board is used for **power, flashing, and serial console access**. The lamp does **not** expose a user-facing USB communication interface or protocol; normal device behavior is handled through GPIO and WiFi.

## GPIO Pinout

| GPIO | Function | Hardware | Notes |
|------|----------|----------|-------|
| 1 | Tilt Switch | Orientation detection | 300 ms debounce |
| 3 | LED Data | WS2812 strip (18 LEDs) | RMT protocol |


## Power

- **Input**: 5V USB-C
- **Idle**: ~15-20 mA
- **WiFi active**: ~80-120 mA
- **Full brightness LEDs**: ~1000+ mA

Recommended: **5V/2A USB supply** for stable operation.

## Component List (Homemade Setup)

| Part | Source | Cost |
|------|--------|------|
| ESP32-S3 Zero | AliExpress | ~€6 |
| WS2812B Strip (18x) | AliExpress | ~€2-3 |
| Tilt Switch | AliExpress | ~€0.5 |
| USB Cable (5V/2A) | Any | ~€2-5 |
| **Total** | | ~€10-15 |

## See Also

- [SETUP.md](SETUP.md) - Setup guide
- [BUILD_AND_FLASH.md](BUILD_AND_FLASH.md) - Build instructions
- [COMPONENTS.md](COMPONENTS.md) - Firmware components
