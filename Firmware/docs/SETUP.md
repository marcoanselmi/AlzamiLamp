# Development Environment Setup

## Prerequisites

- **OS**: Linux, Windows (WSL2 recommended), or macOS
- **ESP-IDF**: v6.0.0 or later
- **Python**: 3.8 or higher
- **Git**: 2.0 or later

## Installation Steps

1. Install ESP-IDF 6.0.0 or later.
2. Source the ESP-IDF environment before building.
3. Clone the repository and enter the `Firmware` folder.
4. Run `idf.py set-target esp32s3` and `idf.py update-requirements`.

## Environment Setup

```bash
source /path/to/esp-idf/export.sh
echo $IDF_PATH
```

## Editor Setup

- VS Code with the ESP-IDF extension is the easiest option.
- The command line also works fine with `idf.py`.

## Verify Setup

```bash
idf.py --version
python --version
idf.py build
```
