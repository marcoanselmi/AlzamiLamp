# Components Documentation

## Overview

Core components that make up the AlzamiLamp lamp controller.

## Core Components

### WS2812 (`ws2812/`)

Controls the addressable RGB LED strip (18 LEDs).

**Key Functions:**
- `ws2812_task()` - Main LED driver task
- `ws2812_set_all_color(color)` - Set all LEDs to one color
- `ws2812_set_led_chain(chain)` - Set individual LED colors

**Configuration:** `WS2812_NUM_LEDS` defines LED count (default 18)

**Details:** Uses GPIO 3 with RMT protocol for precise timing

---

### Switch (`switch/`)

Reads the tilt switch on GPIO 1.

**Key Functions:**
- `switch_task()` - Main input handler
- `switch_get_event()` - Returns latest switch state

**Details:** 300 ms debounce, pull-down enabled, detects lamp orientation

---

### WiFi (`wifi/`)

Manages WiFi connectivity (AP mode or STA mode).

**Key Functions:**
- `wifi_init()` - Initialize WiFi
- `wifi_is_connected()` - Check STA connection
- `wifi_is_ap()` - Check if in AP mode
- `wifi_wait_for_connection()` - Block until connected

**Behavior:** Reads SSID/password from NVS, tries STA connection, falls back to AP mode if failed

---

### Lamp Settings (`lamp_settings/`)

Stores and retrieves configuration in NVS (non-volatile storage).

**Key Functions:**
- `settings_init()` - Initialize NVS and load defaults
- `lamp_settings_get()`, `lamp_settings_set()` - Read/write lamp settings
- `wifi_settings_get()`, `wifi_settings_set()` - Read/write WiFi settings

**Stored Settings:**
- Lamp colors (on/off)
- WiFi credentials
- UDP/MQTT configuration
- Network mode

---

### Lamp Command Queue (`lamp_cmd/`)

Central command queue for inter-task communication.

**Command Types:**
- `LAMP_CMD_ON/OFF` - Power control
- `LAMP_CMD_SET_ON_COLOR` - Set lamp on-color
- `LAMP_CMD_SET_SETTING` - Update NVS settings
- `LAMP_CMD_RESTART` - Restart device

**Details:** Thread-safe queue for all subsystems to send commands

---

### HTTP Server (`lamp_http/`)

Web interface for controlling and configuring the lamp.

**Key Functions:**
- `http_server_start()` - Start web server on port 80

**Usage:** Access via http://[device-ip]/ in both AP and STA modes

---

### UDP Listener (`lamp_udp/`)

Receives lamp commands via UDP packets.

**Key Functions:**
- `udp_start()` - Start UDP listener

**Details:** Reads port and enable flag from settings; only runs in STA mode

---

### MQTT Client (`lamp_mqtt/`)

Publishes lamp status and receives commands via MQTT.

**Key Functions:**
- `mqtt_start()` - Connect and subscribe
- `mqtt_publish_status()` - Send status update

**Details:** Reads broker URI and topic from settings; only runs in STA mode

---

### NTP Time Sync (`ntp_time/`)

Synchronizes system time with NTP server.

**Key Functions:**
- `sync_time()` - Sync with NTP
- `get_time()` - Get current hour

**Usage:** Provides accurate time for logging and time-based features

---

### Main Logic (`main_logic/`)

High-level lamp logic orchestrator.

**Purpose:** Runs the main state machine for lamp behavior based on switch state, network commands, and settings

---

## Component Dependencies

```
main
├── wifi
│   ├── esp_wifi / esp_netif
│   └── nvs_flash
├── ws2812
├── switch
├── lamp_settings
│   └── nvs_flash
├── lamp_cmd
├── main_logic
├── ntp_time
├── lamp_http
├── lamp_mqtt
│   └── espressif/mqtt
├── lamp_udp
└── (external: espressif/mqtt, espressif/cjson)
```

## Initialization Order

1. `NVS` - Initialize non-volatile storage
2. `settings_init()` - Load settings from NVS
3. `lamp_cmd_queue_init()` - Create command queue
4. `ws2812_task()` - Start LED driver
5. `switch_task()` - Start input handler
6. `main_logic_task()` - Start main controller
7. `wifi_init()` - Initialize WiFi
8. `http_server_start()` - Start web server
9. `udp_start()` - Start UDP (STA mode only)
10. `mqtt_start()` - Start MQTT (STA mode only)

## Adding a New Component

1. Create `components/mycomponent/` folder
2. Add `CMakeLists.txt`, `idf_component.yml`, `mycomponent.h`, `mycomponent.c`
3. Update main `CMakeLists.txt` to include it
4. Initialize in `main.c` as needed

## See Also

- [API.md](API.md) - Function signatures
- [ARCHITECTURE.md](ARCHITECTURE.md) - System design
- [CONFIGURATION.md](CONFIGURATION.md) - Settings guide
