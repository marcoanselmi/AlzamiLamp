# Wiring & Connections

## ESP32-S3 Pinout

```
ESP32-S3 Zero Board (Bottom view)

                    USB-C
                      |
   GND -- GND    5V -- 5V
    15 -- 15     3V3 -- 3V3
    16 -- 16     GND -- GND
    17 -- 17      
    18 -- 18     GPIO pins on right side
    
Pin Assignment:
- GPIO 3  : WS2812 LED Strip (DATA)
- GPIO 1  : Tilt Switch
- GND     : Ground (common)
- 5V      : Power rail
- 3V3     : ESP32 logic power
```

## Component Connections

### WS2812 RGB LED Strip
- **DIN (Data In)**: GPIO 3
- **5V+**: 5V power rail
- **GND**: Ground

### Tilt Switch
- **Pin 1**: GPIO 1
- **Pin 2**: GND

### USB-C (Power & Programming)
- **5V**: Supplies both ESP32 and LED strip
- **GND**: Common ground
- **D+/D-**: For UART/programming (auto-detected by ESP32-S3)

## Power Distribution

```
USB-C (5V)
  ├─→ 5V Rail
  │    ├─→ ESP32-S3 (via onboard regulator → 3V3)
  │    └─→ WS2812 Strip DIN power
  │
  └─→ GND Rail (common ground)
       ├─→ ESP32-S3
       ├─→ WS2812 Strip
       └─→ Tilt Switch
```

## USB Cable Power Extraction

The same USB-C cable provides both **power** and **programming**. To avoid overloading the ESP32-S3 onboard regulator, the **5V and GND wires are extracted directly from the USB cable** to power the LED strip independently.

**This setup is safe:**
- USB cable has 4 wires: 5V, GND, D+, D− (CC optional)
- Extract 5V and GND from the terminal end of the cable
- LED strip gets direct 5V power (bypassing ESP32)
- D+/D− still go through ESP32 for programming/serial
- No damage; no short circuit risk if done carefully
- Reduces power stress on the ESP32 LDO regulator

## Assembly Notes

1. Connect LED strip **data line** (DIN) to **GPIO 3** with a short wire
2. Connect tilt switch between **GPIO 1** and **GND**
3. **Extract 5V and GND wires from USB cable** for LED strip direct power
4. Keep ground connections short and solid
5. Use heat shrink tubing on exposed solder joints and cable cuts
6. Total current draw ~500mA at full brightness (18 LEDs @ max); USB 2.0 provides 500mA

---

See [../BOM.md](../BOM.md) for component details.
See [../../Firmware/docs/HARDWARE.md](../../Firmware/docs/HARDWARE.md) for firmware GPIO references.
