/**
 * @file vault_protocol.h
 * @brief Binary Protocol Definitions for EspVault Universal Node
 * 
 * This file defines the binary packet structure and protocol commands
 * for efficient data transfer between ESP32 nodes and MQTT broker.
 */

#ifndef VAULT_PROTOCOL_H
#define VAULT_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Protocol Constants
#define VAULT_PROTO_HEAD        0xAA    // Start of Frame marker
#define VAULT_PROTO_PACKET_SIZE 13      // Total packet size in bytes

// Command IDs
#define VAULT_CMD_CONFIG        0x01    // Configuration command
#define VAULT_CMD_EVENT         0x02    // Event notification
#define VAULT_CMD_HEARTBEAT     0x03    // Health check heartbeat
#define VAULT_CMD_REPLAY        0x04    // Replay request from broker

// Flags bit definitions
#define VAULT_FLAG_IS_REPLAY    (1 << 0)  // Bit 0: Is this a replayed event?
#define VAULT_FLAG_INPUT_STATE  (1 << 1)  // Bit 1: Current input state (HIGH/LOW)

/**
 * @brief Binary packet structure (13 bytes total)
 * 
 * This structure uses packed attribute to ensure no padding is added.
 * All multi-byte values are in little-endian format.
 */
typedef struct __attribute__((packed)) {
    uint8_t  head;      // Byte 0: Start of Frame (0xAA)
    uint8_t  cmd;       // Byte 1: Command ID
    uint32_t seq;       // Bytes 2-5: Sequence counter
    uint8_t  pin;       // Byte 6: Target GPIO index
    uint8_t  flags;     // Byte 7: Status flags
    uint32_t val;       // Bytes 8-11: Pulse width (µs) or data value
    uint8_t  crc;       // Byte 12: CRC8 checksum
} vault_packet_t;

/**
 * @brief Calculate CRC8 checksum for packet validation
 * 
 * @param data Pointer to data buffer
 * @param len Length of data in bytes
 * @return uint8_t CRC8 checksum value
 */
uint8_t vault_protocol_calc_crc8(const uint8_t *data, size_t len);

/**
 * @brief Initialize a packet with default values
 * 
 * @param packet Pointer to packet structure
 * @param cmd Command ID
 * @param seq Sequence number
 */
void vault_protocol_init_packet(vault_packet_t *packet, uint8_t cmd, uint32_t seq);

/**
 * @brief Finalize packet by calculating and setting CRC
 * 
 * @param packet Pointer to packet structure
 */
void vault_protocol_finalize_packet(vault_packet_t *packet);

/**
 * @brief Validate packet integrity
 * 
 * @param packet Pointer to packet structure
 * @return true if packet is valid, false otherwise
 */
bool vault_protocol_validate_packet(const vault_packet_t *packet);

/**
 * @brief Parse binary data into packet structure
 * 
 * @param data Raw binary data
 * @param len Length of data (must be VAULT_PROTO_PACKET_SIZE)
 * @param packet Output packet structure
 * @return true if parsing successful, false otherwise
 */
bool vault_protocol_parse(const uint8_t *data, size_t len, vault_packet_t *packet);

/**
 * @brief Serialize packet structure to binary data
 * 
 * @param packet Input packet structure
 * @param data Output buffer (must be at least VAULT_PROTO_PACKET_SIZE bytes)
 * @return Number of bytes written
 */
size_t vault_protocol_serialize(const vault_packet_t *packet, uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif // VAULT_PROTOCOL_H
