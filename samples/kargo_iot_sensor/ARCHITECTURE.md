# Kargo IoT Sensor - System Architecture

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        ESP32-S3 Module                          │
│                                                                 │
│  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐ │
│  │   Sensor     │      │     MQTT     │      │   Publish    │ │
│  │   Thread     │─────▶│   Thread     │─────▶│   Thread     │ │
│  │  (Read I2C)  │      │ (Connection) │      │  (Transmit)  │ │
│  └──────────────┘      └──────────────┘      └──────────────┘ │
│         │                      │                      │        │
│         ▼                      ▼                      ▼        │
│  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐ │
│  │  SHT4x I2C   │      │  WiFi Stack  │      │  NVS Flash   │ │
│  │  LIS3DH I2C  │      │  TLS Stack   │      │   Storage    │ │
│  └──────────────┘      └──────────────┘      └──────────────┘ │
│                                │                               │
│                                ▼                               │
│                        ┌──────────────┐                        │
│                        │ Power Mgmt   │                        │
│                        │ Deep Sleep   │                        │
│                        └──────────────┘                        │
└─────────────────────────────────────────────────────────────────┘
                               │
                               │ WiFi 2.4GHz
                               │ MQTT/TLS
                               ▼
                    ┌─────────────────────┐
                    │   AWS IoT Core      │
                    │   MQTT Broker       │
                    └─────────────────────┘
                               │
                               ▼
                    ┌─────────────────────┐
                    │   AWS Services      │
                    │ - DynamoDB          │
                    │ - Lambda            │
                    │ - CloudWatch        │
                    └─────────────────────┘
```

## Data Flow Diagram

```
     ┌──────────┐
     │  Start   │
     └────┬─────┘
          │
          ▼
  ┌───────────────┐
  │ Read Sensors  │
  │ (Every 15min) │
  └───────┬───────┘
          │
          ▼
  ┌───────────────┐
  │ Format JSON   │
  │   Payload     │
  └───────┬───────┘
          │
          ▼
     ┌─────────┐
     │ WiFi    │───No───┐
     │Connected?        │
     └────┬────┘        │
          │Yes          │
          ▼             ▼
  ┌──────────────┐  ┌──────────────┐
  │ MQTT Publish │  │ Buffer to    │
  │              │  │    Flash     │
  └──────┬───────┘  └──────┬───────┘
         │                 │
         │                 │
    ┌────┴─────┐           │
    │ Success? │───No──────┘
    └────┬─────┘
         │Yes
         ▼
  ┌──────────────┐
  │ Check Buffer │
  │    Count     │
  └──────┬───────┘
         │
         ▼
    ┌────────┐
    │Buffer  │───Yes───┐
    │ > 0?   │         │
    └────┬───┘         │
         │No           │
         │             ▼
         │    ┌────────────────┐
         │    │ Publish Buffer │
         │    │   Readings     │
         │    └────────┬───────┘
         │             │
         └─────┬───────┘
               │
               ▼
      ┌────────────────┐
      │  Deep Sleep    │
      │ (14.5 minutes) │
      └────────┬───────┘
               │
               └──────▶ (Loop)
```

## State Machine

```
         ┌─────────────────────────────────┐
         │         INITIALIZING            │
         │  - Load NVS                     │
         │  - Init Sensors                 │
         │  - Init WiFi                    │
         └───────────┬─────────────────────┘
                     │
                     ▼
         ┌─────────────────────────────────┐
         │      CONNECTING_WIFI            │
         │  - Scan Networks                │
         │  - Authenticate                 │
         │  - Get IP Address               │
         └───────────┬─────────────────────┘
                     │
                     ▼
         ┌─────────────────────────────────┐
         │     CONNECTING_MQTT             │
         │  - Load Certificates            │
         │  - TLS Handshake                │
         │  - MQTT CONNECT                 │
         └───────────┬─────────────────────┘
                     │
        ┌────────────┴────────────┐
        │                         │
        ▼                         ▼
  ┌──────────┐            ┌──────────────┐
  │ ONLINE   │◀──────────▶│   OFFLINE    │
  │          │  Network   │              │
  │ - Read   │  Issues    │ - Read       │
  │ - Publish│            │ - Buffer     │
  └────┬─────┘            └──────┬───────┘
       │                         │
       └────────────┬────────────┘
                    │
                    ▼
         ┌─────────────────────────────────┐
         │        SLEEPING                 │
         │  - Deep Sleep Mode              │
         │  - Wake Timer Set               │
         │  - Low Power State              │
         └───────────┬─────────────────────┘
                     │
                     └──────▶ (Wake & Loop)
```

## Thread Interaction

```
Time ──────────────────────────────────────────▶

Sensor    │▓▓▓│              │▓▓▓│              │
Thread    Read               Read
          └──signal──┐        └──signal──┐
                     │                   │
Publish             │▓▓▓▓│              │▓▓▓▓│
Thread            Publish            Publish
                    │                   │
MQTT      │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
Thread        Maintain Connection & Process

Legend:
  ▓▓▓  = Active processing
  │    = Waiting/Idle
```

## Power State Transitions

```
                 ┌──────────────────┐
         ┌──────▶│  ACTIVE (50mA)   │◀───────┐
         │       │  - WiFi TX/RX    │        │
         │       │  - CPU 240MHz    │        │
         │       │  - Sensors ON    │        │
         │       └────────┬─────────┘        │
         │                │                  │
    Wake │                │ Work Complete    │ Interrupt
    Timer│                ▼                  │ (Optional)
         │       ┌──────────────────┐        │
         │       │ LIGHT SLEEP      │        │
         │       │  (<5mA)          │────────┘
         │       │  - WiFi OFF      │
         │       │  - CPU Idle      │
         │       └────────┬─────────┘
         │                │
         │                │ Timer Expire
         │                ▼
         │       ┌──────────────────┐
         └───────│  DEEP SLEEP      │
                 │  (<1mA)          │
                 │  - All OFF       │
                 │  - RTC Active    │
                 └──────────────────┘
```

## Security Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     ESP32-S3 Device                         │
│                                                             │
│  ┌────────────────────────────────────────────────────┐    │
│  │        Application Layer (main.c)                  │    │
│  └───────────────────┬────────────────────────────────┘    │
│                      │                                      │
│  ┌───────────────────▼────────────────────────────────┐    │
│  │        MQTT Client (Zephyr)                        │    │
│  └───────────────────┬────────────────────────────────┘    │
│                      │                                      │
│  ┌───────────────────▼────────────────────────────────┐    │
│  │        TLS 1.2 Layer (mbedTLS)                     │    │
│  │  - Server Auth (Root CA)                           │    │
│  │  - Client Auth (Device Cert + Key)                 │    │
│  │  - AES-256 Encryption                              │    │
│  └───────────────────┬────────────────────────────────┘    │
│                      │                                      │
│  ┌───────────────────▼────────────────────────────────┐    │
│  │        WiFi Stack (ESP32-S3)                       │    │
│  └───────────────────┬────────────────────────────────┘    │
└──────────────────────┼──────────────────────────────────────┘
                       │
                  Encrypted
                  MQTT/TLS
                       │
                       ▼
          ┌────────────────────────┐
          │   AWS IoT Core         │
          │   - Verify Client Cert │
          │   - Check Policy       │
          │   - Route to Topics    │
          └────────────────────────┘
```

## File Organization

```
kargo_iot_sensor/
│
├── Core Application
│   ├── src/main.c                 [Main Logic]
│   └── CMakeLists.txt             [Build Config]
│
├── Configuration
│   ├── prj.conf                   [Zephyr Config]
│   ├── sample.conf                [Template]
│   ├── .env.template              [Environment]
│   └── esp32s3_devkitc.overlay   [Device Tree]
│
├── Security
│   ├── certs/                     [Certificates]
│   │   ├── generate_credentials.py
│   │   ├── README.md
│   │   └── .gitignore
│   └── src/aws_iot_credentials.h [Generated]
│
├── Build Tools
│   ├── build.sh                   [Linux/macOS]
│   └── build.ps1                  [Windows]
│
├── Documentation
│   ├── README.md                  [Complete Guide]
│   ├── QUICKSTART.md             [Quick Setup]
│   ├── PROJECT_SUMMARY.md        [Overview]
│   └── ARCHITECTURE.md           [This File]
│
└── Testing
    └── sample.yaml                [Test Config]
```

---

**Note**: This architecture supports scalability to thousands of devices
with proper AWS IoT fleet management and certificate rotation.
