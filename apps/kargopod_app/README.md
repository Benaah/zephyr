# KargoPod Firmware Project

Production-ready Zephyr RTOS firmware for KargoPod IoT sensor devices with dual-platform support (ESP32-S3 prototype / nRF9160 commercial).

##  Features

- **Multi-sensor support**: SHT4x (temp/humidity), LIS3DH/BMA4xx (accelerometer), GNSS
- **Connectivity**: WiFi (ESP32-S3) or LTE-M/NB-IoT (nRF9160)
- **MQTT over TLS**: Secure AWS IoT Core integration
- **Offline buffering**: Flash-based circular buffer (100 readings)
- **Power optimization**: Deep sleep, PSM, eDRX support
- **BLE diagnostics**: Remote monitoring and configuration
- **OTA updates**: MCUboot-based secure firmware updates
- **Dual platform**: ESP32-S3 for prototyping, nRF9160 for production

##  Project Structure

```
kargoos/
├── zephyr/                     # Zephyr RTOS (submodule)
├── modules/
│   ├── kargo_drivers/          # Custom sensor drivers
│   │   ├── drivers/sensor/     # SHT4x, accelerometer drivers
│   │   └── utils/              # Flash buffer, power management
│   └── kargo_comm/             # MQTT, modem, GNSS wrappers
├── boards/
│   ├── kargopod_esp32s3/       # ESP32-S3 board definition
│   └── kargopod_nrf9160/       # nRF9160 board definition
├── apps/
│   └── kargopod_app/           # Main application
│       ├── src/                # Application source
│       ├── prj.conf            # Base configuration
│       ├── esp32s3.conf        # ESP32-S3 overlay
│       ├── nrf9160.conf        # nRF9160 overlay
│       └── build.sh/ps1        # Build scripts
└── tools/
    └── provisioning/           # Certificate provisioning
```

##  Quick Start

### Prerequisites

```bash
# Install Zephyr SDK and dependencies
pip install west
west init -m https://github.com/zephyrproject-rtos/zephyr zephyrproject
cd zephyrproject
west update
west zephyr-export
pip install -r zephyr/scripts/requirements.txt
```

### Clone Repository

```bash
git clone <your-repo-url> kargoos
cd kargoos
```

### Build for ESP32-S3 (Prototype)

```bash
cd zephyr/apps/kargopod_app

# Linux/Mac
./build.sh -b kargopod_esp32s3

# Windows
.\build.ps1 -Board kargopod_esp32s3
```

### Build for nRF9160 (Commercial)

```bash
# Linux/Mac
./build.sh -b kargopod_nrf9160

# Windows
.\build.ps1 -Board kargopod_nrf9160
```

### Flash Firmware

```bash
# ESP32-S3
west flash -r esp32

# nRF9160
west flash -r nrfjprog
```

## ⚙️ Configuration

### WiFi (ESP32-S3)

Edit `esp32s3.conf`:
```conf
CONFIG_WIFI_SSID="YourSSID"
CONFIG_WIFI_PASSWORD="YourPassword"
```

### AWS IoT Credentials

Edit `prj.conf` or board overlay:
```conf
CONFIG_AWS_IOT_ENDPOINT="xxxxx.iot.region.amazonaws.com"
CONFIG_AWS_IOT_CLIENT_ID="kargopod-001"
CONFIG_AWS_IOT_SEC_TAG=1
```

### Provision Certificates

```bash
cd tools/provisioning
python provision_aws_certs.py kargopod-001 /path/to/certs
```

##  Sensor Configuration

Default sensor read interval: **15 minutes**

Modify in `prj.conf`:
```conf
CONFIG_APP_SENSOR_READ_INTERVAL_MIN=15
```

##  Power Management

- **ESP32-S3**: Deep sleep between readings
- **nRF9160**: PSM (Power Saving Mode) + eDRX

Enable/disable:
```conf
CONFIG_APP_DEEP_SLEEP_ENABLED=y
```

##  BLE Diagnostics

Connect via BLE to:
- Monitor system status
- Check MQTT connection
- View sensor readings
- Trigger diagnostics

Service UUID: `12345678-1234-5678-1234-56789abcdef0`

##  OTA Updates

MCUboot-enabled for secure OTA:

1. Build update image:
```bash
west build -b kargopod_nrf9160
```

2. Sign image:
```bash
west sign -t imgtool -- --key bootloader/mcuboot/root-rsa-2048.pem
```

3. Upload via MQTT or HTTP

##  Testing

```bash
# Unit tests
west build -b native_posix -t run

# Hardware-in-loop tests
west build -b kargopod_esp32s3
west flash
west attach
```

##  Development Workflow

1. **Prototype on ESP32-S3**: Fast iteration with WiFi
2. **Test sensors**: Verify I2C communication and readings
3. **Port to nRF9160**: Add LTE-M connectivity
4. **Field testing**: Deploy and monitor via AWS IoT
5. **OTA updates**: Roll out fixes and features

##  CI/CD

GitHub Actions automatically builds both platforms on push:
- ESP32-S3 firmware artifacts
- nRF9160 firmware artifacts
- Static analysis (checkpatch)
- Unit tests

##  Documentation

- [Hardware Setup](docs/hardware_setup.md)
- [API Reference](docs/api_reference.md)
- [Power Optimization](docs/power_optimization.md)
- [Troubleshooting](docs/troubleshooting.md)

##  Contributing

1. Create feature branch
2. Make changes
3. Run tests: `./build.sh -b kargopod_esp32s3`
4. Submit pull request

##  License

Apache-2.0 - See LICENSE file

##  Support

- Issues: GitHub Issues
- Discussions: GitHub Discussions
- Email: support@kargochain.com

---

**Built by Kargo Chain Team**
