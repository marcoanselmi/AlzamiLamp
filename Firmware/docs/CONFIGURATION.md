# Configuration Guide

## Overview

Configuration is split into two parts:

1. Build-time settings in `sdkconfig`
2. Runtime settings stored in NVS

## Build-Time Settings

Use `idf.py menuconfig` to change ESP-IDF and component options.

```bash
idf.py menuconfig
idf.py build
```

If you want to reset build settings, delete `sdkconfig` and build again.

## Runtime Settings

Runtime settings are loaded at boot by `settings_init()` and stored in NVS.

### Lamp Settings

Stored in `lamp_settings`.

| Key | Type | Default | Purpose |
|-----|------|---------|---------|
| `on_color` | RGB | `250,200,200` | Main lamp color |
| `off_color_enabled` | bool | `true` | Use a separate off color |
| `off_color` | RGB | `0,0,20` | Color used when lamp is off |

### WiFi Settings

Stored in `wifi_settings`.

| Key | Type | Default | Purpose |
|-----|------|---------|---------|
| `ssid` | string | empty | WiFi network name |
| `password` | string | empty | WiFi password |
| `ip_static` | string | empty | Static IP, or empty for DHCP |
| `udp_en` | bool | `false` | Enable UDP listener |
| `udp_port` | u16 | `4210` | UDP port |
| `mqtt_en` | bool | `false` | Enable MQTT client |
| `mqtt_broker` | string | empty | MQTT broker URI |
| `mqtt_topic` | string | `AlzamiLamp` | MQTT base topic |

## How Settings Are Used

- `wifi_init()` reads WiFi settings and starts AP or STA mode
- `http_server_start()` runs in both AP and STA modes
- `udp_start()` and `mqtt_start()` run only when WiFi is connected in STA mode

## Changing Settings

Settings are updated through the firmware APIs or the HTTP interface.

Main APIs:

```c
settings_init();
lamp_settings_get(key, out);
lamp_settings_set(key, value);
wifi_settings_get(key, out);
wifi_settings_set(key, value);
```

## Resetting Settings

To clear stored settings and restore defaults:

```bash
idf.py erase-flash
idf.py build flash
```

## See Also

- [BUILD_AND_FLASH.md](BUILD_AND_FLASH.md) - Build and flash steps
- [COMPONENTS.md](COMPONENTS.md) - Component overview
