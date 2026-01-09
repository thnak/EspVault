# QEMU Network Testing Guide

This guide explains how to test EspVault's remote provisioning system using QEMU's Ethernet networking capabilities and a real MQTT broker.

## Overview

QEMU provides user-mode networking (SLIRP) that allows the emulated ESP32 to access the host machine's network. This enables testing of:

- Ethernet connectivity
- MQTT broker communication
- End-to-end provisioning workflow
- Network error handling
- SSL/TLS connections (with proper broker setup)

## Network Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Host Machine                          │
│                                                              │
│  ┌──────────────────┐              ┌──────────────────┐    │
│  │  MQTT Broker     │              │   Test Scripts   │    │
│  │  localhost:1883  │◄────────────►│  Python/Shell    │    │
│  └────────┬─────────┘              └──────────────────┘    │
│           │                                                  │
│           │ 10.0.2.2:1883                                   │
│  ┌────────▼─────────────────────────────────────────────┐  │
│  │             QEMU User Network (SLIRP)                 │  │
│  │          Gateway: 10.0.2.2                            │  │
│  │          Network: 10.0.2.0/24                         │  │
│  └────────┬─────────────────────────────────────────────┘  │
│           │ 10.0.2.15                                       │
│  ┌────────▼─────────────────────────────────────────────┐  │
│  │              ESP32 (QEMU Emulated)                    │  │
│  │   - Ethernet Interface                                │  │
│  │   - MQTT Client (connects to 10.0.2.2:1883)          │  │
│  │   - Provisioning Logic                                │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Prerequisites

### Required Software

```bash
# Ubuntu/Debian
sudo apt-get install mosquitto mosquitto-clients

# macOS
brew install mosquitto

# Verify installation
mosquitto -h
mosquitto_pub --help
mosquitto_sub --help
```

### ESP-IDF with QEMU

Ensure QEMU is installed with ESP-IDF:

```bash
$IDF_PATH/install.sh --enable-qemu
```

## Quick Start

### Option 1: Automated Script

The easiest way to run network tests:

```bash
cd test/qemu
./run_network_tests.sh
```

This script:
1. ✅ Checks for mosquitto installation
2. ✅ Starts MQTT broker on localhost:1883
3. ✅ Builds test firmware
4. ✅ Runs QEMU with network support
5. ✅ Stops broker on completion

### Option 2: Manual Setup

For more control over the testing process:

```bash
# Terminal 1: Start MQTT broker
mosquitto -v -p 1883

# Terminal 2: Monitor MQTT traffic
mosquitto_sub -h localhost -t "dev/#" -v

# Terminal 3: Build and run tests
cd test/qemu
idf.py build
idf.py qemu monitor

# Terminal 4 (optional): Publish test messages
mosquitto_pub -h localhost -t "dev/cfg/aabbccddeeff" -f config.json
```

## Test Suites

### Test Suite 4: Network & Ethernet

Tests basic Ethernet connectivity in QEMU:

#### test_network_info
- Displays QEMU network configuration
- Shows how to access host services
- Provides MQTT broker setup instructions

#### test_network_ethernet_init
- Initializes TCP/IP stack
- Creates Ethernet netif
- Registers event handlers

#### test_network_ethernet_connect
- Simulates Ethernet link up
- Tests DHCP IP acquisition
- Verifies network interface state

#### test_network_mqtt_connect
- Connects to MQTT broker at 10.0.2.2:1883
- Tests MQTT v5 client initialization
- Verifies connection state

#### test_network_mqtt_pubsub
- Subscribes to test topic
- Publishes test message
- Receives and verifies message (loopback)

#### test_network_cleanup
- Stops MQTT client
- Cleans up network resources
- Verifies proper shutdown

### Test Suite 5: Integration Tests

End-to-end provisioning workflow tests:

#### test_integration_full_provisioning_flow
Simulates complete provisioning:
1. Boot with default staging configuration
2. Connect to staging network (Ethernet)
3. Connect to staging MQTT broker
4. Receive provisioning payload
5. Parse and validate configuration
6. Save to NVS
7. Send success response
8. Restart with production configuration

#### test_integration_provisioning_with_ssl
Tests SSL/TLS provisioning:
- Validates SSL configuration parsing
- Tests certificate handling
- Verifies key storage

#### test_integration_provisioning_failure_recovery
Tests error handling:
- Invalid JSON detection
- Missing required fields
- Fallback to default configuration
- Error response generation

#### test_integration_network_simulation_info
Comprehensive setup guide for network testing

## QEMU Network Configuration

### User-Mode Networking (SLIRP)

QEMU uses user-mode networking by default:

| Component | Address |
|-----------|---------|
| Guest IP | 10.0.2.15 |
| Gateway | 10.0.2.2 |
| DNS Server | 10.0.2.3 |
| Host (from guest) | 10.0.2.2 |

### Accessing Host Services

From the QEMU guest (ESP32), access host services at **10.0.2.2**:

```c
// MQTT broker on host
esp_mqtt_client_config_t mqtt_cfg = {
    .broker = {
        .address = {
            .uri = "mqtt://10.0.2.2:1883",
        },
    },
};
```

### Port Forwarding (Optional)

To access guest services from host:

```bash
# Add to QEMU command line
-netdev user,id=net0,hostfwd=tcp::2222-:22

# Now you can SSH to guest
ssh -p 2222 root@localhost
```

## Testing Provisioning Workflow

### Step 1: Start MQTT Broker

```bash
# Terminal 1
mosquitto -v -p 1883
```

Expected output:
```
1673456789: mosquitto version 2.0.15 starting
1673456789: Config loaded from /etc/mosquitto/mosquitto.conf
1673456789: Opening ipv4 listen socket on port 1883
```

### Step 2: Monitor Traffic

```bash
# Terminal 2
mosquitto_sub -h localhost -t "dev/#" -v
```

You'll see messages published to `dev/cfg/*` and `dev/res/*`.

### Step 3: Run QEMU Tests

```bash
# Terminal 3
cd test/qemu
idf.py build
idf.py qemu monitor
```

Expected test output:
```
Test Suite 4: Network & Ethernet (QEMU)

test_network_info... PASS
test_network_ethernet_init... PASS
test_network_ethernet_connect... PASS
test_network_mqtt_connect... PASS
test_network_mqtt_pubsub... PASS
test_network_cleanup... PASS

Test Suite 5: Integration Tests

test_integration_network_simulation_info... PASS
test_integration_full_provisioning_flow... PASS
test_integration_provisioning_with_ssl... PASS
test_integration_provisioning_failure_recovery... PASS
```

### Step 4: Send Provisioning Config

```bash
# Terminal 4
cd ../../examples/provisioning

# Create config
cat > test_config.json << EOF
{
  "id": 200,
  "wifi": {"s": "TestNetwork", "p": "password123"},
  "ip": {"t": "d"},
  "mqtt": {
    "u": "mqtt://production.broker.io",
    "port": 1883,
    "ssl": false
  }
}
EOF

# Send via MQTT
mosquitto_pub -h localhost -t "dev/cfg/aabbccddeeff" -f test_config.json
```

### Step 5: Verify Response

In Terminal 2 (monitoring), you should see:

```
dev/cfg/aabbccddeeff {"id":200,"wifi":{"s":"TestNetwork",...}
dev/res/aabbccddeeff {"status":"applied","details":"Configuration saved",...}
```

## Python Test Script

Use the provisioning script with local broker:

```bash
cd examples/provisioning

python provision_device.py \
  --broker localhost \
  --port 1883 \
  --mac aabbccddeeff \
  --config example_config.json
```

Expected output:
```
✓ Connected to MQTT broker
✓ Configuration validated (size: 342 bytes)
✓ Published to dev/cfg/aabbccddeeff
⏳ Waiting for response...
✓ Response received: applied
✓ Device provisioned successfully
```

## Advanced Testing

### Testing with SSL/TLS

1. Generate certificates:
```bash
# CA certificate
openssl req -new -x509 -days 365 -extensions v3_ca \
  -keyout ca.key -out ca.crt

# Server certificate
openssl genrsa -out server.key 2048
openssl req -out server.csr -key server.key -new
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out server.crt -days 365
```

2. Configure mosquitto for TLS:
```bash
# /etc/mosquitto/conf.d/tls.conf
listener 8883
cafile /path/to/ca.crt
certfile /path/to/server.crt
keyfile /path/to/server.key
```

3. Update test config:
```json
{
  "mqtt": {
    "u": "mqtts://10.0.2.2",
    "port": 8883,
    "ssl": true,
    "cert": "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----"
  }
}
```

### Load Testing

Test multiple devices:

```bash
# Simulate 10 devices
for i in {1..10}; do
  MAC=$(printf "%012x" $i)
  python provision_device.py --broker localhost --mac $MAC &
done
wait
```

### Network Failure Simulation

Test error handling:

```bash
# Stop broker mid-test
killall mosquitto

# Test should handle disconnection gracefully
# Check for retry logic and error messages
```

## Troubleshooting

### Broker Connection Failed

**Symptoms**: MQTT_EVENT_ERROR, connection refused

**Solutions**:
```bash
# Check if broker is running
netstat -an | grep 1883

# Verify broker is listening
sudo lsof -i :1883

# Check firewall
sudo ufw status
```

### QEMU Network Not Working

**Symptoms**: No connectivity from guest

**Solutions**:
```bash
# Verify QEMU network configuration
idf.py qemu --help

# Check QEMU process
ps aux | grep qemu

# Review ESP-IDF logs
idf.py qemu monitor -p /dev/pts/X
```

### Tests Timeout

**Symptoms**: Tests hang or timeout

**Solutions**:
- Increase timeout in run_network_tests.sh (TEST_TIMEOUT)
- Check for infinite loops in test code
- Verify QEMU has enough resources
- Look for blocking operations without timeouts

### Message Not Received

**Symptoms**: MQTT publishes but no data received

**Solutions**:
```bash
# Verify subscription
mosquitto_sub -h localhost -t "#" -v

# Check topic names match
# dev/cfg/[MAC] (incoming)
# dev/res/[MAC] (outgoing)

# Enable MQTT debug logging
ESP_LOG_LEVEL_MQTT=DEBUG
```

## Performance Considerations

QEMU runs slower than real hardware:

| Operation | Real Hardware | QEMU | Factor |
|-----------|--------------|------|--------|
| CPU | 240 MHz | ~50 MHz | 5x slower |
| Network | 100 Mbps | 10-20 Mbps | 5-10x slower |
| Flash | Fast | Slow | 10x slower |

**Adjust timeouts accordingly**:
- Connection timeouts: 10s → 30s
- Data receive timeouts: 5s → 15s
- Retry intervals: 1s → 3s

## CI/CD Integration

### GitHub Actions Example

```yaml
name: QEMU Network Tests

on: [push, pull_request]

jobs:
  network-test:
    runs-on: ubuntu-latest
    
    services:
      mosquitto:
        image: eclipse-mosquitto:latest
        ports:
          - 1883:1883
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.4
          target: esp32
      
      - name: Run Network Tests
        run: |
          cd test/qemu
          ./run_network_tests.sh --clean
```

## References

- [QEMU Networking Documentation](https://wiki.qemu.org/Documentation/Networking)
- [ESP-IDF QEMU Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/qemu.html)
- [Mosquitto Documentation](https://mosquitto.org/documentation/)
- [EspVault Remote Provisioning](../../docs/REMOTE_PROVISIONING.md)

## Support

For network testing issues:
1. Check mosquitto and QEMU logs
2. Verify network configuration
3. Test with mosquitto_pub/sub first
4. Review test code for timing issues
5. Open an issue with full logs and configuration
