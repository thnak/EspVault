/**
 * @file main.c
 * @brief Main application for EspVault Universal Node
 * 
 * Implements dual-core task architecture:
 * - Core 0 (PRO_CPU): Capture Task (P15), Logic Task (P10)
 * - Core 1 (APP_CPU): Network Task (P5), Health Task (P1)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "vault_protocol.h"
#include "vault_memory.h"
#include "vault_mqtt.h"
#include "vault_provisioning.h"

static const char *TAG = "main";

// Task handles (exported for provisioning)
TaskHandle_t capture_task_handle = NULL;
TaskHandle_t logic_task_handle = NULL;
TaskHandle_t network_task_handle = NULL;
TaskHandle_t health_task_handle = NULL;

// Global handles
static vault_memory_t *g_memory = NULL;
static vault_mqtt_t *g_mqtt = NULL;
static vault_provisioning_t *g_provisioning = NULL;

// Task stack sizes
#define CAPTURE_TASK_STACK_SIZE  4096
#define LOGIC_TASK_STACK_SIZE    4096
#define NETWORK_TASK_STACK_SIZE  8192
#define HEALTH_TASK_STACK_SIZE   2048

// Task priorities
#define CAPTURE_TASK_PRIORITY    15  // Maximum priority for RMT interrupts
#define LOGIC_TASK_PRIORITY      10
#define NETWORK_TASK_PRIORITY    5
#define HEALTH_TASK_PRIORITY     1

// Core assignments
#define PRO_CPU_NUM              0
#define APP_CPU_NUM              1

/**
 * @brief Capture Task - Handles RMT interrupts and raw hardware pulse timing
 * 
 * This task runs on Core 0 with maximum priority to ensure real-time
 * capture of hardware events without missing data.
 */
static void capture_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Capture Task started on Core %d", xPortGetCoreID());
    
    while (1) {
        // TODO: Implement RMT capture logic
        // - Configure RMT peripheral for pulse width measurement
        // - Handle RMT interrupts
        // - Create event packets from captured data
        
        // Example: Create a test event packet
        vault_packet_t packet;
        vault_protocol_init_packet(&packet, VAULT_CMD_EVENT, 
                                   vault_memory_get_next_seq(g_memory));
        packet.pin = 5;  // Example GPIO
        packet.val = 1000;  // Example pulse width in µs
        packet.flags = VAULT_FLAG_INPUT_STATE;
        vault_protocol_finalize_packet(&packet);
        
        // Store in history
        vault_memory_store_history(g_memory, &packet);
        
        // Queue for network transmission
        vault_memory_queue_network(g_memory, &packet, 100);
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // Placeholder delay
    }
}

/**
 * @brief Logic Task - Manages PSRAM history indexing and sequence counters
 * 
 * This task runs on Core 0 with high priority to maintain data integrity
 * and handle replay requests efficiently.
 */
static void logic_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Logic Task started on Core %d", xPortGetCoreID());
    
    while (1) {
        // TODO: Implement logic processing
        // - Process events from capture task
        // - Manage sequence counter synchronization
        // - Handle configuration updates
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Network Task - Handles Wi-Fi, MQTT, TLS/SSL encryption
 * 
 * This task runs on Core 1 to avoid interfering with time-critical
 * capture operations on Core 0.
 */
static void network_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Network Task started on Core %d", xPortGetCoreID());
    
    while (1) {
        // Dequeue packets from network queue
        vault_packet_t packet;
        if (vault_memory_dequeue_network(g_memory, &packet, 1000)) {
            // Publish to MQTT broker
            if (g_mqtt != NULL && vault_mqtt_is_connected(g_mqtt)) {
                vault_mqtt_publish_event(g_mqtt, &packet);
                ESP_LOGD(TAG, "Published event seq=%u", (unsigned int)packet.seq);
            } else {
                ESP_LOGW(TAG, "MQTT not connected, event buffered");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Health Task - Diagnostic reporting and system heartbeats
 * 
 * This task runs on Core 1 with lowest priority for non-critical
 * monitoring operations.
 */
static void health_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Health Task started on Core %d", xPortGetCoreID());
    
    TickType_t last_heartbeat = xTaskGetTickCount();
    const TickType_t heartbeat_interval = pdMS_TO_TICKS(30000);  // 30 seconds
    
    while (1) {
        TickType_t now = xTaskGetTickCount();
        
        // Send periodic heartbeat
        if ((now - last_heartbeat) >= heartbeat_interval) {
            if (g_mqtt != NULL && vault_mqtt_is_connected(g_mqtt)) {
                vault_mqtt_publish_heartbeat(g_mqtt);
                ESP_LOGI(TAG, "Heartbeat sent");
            }
            last_heartbeat = now;
        }
        
        // Report system health metrics
        ESP_LOGI(TAG, "Free heap: %u bytes, Free PSRAM: %u bytes",
                 (unsigned int)esp_get_free_heap_size(),
                 (unsigned int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        
        vTaskDelay(pdMS_TO_TICKS(10000));  // Check every 10 seconds
    }
}

/**
 * @brief Initialize WiFi in station mode
 */
static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // TODO: Load WiFi credentials from NVS
    // WARNING: These are placeholder values for development only.
    // For production, use: CONFIG_VAULT_WIFI_SSID and CONFIG_VAULT_WIFI_PASSWORD
    // or load from encrypted factory NVS partition
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "YOUR_WIFI_SSID",  // REPLACE WITH REAL SSID
            .password = "YOUR_WIFI_PASSWORD",  // REPLACE WITH REAL PASSWORD
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialization finished");
}

/**
 * @brief Provisioning callback handler for MQTT v5 messages
 */
static void provisioning_message_handler(const char *data, int data_len,
                                        const char *response_topic,
                                        const char *correlation_data,
                                        void *user_data)
{
    ESP_LOGI(TAG, "Provisioning message received (%d bytes)", data_len);
    
    if (g_provisioning == NULL) {
        ESP_LOGE(TAG, "Provisioning manager not initialized");
        return;
    }
    
    // Enter setup mode to free resources
    vault_provisioning_enter_setup_mode(g_provisioning);
    
    // Parse configuration
    vault_prov_config_t config;
    esp_err_t ret = vault_provisioning_parse_config(data, data_len, &config);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse provisioning configuration");
        vault_provisioning_send_response(g_provisioning, response_topic,
                                        correlation_data,
                                        VAULT_PROV_STATUS_PARSE_ERROR,
                                        "Failed to parse JSON configuration");
        vault_provisioning_exit_setup_mode(g_provisioning);
        return;
    }
    
    // Apply configuration (this will test and save)
    ret = vault_provisioning_apply_config(g_provisioning, &config, correlation_data);
    
    // Free allocated resources
    vault_provisioning_free_config(&config);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply configuration");
        vault_provisioning_exit_setup_mode(g_provisioning);
    }
    // Note: If successful, device will restart before exiting setup mode
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "EspVault Universal Node starting...");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");
    
    // Initialize memory manager (PSRAM)
    g_memory = vault_memory_init();
    if (g_memory == NULL) {
        ESP_LOGE(TAG, "Failed to initialize memory manager");
        return;
    }
    ESP_LOGI(TAG, "Memory manager initialized");
    
    // Initialize WiFi
    wifi_init_sta();
    
    // Initialize MQTT client
    // WARNING: These are placeholder values for development only.
    // For production:
    // 1. Enable TLS (.use_tls = true)
    // 2. Load credentials from encrypted factory NVS
    // 3. Use CONFIG_VAULT_MQTT_* settings from menuconfig
    vault_mqtt_config_t mqtt_config = {
        .broker_uri = "mqtt://broker.example.com",  // REPLACE: Use mqtts:// for TLS
        .client_id = "esp32_vault_001",  // REPLACE: Load from NVS
        .username = NULL,  // REPLACE: Load from encrypted NVS
        .password = NULL,  // REPLACE: Load from encrypted NVS
        .ca_cert = NULL,   // REPLACE: Embed or load CA certificate
        .port = 1883,      // REPLACE: Use 8883 for TLS
        .use_tls = false,  // WARNING: Enable TLS for production
    };
    
    // TODO: Load MQTT configuration from encrypted factory NVS
    g_mqtt = vault_mqtt_init(&mqtt_config, g_memory);
    if (g_mqtt != NULL) {
        vault_mqtt_start(g_mqtt);
        ESP_LOGI(TAG, "MQTT client started");
    } else {
        ESP_LOGW(TAG, "Failed to initialize MQTT client");
    }
    
    // Initialize provisioning manager
    if (g_mqtt != NULL) {
        g_provisioning = vault_provisioning_init(g_mqtt, g_memory);
        if (g_provisioning != NULL) {
            // Register provisioning callback
            vault_mqtt_register_provisioning_cb(g_mqtt, 
                                               provisioning_message_handler,
                                               NULL);
            ESP_LOGI(TAG, "Provisioning manager initialized");
        } else {
            ESP_LOGW(TAG, "Failed to initialize provisioning manager");
        }
    }
    
    // Create dual-core tasks
    
    // Core 0 (PRO_CPU) - Time-critical tasks
    xTaskCreatePinnedToCore(
        capture_task,
        "capture_task",
        CAPTURE_TASK_STACK_SIZE,
        NULL,
        CAPTURE_TASK_PRIORITY,
        &capture_task_handle,
        PRO_CPU_NUM
    );
    ESP_LOGI(TAG, "Capture Task created on Core %d", PRO_CPU_NUM);
    
    xTaskCreatePinnedToCore(
        logic_task,
        "logic_task",
        LOGIC_TASK_STACK_SIZE,
        NULL,
        LOGIC_TASK_PRIORITY,
        &logic_task_handle,
        PRO_CPU_NUM
    );
    ESP_LOGI(TAG, "Logic Task created on Core %d", PRO_CPU_NUM);
    
    // Core 1 (APP_CPU) - Network and monitoring tasks
    xTaskCreatePinnedToCore(
        network_task,
        "network_task",
        NETWORK_TASK_STACK_SIZE,
        NULL,
        NETWORK_TASK_PRIORITY,
        &network_task_handle,
        APP_CPU_NUM
    );
    ESP_LOGI(TAG, "Network Task created on Core %d", APP_CPU_NUM);
    
    xTaskCreatePinnedToCore(
        health_task,
        "health_task",
        HEALTH_TASK_STACK_SIZE,
        NULL,
        HEALTH_TASK_PRIORITY,
        &health_task_handle,
        APP_CPU_NUM
    );
    ESP_LOGI(TAG, "Health Task created on Core %d", APP_CPU_NUM);
    
    ESP_LOGI(TAG, "All tasks created successfully");
    ESP_LOGI(TAG, "EspVault Universal Node is running");
}
