# Architecture Overview

This document provides a detailed overview of the EspVault Universal Node architecture.

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        ESP32 Dual-Core                       │
├─────────────────────────────┬───────────────────────────────┤
│         Core 0 (PRO_CPU)    │      Core 1 (APP_CPU)        │
│                             │                               │
│  ┌────────────────────┐    │   ┌─────────────────────┐    │
│  │  Capture Task      │    │   │  Network Task       │    │
│  │  Priority: 15      │    │   │  Priority: 5        │    │
│  │  • RMT ISR         │────┼──▶│  • WiFi Stack       │    │
│  │  • Pulse Timing    │    │   │  • MQTT Client      │    │
│  │  • Event Creation  │    │   │  • TLS/SSL          │    │
│  └────────────────────┘    │   └─────────────────────┘    │
│           │                 │             ▲                 │
│           ▼                 │             │                 │
│  ┌────────────────────┐    │   ┌─────────────────────┐    │
│  │  Logic Task        │    │   │  Health Task        │    │
│  │  Priority: 10      │    │   │  Priority: 1        │    │
│  │  • History Index   │    │   │  • Heartbeat        │    │
│  │  • Seq Counter     │    │   │  • Diagnostics      │    │
│  │  • Replay Logic    │    │   │  • Monitoring       │    │
│  └────────────────────┘    │   └─────────────────────┘    │
└─────────────────────────────┴───────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │   4MB PSRAM     │
                    ├─────────────────┤
                    │ 2MB History     │
                    │ (Flight Rec.)   │
                    ├─────────────────┤
                    │ 1MB Network     │
                    │ Queue (Ring)    │
                    ├─────────────────┤
                    │ 1MB Reserved    │
                    └─────────────────┘
```

## Component Architecture

### vault_protocol

Binary packet protocol implementation.

**Responsibilities:**
- Packet serialization/deserialization
- CRC8 validation
- Protocol constants

**Data Flow:**
```
Raw Bytes ──▶ Parse ──▶ Validate ──▶ vault_packet_t
vault_packet_t ──▶ Finalize ──▶ Serialize ──▶ Binary Data
```

### vault_memory

PSRAM management and cross-core communication.

**Responsibilities:**
- PSRAM allocation (4MB)
- History buffer management (2MB circular)
- Network queue (1MB lock-free ring buffer)
- Sequence counter (with NVS persistence)

**Key Features:**
- Lock-free ring buffer for Core 0 → Core 1 transfer
- Atomic sequence counter operations
- Efficient history search (O(n) linear, can be optimized)

### vault_mqtt

MQTT 5.0 client with replay support.

**Responsibilities:**
- MQTT connection management
- Event publishing
- Command subscription
- Replay request handling

**Communication Flow:**
```
ESP32 ─────▶ MQTT Broker
  │            │
  │◀───────────┘
  │ (commands)
  │
  ▼
Replay Logic ──▶ History Buffer ──▶ Re-publish Events
```

## Data Flow

### Normal Operation

```
1. Hardware Event (GPIO/RMT)
   │
   ▼
2. Capture Task (Core 0)
   │ Creates vault_packet_t
   │ Assigns sequence number
   │
   ├──▶ Store in History Buffer (PSRAM)
   │
   └──▶ Queue to Ring Buffer
         │
         ▼
3. Network Task (Core 1)
   │ Dequeue from Ring Buffer
   │
   ▼
4. MQTT Publish
   │
   ▼
5. Broker receives event
```

### Replay Operation

```
1. Broker detects gap in sequence numbers
   │
   ▼
2. Send REPLAY command (SEQ_START, SEQ_END)
   │
   ▼
3. Network Task receives command
   │
   ▼
4. Logic Task searches History Buffer
   │
   ▼
5. Re-publish missing events with REPLAY flag
   │
   ▼
6. Broker fills gap in event log
```

## Memory Map

### Flash Partition Table (4MB)

```
0x0000   ┌─────────────────────┐
         │ Bootloader (28KB)   │
0x8000   ├─────────────────────┤
         │ Partition Table     │
0x9000   ├─────────────────────┤
         │ NVS (24KB)          │
0xF000   ├─────────────────────┤
         │ PHY Init (4KB)      │
0x10000  ├─────────────────────┤
         │ Factory NVS (24KB)  │
         │ (Encrypted)         │
0x16000  ├─────────────────────┤
         │ OTA Data (8KB)      │
0x20000  ├─────────────────────┤
         │ OTA_0 (1.5MB)       │
0x1A0000 ├─────────────────────┤
         │ OTA_1 (1.5MB)       │
0x320000 ├─────────────────────┤
         │ Storage (832KB)     │
0x3F0000 └─────────────────────┘
```

### PSRAM Layout (4MB)

```
0x0000000 ┌─────────────────────┐
          │ History Buffer      │
          │ (2MB Circular)      │
          │ ~153k packets       │
0x0200000 ├─────────────────────┤
          │ Network Queue       │
          │ (1MB Ring Buffer)   │
          │ ~76k packets        │
0x0300000 ├─────────────────────┤
          │ Reserved            │
          │ (1MB)               │
0x0400000 └─────────────────────┘
```

## Task Scheduling

### Priority Assignment Rationale

1. **Capture Task (15)**: Highest priority for real-time hardware event capture
2. **Logic Task (10)**: High priority for data integrity operations
3. **Network Task (5)**: Medium priority, can tolerate some latency
4. **Health Task (1)**: Lowest priority, runs when system is idle

### Core Affinity Rationale

**Core 0 (PRO_CPU):**
- Time-critical operations
- Direct hardware access (RMT)
- No WiFi/network interference

**Core 1 (APP_CPU):**
- Network stack runs here by default
- Non-critical monitoring
- Separates I/O from real-time capture

## Security Model

### Trust Boundaries

```
┌──────────────────────────────────────────────┐
│           Encrypted Flash                    │
│  ┌────────────────────────────────────┐     │
│  │  Secure Boot V2                     │     │
│  │  ┌──────────────────────────────┐  │     │
│  │  │  Application Code            │  │     │
│  │  │  • Verified on boot          │  │     │
│  │  │  • Signature checked         │  │     │
│  │  └──────────────────────────────┘  │     │
│  └────────────────────────────────────┘     │
│                                              │
│  ┌────────────────────────────────────┐     │
│  │  Encrypted NVS                      │     │
│  │  • WiFi credentials                 │     │
│  │  • MQTT passwords                   │     │
│  │  • Device keys                      │     │
│  └────────────────────────────────────┘     │
└──────────────────────────────────────────────┘
         │
         ▼ TLS/SSL
┌──────────────────────────────────────────────┐
│           MQTT Broker                        │
│  • Certificate validation                    │
│  • Username/password auth                    │
│  • Topic ACLs                                │
└──────────────────────────────────────────────┘
```

## Performance Characteristics

### Throughput

- **Event Capture Rate**: ~10,000 events/sec (theoretical)
- **MQTT Publish Rate**: ~100 messages/sec (network limited)
- **History Search**: O(n) linear, ~1ms per 1000 entries

### Latency

- **Capture to Storage**: <1ms (PSRAM write)
- **Capture to Queue**: <1ms (ring buffer send)
- **Queue to MQTT**: 10-100ms (network dependent)

### Memory Usage

- **Static RAM**: ~50KB (code + stack)
- **PSRAM**: 3MB allocated (2MB history + 1MB queue)
- **Heap**: ~100KB free (ESP32 with 4MB flash)

## Scalability

### Event Storage Capacity

```
History Buffer: 2MB / 13 bytes = ~153,000 events
At 1 event/sec: ~42 hours of history
At 10 events/sec: ~4.2 hours of history
At 100 events/sec: ~25 minutes of history
```

### Network Queue Capacity

```
Network Queue: 1MB / 13 bytes = ~76,000 events
At 100 msg/sec publish rate: ~12 minutes of buffering
```

## Future Enhancements

1. **History Index**: Add B-tree or hash table for O(log n) or O(1) lookup
2. **Compression**: Implement delta encoding for sequential events
3. **Multi-broker**: Support failover to backup MQTT brokers
4. **Local Storage**: Use SPIFFS partition for long-term archival
5. **Analytics**: On-device event aggregation and statistics

## References

- [FreeRTOS Task Management](https://www.freertos.org/taskandcr.html)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [MQTT 5.0 Specification](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html)
