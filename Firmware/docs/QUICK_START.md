# Quick Start Guide

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash
idf.py monitor
```

## Notes

- Use `idf.py menuconfig` if you need to change settings.
- The device starts in AP mode if no WiFi SSID is saved.
- Use `idf.py erase-flash` if you want a full reset.
