# Example: Basic Event Capture and Publishing

This example demonstrates the core functionality of EspVault.

## Overview

This example shows how to:
1. Initialize memory manager
2. Connect to MQTT broker
3. Capture events
4. Store in history
5. Publish to broker

## Hardware Required

- ESP32 or ESP32-S3 with 4MB PSRAM
- WiFi connection
- MQTT broker (can use test.mosquitto.org for testing)

## Configuration

Update in `idf.py menuconfig`:
- WiFi SSID and password
- MQTT Broker URI
- Client ID

## Next Steps

- Add GPIO interrupt handling
- Implement RMT peripheral
- Add error handling
