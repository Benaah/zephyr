# Kargo IoT Sensor Application

A production-ready Zephyr RTOS firmware for ESP32-S3 that reads environmental and motion sensor data, publishes to AWS IoT Core via MQTT, and includes intelligent offline buffering and power management.

##  Features

- **Multi-Sensor Support**
  - SHT4x: Temperature & Humidity
  - LIS3DH: 3-axis Accelerometer
  - 15-minute reading intervals (configurable)

- **AWS IoT Core Integration**
  - Secure MQTT over TLS 1.2
  - Automatic reconnection handling
  - JSON formatted telemetry

- **Offline Resilience**
  - Flash-based reading buffer (up to 100 readings)
  - Automatic sync when connection restored
  - No data loss during network outages

- **Power Management**
  - Deep sleep between readings
  - WiFi power saving modes
  - Optimized for battery operation

- **BLE Diagnostics**
  - Real-time system status via Bluetooth
  - WiFi/MQTT connection state
  - Buffered reading count
  - System uptime monitoring

##  Hardware Requirements

### Required Components

1. **ESP32-S3 Development Board**
   - ESP32-S3-DevKitC-1 or compatible
   - Minimum 4MB Flash, 2MB PSRAM recommended

2. **Sensors**
   - Sensirion SHT4x (Temperature/Humidity) - I2C
   - STMicroelectronics LIS3DH (Accelerometer) - I2C

3. **Power Supply**
   - USB or Li-Po battery (3.7V)
   - Optional: Battery management circuitry

### Wiring Diagram

```
ESP32-S3          SHT4x          LIS3DH
--------          -----          ------
3V3       ------> VDD    ------> VDD
GND       ------> GND    ------> GND
GPIO8     ------> SDA    ------> SDA (I2C0)
GPIO9     ------> SCL    ------> SCL (I2C0)
```

##  Software Setup

### Prerequisites

1. **Zephyr SDK** (v0.16.0 or later)
   ```bash
   # Install Zephyr SDK
   wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.5/zephyr-sdk-0.16.5_linux-x86_64.tar.xz
   tar xvf zephyr-sdk-0.16.5_linux-x86_64.tar.xz
   cd zephyr-sdk-0.16.5
   ./setup.sh
   ```

2. **West Build Tool**
   ```bash
   pip3 install west
   ```

3. **ESP32 Tools**
   ```bash
   # Install ESP-IDF requirements
   pip3 install pyserial esptool
   ```

### Environment Setup

```bash
# Clone the repository (if not already cloned)
git clone <your-repo-url>
cd Kargo-Chain/kargoos/zephyr

# Initialize Zephyr workspace
west init -l .
west update

# Set up Zephyr environment
source zephyr-env.sh  # Linux/macOS
# or
zephyr-env.cmd        # Windows
```

##  Configuration

### 1. WiFi Credentials

Edit `prj.conf` and update:

```ini
CONFIG_WIFI_SSID="YourWiFiSSID"
CONFIG_WIFI_PASSWORD="YourWiFiPassword"
```

### 2. AWS IoT Core Setup

#### a. Create AWS IoT Thing

```bash
# Create a Thing in AWS IoT Core
aws iot create-thing --thing-name kargo_sensor_001

# Create and download certificates
aws iot create-keys-and-certificate \
    --set-as-active \
    --certificate-pem-outfile certs/device-cert.pem \
    --private-key-outfile certs/device-key.pem \
    --public-key-outfile certs/device-public.pem

# Download Root CA
cd certs
curl -o AmazonRootCA1.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem
```

#### b. Create and Attach Policy

Create a policy file `kargo-sensor-policy.json`:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": "arn:aws:iot:REGION:ACCOUNT_ID:client/kargo_sensor_*"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Publish",
      "Resource": "arn:aws:iot:REGION:ACCOUNT_ID:topic/kargo/sensors/*"
    }
  ]
}
```

Apply the policy:

```bash
# Create policy
aws iot create-policy \
    --policy-name KargoSensorPolicy \
    --policy-document file://kargo-sensor-policy.json

# Attach to certificate
aws iot attach-policy \
    --policy-name KargoSensorPolicy \
    --target <certificate-arn>
```

#### c. Generate Credential Header

```bash
cd certs
python3 generate_credentials.py \
    --ca-cert AmazonRootCA1.pem \
    --device-cert device-cert.pem \
    --device-key device-key.pem \
    --output ../src/aws_iot_credentials.h
```

#### d. Update Configuration

Edit `prj.conf`:

```ini
CONFIG_AWS_IOT_ENDPOINT="your-endpoint.iot.us-east-1.amazonaws.com"
CONFIG_AWS_IOT_CLIENT_ID="kargo_sensor_001"
CONFIG_AWS_IOT_SEC_TAG=1
```

Get your endpoint:
```bash
aws iot describe-endpoint --endpoint-type iot:Data-ATS
```

### 3. Device Tree (Optional)

If your sensor connections differ, create `esp32s3_devkitc.overlay`:

```dts
&i2c0 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_STANDARD>;

    sht4x@44 {
        compatible = "sensirion,sht4x";
        reg = <0x44>;
    };

    lis3dh@18 {
        compatible = "st,lis3dh";
        reg = <0x18>;
    };
};
```

##  Building

### Standard Build

```bash
# Navigate to the sample directory
cd samples/kargo_iot_sensor

# Build for ESP32-S3
west build -b esp32s3_devkitc/esp32s3/procpu
```

### Build with Custom Configuration

```bash
# Clean previous build
west build -t clean

# Build with menuconfig (interactive)
west build -b esp32s3_devkitc/esp32s3/procpu -t menuconfig

# Build
west build
```

### Optimization Builds

```bash
# Size-optimized build (smaller binary)
west build -b esp32s3_devkitc/esp32s3/procpu -- -DCONFIG_SIZE_OPTIMIZATIONS=y

# Speed-optimized build (better performance)
west build -b esp32s3_devkitc/esp32s3/procpu -- -DCONFIG_SPEED_OPTIMIZATIONS=y
```

##  Flashing

### Via USB (esptool)

```bash
# Flash the firmware
west flash

# Or manually with esptool
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 \
    write_flash -z 0x0 build/zephyr/zephyr.bin
```

### Monitor Serial Output

```bash
# Using West
west espressif monitor

# Or using screen
screen /dev/ttyUSB0 115200

# Or using minicom
minicom -D /dev/ttyUSB0 -b 115200
```

##  Data Format

### MQTT Message Structure

**Topic:** `kargo/sensors/{CLIENT_ID}/data`

**Payload (JSON):**
```json
{
  "timestamp": 1234567890,
  "temperature": 23.45,
  "humidity": 55.20,
  "accel": {
    "x": 0.12,
    "y": -0.03,
    "z": 9.81
  }
}
```

### Units

- **Temperature**: Degrees Celsius (°C)
- **Humidity**: Relative Humidity (%)
- **Acceleration**: Meters per second squared (m/s²)
- **Timestamp**: Milliseconds since boot

##  BLE Diagnostics

### Connecting via BLE

The device advertises as "Kargo Sensor" when BLE is enabled.

1. **Using nRF Connect (Mobile)**
   - Scan for "Kargo Sensor"
   - Connect to device
   - Navigate to Kargo Service UUID: `12345678-1234-5678-1234-56789abcdef0`
   - Read diagnostic characteristic

2. **Using bluetoothctl (Linux)**
   ```bash
   bluetoothctl
   scan on
   # Note the device MAC address
   connect <MAC_ADDRESS>
   # Access GATT characteristics
   ```

### Diagnostic Data Format

```
WiFi:ON MQTT:ON Buffered:0 Uptime:3600
```

- **WiFi**: Connection status (ON/OFF)
- **MQTT**: MQTT broker connection status (ON/OFF)
- **Buffered**: Number of readings in offline buffer
- **Uptime**: System uptime in seconds

##  Power Management

### Deep Sleep Configuration

The device enters deep sleep between sensor readings:

- **Active Time**: ~10 seconds (sensor read + publish)
- **Sleep Time**: ~14 minutes 50 seconds
- **Average Current**: ~50mA active, <1mA sleep
- **Battery Life**: ~2-3 months on 2500mAh Li-Po (estimate)

### Optimizing Battery Life

1. **Increase reading interval** (in `src/main.c`):
   ```c
   #define SENSOR_READ_INTERVAL_MS (30 * 60 * 1000)  // 30 minutes
   ```

2. **Disable BLE** (in `prj.conf`):
   ```ini
   CONFIG_BT=n
   ```

3. **Reduce WiFi power**:
   ```ini
   CONFIG_ESP32_WIFI_STA_PS_ENABLED=y
   ```

## 🧪 Testing

### Unit Tests

```bash
# Build tests
west build -b native_posix tests/

# Run tests
./build/zephyr/zephyr.exe
```

### Integration Testing

1. **Sensor Reading Test**
   - Monitor serial output for sensor values
   - Verify readings are within expected ranges

2. **MQTT Connectivity Test**
   ```bash
   # Subscribe to MQTT topic using AWS IoT Test Client
   aws iot-data subscribe --topic 'kargo/sensors/+/data'
   ```

3. **Offline Buffer Test**
   - Disconnect WiFi
   - Wait for multiple reading intervals
   - Reconnect WiFi
   - Verify buffered readings are published

##  Troubleshooting

### Build Issues

**Problem**: `fatal error: aws_iot_credentials.h: No such file or directory`
- **Solution**: Run the certificate generation script (see Configuration step 2c)

**Problem**: `Device tree error: i2c0 not found`
- **Solution**: Create a device tree overlay file (see Configuration step 3)

### Connection Issues

**Problem**: WiFi connection fails
- Check SSID and password in `prj.conf`
- Verify WiFi network is 2.4GHz (ESP32-S3 doesn't support 5GHz)
- Check signal strength

**Problem**: MQTT connection refused
- Verify AWS IoT endpoint is correct
- Check that certificates are valid and active
- Ensure policy is attached to certificate
- Verify security group allows port 8883

### Sensor Issues

**Problem**: Sensor not detected
- Verify I2C wiring (SDA/SCL, VDD/GND)
- Check I2C address (use `i2cdetect` command)
- Ensure pull-up resistors on I2C lines (usually 4.7kΩ)

**Problem**: Incorrect sensor readings
- Verify sensor power supply is stable (3.3V)
- Check for electrical noise or interference
- Ensure proper sensor orientation (for accelerometer)

##  Additional Resources

- [Zephyr Documentation](https://docs.zephyrproject.org/)
- [ESP32-S3 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [AWS IoT Core Documentation](https://docs.aws.amazon.com/iot/)
- [SHT4x Datasheet](https://www.sensirion.com/en/environmental-sensors/humidity-sensors/humidity-sensor-sht4x/)
- [LIS3DH Datasheet](https://www.st.com/resource/en/datasheet/lis3dh.pdf)

##  Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

##  License

Copyright (c) 2025 Kargo Chain
SPDX-License-Identifier: Apache-2.0

##  Support

For issues and questions:
- Open an issue on GitHub
- Contact: support@kargochain.com

---

**Note**: This is a reference implementation. For production deployment, consider:
- Hardware security features (secure boot, flash encryption)
- Certificate rotation mechanisms
- OTA (Over-The-Air) firmware updates
- Comprehensive error handling and recovery
- Production-grade logging and monitoring
