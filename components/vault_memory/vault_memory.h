/**
 * @file vault_memory.h
 * @brief Memory Management for EspVault Universal Node (4MB PSRAM)
 * 
 * This file defines the memory allocation strategy for PSRAM:
 * - 2MB: Flight Recorder History Buffer
 * - 1MB: Network Queue Buffer (MQTT outbox)
 * - 1MB: Reserved for other operations
 */

#ifndef VAULT_MEMORY_H
#define VAULT_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "vault_protocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

// Memory allocation sizes
#define VAULT_HISTORY_BUFFER_SIZE   (2 * 1024 * 1024)  // 2MB for flight recorder
#define VAULT_NETWORK_QUEUE_SIZE    (1 * 1024 * 1024)  // 1MB for MQTT outbox
#define VAULT_HISTORY_MAX_ENTRIES   (VAULT_HISTORY_BUFFER_SIZE / sizeof(vault_packet_t))

// Sequence counter persistence
#define VAULT_SEQ_SYNC_INTERVAL     10  // Sync to NVS every 10 events

/**
 * @brief History entry metadata for efficient searching
 */
typedef struct {
    uint32_t seq;           // Sequence number
    uint32_t timestamp;     // Timestamp in milliseconds
    uint32_t offset;        // Offset in circular buffer
} vault_history_index_t;

/**
 * @brief Memory manager handle
 */
typedef struct {
    // History buffer (2MB circular buffer in PSRAM)
    uint8_t *history_buffer;
    uint32_t history_write_pos;
    uint32_t history_count;
    
    // Network queue (lock-free ring buffer)
    RingbufHandle_t network_queue;
    
    // Sequence counter
    uint32_t seq_counter;
    uint32_t seq_last_synced;
    
    // Synchronization
    bool initialized;
} vault_memory_t;

/**
 * @brief Initialize memory management system
 * 
 * Allocates PSRAM buffers and initializes ring buffers
 * 
 * @return Pointer to memory manager handle, NULL on failure
 */
vault_memory_t* vault_memory_init(void);

/**
 * @brief Deinitialize and free memory resources
 * 
 * @param mem Pointer to memory manager handle
 */
void vault_memory_deinit(vault_memory_t *mem);

/**
 * @brief Get next sequence number (thread-safe)
 * 
 * @param mem Pointer to memory manager handle
 * @return Next sequence number
 */
uint32_t vault_memory_get_next_seq(vault_memory_t *mem);

/**
 * @brief Store packet in history buffer
 * 
 * @param mem Pointer to memory manager handle
 * @param packet Packet to store
 * @return true on success, false on failure
 */
bool vault_memory_store_history(vault_memory_t *mem, const vault_packet_t *packet);

/**
 * @brief Search history buffer for a specific sequence number
 * 
 * @param mem Pointer to memory manager handle
 * @param seq Sequence number to find
 * @param packet Output buffer for packet
 * @return true if found, false otherwise
 */
bool vault_memory_find_by_seq(vault_memory_t *mem, uint32_t seq, vault_packet_t *packet);

/**
 * @brief Retrieve packets from sequence range (for replay)
 * 
 * @param mem Pointer to memory manager handle
 * @param seq_start Starting sequence number
 * @param seq_end Ending sequence number (inclusive)
 * @param packets Output buffer for packets
 * @param max_count Maximum number of packets to retrieve
 * @return Number of packets retrieved
 */
size_t vault_memory_get_range(vault_memory_t *mem, uint32_t seq_start, 
                               uint32_t seq_end, vault_packet_t *packets, 
                               size_t max_count);

/**
 * @brief Send packet to network queue (lock-free)
 * 
 * This is used for cross-core communication from Core 0 to Core 1
 * 
 * @param mem Pointer to memory manager handle
 * @param packet Packet to queue
 * @param timeout_ms Timeout in milliseconds
 * @return true on success, false on timeout
 */
bool vault_memory_queue_network(vault_memory_t *mem, const vault_packet_t *packet, 
                                 uint32_t timeout_ms);

/**
 * @brief Receive packet from network queue (lock-free)
 * 
 * This is called by the network task on Core 1
 * 
 * @param mem Pointer to memory manager handle
 * @param packet Output buffer for packet
 * @param timeout_ms Timeout in milliseconds
 * @return true if packet received, false on timeout
 */
bool vault_memory_dequeue_network(vault_memory_t *mem, vault_packet_t *packet, 
                                   uint32_t timeout_ms);

/**
 * @brief Sync sequence counter to NVS (if needed)
 * 
 * @param mem Pointer to memory manager handle
 * @return true on success, false on failure
 */
bool vault_memory_sync_seq_to_nvs(vault_memory_t *mem);

/**
 * @brief Load sequence counter from NVS on boot
 * 
 * @param mem Pointer to memory manager handle
 * @return true on success, false on failure
 */
bool vault_memory_load_seq_from_nvs(vault_memory_t *mem);

#ifdef __cplusplus
}
#endif

#endif // VAULT_MEMORY_H
