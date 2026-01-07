# Remote Provisioning Examples

This directory contains example scripts and configurations for remote provisioning of EspVault devices.

## Files

- `provision_device.py` - Python script for sending provisioning commands
- `test_broker.py` - MQTT v5 test broker monitor for development/testing
- `example_config.json` - Basic configuration example (DHCP, no SSL)
- `example_config_ssl.json` - Advanced configuration with SSL/TLS

## Prerequisites

Install paho-mqtt library:
```bash
pip install paho-mqtt
```

## Basic Usage

### 1. Prepare Configuration File

Edit `example_config.json` with your network details:
```json
{
  "id": 100,
  "wifi": {
    "s": "YourWiFiSSID",
    "p": "YourWiFiPassword"
  },
  "ip": {
    "t": "d"
  },
  "mqtt": {
    "u": "mqtt://your-broker.com",
    "port": 1883,
    "ssl": false
  }
}
```

### 2. Get Device MAC Address

The device MAC address can be found:
- In device logs during boot
- From the MQTT client ID
- Using `esptool.py read_mac`

Format: `aabbccddeeff` or `aa:bb:cc:dd:ee:ff`

### 3. Run Provisioning Script

```bash
python provision_device.py --mac aabbccddeeff --config example_config.json
```

### With Custom Broker

```bash
python provision_device.py \
  --mac aabbccddeeff \
  --config example_config.json \
  --broker staging.example.com \
  --port 1883
```

### With Authentication

```bash
python provision_device.py \
  --mac aabbccddeeff \
  --config example_config.json \
  --broker staging.example.com \
  --user admin \
  --pass secret123
```

## Testing with Test Broker

For development and testing without actual ESP32 hardware, use the test broker monitor:

### 1. Start Test Broker

```bash
python test_broker.py
```

This will:
- Connect to MQTT broker (default: localhost:1883)
- Subscribe to all `dev/cfg/#` and `dev/res/#` topics
- Monitor and display all provisioning traffic
- Show device responses with formatted JSON

### 2. Test with Postman

1. Install Postman and enable MQTT support
2. Create new MQTT request:
   - Protocol: **MQTT 5.0**
   - Broker: `mqtt://localhost:1883`
   - Topic: `dev/cfg/aabbccddeeff` (replace with your test MAC)
3. Add MQTT v5 Properties:
   - **Response Topic**: `dev/res/aabbccddeeff`
   - **Correlation Data**: `test-session-123`
4. Set payload to JSON configuration (see example_config.json)
5. Publish message

The test broker will show both the config command and the (simulated) device response.

### 3. Test with mosquitto_pub

```bash
mosquitto_pub -h localhost -t 'dev/cfg/aabbccddeeff' \
  -q 1 -V 5 \
  -D PUBLISH response-topic 'dev/res/aabbccddeeff' \
  -D PUBLISH correlation-data 'test-123' \
  -m '{"id":100,"wifi":{"s":"TestNet","p":"Pass"},"ip":{"t":"d"},"mqtt":{"u":"mqtt://broker","port":1883,"ssl":false}}'
```

### Test Broker Features

- **Real-time monitoring**: See all provisioning traffic
- **JSON formatting**: Automatically formats responses
- **Status tracking**: Shows success/failure status for each device
- **Memory reports**: Displays device memory information
- **Command line options**:
  ```bash
  python test_broker.py --broker staging.local --port 1883
  python test_broker.py --usage  # Show detailed usage examples
  ```

## Expected Output

### Successful Provisioning

```
📄 Loading configuration from example_config.json
✓ Configuration validated (234 bytes)

🎯 Target device: aabbccddeeff

🔌 Connecting to localhost:1883...
✓ Connected to MQTT broker at localhost:1883
📡 Subscribed to response: dev/res/aabbccddeeff

📤 Sending provisioning command...
  Topic: dev/cfg/aabbccddeeff
  Correlation ID: 4a2b1c3d-5e6f-7a8b-9c0d-1e2f3a4b5c6d
  Payload size: 234 bytes
✓ Command sent, waiting for response...

📩 Response received on dev/res/aabbccddeeff

Response Details:
{
  "cor_id": "4a2b1c3d-5e6f-7a8b-9c0d-1e2f3a4b5c6d",
  "status": "applied",
  "details": "Configuration applied successfully. Device will restart.",
  "mem_report": {
    "free_heap": 85120,
    "free_psram": 3145728
  }
}

✓ SUCCESS: Device provisioned successfully!
  Device will restart and connect to production network.

Memory Report:
  Free Heap: 85,120 bytes
  Free PSRAM: 3,145,728 bytes
```

### Failed Provisioning

```
📩 Response received on dev/res/aabbccddeeff

Response Details:
{
  "cor_id": "4a2b1c3d-5e6f-7a8b-9c0d-1e2f3a4b5c6d",
  "status": "wifi_failed",
  "details": "WiFi configuration validation failed",
  "mem_report": {
    "free_heap": 120000,
    "free_psram": 4000000
  }
}

✗ FAILED: Status = wifi_failed
  Details: WiFi configuration validation failed
```

## Configuration Options

### WiFi Settings

```json
"wifi": {
  "s": "NetworkName",    // SSID (max 32 chars)
  "p": "Password123"     // Password (max 64 chars)
}
```

### IP Configuration

**DHCP (Automatic):**
```json
"ip": {
  "t": "d"
}
```

**Static IP:**
```json
"ip": {
  "t": "s",
  "a": "192.168.1.100",   // IP address
  "g": "192.168.1.1",     // Gateway
  "m": "255.255.255.0"    // Netmask
}
```

### MQTT Configuration

**Without SSL:**
```json
"mqtt": {
  "u": "mqtt://broker.local",
  "port": 1883,
  "ssl": false
}
```

**With SSL/TLS:**
```json
"mqtt": {
  "u": "mqtts://secure.broker.com",
  "port": 8883,
  "ssl": true,
  "cert": "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----",
  "user": "device_001",
  "pass": "secret"
}
```

## Troubleshooting

### Device Not Responding

1. Check device is connected to staging network
2. Verify MAC address is correct
3. Check MQTT broker logs
4. Ensure device subscribed to `dev/cfg/{MAC}` topic

### Parse Error

1. Validate JSON syntax: `python -m json.tool config.json`
2. Check payload size (max 8KB)
3. Verify all required fields present

### Connection Timeout

1. Increase timeout: `--timeout 60`
2. Check network connectivity
3. Verify broker is accessible
4. Check firewall rules

### Certificate Issues

1. Use proper PEM format with `\n` newlines
2. Check certificate size (max 2KB)
3. Verify certificate chain is complete
4. Test certificate with `openssl s_client`

## Advanced Usage

### Batch Provisioning

Create a script to provision multiple devices:

```bash
#!/bin/bash
for mac in aa:bb:cc:dd:ee:01 aa:bb:cc:dd:ee:02 aa:bb:cc:dd:ee:03; do
    echo "Provisioning device $mac..."
    python provision_device.py \
        --mac "$mac" \
        --config device_config.json \
        --broker staging.local
    sleep 5
done
```

### Custom Configurations

Generate device-specific configs:

```python
import json

def generate_config(device_id, ip_address):
    return {
        "id": device_id,
        "wifi": {
            "s": "ProductionNet",
            "p": "SecurePass"
        },
        "ip": {
            "t": "s",
            "a": ip_address,
            "g": "192.168.1.1",
            "m": "255.255.255.0"
        },
        "mqtt": {
            "u": "mqtt://broker.local",
            "port": 1883,
            "ssl": False
        }
    }

# Generate configs for 10 devices
for i in range(1, 11):
    config = generate_config(i, f"192.168.1.{100+i}")
    with open(f"device_{i:03d}_config.json", 'w') as f:
        json.dump(config, f, indent=2)
```

## See Also

- [Remote Provisioning Guide](../../docs/REMOTE_PROVISIONING.md) - Complete documentation
- [Payload Examples](../../docs/PROVISIONING_EXAMPLES.md) - More configuration examples
- [MQTT v5 Specification](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html)

## Support

For issues:
- GitHub Issues: [EspVault Repository](https://github.com/thnak/EspVault)
- Check device logs via serial monitor
- Enable verbose MQTT logging in broker
