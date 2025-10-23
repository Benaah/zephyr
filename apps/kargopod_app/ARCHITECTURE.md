# KargoPod Architecture Implementation Guide

## Overview

This document provides the complete architecture and implementation guide for the KargoPod firmware based on Zephyr RTOS.

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Application Layer                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐ │
│  │  Sensor  │  │   MQTT   │  │  Power   │  │   BLE   │ │
│  │ Manager  │  │ Manager  │  │ Manager  │  │  Diag   │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘ │
└─────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────────┐
│              Kargo Custom Modules Layer                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐ │
│  │  Sensor  │  │  Flash   │  │   MQTT   │  │  GNSS   │ │
│  │ Drivers  │  │  Buffer  │  │  Wrapper │  │  Mgmt   │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘ │
└─────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────────┐
│                Zephyr RTOS Core Layer                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐ │
│  │ Drivers  │  │Networking│  │ Security │  │  Power  │ │
│  │  (I2C,   │  │  (MQTT,  │  │  (TLS,   │  │  Mgmt   │ │
│  │  SPI)    │  │   TCP)   │  │  Crypto) │  │  (PSM)  │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘ │
└─────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────────┐
│                   Hardware Layer                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐ │
│  │ ESP32-S3 │  │ nRF9160  │  │ Sensors  │  │  Flash  │ │
│  │   WiFi   │  │  LTE-M   │  │(SHT4x,   │  │ Storage │ │
│  │          │  │  GNSS    │  │ LIS3DH)  │  │         │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘ │
└─────────────────────────────────────────────────────────┘
```

## Data Flow

### 1. Sensor Reading Flow
```
Timer (15 min) → Sensor Manager → Read I2C Sensors → Store Reading
                                                    ↓
                                          MQTT Manager
                                                    ↓
                                    ┌───────────────┴──────────┐
                                    │                          │
                              Connected?                 Offline?
                                    │                          │
                              Publish MQTT           Flash Buffer Store
                                    │                          │
                                AWS IoT Core         Local Storage (NVS)
```

### 2. Offline Buffering Flow
```
Network Down → Reading Generated → Flash Buffer Store
                                          ↓
                                  NVS Write (Circular)
                                          ↓
                                  Buffer Count++
                                          
Network Up → MQTT Connected → Retrieve Buffered → Publish → Buffer Count--
```

### 3. Power Management Flow
```
Reading Complete → Check Network Status
                          ↓
        ┌─────────────────┴─────────────────┐
        │                                    │
   Connected?                           Offline?
        │                                    │
   Normal Sleep                    Deep Sleep / PSM
   (15 min)                        (15 min w/ RTC wake)
        │                                    │
        └─────────────────┬─────────────────┘
                          ↓
                    Next Reading Cycle
```

## Module Descriptions

### Sensor Manager (`sensor_manager.c`)
- **Responsibility**: Read all sensors periodically
- **Features**:
  - SHT4x temperature/humidity
  - LIS3DH accelerometer
  - GNSS location (nRF9160/u-blox)
  - Error handling and retries
  - Reading counter

### MQTT Manager (`mqtt_manager.c`)
- **Responsibility**: Cloud connectivity
- **Features**:
  - TLS 1.2 with AWS IoT Core
  - Automatic reconnection
  - QoS 1 publishing
  - Offline buffering integration
  - Background thread for connection management

### Power Manager (`power_manager.c`)
- **Responsibility**: Energy optimization
- **Features**:
  - Deep sleep coordination
  - PSM/eDRX configuration (nRF9160)
  - Battery monitoring
  - Peripheral power control

### BLE Diagnostic (`ble_diag.c`)
- **Responsibility**: Field diagnostics
- **Features**:
  - Status broadcasting
  - Command interface
  - Connection monitoring
  - Remote configuration

### Flash Buffer (`kargo_flash_buffer.c`)
- **Responsibility**: Offline data persistence
- **Features**:
  - Circular buffer in NVS
  - 100 reading capacity
  - Power-loss safe
  - Efficient wear leveling

## Configuration Management

### Base Configuration (`prj.conf`)
Common settings for all platforms

### Platform Overlays
- `esp32s3.conf`: WiFi, ESP32-specific
- `nrf9160.conf`: LTE-M, PSM, GNSS

### Build-time Selection
```bash
west build -b kargopod_esp32s3 -- -DOVERLAY_CONFIG="esp32s3.conf"
west build -b kargopod_nrf9160 -- -DOVERLAY_CONFIG="nrf9160.conf"
```

## Security Implementation

### TLS Configuration
```c
tls_config->peer_verify = TLS_PEER_VERIFY_REQUIRED;
tls_config->sec_tag_list = (sec_tag_t[]){CONFIG_AWS_IOT_SEC_TAG};
tls_config->hostname = AWS_IOT_ENDPOINT;
```

### Certificate Storage
- **nRF9160**: Modem secure storage (sec_tag)
- **ESP32-S3**: Embedded in firmware or partition

### Provisioning Process
1. Generate certificates in AWS IoT Console
2. Download CA, device cert, private key
3. Run provisioning script
4. Flash firmware with credentials

## Testing Strategy

### Unit Tests
- Mock sensor drivers
- Flash buffer operations
- MQTT message formatting
- Power state transitions

### Integration Tests
- Full stack on `native_posix`
- Network simulation
- Timing verification

### Hardware Tests
- Sensor accuracy
- Power consumption
- Network reliability
- OTA updates

## Performance Targets

| Metric | ESP32-S3 | nRF9160 |
|--------|----------|---------|
| Deep Sleep Current | <50 µA | <3 µA |
| Active Current | ~80 mA | ~90 mA |
| Wake Time | ~200 ms | ~5 s |
| Battery Life* | ~6 months | ~2 years |

*With 2500mAh battery, 15-min intervals

## Build Variants

### Debug Build
```bash
west build -b kargopod_esp32s3 -- -DCMAKE_BUILD_TYPE=Debug
```
- Full logging
- Assertions enabled
- No optimization
- UART console active

### Release Build
```bash
west build -b kargopod_nrf9160 -- -DCMAKE_BUILD_TYPE=MinSizeRel
```
- Reduced logging
- Size optimization
- Console optional
- Production ready

## Deployment Pipeline

1. **Development**: ESP32-S3 with debug
2. **Testing**: ESP32-S3 with release config
3. **Pre-production**: nRF9160 field tests
4. **Production**: nRF9160 with OTA enabled
5. **Monitoring**: AWS IoT device shadows

## Troubleshooting

### Common Issues

**Sensors not detected**
- Check I2C pull-ups (4.7kΩ)
- Verify device tree configuration
- Test with `i2c scan` shell command

**MQTT not connecting**
- Verify certificates installed
- Check network connectivity
- Validate AWS IoT endpoint
- Review TLS logs

**High power consumption**
- Ensure deep sleep enabled
- Check for active peripherals
- Monitor wake sources
- Profile with power analyzer

**Flash buffer full**
- Increase buffer size in Kconfig
- Improve network reliability
- Add buffer health monitoring

## Future Enhancements

- [ ] FOTA via AWS IoT Jobs
- [ ] Edge ML for anomaly detection
- [ ] LoRaWAN fallback connectivity
- [ ] Multi-sensor fusion algorithms
- [ ] Predictive maintenance alerts
- [ ] Solar charging support

---

This architecture provides a solid foundation for production IoT devices with professional-grade reliability, security, and maintainability.
