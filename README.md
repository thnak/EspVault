# EspVault - Universal Node for ESP32

A data-driven "Universal Node" firmware for Dual-Core ESP32/ESP32-S3 that enables dynamic hardware configuration via MQTT, designed for IoT deployments requiring zero-loss data integrity.

## Features

### Core Capabilities
- **Dynamic Configuration**: Remote hardware behavior definition via MQTT broker
- **Binary Protocol**: Efficient 13-byte packet structure for low-bandwidth communication
- **Zero-Loss Logging**: Sequential event tracking with gap detection and replay
- **Dual-Core Architecture**: Optimized task distribution across ESP32 cores
- **4MB PSRAM Management**: 
  - 2MB Flight Recorder history buffer
  - 1MB Network queue for offline protection
- **Lock-free Communication**: esp_ringbuf for cross-core data transfer

### Hardware Support
- ESP32 / ESP32-S3 (Dual-Core)
- Minimum 4MB PSRAM required
- RMT peripheral for microsecond-precision pulse width measurement
- WiFi (MQTT) + ESP-NOW for mesh networking

## Project Structure

```
EspVault/
├── components/
│   ├── vault_protocol/      # Binary packet parsing and serialization
│   ├── vault_memory/         # 4MB PSRAM management and ring buffers
│   └── vault_mqtt/           # MQTT 5.0 client with replay support
├── factory/
│   ├── certs/                # Production SSL certificates
│   └── nvs_config.csv        # Factory provisioning template
├── main/
│   └── main.c                # Dual-core task initialization
├── partitions.csv            # 4MB Flash partition table
└── CMakeLists.txt
```

## Task Architecture

| Task Name | Priority | Core | Responsibility |
|-----------|----------|------|----------------|
| **Capture Task** | 15 (Max) | PRO_CPU (0) | RMT interrupt handling, hardware pulse timing |
| **Logic Task** | 10 | PRO_CPU (0) | PSRAM history indexing, sequence counters |
| **Network Task** | 5 | APP_CPU (1) | WiFi, MQTT 5.0, TLS/SSL encryption |
| **Health Task** | 1 | APP_CPU (1) | Diagnostics, system heartbeats |

## Binary Packet Protocol

13-byte packet structure with CRC8 validation:

| Offset | Type | Field | Description |
|--------|------|-------|-------------|
| 0 | uint8_t | HEAD | Start of Frame (0xAA) |
| 1 | uint8_t | CMD | Command ID (0x01=Config, 0x04=Replay) |
| 2-5 | uint32_t | SEQ | Unique Sequence Counter |
| 6 | uint8_t | PIN | Target GPIO Index |
| 7 | uint8_t | FLAGS | Bit 0: Is Replay?, Bit 1: Input State |
| 8-11 | uint32_t | VAL | Pulse Width (µs) or Data Value |
| 12 | uint8_t | CRC | CRC8 Checksum |

## Building

### Prerequisites
- ESP-IDF v5.5 or later
- ESP32 or ESP32-S3 target
- 4MB PSRAM module

### Build Commands

```bash
# Set target
idf.py set-target esp32

# Clean build (recommended for first build or after updates)
idf.py fullclean
rm -f sdkconfig sdkconfig.old

# Build
idf.py build

# Flash
idf.py flash monitor
```

**Build Issues?** See [Build Troubleshooting Guide](docs/BUILD_TROUBLESHOOTING.md) for common problems and solutions.

### Key Configuration Options

Enable in `sdkconfig` or via `idf.py menuconfig`:

```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_FREERTOS_UNICORE=n  # Dual-core mode
```

## Security Features (Production)

### Flash Encryption
```bash
idf.py efuse-burn-key BLOCK1 flash_encryption_key.bin FLASH_ENCRYPTION
```

### Secure Boot V2
```bash
idf.py secure-boot-v2-generate-key secure_boot_signing_key.pem
idf.py efuse-burn-key BLOCK2 secure_boot_signing_key.pem SECURE_BOOT_V2
```

## Factory Provisioning

### Generate NVS Partition

```bash
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate \
  factory/nvs_config.csv factory_nvs.bin 0x6000
```

### Flash Factory Partition

```bash
esptool.py --port /dev/ttyUSB0 write_flash 0x10000 factory_nvs.bin
```

## MQTT Topics

- `vault/event` - Event notifications with sensor data
- `vault/config` - Configuration updates from broker
- `vault/heartbeat` - Periodic health checks
- `vault/command` - Control commands (replay requests)

## Data Integrity

### Sequence Counter Management
- Every event assigned unique uint32_t sequence number
- Counter synced to NVS every 10 events
- Persistent across power cycles

### Replay Mechanism
Broker can request missing packets:
```
CMD_REPLAY_FROM [SEQ_START] TO [SEQ_END]
```

ESP32 searches 2MB PSRAM history and republishes with `IS_REPLAY` flag set.

## Remote Provisioning (MQTT v5)

EspVault supports **remote provisioning** via MQTT v5 for zero-touch deployment:

- **Staging Network**: Dedicated WiFi/MQTT network for initial configuration
- **Setup Mode**: Suspends operational tasks to free resources for large payloads
- **Request-Response**: MQTT v5 Response Topic pattern with Correlation Data
- **Dry-Run Testing**: Validates configuration before committing to prevent bricking
- **SSL/TLS Support**: Optional certificate provisioning (up to 2KB per cert)

### Quick Example

```bash
# Send provisioning command
python examples/provisioning/provision_device.py \
  --mac aabbccddeeff \
  --config device_config.json \
  --broker staging.local
```

See documentation:
- [Remote Provisioning Guide](docs/REMOTE_PROVISIONING.md) - Complete workflow and API
- [Default Configuration Guide](docs/DEFAULT_CONFIGURATION.md) - How to set staging credentials
- [Provisioning Examples](docs/PROVISIONING_EXAMPLES.md) - Payload examples and common issues

## Development Milestones

- [x] Phase 1: Project structure and component architecture
- [x] Phase 2: Binary protocol implementation
- [x] Phase 3: PSRAM memory management with ring buffers
- [x] Phase 4: MQTT 5.0 client with replay support
- [x] Phase 5: Dual-core task architecture
- [x] Phase 6: Remote provisioning with MQTT v5 (Staging Network)
- [ ] Phase 7: RMT peripheral integration for pulse capture
- [ ] Phase 8: ESP-NOW gateway functionality
- [ ] Phase 9: Secure Boot and Flash Encryption
- [ ] Phase 10: OTA firmware updates

## Testing Strategy

### Tier 1: Linux Host
- Protocol parsing and serialization
- Sequence counter logic
- Memory management unit tests

### Tier 2: QEMU ✅
- Bootloader validation
- Partition table verification
- NVS operations
- Provisioning configuration tests
- Memory management validation

**Run QEMU tests**:
```bash
cd test/qemu
./run_qemu_tests.sh
```

See [QEMU Test Documentation](test/qemu/README.md) for details.

### Tier 3: Wokwi CI
- Hardware-in-loop simulation
- RMT peripheral testing
- Dual-core race condition detection

## License

[Specify your license here]

## Contributing

[Contribution guidelines]

## Contact

[Contact information]
