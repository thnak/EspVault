/**
 * @file test_main.c
 * @brief QEMU Test Entry Point for EspVault
 * 
 * Tests basic functionality including:
 * - NVS operations
 * - Memory management
 * - Provisioning configuration parsing
 * - Dual-core task creation
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "qemu_test";

// External test functions
extern void test_nvs_init_and_read_write(void);
extern void test_nvs_default_config(void);
extern void test_provisioning_json_parse(void);
extern void test_provisioning_wifi_config(void);
extern void test_provisioning_mqtt_config(void);
extern void test_provisioning_save_load(void);
extern void test_memory_allocation(void);
extern void test_memory_heap_caps(void);
extern void test_network_ethernet_init(void);
extern void test_network_ethernet_connect(void);
extern void test_network_mqtt_connect(void);
extern void test_network_mqtt_pubsub(void);
extern void test_network_cleanup(void);
extern void test_network_info(void);
extern void test_integration_full_provisioning_flow(void);
extern void test_integration_provisioning_with_ssl(void);
extern void test_integration_provisioning_failure_recovery(void);
extern void test_integration_network_simulation_info(void);

void setUp(void)
{
    // Common setup for all tests
}

void tearDown(void)
{
    // Common teardown for all tests
}

static void print_test_banner(const char *message)
{
    printf("\n");
    printf("===============================================\n");
    printf("  %s\n", message);
    printf("===============================================\n");
    printf("\n");
}

static void print_system_info(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║         EspVault QEMU Test Suite            ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("System Information:\n");
    printf("  ESP-IDF Version: %s\n", esp_get_idf_version());
    printf("  Chip Model: %s\n", CONFIG_IDF_TARGET);
    printf("  Chip Revision: %d\n", esp_chip_info().revision);
    printf("  CPU Cores: %d\n", esp_chip_info().cores);
    printf("  Free Heap: %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    
#ifdef CONFIG_SPIRAM
    printf("  Free PSRAM: %lu bytes\n", 
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("  Total PSRAM: %lu bytes\n", 
           (unsigned long)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
#else
    printf("  PSRAM: Not enabled\n");
#endif
    
    printf("\n");
}

void app_main(void)
{
    /* Initialize NVS for tests */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    /* Print system information */
    print_system_info();
    
    /* Wait a bit for QEMU to stabilize */
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* Initialize Unity test framework */
    UNITY_BEGIN();
    
    /* ============================================ */
    /* Test Suite 1: NVS Operations                */
    /* ============================================ */
    print_test_banner("Test Suite 1: NVS Operations");
    
    RUN_TEST(test_nvs_init_and_read_write);
    RUN_TEST(test_nvs_default_config);
    
    /* ============================================ */
    /* Test Suite 2: Memory Management             */
    /* ============================================ */
    print_test_banner("Test Suite 2: Memory Management");
    
    RUN_TEST(test_memory_allocation);
    RUN_TEST(test_memory_heap_caps);
    
    /* ============================================ */
    /* Test Suite 3: Provisioning                  */
    /* ============================================ */
    print_test_banner("Test Suite 3: Provisioning Configuration");
    
    RUN_TEST(test_provisioning_json_parse);
    RUN_TEST(test_provisioning_wifi_config);
    RUN_TEST(test_provisioning_mqtt_config);
    RUN_TEST(test_provisioning_save_load);
    
    /* ============================================ */
    /* Test Suite 4: Network & Ethernet            */
    /* ============================================ */
    print_test_banner("Test Suite 4: Network & Ethernet (QEMU)");
    
    RUN_TEST(test_network_info);
    RUN_TEST(test_network_ethernet_init);
    RUN_TEST(test_network_ethernet_connect);
    RUN_TEST(test_network_mqtt_connect);
    RUN_TEST(test_network_mqtt_pubsub);
    RUN_TEST(test_network_cleanup);
    
    /* ============================================ */
    /* Test Suite 5: Integration Tests             */
    /* ============================================ */
    print_test_banner("Test Suite 5: Integration Tests");
    
    RUN_TEST(test_integration_network_simulation_info);
    RUN_TEST(test_integration_full_provisioning_flow);
    RUN_TEST(test_integration_provisioning_with_ssl);
    RUN_TEST(test_integration_provisioning_failure_recovery);
    
    /* ============================================ */
    /* Test Complete                                */
    /* ============================================ */
    
    UNITY_END();
    
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║         All Tests Completed                  ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Final System State:\n");
    printf("  Free Heap: %lu bytes\n", (unsigned long)esp_get_free_heap_size());
#ifdef CONFIG_SPIRAM
    printf("  Free PSRAM: %lu bytes\n", 
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#endif
    printf("\n");
    
    /* Exit QEMU */
    printf("Exiting QEMU...\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* Signal test completion */
    ESP_LOGI(TAG, "Tests completed successfully");
}
