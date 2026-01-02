/**
 * @file vault_memory.c
 * @brief Memory Management Implementation for EspVault Universal Node
 */

#include "vault_memory.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "vault_memory";

// NVS namespace for sequence counter
#define NVS_NAMESPACE "vault"
#define NVS_SEQ_KEY "seq_counter"

vault_memory_t* vault_memory_init(void)
{
    vault_memory_t *mem = heap_caps_malloc(sizeof(vault_memory_t), MALLOC_CAP_8BIT);
    if (mem == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory manager structure");
        return NULL;
    }
    
    memset(mem, 0, sizeof(vault_memory_t));
    
    // Allocate history buffer in PSRAM (2MB)
    mem->history_buffer = heap_caps_malloc(VAULT_HISTORY_BUFFER_SIZE, 
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (mem->history_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes in PSRAM for history buffer", 
                 VAULT_HISTORY_BUFFER_SIZE);
        free(mem);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Allocated %d bytes in PSRAM for history buffer", 
             VAULT_HISTORY_BUFFER_SIZE);
    
    // Create lock-free ring buffer for network queue (1MB in PSRAM)
    uint8_t *ringbuf_storage = heap_caps_malloc(VAULT_NETWORK_QUEUE_SIZE,
                                                 MALLOC_CAP_SPIRAM);
    StaticRingbuffer_t *ringbuf_struct = heap_caps_malloc(sizeof(StaticRingbuffer_t),
                                                           MALLOC_CAP_8BIT);
    
    if (ringbuf_storage == NULL || ringbuf_struct == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for ring buffer");
        free(ringbuf_storage);
        free(ringbuf_struct);
        free(mem->history_buffer);
        free(mem);
        return NULL;
    }
    
    mem->network_queue = xRingbufferCreateStatic(VAULT_NETWORK_QUEUE_SIZE,
                                                  RINGBUF_TYPE_NOSPLIT,
                                                  ringbuf_storage,
                                                  ringbuf_struct);
    
    if (mem->network_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create network queue ring buffer");
        free(ringbuf_storage);
        free(ringbuf_struct);
        free(mem->history_buffer);
        free(mem);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Created %d bytes ring buffer in PSRAM for network queue", 
             VAULT_NETWORK_QUEUE_SIZE);
    
    // Load sequence counter from NVS
    if (!vault_memory_load_seq_from_nvs(mem)) {
        ESP_LOGW(TAG, "Failed to load sequence counter from NVS, starting from 0");
        mem->seq_counter = 0;
    }
    
    mem->seq_last_synced = mem->seq_counter;
    mem->initialized = true;
    
    ESP_LOGI(TAG, "Memory manager initialized, starting sequence: %lu", mem->seq_counter);
    
    return mem;
}

void vault_memory_deinit(vault_memory_t *mem)
{
    if (mem == NULL) {
        return;
    }
    
    // Sync final sequence counter to NVS
    vault_memory_sync_seq_to_nvs(mem);
    
    // Free resources
    if (mem->network_queue != NULL) {
        vRingbufferDelete(mem->network_queue);
    }
    
    if (mem->history_buffer != NULL) {
        free(mem->history_buffer);
    }
    
    free(mem);
    
    ESP_LOGI(TAG, "Memory manager deinitialized");
}

uint32_t vault_memory_get_next_seq(vault_memory_t *mem)
{
    if (mem == NULL || !mem->initialized) {
        return 0;
    }
    
    // Atomic increment (thread-safe on ESP32)
    uint32_t seq = __atomic_fetch_add(&mem->seq_counter, 1, __ATOMIC_SEQ_CST);
    
    // Check if we need to sync to NVS
    if ((seq - mem->seq_last_synced) >= VAULT_SEQ_SYNC_INTERVAL) {
        vault_memory_sync_seq_to_nvs(mem);
    }
    
    return seq;
}

bool vault_memory_store_history(vault_memory_t *mem, const vault_packet_t *packet)
{
    if (mem == NULL || !mem->initialized || packet == NULL) {
        return false;
    }
    
    // Calculate position in circular buffer
    uint32_t offset = (mem->history_write_pos * sizeof(vault_packet_t)) 
                     % VAULT_HISTORY_BUFFER_SIZE;
    
    // Copy packet to history buffer
    memcpy(mem->history_buffer + offset, packet, sizeof(vault_packet_t));
    
    // Update write position
    mem->history_write_pos++;
    if (mem->history_count < VAULT_HISTORY_MAX_ENTRIES) {
        mem->history_count++;
    }
    
    return true;
}

bool vault_memory_find_by_seq(vault_memory_t *mem, uint32_t seq, vault_packet_t *packet)
{
    if (mem == NULL || !mem->initialized || packet == NULL) {
        return false;
    }
    
    // Linear search through circular buffer - O(n)
    // TODO: Optimize with B-tree or hash table for O(log n) or O(1) lookup
    // This is acceptable for development but should be improved for production
    // with high event rates or frequent replay requests
    uint32_t search_count = (mem->history_count < VAULT_HISTORY_MAX_ENTRIES) 
                           ? mem->history_count : VAULT_HISTORY_MAX_ENTRIES;
    
    for (uint32_t i = 0; i < search_count; i++) {
        uint32_t pos = (mem->history_write_pos - search_count + i) % VAULT_HISTORY_MAX_ENTRIES;
        uint32_t offset = pos * sizeof(vault_packet_t);
        
        vault_packet_t *stored = (vault_packet_t *)(mem->history_buffer + offset);
        if (stored->seq == seq) {
            memcpy(packet, stored, sizeof(vault_packet_t));
            return true;
        }
    }
    
    return false;
}

size_t vault_memory_get_range(vault_memory_t *mem, uint32_t seq_start, 
                               uint32_t seq_end, vault_packet_t *packets, 
                               size_t max_count)
{
    if (mem == NULL || !mem->initialized || packets == NULL || max_count == 0) {
        return 0;
    }
    
    size_t found = 0;
    uint32_t search_count = (mem->history_count < VAULT_HISTORY_MAX_ENTRIES) 
                           ? mem->history_count : VAULT_HISTORY_MAX_ENTRIES;
    
    for (uint32_t i = 0; i < search_count && found < max_count; i++) {
        uint32_t pos = (mem->history_write_pos - search_count + i) % VAULT_HISTORY_MAX_ENTRIES;
        uint32_t offset = pos * sizeof(vault_packet_t);
        
        vault_packet_t *stored = (vault_packet_t *)(mem->history_buffer + offset);
        if (stored->seq >= seq_start && stored->seq <= seq_end) {
            memcpy(&packets[found], stored, sizeof(vault_packet_t));
            found++;
        }
    }
    
    return found;
}

bool vault_memory_queue_network(vault_memory_t *mem, const vault_packet_t *packet, 
                                 uint32_t timeout_ms)
{
    if (mem == NULL || !mem->initialized || packet == NULL) {
        return false;
    }
    
    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    
    return xRingbufferSend(mem->network_queue, packet, sizeof(vault_packet_t), 
                          ticks) == pdTRUE;
}

bool vault_memory_dequeue_network(vault_memory_t *mem, vault_packet_t *packet, 
                                   uint32_t timeout_ms)
{
    if (mem == NULL || !mem->initialized || packet == NULL) {
        return false;
    }
    
    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    size_t item_size;
    
    void *item = xRingbufferReceive(mem->network_queue, &item_size, ticks);
    if (item != NULL && item_size == sizeof(vault_packet_t)) {
        memcpy(packet, item, sizeof(vault_packet_t));
        vRingbufferReturnItem(mem->network_queue, item);
        return true;
    }
    
    return false;
}

bool vault_memory_sync_seq_to_nvs(vault_memory_t *mem)
{
    if (mem == NULL || !mem->initialized) {
        return false;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_u32(nvs_handle, NVS_SEQ_KEY, mem->seq_counter);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write sequence to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        mem->seq_last_synced = mem->seq_counter;
        ESP_LOGD(TAG, "Synced sequence counter to NVS: %lu", mem->seq_counter);
        return true;
    }
    
    return false;
}

bool vault_memory_load_seq_from_nvs(vault_memory_t *mem)
{
    if (mem == NULL) {
        return false;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS for reading: %s", esp_err_to_name(err));
        return false;
    }
    
    uint32_t seq = 0;
    err = nvs_get_u32(nvs_handle, NVS_SEQ_KEY, &seq);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        mem->seq_counter = seq;
        ESP_LOGI(TAG, "Loaded sequence counter from NVS: %lu", seq);
        return true;
    }
    
    return false;
}
