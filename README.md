# Marrty IFR31 - Firmware (ESP32-S3)

ESP32-S3 Firmware for Face Capture and AWS IoT Integration.

## Features
- Connects to WiFi/AWS IoT (MQTTS).
- Captures images via OV2640.
- Helper script for Base64 encoding.
- Publishes to `marrty/+/recognize`.

## Setup
1. Edit `src/secrets.h` with credentials.
2. Build via PlatformIO.
3. Flash to ESP32-S3 DevKitC-1.
