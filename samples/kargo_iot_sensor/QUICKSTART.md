# 🚀 Quick Start Guide - Kargo IoT Sensor

Get your Kargo IoT Sensor up and running in 15 minutes!

## Prerequisites Checklist

- [ ] ESP32-S3 development board
- [ ] SHT4x sensor (connected via I2C)
- [ ] LIS3DH accelerometer (connected via I2C)
- [ ] USB cable
- [ ] WiFi network (2.4GHz)
- [ ] AWS account with IoT Core access

## Step 1: Hardware Setup (5 minutes)

### Wiring

Connect sensors to ESP32-S3:

```
ESP32-S3  →  Sensors (SHT4x & LIS3DH)
--------     ------------------------
3V3       →  VDD
GND       →  GND
GPIO8     →  SDA
GPIO9     →  SCL
```

### Verify Connections

Power on the board and check:
- Power LED is lit
- No loose connections
- Sensors are firmly connected

## Step 2: Software Setup (5 minutes)

### Install Dependencies

```bash
# Install West (Zephyr build tool)
pip3 install west

# Clone and initialize workspace
cd kargoos/zephyr
west init -l .
west update

# Source Zephyr environment
source zephyr-env.sh  # Linux/macOS
# or
zephyr-env.cmd        # Windows
```

## Step 3: Configure AWS IoT Core (5 minutes)

### Create IoT Thing

```bash
# 1. Create certificates
aws iot create-keys-and-certificate \
    --set-as-active \
    --certificate-pem-outfile samples/kargo_iot_sensor/certs/device-cert.pem \
    --private-key-outfile samples/kargo_iot_sensor/certs/device-key.pem

# 2. Download Root CA
cd samples/kargo_iot_sensor/certs
curl -o AmazonRootCA1.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem

# 3. Generate credential header
python3 generate_credentials.py \
    --ca-cert AmazonRootCA1.pem \
    --device-cert device-cert.pem \
    --device-key device-key.pem \
    --output ../src/aws_iot_credentials.h
```

### Get Your AWS IoT Endpoint

```bash
aws iot describe-endpoint --endpoint-type iot:Data-ATS
```

Copy the endpoint (something like: `xxxxxx-ats.iot.us-east-1.amazonaws.com`)

### Update Configuration

Edit `samples/kargo_iot_sensor/prj.conf`:

```ini
# Update these three lines
CONFIG_WIFI_SSID="YourWiFiSSID"
CONFIG_WIFI_PASSWORD="YourWiFiPassword"
CONFIG_AWS_IOT_ENDPOINT="your-endpoint-ats.iot.us-east-1.amazonaws.com"
```

## Step 4: Build and Flash (2 minutes)

### Using Build Script (Recommended)

**Linux/macOS:**
```bash
cd samples/kargo_iot_sensor
chmod +x build.sh
./build.sh -f -m  # Build, flash, and monitor
```

**Windows (PowerShell):**
```powershell
cd samples\kargo_iot_sensor
.\build.ps1 -Flash -Monitor
```

### Manual Build

```bash
cd samples/kargo_iot_sensor

# Build
west build -b esp32s3_devkitc/esp32s3/procpu

# Flash
west flash

# Monitor
west espressif monitor
```

## Step 5: Verify Operation (2 minutes)

### Check Serial Output

You should see:

```
*** Booting Zephyr OS ***
[00:00:00.123] <inf> kargo_iot: Kargo IoT Sensor Application Starting...
[00:00:00.456] <inf> kargo_iot: WiFi connected
[00:00:01.234] <inf> kargo_iot: MQTT connected
[00:00:15.001] <inf> kargo_iot: SHT4x - Temp: 23.45°C, Humidity: 55.20%
[00:00:15.002] <inf> kargo_iot: LIS3DH - X: 0.12, Y: -0.03, Z: 9.81 m/s²
[00:00:15.100] <inf> kargo_iot: Publishing: {"timestamp":15001,"temperature":23.45...}
[00:00:15.200] <inf> kargo_iot: Reading published successfully
```

### Check AWS IoT Core

1. Open AWS IoT Console
2. Navigate to **Test** → **MQTT test client**
3. Subscribe to topic: `kargo/sensors/+/data`
4. Wait for messages to appear (every 15 minutes)

### Test BLE Diagnostics (Optional)

1. Open nRF Connect app on phone
2. Scan for "Kargo Sensor"
3. Connect and read diagnostic characteristic
4. Should show: `WiFi:ON MQTT:ON Buffered:0 Uptime:xxx`

## Troubleshooting

### Problem: Build Fails

**Error:** `aws_iot_credentials.h not found`
- **Fix:** Run certificate generation script (Step 3)

**Error:** `Device tree error`
- **Fix:** Check if `esp32s3_devkitc.overlay` exists

### Problem: WiFi Won't Connect

- Verify SSID and password in `prj.conf`
- Ensure network is 2.4GHz (not 5GHz)
- Check WiFi signal strength
- Try resetting the board

### Problem: MQTT Connection Fails

- Verify AWS IoT endpoint in `prj.conf`
- Check certificates are correctly generated
- Ensure IoT policy is attached to certificate
- Verify security group allows port 8883

### Problem: No Sensor Data

- Check I2C wiring (SDA/SCL)
- Verify power supply (3.3V)
- Ensure sensors are I2C-compatible
- Check I2C address (SHT4x: 0x44, LIS3DH: 0x18)

## Next Steps

✅ **You're now running!** Here's what to do next:

1. **Monitor Data Flow**
   - Watch AWS IoT Console for incoming messages
   - Set up IoT Rules to forward to other services

2. **Test Offline Buffering**
   - Disconnect WiFi router
   - Wait for multiple reading cycles (30+ minutes)
   - Reconnect WiFi
   - Verify buffered readings are published

3. **Optimize Power Consumption**
   - Measure current draw
   - Adjust sleep intervals
   - Consider disabling BLE for production

4. **Add AWS Integration**
   - Connect to DynamoDB for storage
   - Set up SNS alerts for anomalies
   - Create CloudWatch dashboards

5. **Deploy to Production**
   - Enable secure boot
   - Configure OTA updates
   - Implement certificate rotation
   - Set up fleet management

## Useful Commands

```bash
# Clean build
west build -t clean

# Size-optimized build
west build -b esp32s3_devkitc/esp32s3/procpu -- -DCONFIG_SIZE_OPTIMIZATIONS=y

# Interactive configuration
west build -t menuconfig

# List connected devices
west espressif device_list

# Erase flash completely
esptool.py --port /dev/ttyUSB0 erase_flash
```

## Getting Help

- 📖 [Full Documentation](README.md)
- 🔒 [Certificate Setup Guide](certs/README.md)
- 💬 [GitHub Issues](https://github.com/your-repo/issues)
- 📧 Email: support@kargochain.com

---

**Happy Building! 🎉**

Your sensor should now be collecting data and publishing to AWS IoT Core every 15 minutes.
