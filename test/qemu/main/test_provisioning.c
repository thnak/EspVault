/**
 * @file test_provisioning.c
 * @brief Provisioning Tests for QEMU
 * 
 * Tests provisioning functionality including:
 * - JSON parsing
 * - Configuration validation
 * - NVS save/load operations
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "cJSON.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "test_prov";

/**
 * Test JSON parsing for provisioning payloads
 */
void test_provisioning_json_parse(void)
{
    ESP_LOGI(TAG, "Testing JSON parsing...");
    
    /* Sample provisioning payload */
    const char *json_payload = 
        "{"
        "  \"id\": 200,"
        "  \"wifi\": {"
        "    \"s\": \"TestNetwork\","
        "    \"p\": \"password123\""
        "  },"
        "  \"mqtt\": {"
        "    \"u\": \"mqtt://broker.local\","
        "    \"port\": 1883"
        "  }"
        "}";
    
    /* Parse JSON */
    cJSON *root = cJSON_Parse(json_payload);
    TEST_ASSERT_NOT_NULL(root);
    
    /* Verify ID */
    cJSON *id = cJSON_GetObjectItem(root, "id");
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_TRUE(cJSON_IsNumber(id));
    TEST_ASSERT_EQUAL(200, id->valueint);
    
    /* Verify WiFi configuration */
    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    TEST_ASSERT_NOT_NULL(wifi);
    TEST_ASSERT_TRUE(cJSON_IsObject(wifi));
    
    cJSON *ssid = cJSON_GetObjectItem(wifi, "s");
    TEST_ASSERT_NOT_NULL(ssid);
    TEST_ASSERT_TRUE(cJSON_IsString(ssid));
    TEST_ASSERT_EQUAL_STRING("TestNetwork", ssid->valuestring);
    
    cJSON *password = cJSON_GetObjectItem(wifi, "p");
    TEST_ASSERT_NOT_NULL(password);
    TEST_ASSERT_TRUE(cJSON_IsString(password));
    TEST_ASSERT_EQUAL_STRING("password123", password->valuestring);
    
    /* Verify MQTT configuration */
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    TEST_ASSERT_NOT_NULL(mqtt);
    TEST_ASSERT_TRUE(cJSON_IsObject(mqtt));
    
    cJSON *broker = cJSON_GetObjectItem(mqtt, "u");
    TEST_ASSERT_NOT_NULL(broker);
    TEST_ASSERT_TRUE(cJSON_IsString(broker));
    TEST_ASSERT_EQUAL_STRING("mqtt://broker.local", broker->valuestring);
    
    cJSON *port = cJSON_GetObjectItem(mqtt, "port");
    TEST_ASSERT_NOT_NULL(port);
    TEST_ASSERT_TRUE(cJSON_IsNumber(port));
    TEST_ASSERT_EQUAL(1883, port->valueint);
    
    /* Cleanup */
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "JSON parsing test passed");
}

/**
 * Test WiFi configuration validation
 */
void test_provisioning_wifi_config(void)
{
    ESP_LOGI(TAG, "Testing WiFi config validation...");
    
    /* Valid WiFi config */
    const char *valid_wifi = 
        "{"
        "  \"s\": \"MyNetwork\","
        "  \"p\": \"mypassword\""
        "}";
    
    cJSON *wifi = cJSON_Parse(valid_wifi);
    TEST_ASSERT_NOT_NULL(wifi);
    
    /* Check required fields */
    cJSON *ssid = cJSON_GetObjectItem(wifi, "s");
    TEST_ASSERT_NOT_NULL(ssid);
    TEST_ASSERT_TRUE(cJSON_IsString(ssid));
    TEST_ASSERT_GREATER_THAN(0, strlen(ssid->valuestring));
    TEST_ASSERT_LESS_OR_EQUAL(32, strlen(ssid->valuestring)); // Max SSID length
    
    cJSON *password = cJSON_GetObjectItem(wifi, "p");
    TEST_ASSERT_NOT_NULL(password);
    TEST_ASSERT_TRUE(cJSON_IsString(password));
    TEST_ASSERT_GREATER_OR_EQUAL(8, strlen(password->valuestring)); // Min password length
    TEST_ASSERT_LESS_OR_EQUAL(64, strlen(password->valuestring)); // Max password length
    
    cJSON_Delete(wifi);
    
    /* Test static IP configuration */
    const char *static_ip_wifi = 
        "{"
        "  \"s\": \"Office\","
        "  \"p\": \"password\","
        "  \"ip\": {"
        "    \"t\": \"s\","
        "    \"a\": \"192.168.1.100\","
        "    \"g\": \"192.168.1.1\","
        "    \"m\": \"255.255.255.0\""
        "  }"
        "}";
    
    wifi = cJSON_Parse(static_ip_wifi);
    TEST_ASSERT_NOT_NULL(wifi);
    
    cJSON *ip = cJSON_GetObjectItem(wifi, "ip");
    TEST_ASSERT_NOT_NULL(ip);
    
    cJSON *type = cJSON_GetObjectItem(ip, "t");
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_EQUAL_STRING("s", type->valuestring);
    
    cJSON *address = cJSON_GetObjectItem(ip, "a");
    TEST_ASSERT_NOT_NULL(address);
    TEST_ASSERT_TRUE(cJSON_IsString(address));
    
    cJSON *gateway = cJSON_GetObjectItem(ip, "g");
    TEST_ASSERT_NOT_NULL(gateway);
    
    cJSON *netmask = cJSON_GetObjectItem(ip, "m");
    TEST_ASSERT_NOT_NULL(netmask);
    
    cJSON_Delete(wifi);
    
    ESP_LOGI(TAG, "WiFi config validation test passed");
}

/**
 * Test MQTT configuration validation
 */
void test_provisioning_mqtt_config(void)
{
    ESP_LOGI(TAG, "Testing MQTT config validation...");
    
    /* Valid MQTT config without SSL */
    const char *mqtt_no_ssl = 
        "{"
        "  \"u\": \"mqtt://broker.example.com\","
        "  \"port\": 1883,"
        "  \"ssl\": false"
        "}";
    
    cJSON *mqtt = cJSON_Parse(mqtt_no_ssl);
    TEST_ASSERT_NOT_NULL(mqtt);
    
    cJSON *uri = cJSON_GetObjectItem(mqtt, "u");
    TEST_ASSERT_NOT_NULL(uri);
    TEST_ASSERT_TRUE(cJSON_IsString(uri));
    TEST_ASSERT_LESS_OR_EQUAL(128, strlen(uri->valuestring)); // Max URI length
    
    cJSON *port = cJSON_GetObjectItem(mqtt, "port");
    TEST_ASSERT_NOT_NULL(port);
    TEST_ASSERT_TRUE(cJSON_IsNumber(port));
    TEST_ASSERT_GREATER_THAN(0, port->valueint);
    TEST_ASSERT_LESS_THAN(65536, port->valueint);
    
    cJSON *ssl = cJSON_GetObjectItem(mqtt, "ssl");
    TEST_ASSERT_NOT_NULL(ssl);
    TEST_ASSERT_TRUE(cJSON_IsBool(ssl));
    TEST_ASSERT_FALSE(cJSON_IsTrue(ssl));
    
    cJSON_Delete(mqtt);
    
    /* Valid MQTT config with SSL */
    const char *mqtt_with_ssl = 
        "{"
        "  \"u\": \"mqtts://secure.example.com\","
        "  \"port\": 8883,"
        "  \"ssl\": true,"
        "  \"cert\": \"-----BEGIN CERTIFICATE-----\\nMIID...\","
        "  \"user\": \"device_001\""
        "}";
    
    mqtt = cJSON_Parse(mqtt_with_ssl);
    TEST_ASSERT_NOT_NULL(mqtt);
    
    uri = cJSON_GetObjectItem(mqtt, "u");
    TEST_ASSERT_NOT_NULL(uri);
    TEST_ASSERT_TRUE(strstr(uri->valuestring, "mqtts://") != NULL);
    
    port = cJSON_GetObjectItem(mqtt, "port");
    TEST_ASSERT_NOT_NULL(port);
    TEST_ASSERT_EQUAL(8883, port->valueint);
    
    ssl = cJSON_GetObjectItem(mqtt, "ssl");
    TEST_ASSERT_NOT_NULL(ssl);
    TEST_ASSERT_TRUE(cJSON_IsTrue(ssl));
    
    cJSON *cert = cJSON_GetObjectItem(mqtt, "cert");
    TEST_ASSERT_NOT_NULL(cert);
    TEST_ASSERT_TRUE(cJSON_IsString(cert));
    
    cJSON *user = cJSON_GetObjectItem(mqtt, "user");
    TEST_ASSERT_NOT_NULL(user);
    TEST_ASSERT_EQUAL_STRING("device_001", user->valuestring);
    
    cJSON_Delete(mqtt);
    
    ESP_LOGI(TAG, "MQTT config validation test passed");
}

/**
 * Test provisioning configuration save/load
 */
void test_provisioning_save_load(void)
{
    ESP_LOGI(TAG, "Testing config save/load...");
    
    nvs_handle_t handle;
    esp_err_t err;
    
    /* Open NVS */
    err = nvs_open("prov", NVS_READWRITE, &handle);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Simulate saving a configuration */
    const char *test_ssid = "ProductionNetwork";
    const char *test_broker = "mqtts://prod.broker.io";
    uint32_t test_id = 201;
    uint16_t test_port = 8883;
    
    err = nvs_set_str(handle, "ssid", test_ssid);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    err = nvs_set_str(handle, "broker", test_broker);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    err = nvs_set_u32(handle, "id", test_id);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    err = nvs_set_u16(handle, "port", test_port);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    err = nvs_commit(handle);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Close and reopen to simulate restart */
    nvs_close(handle);
    
    err = nvs_open("prov", NVS_READONLY, &handle);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Load configuration */
    size_t ssid_len;
    err = nvs_get_str(handle, "ssid", NULL, &ssid_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    char *loaded_ssid = malloc(ssid_len);
    TEST_ASSERT_NOT_NULL(loaded_ssid);
    err = nvs_get_str(handle, "ssid", loaded_ssid, &ssid_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING(test_ssid, loaded_ssid);
    free(loaded_ssid);
    
    size_t broker_len;
    err = nvs_get_str(handle, "broker", NULL, &broker_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    char *loaded_broker = malloc(broker_len);
    TEST_ASSERT_NOT_NULL(loaded_broker);
    err = nvs_get_str(handle, "broker", loaded_broker, &broker_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING(test_broker, loaded_broker);
    free(loaded_broker);
    
    uint32_t loaded_id;
    err = nvs_get_u32(handle, "id", &loaded_id);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(test_id, loaded_id);
    
    uint16_t loaded_port;
    err = nvs_get_u16(handle, "port", &loaded_port);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(test_port, loaded_port);
    
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Config save/load test passed");
}
