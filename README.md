# Marrty IFR31 - Firmware (ESP32-S3)

This is the firmware for the ESP32-S3 device used in the Marrty IFR31 Student Face Attendance System.

## Features
- Connects to AWS IoT Core (MQTTS).
- Captures images using OV2640 camera.
- Encodes images to Base64.
- Publishes to `marrty/DEVICE_ID/recognize`.
- Subscribes to `marrty/DEVICE_ID/result`.

## Setup
1. Install [PlatformIO](https://platformio.org/).
2. Open this folder in VS Code with PlatformIO extension.
3. Edit `src/secrets.h` with your WiFi credentials and AWS Certs.
   - Root CA 1
   - Device Certificate
   - Device Private Key
   - IoT Endpoint
4. Flash to ESP32-S3.

## Hardware
- ESP32-S3 DevKitC-1
- OV2640 Camera
