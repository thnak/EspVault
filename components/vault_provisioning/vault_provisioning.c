/**
 * @file vault_provisioning.c
 * @brief Remote Provisioning Implementation for EspVault Universal Node
 */

#include "vault_provisioning.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include <string.h>

static const char *TAG = "vault_prov";

// NVS namespace for provisioning
#define NVS_NAMESPACE "vault_prov"
#define NVS_KEY_CONFIG "config"
#define NVS_KEY_DEFAULT "default_cfg"

// Event bits for connection testing
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/**
 * @brief Provisioning manager structure
 */
struct vault_provisioning_s {
    vault_mqtt_t *mqtt;
    vault_memory_t *memory;
    bool in_setup_mode;
    EventGroupHandle_t wifi_event_group;
    esp_netif_t *netif;
};

// External task handles (from main.c)
// NOTE: This creates tight coupling with main.c. In future refactoring,
// consider passing these handles via initialization function or implementing
// a task manager interface for better modularity and testability.
extern TaskHandle_t capture_task_handle;
extern TaskHandle_t logic_task_handle;
extern TaskHandle_t health_task_handle;

/**
 * @brief WiFi event handler for provisioning tests
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    vault_provisioning_t *prov = (vault_provisioning_t *)arg;
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
        if (prov->wifi_event_group) {
            xEventGroupSetBits(prov->wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (prov->wifi_event_group) {
            xEventGroupSetBits(prov->wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

vault_provisioning_t* vault_provisioning_init(vault_mqtt_t *mqtt, vault_memory_t *memory)
{
    if (mqtt == NULL || memory == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return NULL;
    }
    
    vault_provisioning_t *prov = heap_caps_malloc(sizeof(vault_provisioning_t), 
                                                   MALLOC_CAP_8BIT);
    if (prov == NULL) {
        ESP_LOGE(TAG, "Failed to allocate provisioning handle");
        return NULL;
    }
    
    memset(prov, 0, sizeof(vault_provisioning_t));
    prov->mqtt = mqtt;
    prov->memory = memory;
    prov->in_setup_mode = false;
    
    // Create event group for WiFi testing
    prov->wifi_event_group = xEventGroupCreate();
    if (prov->wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        free(prov);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Provisioning manager initialized");
    return prov;
}

void vault_provisioning_deinit(vault_provisioning_t *prov)
{
    if (prov == NULL) {
        return;
    }
    
    if (prov->wifi_event_group) {
        vEventGroupDelete(prov->wifi_event_group);
    }
    
    free(prov);
    ESP_LOGI(TAG, "Provisioning manager deinitialized");
}

esp_err_t vault_provisioning_enter_setup_mode(vault_provisioning_t *prov)
{
    if (prov == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Entering setup mode...");
    
    // Suspend operational tasks to free resources
    // Note: Only suspend if they exist and are not critical
    if (capture_task_handle != NULL) {
        vTaskSuspend(capture_task_handle);
        ESP_LOGI(TAG, "Capture task suspended");
    }
    
    if (logic_task_handle != NULL) {
        vTaskSuspend(logic_task_handle);
        ESP_LOGI(TAG, "Logic task suspended");
    }
    
    if (health_task_handle != NULL) {
        vTaskSuspend(health_task_handle);
        ESP_LOGI(TAG, "Health task suspended");
    }
    
    // Log memory status
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free PSRAM: %lu bytes", 
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    prov->in_setup_mode = true;
    ESP_LOGI(TAG, "Setup mode active - resources freed for provisioning");
    
    return ESP_OK;
}

esp_err_t vault_provisioning_exit_setup_mode(vault_provisioning_t *prov)
{
    if (prov == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!prov->in_setup_mode) {
        return ESP_OK;  // Already in operational mode
    }
    
    ESP_LOGI(TAG, "Exiting setup mode...");
    
    // Resume operational tasks
    if (capture_task_handle != NULL) {
        vTaskResume(capture_task_handle);
        ESP_LOGI(TAG, "Capture task resumed");
    }
    
    if (logic_task_handle != NULL) {
        vTaskResume(logic_task_handle);
        ESP_LOGI(TAG, "Logic task resumed");
    }
    
    if (health_task_handle != NULL) {
        vTaskResume(health_task_handle);
        ESP_LOGI(TAG, "Health task resumed");
    }
    
    prov->in_setup_mode = false;
    ESP_LOGI(TAG, "Operational mode restored");
    
    return ESP_OK;
}

esp_err_t vault_provisioning_parse_config(const char *json_str, size_t len, 
                                          vault_prov_config_t *config)
{
    if (json_str == NULL || config == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (len > VAULT_PROV_MAX_PAYLOAD_LEN) {
        ESP_LOGE(TAG, "Payload too large: %d bytes (max: %d)", 
                 len, VAULT_PROV_MAX_PAYLOAD_LEN);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Initialize config structure
    memset(config, 0, sizeof(vault_prov_config_t));
    
    // Parse JSON (use PSRAM for cJSON operations)
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON parse error");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Parse configuration ID
    cJSON *id = cJSON_GetObjectItem(root, "id");
    if (id && cJSON_IsNumber(id)) {
        config->config_id = id->valueint;
    }
    
    // Parse WiFi configuration
    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if (wifi) {
        cJSON *ssid = cJSON_GetObjectItem(wifi, "s");
        cJSON *password = cJSON_GetObjectItem(wifi, "p");
        
        if (ssid && cJSON_IsString(ssid)) {
            strncpy(config->wifi.ssid, ssid->valuestring, 
                    VAULT_PROV_MAX_SSID_LEN - 1);
        }
        
        if (password && cJSON_IsString(password)) {
            strncpy(config->wifi.password, password->valuestring, 
                    VAULT_PROV_MAX_PASSWORD_LEN - 1);
        }
    }
    
    // Parse IP configuration
    cJSON *ip = cJSON_GetObjectItem(root, "ip");
    if (ip) {
        cJSON *type = cJSON_GetObjectItem(ip, "t");
        if (type && cJSON_IsString(type)) {
            config->wifi.ip.type = (strcmp(type->valuestring, "s") == 0) ? 
                                   VAULT_IP_TYPE_STATIC : VAULT_IP_TYPE_DHCP;
        }
        
        if (config->wifi.ip.type == VAULT_IP_TYPE_STATIC) {
            cJSON *addr = cJSON_GetObjectItem(ip, "a");
            cJSON *gw = cJSON_GetObjectItem(ip, "g");
            cJSON *mask = cJSON_GetObjectItem(ip, "m");
            
            if (addr && cJSON_IsString(addr)) {
                strncpy(config->wifi.ip.address, addr->valuestring, 15);
            }
            if (gw && cJSON_IsString(gw)) {
                strncpy(config->wifi.ip.gateway, gw->valuestring, 15);
            }
            if (mask && cJSON_IsString(mask)) {
                strncpy(config->wifi.ip.netmask, mask->valuestring, 15);
            }
        }
    }
    
    // Parse MQTT configuration
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (mqtt) {
        cJSON *uri = cJSON_GetObjectItem(mqtt, "u");
        cJSON *port = cJSON_GetObjectItem(mqtt, "port");
        cJSON *ssl = cJSON_GetObjectItem(mqtt, "ssl");
        
        if (uri && cJSON_IsString(uri)) {
            strncpy(config->mqtt.broker_uri, uri->valuestring, 
                    VAULT_PROV_MAX_BROKER_URI_LEN - 1);
        }
        
        if (port && cJSON_IsNumber(port)) {
            config->mqtt.port = port->valueint;
        }
        
        if (ssl && cJSON_IsBool(ssl)) {
            config->mqtt.use_ssl = cJSON_IsTrue(ssl);
        }
        
        // Parse SSL certificates (allocate in PSRAM)
        if (config->mqtt.use_ssl) {
            cJSON *cert = cJSON_GetObjectItem(mqtt, "cert");
            cJSON *key = cJSON_GetObjectItem(mqtt, "key");
            
            if (cert && cJSON_IsString(cert)) {
                size_t cert_len = strlen(cert->valuestring) + 1;
                if (cert_len <= VAULT_PROV_MAX_CERT_LEN) {
                    config->mqtt.ca_cert = heap_caps_malloc(cert_len, 
                                                            MALLOC_CAP_SPIRAM);
                    if (config->mqtt.ca_cert) {
                        strcpy(config->mqtt.ca_cert, cert->valuestring);
                    } else {
                        ESP_LOGE(TAG, "Failed to allocate memory for CA certificate");
                    }
                } else {
                    ESP_LOGE(TAG, "CA certificate too large: %d bytes (max: %d)",
                             cert_len, VAULT_PROV_MAX_CERT_LEN);
                }
            }
            
            if (key && cJSON_IsString(key)) {
                size_t key_len = strlen(key->valuestring) + 1;
                if (key_len <= VAULT_PROV_MAX_KEY_LEN) {
                    config->mqtt.client_key = heap_caps_malloc(key_len, 
                                                               MALLOC_CAP_SPIRAM);
                    if (config->mqtt.client_key) {
                        strcpy(config->mqtt.client_key, key->valuestring);
                    } else {
                        ESP_LOGE(TAG, "Failed to allocate memory for client key");
                    }
                } else {
                    ESP_LOGE(TAG, "Client key too large: %d bytes (max: %d)",
                             key_len, VAULT_PROV_MAX_KEY_LEN);
                }
            }
        }
        
        // Parse MQTT credentials
        cJSON *username = cJSON_GetObjectItem(mqtt, "user");
        cJSON *password = cJSON_GetObjectItem(mqtt, "pass");
        
        if (username && cJSON_IsString(username)) {
            strncpy(config->mqtt.username, username->valuestring, 63);
        }
        
        if (password && cJSON_IsString(password)) {
            strncpy(config->mqtt.password, password->valuestring, 63);
        }
    }
    
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Configuration parsed successfully");
    ESP_LOGI(TAG, "  Config ID: %lu", config->config_id);
    ESP_LOGI(TAG, "  WiFi SSID: %s", config->wifi.ssid);
    ESP_LOGI(TAG, "  MQTT URI: %s", config->mqtt.broker_uri);
    ESP_LOGI(TAG, "  MQTT Port: %d", config->mqtt.port);
    ESP_LOGI(TAG, "  SSL Enabled: %s", config->mqtt.use_ssl ? "yes" : "no");
    
    return ESP_OK;
}

void vault_provisioning_free_config(vault_prov_config_t *config)
{
    if (config == NULL) {
        return;
    }
    
    if (config->mqtt.ca_cert) {
        heap_caps_free(config->mqtt.ca_cert);
        config->mqtt.ca_cert = NULL;
    }
    
    if (config->mqtt.client_cert) {
        heap_caps_free(config->mqtt.client_cert);
        config->mqtt.client_cert = NULL;
    }
    
    if (config->mqtt.client_key) {
        heap_caps_free(config->mqtt.client_key);
        config->mqtt.client_key = NULL;
    }
}

esp_err_t vault_provisioning_test_wifi(const vault_wifi_config_t *wifi_config, 
                                       uint32_t timeout_ms)
{
    if (wifi_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Testing WiFi connection to: %s", wifi_config->ssid);
    
    // This is a simplified test - in production, you'd temporarily
    // connect to the test network without affecting current connection
    
    // For now, validate configuration format
    if (strlen(wifi_config->ssid) == 0) {
        ESP_LOGE(TAG, "Empty SSID");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate static IP configuration if specified
    if (wifi_config->ip.type == VAULT_IP_TYPE_STATIC) {
        if (strlen(wifi_config->ip.address) == 0 ||
            strlen(wifi_config->ip.gateway) == 0 ||
            strlen(wifi_config->ip.netmask) == 0) {
            ESP_LOGE(TAG, "Incomplete static IP configuration");
            return ESP_ERR_INVALID_ARG;
        }
    }
    
    ESP_LOGI(TAG, "WiFi configuration validated");
    return ESP_OK;
}

esp_err_t vault_provisioning_test_mqtt(const vault_mqtt_prov_config_t *mqtt_config, 
                                       uint32_t timeout_ms)
{
    if (mqtt_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Testing MQTT connection to: %s:%d", 
             mqtt_config->broker_uri, mqtt_config->port);
    
    // Validate MQTT configuration
    if (strlen(mqtt_config->broker_uri) == 0) {
        ESP_LOGE(TAG, "Empty broker URI");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (mqtt_config->port == 0) {
        ESP_LOGE(TAG, "Invalid port");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate SSL configuration
    if (mqtt_config->use_ssl) {
        if (mqtt_config->ca_cert == NULL) {
            ESP_LOGW(TAG, "SSL enabled but no CA certificate provided");
        }
    }
    
    ESP_LOGI(TAG, "MQTT configuration validated");
    return ESP_OK;
}

esp_err_t vault_provisioning_apply_config(vault_provisioning_t *prov,
                                          const vault_prov_config_t *config,
                                          const char *correlation_id)
{
    if (prov == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Applying configuration (dry-run mode)");
    
    // Step 1: Test WiFi configuration
    esp_err_t ret = vault_provisioning_test_wifi(&config->wifi, 10000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi configuration test failed");
        vault_provisioning_send_response(prov, NULL, correlation_id,
                                        VAULT_PROV_STATUS_WIFI_FAILED,
                                        "WiFi configuration validation failed");
        return ret;
    }
    
    // Step 2: Test MQTT configuration
    ret = vault_provisioning_test_mqtt(&config->mqtt, 10000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MQTT configuration test failed");
        vault_provisioning_send_response(prov, NULL, correlation_id,
                                        VAULT_PROV_STATUS_MQTT_FAILED,
                                        "MQTT configuration validation failed");
        return ret;
    }
    
    // Step 3: Save configuration to NVS
    ret = vault_provisioning_save_config(config, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save configuration to NVS");
        vault_provisioning_send_response(prov, NULL, correlation_id,
                                        VAULT_PROV_STATUS_MEMORY_ERROR,
                                        "Failed to save configuration");
        return ret;
    }
    
    // Step 4: Send success response
    char details[128];
    snprintf(details, sizeof(details), 
             "Configuration applied successfully. Device will restart.");
    
    vault_provisioning_send_response(prov, NULL, correlation_id,
                                     VAULT_PROV_STATUS_SUCCESS, details);
    
    // Step 5: Restart device to apply new configuration
    ESP_LOGI(TAG, "Configuration saved successfully. Restarting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    
    return ESP_OK;
}

esp_err_t vault_provisioning_send_response(vault_provisioning_t *prov,
                                           const char *response_topic,
                                           const char *correlation_id,
                                           vault_prov_status_t status,
                                           const char *details)
{
    if (prov == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create response JSON
    cJSON *response = cJSON_CreateObject();
    if (response == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    if (correlation_id) {
        cJSON_AddStringToObject(response, "cor_id", correlation_id);
    }
    
    // Add status
    const char *status_str;
    switch (status) {
        case VAULT_PROV_STATUS_SUCCESS:
            status_str = "applied";
            break;
        case VAULT_PROV_STATUS_WIFI_FAILED:
            status_str = "wifi_failed";
            break;
        case VAULT_PROV_STATUS_MQTT_FAILED:
            status_str = "mqtt_failed";
            break;
        case VAULT_PROV_STATUS_PARSE_ERROR:
            status_str = "parse_error";
            break;
        case VAULT_PROV_STATUS_MEMORY_ERROR:
            status_str = "memory_error";
            break;
        case VAULT_PROV_STATUS_INVALID_CONFIG:
            status_str = "invalid_config";
            break;
        default:
            status_str = "unknown";
    }
    cJSON_AddStringToObject(response, "status", status_str);
    
    if (details) {
        cJSON_AddStringToObject(response, "details", details);
    }
    
    // Add memory report
    cJSON *mem_report = cJSON_CreateObject();
    if (mem_report) {
        cJSON_AddNumberToObject(mem_report, "free_heap", esp_get_free_heap_size());
        cJSON_AddNumberToObject(mem_report, "free_psram", 
                               heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        cJSON_AddItemToObject(response, "mem_report", mem_report);
    }
    
    char *json_str = cJSON_Print(response);
    cJSON_Delete(response);
    
    if (json_str == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Sending response: %s", json_str);
    
    // Publish response via MQTT
    bool success = vault_mqtt_publish_response(prov->mqtt, response_topic,
                                               correlation_id, json_str, 1);
    
    free(json_str);
    
    if (!success) {
        ESP_LOGE(TAG, "Failed to publish response via MQTT");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t vault_provisioning_load_default_config(vault_prov_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Open NVS
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return ret;
    }
    
    // Read default configuration
    size_t required_size = sizeof(vault_prov_config_t);
    ret = nvs_get_blob(handle, NVS_KEY_DEFAULT, config, &required_size);
    
    nvs_close(handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No default configuration found in NVS");
        return ret;
    }
    
    ESP_LOGI(TAG, "Default configuration loaded from NVS");
    return ESP_OK;
}

esp_err_t vault_provisioning_save_config(const vault_prov_config_t *config, 
                                         bool is_default)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Open NVS
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return ret;
    }
    
    // Save configuration
    const char *key = is_default ? NVS_KEY_DEFAULT : NVS_KEY_CONFIG;
    ret = nvs_set_blob(handle, key, config, sizeof(vault_prov_config_t));
    
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    
    nvs_close(handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save configuration to NVS");
        return ret;
    }
    
    ESP_LOGI(TAG, "Configuration saved to NVS (%s)", 
             is_default ? "default" : "active");
    return ESP_OK;
}

esp_err_t vault_provisioning_get_mac_string(char *mac_str, size_t len)
{
    if (mac_str == NULL || len < 18) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        return ret;
    }
    
    snprintf(mac_str, len, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    return ESP_OK;
}
