# Testing Guide for EspVault

This document outlines the testing strategy for EspVault Universal Node firmware.

## Testing Strategy

EspVault implements a three-tier testing approach:

### Tier 1: Linux Host Testing
Unit tests for protocol logic, memory management, and sequence counters.

### Tier 2: QEMU Emulation
Bootloader, partition table, and NVS operations testing.

### Tier 3: Hardware-in-Loop (Wokwi CI)
RMT peripheral testing and dual-core race condition detection.

## Unit Tests

### Protocol Tests

Test the binary protocol implementation:

```c
#include "unity.h"
#include "vault_protocol.h"

void test_packet_init(void) {
    vault_packet_t packet;
    vault_protocol_init_packet(&packet, VAULT_CMD_EVENT, 123);
    
    TEST_ASSERT_EQUAL(VAULT_PROTO_HEAD, packet.head);
    TEST_ASSERT_EQUAL(VAULT_CMD_EVENT, packet.cmd);
    TEST_ASSERT_EQUAL(123, packet.seq);
}

void test_crc_calculation(void) {
    uint8_t data[] = {0xAA, 0x01, 0x00, 0x00, 0x00, 0x01};
    uint8_t crc = vault_protocol_calc_crc8(data, sizeof(data));
    
    TEST_ASSERT_NOT_EQUAL(0, crc);
}

void test_packet_validation(void) {
    vault_packet_t packet;
    vault_protocol_init_packet(&packet, VAULT_CMD_EVENT, 456);
    vault_protocol_finalize_packet(&packet);
    
    TEST_ASSERT_TRUE(vault_protocol_validate_packet(&packet));
    
    // Corrupt CRC
    packet.crc = 0xFF;
    TEST_ASSERT_FALSE(vault_protocol_validate_packet(&packet));
}
```

### Memory Tests

Test PSRAM management and ring buffer operations:

```c
void test_sequence_counter(void) {
    vault_memory_t *mem = vault_memory_init();
    TEST_ASSERT_NOT_NULL(mem);
    
    uint32_t seq1 = vault_memory_get_next_seq(mem);
    uint32_t seq2 = vault_memory_get_next_seq(mem);
    
    TEST_ASSERT_EQUAL(seq1 + 1, seq2);
    
    vault_memory_deinit(mem);
}

void test_history_storage(void) {
    vault_memory_t *mem = vault_memory_init();
    
    vault_packet_t packet;
    vault_protocol_init_packet(&packet, VAULT_CMD_EVENT, 100);
    packet.pin = 5;
    packet.val = 1000;
    vault_protocol_finalize_packet(&packet);
    
    TEST_ASSERT_TRUE(vault_memory_store_history(mem, &packet));
    
    vault_packet_t retrieved;
    TEST_ASSERT_TRUE(vault_memory_find_by_seq(mem, 100, &retrieved));
    TEST_ASSERT_EQUAL(100, retrieved.seq);
    TEST_ASSERT_EQUAL(5, retrieved.pin);
    
    vault_memory_deinit(mem);
}
```

## Integration Tests

### MQTT Connection Test

```c
void test_mqtt_connection(void) {
    vault_memory_t *mem = vault_memory_init();
    
    vault_mqtt_config_t config = {
        .broker_uri = "mqtt://test.mosquitto.org",
        .client_id = "test_client",
        .use_tls = false
    };
    
    vault_mqtt_t *mqtt = vault_mqtt_init(&config, mem);
    TEST_ASSERT_NOT_NULL(mqtt);
    
    TEST_ASSERT_TRUE(vault_mqtt_start(mqtt));
    
    // Wait for connection
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    TEST_ASSERT_TRUE(vault_mqtt_is_connected(mqtt));
    
    vault_mqtt_stop(mqtt);
    vault_mqtt_deinit(mqtt);
    vault_memory_deinit(mem);
}
```

## Running Tests

### Build and Run Tests

```bash
# Navigate to test directory
cd test

# Build tests
idf.py build

# Flash and run
idf.py flash monitor
```

### Expected Output

```
Running vault_protocol tests...
✓ test_packet_init
✓ test_crc_calculation
✓ test_packet_validation

Running vault_memory tests...
✓ test_sequence_counter
✓ test_history_storage

All tests passed!
```

## Performance Testing

### Throughput Test

Measure event capture and network transmission rate:

```c
void test_throughput(void) {
    const int NUM_EVENTS = 1000;
    TickType_t start = xTaskGetTickCount();
    
    for (int i = 0; i < NUM_EVENTS; i++) {
        vault_packet_t packet;
        vault_protocol_init_packet(&packet, VAULT_CMD_EVENT, i);
        vault_protocol_finalize_packet(&packet);
        
        vault_memory_store_history(g_memory, &packet);
        vault_memory_queue_network(g_memory, &packet, 100);
    }
    
    TickType_t end = xTaskGetTickCount();
    uint32_t duration_ms = (end - start) * portTICK_PERIOD_MS;
    float events_per_sec = (NUM_EVENTS * 1000.0) / duration_ms;
    
    printf("Throughput: %.2f events/sec\n", events_per_sec);
}
```

### Memory Usage Test

Monitor heap and PSRAM usage:

```c
void test_memory_usage(void) {
    printf("Initial free heap: %lu bytes\n", esp_get_free_heap_size());
    printf("Initial free PSRAM: %lu bytes\n", 
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    vault_memory_t *mem = vault_memory_init();
    
    printf("After init free heap: %lu bytes\n", esp_get_free_heap_size());
    printf("After init free PSRAM: %lu bytes\n", 
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    // Fill history buffer
    for (uint32_t i = 0; i < 10000; i++) {
        vault_packet_t packet;
        vault_protocol_init_packet(&packet, VAULT_CMD_EVENT, i);
        vault_protocol_finalize_packet(&packet);
        vault_memory_store_history(mem, &packet);
    }
    
    printf("After 10k events free heap: %lu bytes\n", esp_get_free_heap_size());
    printf("After 10k events free PSRAM: %lu bytes\n", 
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    vault_memory_deinit(mem);
}
```

## Continuous Integration

### GitHub Actions Workflow

```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Install ESP-IDF
      uses: espressif/install-esp-idf-action@v1
      with:
        version: 'v5.5'
        target: 'esp32'
    
    - name: Build firmware
      run: |
        . $IDF_PATH/export.sh
        idf.py build
    
    - name: Run unit tests
      run: |
        . $IDF_PATH/export.sh
        cd test
        idf.py build flash monitor
```

## Debugging

### Enable Verbose Logging

```c
esp_log_level_set("vault_protocol", ESP_LOG_DEBUG);
esp_log_level_set("vault_memory", ESP_LOG_DEBUG);
esp_log_level_set("vault_mqtt", ESP_LOG_DEBUG);
```

### Core Dump Analysis

Enable core dumps in sdkconfig:
```
CONFIG_ESP_COREDUMP_ENABLE=y
CONFIG_ESP_COREDUMP_TO_FLASH_OR_UART=y
```

Analyze core dump:
```bash
idf.py coredump-info
```

## References

- [ESP-IDF Unit Testing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)
- [Wokwi CI](https://docs.wokwi.com/guides/continuous-integration)
