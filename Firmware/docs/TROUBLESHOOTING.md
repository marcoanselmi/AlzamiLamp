# Troubleshooting

## Build Problems

### `idf.py` not found

- Source the ESP-IDF environment first.
- Check that `IDF_PATH` is set.

### Build fails after changing settings

- Delete `sdkconfig` and rebuild.
- Run `idf.py menuconfig` again if needed.

## Flash Problems

### Device not detected

- Check the USB cable.
- Check the serial port.
- Make sure the board is powered.

### Flash succeeds but monitor shows nothing

- Use the correct port with `idf.py -p [PORT] monitor`.
- Try a lower baud rate if output looks corrupted.

## Runtime Problems

### Tilt switch does not trigger

- Check GPIO 1 wiring.
- Make sure the switch is connected to GND.
- Verify the board is using the expected debounce logic.

### LEDs do not light

- Check GPIO 3 wiring.
- Check LED strip power and ground.
- Verify the strip has the right LED count configured.

### WiFi does not connect

- Check SSID and password in NVS.
- If needed, clear stored settings and configure again.

## See Also

- [SETUP.md](SETUP.md)
- [BUILD_AND_FLASH.md](BUILD_AND_FLASH.md)
- [CONFIGURATION.md](CONFIGURATION.md)
