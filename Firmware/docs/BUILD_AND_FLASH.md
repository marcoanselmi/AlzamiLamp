# Build and Flash Guide

## Quick Steps

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash
idf.py monitor
```

## Notes

- Use `idf.py menuconfig` if you need to change build-time settings.
- Use [CONFIGURATION.md](CONFIGURATION.md) for WiFi and lamp settings.
- If flashing fails, check [TROUBLESHOOTING.md](TROUBLESHOOTING.md).
