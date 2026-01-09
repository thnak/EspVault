# QEMU Tests for EspVault

This directory contains QEMU-based tests for EspVault provisioning and core functionality.

## Overview

QEMU tests run in an emulated ESP32 environment without requiring physical hardware. They validate:

- **NVS Operations**: Storage and retrieval of configuration data
- **Memory Management**: Heap and PSRAM allocation
- **Provisioning**: JSON parsing, configuration validation, save/load operations
- **Network & Ethernet**: Connectivity testing using QEMU's user-mode networking
- **MQTT Communication**: Real broker connectivity and pub/sub testing
- **Integration**: End-to-end provisioning workflows
- **Bootloader**: Partition table and flash layout verification

## Prerequisites

- ESP-IDF v5.4 or later
- QEMU for ESP32 (comes with ESP-IDF)
- Python 3.7+

## Directory Structure

```
test/qemu/
├── CMakeLists.txt              # Project configuration
├── sdkconfig.defaults          # QEMU-specific configuration
├── partitions_qemu.csv         # Partition table for testing
├── main/
│   ├── CMakeLists.txt          # Main component configuration
│   ├── test_main.c             # Test runner and entry point
│   ├── test_nvs.c              # NVS operation tests
│   ├── test_memory.c           # Memory management tests
│   └── test_provisioning.c     # Provisioning configuration tests
├── run_qemu_tests.sh           # Test runner script
└── README.md                   # This file
```

## Running Tests

### Quick Start - Basic Tests

```bash
# Navigate to test directory
cd test/qemu

# Run basic tests (NVS, Memory, Provisioning)
./run_qemu_tests.sh
```

### Network Tests with MQTT Broker

To test real network connectivity and MQTT communication:

```bash
# Run tests with automated broker setup
./run_network_tests.sh

# Or with clean build
./run_network_tests.sh --clean
```

See [NETWORK_TESTING.md](NETWORK_TESTING.md) for detailed network testing guide.

### Manual Execution

```bash
# Navigate to test directory
cd test/qemu

# Build the test firmware
idf.py build

# Run in QEMU
idf.py qemu monitor
```

### Expected Output

```
╔══════════════════════════════════════════════╗
║         EspVault QEMU Test Suite            ║
╚══════════════════════════════════════════════╝

System Information:
  ESP-IDF Version: v5.4.0
  Chip Model: esp32
  Chip Revision: 0
  CPU Cores: 2
  Free Heap: 298516 bytes
  Free PSRAM: 4194304 bytes

===============================================
  Test Suite 1: NVS Operations
===============================================

test_nvs_init_and_read_write... PASS
test_nvs_default_config... PASS

===============================================
  Test Suite 2: Memory Management
===============================================

test_memory_allocation... PASS
test_memory_heap_caps... PASS

===============================================
  Test Suite 3: Provisioning Configuration
===============================================

test_provisioning_json_parse... PASS
test_provisioning_wifi_config... PASS
test_provisioning_mqtt_config... PASS
test_provisioning_save_load... PASS

===============================================
  Test Suite 4: Network & Ethernet (QEMU)
===============================================

test_network_info... PASS
test_network_ethernet_init... PASS
test_network_ethernet_connect... PASS
test_network_mqtt_connect... PASS
test_network_mqtt_pubsub... PASS
test_network_cleanup... PASS

===============================================
  Test Suite 5: Integration Tests
===============================================

test_integration_network_simulation_info... PASS
test_integration_full_provisioning_flow... PASS
test_integration_provisioning_with_ssl... PASS
test_integration_provisioning_failure_recovery... PASS

-----------------------
18 Tests 0 Failures 0 Ignored 
OK

╔══════════════════════════════════════════════╗
║         All Tests Completed                  ║
╚══════════════════════════════════════════════╝
```

## Test Descriptions

### Test Suite 1: NVS Operations

#### test_nvs_init_and_read_write
- Opens NVS namespace
- Writes integer and string values
- Reads back and verifies values
- Tests commit operations

#### test_nvs_default_config
- Stores default provisioning configuration
- Simulates staging network credentials
- Verifies data persistence across open/close cycles

### Test Suite 2: Memory Management

#### test_memory_allocation
- Allocates heap memory
- Writes and verifies patterns
- Checks memory recovery after free

#### test_memory_heap_caps
- Tests internal RAM allocation
- Tests PSRAM allocation (if available)
- Verifies DMA-capable memory
- Checks memory statistics

### Test Suite 3: Provisioning Configuration

#### test_provisioning_json_parse
- Parses complete provisioning JSON payload
- Validates JSON structure
- Extracts configuration fields

#### test_provisioning_wifi_config
- Validates WiFi SSID and password constraints
- Tests DHCP configuration
- Tests static IP configuration
- Verifies field length limits

#### test_provisioning_mqtt_config
- Validates MQTT broker URI format
- Tests port number validation
- Tests SSL/non-SSL configurations
- Validates certificate presence for SSL mode

#### test_provisioning_save_load
- Saves configuration to NVS
- Closes and reopens NVS handle
- Loads configuration back
- Verifies all fields match

## Configuration

### Flash Size

QEMU tests use 4MB flash to match production hardware:

```
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
```

### PSRAM

PSRAM is enabled to test memory management:

```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_BOOT_INIT=y
CONFIG_SPIRAM_SIZE=4194304
```

### Dual-Core

Dual-core mode is enabled to match production:

```
CONFIG_FREERTOS_UNICORE=n
```

## Customizing Tests

### Adding New Tests

1. Create a new test file in `main/`, e.g., `test_mqtt.c`
2. Add test functions following Unity framework conventions
3. Declare test functions as `extern` in `test_main.c`
4. Call `RUN_TEST()` in `app_main()`
5. Update `main/CMakeLists.txt` to include new source file

Example:

```c
// test_mqtt.c
#include "unity.h"
#include "esp_log.h"

void test_mqtt_connect(void)
{
    ESP_LOGI("test_mqtt", "Testing MQTT connection...");
    // Test implementation
    TEST_ASSERT_TRUE(true);
}
```

```c
// test_main.c
extern void test_mqtt_connect(void);

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_mqtt_connect);
    UNITY_END();
}
```

### Adjusting Timeouts

QEMU runs slower than real hardware. Increase timeouts if needed:

```c
vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay
```

## Troubleshooting

### QEMU Not Found

If `idf.py qemu` fails:

```bash
# Install QEMU for ESP32
$IDF_PATH/install.sh --enable-qemu
```

### Test Hangs

- Check for infinite loops
- Verify task delays are appropriate
- Ensure QEMU has enough resources

### NVS Errors

NVS partition must be properly erased between test runs:

```bash
idf.py erase-flash
idf.py flash
idf.py qemu monitor
```

### Memory Errors

If tests fail due to memory issues:
- Check PSRAM configuration
- Verify partition table allocates enough space
- Look for memory leaks in tests

## Continuous Integration

### GitHub Actions

Add QEMU tests to CI pipeline:

```yaml
name: QEMU Tests

on: [push, pull_request]

jobs:
  qemu-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.4
          target: esp32
      
      - name: Build and Run QEMU Tests
        run: |
          cd test/qemu
          ./run_qemu_tests.sh
```

## Network Testing

QEMU provides user-mode networking (SLIRP) that allows testing:

- ✅ **Ethernet connectivity**: TCP/IP stack initialization
- ✅ **MQTT broker communication**: Real broker at 10.0.2.2:1883 (host)
- ✅ **End-to-end provisioning**: Complete workflow testing
- ✅ **SSL/TLS**: Certificate handling (requires broker setup)

### Quick Network Test

```bash
# Terminal 1: Start MQTT broker
mosquitto -v -p 1883

# Terminal 2: Run network tests
cd test/qemu
./run_network_tests.sh
```

See [NETWORK_TESTING.md](NETWORK_TESTING.md) for comprehensive guide.

## Known Limitations

- **WiFi**: QEMU uses Ethernet instead of WiFi (same TCP/IP stack)
- **RMT**: Peripheral simulation is limited
- **Timing**: QEMU runs ~5x slower than real hardware
- **Flash**: Flash wear simulation is simplified
- **Network Performance**: 10-20 Mbps (vs 100 Mbps on hardware)

## References

- [ESP-IDF QEMU Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/qemu.html)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
- [EspVault Provisioning Guide](../../docs/REMOTE_PROVISIONING.md)
- [EspVault Testing Strategy](../../docs/TESTING.md)

## Support

For issues or questions:
- Check existing test output for error messages
- Review ESP-IDF QEMU documentation
- Consult EspVault documentation in `docs/` directory
- Open an issue on the project repository
