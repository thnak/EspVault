# Provisioning Payload Examples

Quick reference for remote provisioning JSON payloads.

## Minimal Configuration (DHCP + No SSL)

```json
{
  "id": 100,
  "wifi": {
    "s": "MyNetwork",
    "p": "MyPassword"
  },
  "ip": {
    "t": "d"
  },
  "mqtt": {
    "u": "mqtt://broker.local",
    "port": 1883,
    "ssl": false
  }
}
```

## Static IP Configuration

```json
{
  "id": 101,
  "wifi": {
    "s": "OfficeNetwork",
    "p": "SecurePass123"
  },
  "ip": {
    "t": "s",
    "a": "192.168.1.150",
    "g": "192.168.1.1",
    "m": "255.255.255.0"
  },
  "mqtt": {
    "u": "mqtt://192.168.1.100",
    "port": 1883,
    "ssl": false
  }
}
```

## Production with SSL/TLS

```json
{
  "id": 200,
  "wifi": {
    "s": "ProductionWiFi",
    "p": "VerySecurePassword"
  },
  "ip": {
    "t": "d"
  },
  "mqtt": {
    "u": "mqtts://prod.broker.io",
    "port": 8883,
    "ssl": true,
    "cert": "-----BEGIN CERTIFICATE-----\nMIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\nMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\nd3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\nQTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\nMRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\nb20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIB...\n-----END CERTIFICATE-----",
    "user": "device_vault_001",
    "pass": "DeviceSecretKey123"
  }
}
```

## Full Configuration with Client Certificate

```json
{
  "id": 300,
  "wifi": {
    "s": "SecureNetwork",
    "p": "NetworkPassword"
  },
  "ip": {
    "t": "s",
    "a": "10.0.1.50",
    "g": "10.0.1.1",
    "m": "255.255.255.0"
  },
  "mqtt": {
    "u": "mqtts://secure.broker.com",
    "port": 8883,
    "ssl": true,
    "cert": "-----BEGIN CERTIFICATE-----\n[CA Certificate PEM]\n-----END CERTIFICATE-----",
    "key": "-----BEGIN PRIVATE KEY-----\n[Client Private Key PEM]\n-----END PRIVATE KEY-----",
    "user": "device_001",
    "pass": "device_secret"
  }
}
```

## Field Reference

### WiFi Fields
| Field | Type | Required | Max Length | Description |
|-------|------|----------|------------|-------------|
| `s` | string | Yes | 32 | WiFi SSID |
| `p` | string | Yes | 64 | WiFi Password |

### IP Fields
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `t` | string | Yes | `"d"` for DHCP, `"s"` for static |
| `a` | string | If static | IP address (e.g., "192.168.1.100") |
| `g` | string | If static | Gateway address |
| `m` | string | If static | Subnet mask |

### MQTT Fields
| Field | Type | Required | Max Length | Description |
|-------|------|----------|------------|-------------|
| `u` | string | Yes | 128 | Broker URI (mqtt:// or mqtts://) |
| `port` | number | Yes | - | Broker port (1883 or 8883) |
| `ssl` | boolean | Yes | - | Enable SSL/TLS |
| `cert` | string | If ssl=true | 2048 | CA certificate in PEM format |
| `key` | string | Optional | 2048 | Client private key in PEM format |
| `user` | string | Optional | 64 | MQTT username |
| `pass` | string | Optional | 64 | MQTT password |

## Size Limits

- **Maximum total payload**: 8192 bytes (8 KB)
- **Certificate max size**: 2048 bytes per certificate
- **URI max length**: 128 bytes
- **Credentials max length**: 64 bytes each

## Certificate Format Notes

1. **Use escaped newlines**: `\n` in JSON strings
2. **Include headers/footers**: `-----BEGIN CERTIFICATE-----` and `-----END CERTIFICATE-----`
3. **No extra whitespace**: Remove leading/trailing spaces
4. **Validate PEM format**: Use `openssl x509 -text` to verify

Example of properly formatted certificate in JSON:
```json
"cert": "-----BEGIN CERTIFICATE-----\nMIIDrzCCApegAwIBAgIQCDvg...\n-----END CERTIFICATE-----"
```

## Testing Payloads

### With `curl` and Mosquitto

```bash
# Publish to MQTT broker
mosquitto_pub -h staging.broker.com -p 1883 \
  -t "dev/cfg/aabbccddeeff" \
  -q 1 \
  -f config.json \
  -D PUBLISH response-topic "dev/res/aabbccddeeff" \
  -D PUBLISH correlation-data "test-session-123"
```

### With Python

```python
import json

config = {
    "id": 100,
    "wifi": {"s": "TestNet", "p": "TestPass"},
    "ip": {"t": "d"},
    "mqtt": {
        "u": "mqtt://test.local",
        "port": 1883,
        "ssl": False
    }
}

# Validate JSON
json_str = json.dumps(config)
print(f"Payload size: {len(json_str)} bytes")
print(json_str)

# Verify it can be parsed
parsed = json.loads(json_str)
print("✓ JSON is valid")
```

## Common Mistakes

### ❌ Missing Required Fields
```json
{
  "wifi": {"s": "Network"},  // Missing password
  "mqtt": {"u": "mqtt://broker"}  // Missing port and ssl
}
```

### ❌ Invalid IP Type
```json
{
  "ip": {
    "t": "static",  // Should be "s", not "static"
    "a": "192.168.1.100"
  }
}
```

### ❌ Certificate Without Newlines
```json
{
  "mqtt": {
    "cert": "-----BEGIN CERTIFICATE-----MIIDrzCCApe..."  // Missing \n
  }
}
```

### ✓ Correct Format
```json
{
  "id": 100,
  "wifi": {"s": "Network", "p": "Password"},
  "ip": {"t": "s", "a": "192.168.1.100", "g": "192.168.1.1", "m": "255.255.255.0"},
  "mqtt": {
    "u": "mqtts://broker.io",
    "port": 8883,
    "ssl": true,
    "cert": "-----BEGIN CERTIFICATE-----\nMIIDrzCCApe...\n-----END CERTIFICATE-----",
    "user": "device",
    "pass": "secret"
  }
}
```

## Response Examples

### Success Response
```json
{
  "cor_id": "test-session-123",
  "status": "applied",
  "details": "Configuration applied successfully. Device will restart.",
  "mem_report": {
    "free_heap": 85120,
    "free_psram": 3145728
  }
}
```

### WiFi Error Response
```json
{
  "cor_id": "test-session-123",
  "status": "wifi_failed",
  "details": "WiFi configuration validation failed",
  "mem_report": {
    "free_heap": 120000,
    "free_psram": 4000000
  }
}
```

### Parse Error Response
```json
{
  "cor_id": "test-session-123",
  "status": "parse_error",
  "details": "Failed to parse JSON configuration",
  "mem_report": {
    "free_heap": 120000,
    "free_psram": 4000000
  }
}
```

## See Also

- [Remote Provisioning Guide](REMOTE_PROVISIONING.md) - Complete documentation
- [MQTT v5 Specification](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html) - Protocol details
- [ESP-IDF MQTT Client](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html) - Client reference
