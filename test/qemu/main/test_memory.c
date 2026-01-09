/**
 * @file test_memory.c
 * @brief Memory Management Tests for QEMU
 * 
 * Tests memory operations including:
 * - Heap allocation
 * - PSRAM allocation
 * - Memory capabilities
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "test_memory";

/**
 * Test basic memory allocation
 */
void test_memory_allocation(void)
{
    ESP_LOGI(TAG, "Testing memory allocation...");
    
    /* Get initial free heap */
    size_t initial_free = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Initial free heap: %u bytes", initial_free);
    
    /* Allocate memory */
    size_t alloc_size = 1024;
    void *ptr = malloc(alloc_size);
    TEST_ASSERT_NOT_NULL(ptr);
    
    /* Verify memory decreased */
    size_t after_alloc_free = esp_get_free_heap_size();
    TEST_ASSERT_LESS_THAN(initial_free, after_alloc_free);
    
    /* Write to allocated memory */
    memset(ptr, 0xAA, alloc_size);
    
    /* Verify pattern */
    uint8_t *byte_ptr = (uint8_t *)ptr;
    for (size_t i = 0; i < alloc_size; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xAA, byte_ptr[i]);
    }
    
    /* Free memory */
    free(ptr);
    
    /* Verify heap recovered */
    size_t after_free = esp_get_free_heap_size();
    TEST_ASSERT_GREATER_OR_EQUAL(after_alloc_free, after_free);
    
    ESP_LOGI(TAG, "Memory allocation test passed");
}

/**
 * Test heap capabilities (internal RAM vs PSRAM)
 */
void test_memory_heap_caps(void)
{
    ESP_LOGI(TAG, "Testing heap capabilities...");
    
    /* Check internal memory */
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "Internal RAM: %u / %u bytes free", 
             internal_free, internal_total);
    TEST_ASSERT_GREATER_THAN(0, internal_free);
    TEST_ASSERT_GREATER_THAN(0, internal_total);
    
#ifdef CONFIG_SPIRAM
    /* Check PSRAM */
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM: %u / %u bytes free", psram_free, psram_total);
    TEST_ASSERT_GREATER_THAN(0, psram_free);
    TEST_ASSERT_GREATER_THAN(0, psram_total);
    
    /* Allocate from PSRAM specifically */
    size_t psram_alloc_size = 4096;
    void *psram_ptr = heap_caps_malloc(psram_alloc_size, MALLOC_CAP_SPIRAM);
    TEST_ASSERT_NOT_NULL(psram_ptr);
    
    /* Verify PSRAM decreased */
    size_t psram_after_alloc = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    TEST_ASSERT_LESS_THAN(psram_free, psram_after_alloc);
    
    /* Write pattern to PSRAM */
    memset(psram_ptr, 0x55, psram_alloc_size);
    
    /* Verify pattern */
    uint8_t *psram_bytes = (uint8_t *)psram_ptr;
    for (size_t i = 0; i < psram_alloc_size; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x55, psram_bytes[i]);
    }
    
    /* Free PSRAM */
    heap_caps_free(psram_ptr);
    
    /* Verify PSRAM recovered */
    size_t psram_after_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    TEST_ASSERT_GREATER_OR_EQUAL(psram_after_alloc, psram_after_free);
    
    ESP_LOGI(TAG, "PSRAM test passed");
#else
    ESP_LOGW(TAG, "PSRAM not enabled, skipping PSRAM tests");
#endif
    
    /* Test DMA capable memory */
    size_t dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
    ESP_LOGI(TAG, "DMA capable memory: %u bytes free", dma_free);
    TEST_ASSERT_GREATER_THAN(0, dma_free);
    
    void *dma_ptr = heap_caps_malloc(512, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(dma_ptr);
    heap_caps_free(dma_ptr);
    
    ESP_LOGI(TAG, "Heap capabilities test passed");
}
