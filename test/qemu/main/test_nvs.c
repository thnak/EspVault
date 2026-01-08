/**
 * @file test_nvs.c
 * @brief NVS (Non-Volatile Storage) Tests for QEMU
 * 
 * Tests NVS operations including:
 * - Read/write operations
 * - Default configuration storage
 * - Namespace operations
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "test_nvs";

/**
 * Test basic NVS initialization and read/write operations
 */
void test_nvs_init_and_read_write(void)
{
    ESP_LOGI(TAG, "Testing NVS init and read/write...");
    
    nvs_handle_t handle;
    esp_err_t err;
    
    /* Open NVS namespace */
    err = nvs_open("test", NVS_READWRITE, &handle);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Write integer value */
    uint32_t write_value = 0x12345678;
    err = nvs_set_u32(handle, "test_val", write_value);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Commit changes */
    err = nvs_commit(handle);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Read back value */
    uint32_t read_value = 0;
    err = nvs_get_u32(handle, "test_val", &read_value);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_HEX32(write_value, read_value);
    
    /* Test string storage */
    const char *test_string = "EspVault_Test";
    err = nvs_set_str(handle, "test_str", test_string);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    err = nvs_commit(handle);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Read back string */
    size_t required_size;
    err = nvs_get_str(handle, "test_str", NULL, &required_size);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(strlen(test_string) + 1, required_size);
    
    char *read_string = malloc(required_size);
    TEST_ASSERT_NOT_NULL(read_string);
    
    err = nvs_get_str(handle, "test_str", read_string, &required_size);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING(test_string, read_string);
    
    free(read_string);
    nvs_close(handle);
    
    ESP_LOGI(TAG, "NVS read/write test passed");
}

/**
 * Test default configuration storage in NVS
 * Simulates storing provisioning default config
 */
void test_nvs_default_config(void)
{
    ESP_LOGI(TAG, "Testing NVS default config storage...");
    
    nvs_handle_t handle;
    esp_err_t err;
    
    /* Open provisioning namespace */
    err = nvs_open("prov", NVS_READWRITE, &handle);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Simulate storing default configuration flag */
    uint8_t has_default = 1;
    err = nvs_set_u8(handle, "prov_has_def", has_default);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Store mock SSID */
    const char *default_ssid = "Staging_Network";
    err = nvs_set_str(handle, "def_ssid", default_ssid);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Store mock broker URI */
    const char *default_broker = "mqtt://staging.local";
    err = nvs_set_str(handle, "def_broker", default_broker);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Store mock config ID */
    uint32_t config_id = 0; // 0 = default
    err = nvs_set_u32(handle, "def_id", config_id);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Commit all changes */
    err = nvs_commit(handle);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Verify by reading back */
    uint8_t read_has_default = 0;
    err = nvs_get_u8(handle, "prov_has_def", &read_has_default);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(1, read_has_default);
    
    /* Read SSID */
    size_t ssid_len;
    err = nvs_get_str(handle, "def_ssid", NULL, &ssid_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    char *read_ssid = malloc(ssid_len);
    TEST_ASSERT_NOT_NULL(read_ssid);
    
    err = nvs_get_str(handle, "def_ssid", read_ssid, &ssid_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING(default_ssid, read_ssid);
    
    free(read_ssid);
    
    /* Read broker URI */
    size_t broker_len;
    err = nvs_get_str(handle, "def_broker", NULL, &broker_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    char *read_broker = malloc(broker_len);
    TEST_ASSERT_NOT_NULL(read_broker);
    
    err = nvs_get_str(handle, "def_broker", read_broker, &broker_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING(default_broker, read_broker);
    
    free(read_broker);
    
    /* Read config ID */
    uint32_t read_id;
    err = nvs_get_u32(handle, "def_id", &read_id);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(0, read_id);
    
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Default config storage test passed");
}
