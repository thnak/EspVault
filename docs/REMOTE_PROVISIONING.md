# Remote Provisioning Guide for EspVault

This document describes the remote provisioning system using MQTT v5 for EspVault Universal Node devices.

## Overview

The remote provisioning system allows you to configure ESP32 devices remotely without physical access. It uses:
- **Staging Network**: A dedicated WiFi/MQTT network for initial configuration
- **MQTT v5 Protocol**: Request-Response pattern with Response Topic and Correlation Data
- **Setup Mode**: Suspends operational tasks to free resources for large payloads
- **Dry-Run Testing**: Tests configuration before committing to prevent bricking

## Architecture

### Staging vs Production Networks

```
┌─────────────────────┐
│  Staging Network    │
│  (Configuration)    │
│                     │
│  WiFi: Staging_Net  │
│  MQTT: staging.com  │
└──────────┬──────────┘
           │
           │ Provisioning
           │ Command
           ▼
    ┌──────────────┐
    │   ESP32      │
    │   Device     │
    └──────┬───────┘
           │
           │ After Success
           │ & Restart
           ▼
┌─────────────────────┐
│ Production Network  │
│ (Operation)         │
│                     │
│ WiFi: Prod_Office   │
│ MQTT: prod.io:8883  │
└─────────────────────┘
```

### Resource Management in Setup Mode

When entering Setup Mode:
1. **Suspend Tasks**: Capture, Logic, and Health tasks are suspended
2. **Free PSRAM**: Existing buffers are freed for configuration parsing
3. **Parse Config**: Large JSON payloads (up to 8KB) can be processed
4. **Allocate Certificates**: SSL certificates stored in PSRAM if needed

## MQTT Topics

### Configuration Topic
```
dev/cfg/{MAC_ADDRESS}
```
Example: `dev/cfg/aabbccddeeff`

### Response Topic
```
dev/res/{MAC_ADDRESS}
```
Example: `dev/res/aabbccddeeff`

## Provisioning Protocol

### Request Message (MQTT v5)

**Topic**: `dev/cfg/{MAC}`  
**QoS**: 1 (At least once delivery)  
**Properties**:
- `Response-Topic`: `dev/res/{MAC}` (where device should respond)
- `Correlation-Data`: Unique session ID for tracking

**Payload** (JSON):
```json
{
  "id": 201,
  "wifi": {
    "s": "Office_Guest",
    "p": "SecurePassword123"
  },
  "ip": {
    "t": "s",
    "a": "192.168.10.15",
    "g": "192.168.10.1",
    "m": "255.255.255.0"
  },
  "mqtt": {
    "u": "mqtts://prod.broker.io",
    "port": 8883,
    "ssl": true,
    "cert": "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----",
    "key": "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----",
    "user": "device_001",
    "pass": "device_secret"
  }
}
```

### Payload Field Definitions

#### WiFi Configuration
- `s` (ssid): Network SSID (max 32 characters)
- `p` (password): Network password (max 64 characters)

#### IP Configuration
- `t` (type): `"d"` for DHCP, `"s"` for static
- `a` (address): Static IP address (e.g., "192.168.1.100")
- `g` (gateway): Gateway address
- `m` (netmask): Subnet mask

#### MQTT Configuration
- `u` (uri): Broker URI (mqtt:// or mqtts://)
- `port`: Broker port (1883 for TCP, 8883 for TLS)
- `ssl`: Boolean flag to enable SSL/TLS
- `cert`: CA certificate in PEM format (optional, only if ssl=true)
- `key`: Client private key in PEM format (optional, only if ssl=true)
- `user`: MQTT username (optional)
- `pass`: MQTT password (optional)

### Response Message (MQTT v5)

**Topic**: Value from Request's Response-Topic  
**QoS**: 1  
**Properties**:
- `Correlation-Data`: Same value from request

**Payload** (JSON):
```json
{
  "cor_id": "unique-session-123",
  "status": "applied",
  "details": "Configuration applied successfully. Device will restart.",
  "mem_report": {
    "free_heap": 85120,
    "free_psram": 3145728
  }
}
```

### Response Status Values

| Status | Description |
|--------|-------------|
| `applied` | Configuration validated and applied successfully |
| `wifi_failed` | WiFi configuration validation failed |
| `mqtt_failed` | MQTT configuration validation failed |
| `parse_error` | JSON parsing error |
| `memory_error` | Insufficient memory or NVS error |
| `invalid_config` | Configuration parameters invalid |

## Provisioning Workflow

### Step-by-Step Process

1. **Device Boots**: Connects to staging network using default/fallback config
2. **Broker Sends Config**: Publishes provisioning message to `dev/cfg/{MAC}`
3. **Device Receives**: Enters Setup Mode, suspends operational tasks
4. **Parse Configuration**: Validates JSON and extracts parameters
5. **Dry-Run Test**:
   - Validates WiFi credentials format
   - Validates MQTT broker configuration
   - Checks IP configuration (if static)
6. **Save to NVS**: Stores configuration for next boot
7. **Send Response**: Reports success/failure to Response Topic
8. **Restart** (on success): Device reboots and connects to production network
9. **Rollback** (on failure): Stays on staging network, awaits new config

### Error Handling

- **Parse Error**: Invalid JSON → Send error response, stay in staging
- **WiFi Failed**: Invalid credentials → Send error response, stay in staging
- **MQTT Failed**: Invalid broker → Send error response, stay in staging
- **Memory Error**: Insufficient PSRAM → Send error response, free memory

## Payload Size Limits

To ensure compatibility with ESP32 MQTT client buffers:

| Parameter | Maximum Size |
|-----------|-------------|
| SSID | 32 bytes |
| Password | 64 bytes |
| Broker URI | 128 bytes |
| CA Certificate | 2048 bytes |
| Client Key | 2048 bytes |
| Total JSON Payload | 8192 bytes (8 KB) |

**Note**: Large certificates may require payload chunking in future versions.

## Security Considerations

### Default Configuration

Always maintain a default/fallback configuration in NVS to prevent bricking:

```c
// Save default staging config on first boot
vault_prov_config_t default_config = {
    .wifi = {
        .ssid = "Staging_Network",
        .password = "staging_pass"
    },
    .mqtt = {
        .broker_uri = "mqtt://staging.local",
        .port = 1883,
        .use_ssl = false
    }
};
vault_provisioning_save_config(&default_config, true);  // is_default=true
```

### SSL/TLS Recommendations

1. **Always use TLS in production**: Set `ssl: true`
2. **Provide CA certificate**: Validates broker identity
3. **Use strong passwords**: For MQTT authentication
4. **Rotate credentials**: Regular key rotation policy

### NVS Encryption

Enable NVS encryption in production:
```
CONFIG_NVS_ENCRYPTION=y
```

## Example Implementations

### Python Broker Script

```python
import paho.mqtt.client as mqtt
import json
import uuid

def provision_device(mac_address, config):
    client = mqtt.Client(protocol=mqtt.MQTTv5)
    client.connect("staging.broker.com", 1883)
    
    # Generate unique correlation ID
    correlation_id = str(uuid.uuid4())
    
    # Set MQTT v5 properties
    properties = mqtt.Properties(mqtt.PacketTypes.PUBLISH)
    properties.ResponseTopic = f"dev/res/{mac_address}"
    properties.CorrelationData = correlation_id.encode()
    
    # Publish configuration
    topic = f"dev/cfg/{mac_address}"
    payload = json.dumps(config)
    
    client.publish(topic, payload, qos=1, properties=properties)
    
    # Subscribe to response
    client.subscribe(f"dev/res/{mac_address}")
    
    print(f"Provisioning command sent. Correlation ID: {correlation_id}")
    client.loop_forever()

# Example usage
config = {
    "id": 201,
    "wifi": {
        "s": "ProductionWiFi",
        "p": "SecurePass123"
    },
    "ip": {
        "t": "d"  # DHCP
    },
    "mqtt": {
        "u": "mqtts://prod.broker.io",
        "port": 8883,
        "ssl": True,
        "cert": "-----BEGIN CERTIFICATE-----\n...",
        "user": "device_001",
        "pass": "device_secret"
    }
}

provision_device("aabbccddeeff", config)
```

### Node.js Broker Script

```javascript
const mqtt = require('mqtt');

function provisionDevice(macAddress, config) {
    const client = mqtt.connect('mqtt://staging.broker.com', {
        protocolVersion: 5
    });
    
    const correlationId = Buffer.from(
        Math.random().toString(36).substring(7)
    );
    
    client.on('connect', () => {
        const topic = `dev/cfg/${macAddress}`;
        const responseTopic = `dev/res/${macAddress}`;
        
        // Subscribe to response
        client.subscribe(responseTopic);
        
        // Publish with MQTT v5 properties
        client.publish(topic, JSON.stringify(config), {
            qos: 1,
            properties: {
                responseTopic: responseTopic,
                correlationData: correlationId
            }
        });
        
        console.log(`Provisioning sent to ${topic}`);
    });
    
    client.on('message', (topic, message, packet) => {
        const response = JSON.parse(message.toString());
        console.log('Response:', response);
        
        if (response.status === 'applied') {
            console.log('Device provisioned successfully!');
        } else {
            console.error('Provisioning failed:', response.details);
        }
        
        client.end();
    });
}

// Example usage
const config = {
    id: 201,
    wifi: {
        s: "ProductionWiFi",
        p: "SecurePass123"
    },
    ip: {
        t: "d"  // DHCP
    },
    mqtt: {
        u: "mqtts://prod.broker.io",
        port: 8883,
        ssl: true,
        user: "device_001",
        pass: "device_secret"
    }
};

provisionDevice("aabbccddeeff", config);
```

## Monitoring and Debugging

### Enable Verbose Logging

Set log level in menuconfig or via serial:
```
esp_log_level_set("vault_prov", ESP_LOG_VERBOSE);
esp_log_level_set("vault_mqtt", ESP_LOG_VERBOSE);
```

### Memory Monitoring

Response includes memory report:
```json
"mem_report": {
  "free_heap": 85120,
  "free_psram": 3145728
}
```

### Common Issues

1. **Device not responding**
   - Check staging network credentials
   - Verify MAC address in topic
   - Check MQTT broker logs

2. **Parse error**
   - Validate JSON syntax
   - Check payload size (max 8KB)
   - Verify all required fields present

3. **Certificate errors**
   - Check PEM format with proper newlines (`\n`)
   - Verify certificate chain completeness
   - Check certificate expiration

## Production Deployment

### Manufacturing Process

1. **Flash Firmware**: Standard application firmware
2. **Provision Default Config**: Flash staging network credentials
3. **Factory Test**: Verify staging connection
4. **Ship Device**: Device ready for remote provisioning

### First Boot Sequence

```
1. Device boots with staging credentials (from NVS default)
2. Connects to staging WiFi
3. Connects to staging MQTT broker
4. Subscribes to dev/cfg/{MAC}
5. Waits for provisioning command
6. Receives config → validates → saves → restarts
7. Boots with production credentials
8. Connects to production network
9. Normal operation begins
```

## API Reference

See header files for complete API documentation:
- `components/vault_provisioning/vault_provisioning.h`
- `components/vault_mqtt/vault_mqtt.h`

## Future Enhancements

- [ ] Payload chunking for large certificates (>2KB)
- [ ] Physical button trigger for setup mode
- [ ] BLE provisioning as alternative
- [ ] Batch provisioning support
- [ ] Configuration templates
- [ ] Web-based provisioning portal
- [ ] OTA firmware update integration

## Support

For issues or questions:
- GitHub Issues: [EspVault Repository](https://github.com/thnak/EspVault)
- Documentation: See `docs/` directory

## License

See LICENSE file in repository root.
