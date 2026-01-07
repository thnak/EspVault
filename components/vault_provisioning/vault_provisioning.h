/**
 * @file vault_provisioning.h
 * @brief Remote Provisioning for EspVault Universal Node
 * 
 * This component implements staging network and remote provisioning
 * functionality using MQTT v5 with Response Topic pattern.
 */

#ifndef VAULT_PROVISIONING_H
#define VAULT_PROVISIONING_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "vault_mqtt.h"
#include "vault_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum payload sizes (aligned with MQTT buffer limits)
#define VAULT_PROV_MAX_SSID_LEN         32
#define VAULT_PROV_MAX_PASSWORD_LEN     64
#define VAULT_PROV_MAX_BROKER_URI_LEN   128
#define VAULT_PROV_MAX_CERT_LEN         2048  // SSL certificate max size
#define VAULT_PROV_MAX_KEY_LEN          2048  // SSL key max size
#define VAULT_PROV_MAX_PAYLOAD_LEN      8192  // Total JSON payload limit

// Provisioning topics
#define VAULT_PROV_TOPIC_CFG_PREFIX     "dev/cfg/"
#define VAULT_PROV_TOPIC_RES_PREFIX     "dev/res/"

/**
 * @brief IP configuration type
 */
typedef enum {
    VAULT_IP_TYPE_DHCP = 0,     // DHCP (automatic)
    VAULT_IP_TYPE_STATIC = 1,   // Static IP
} vault_ip_type_t;

/**
 * @brief IP configuration structure
 */
typedef struct {
    vault_ip_type_t type;       // IP type (DHCP or static)
    char address[16];           // IP address (e.g., "192.168.1.100")
    char gateway[16];           // Gateway address
    char netmask[16];           // Netmask (e.g., "255.255.255.0")
} vault_ip_config_t;

/**
 * @brief WiFi configuration structure
 */
typedef struct {
    char ssid[VAULT_PROV_MAX_SSID_LEN];
    char password[VAULT_PROV_MAX_PASSWORD_LEN];
    vault_ip_config_t ip;
} vault_wifi_config_t;

/**
 * @brief MQTT configuration structure
 */
typedef struct {
    char broker_uri[VAULT_PROV_MAX_BROKER_URI_LEN];
    uint16_t port;
    bool use_ssl;
    char *ca_cert;              // Dynamically allocated in PSRAM
    char *client_cert;          // Dynamically allocated in PSRAM
    char *client_key;           // Dynamically allocated in PSRAM
    char username[64];
    char password[64];
} vault_mqtt_prov_config_t;

/**
 * @brief Complete provisioning configuration
 */
typedef struct {
    uint32_t config_id;         // Configuration ID for tracking
    vault_wifi_config_t wifi;
    vault_mqtt_prov_config_t mqtt;
} vault_prov_config_t;

/**
 * @brief Provisioning response status
 */
typedef enum {
    VAULT_PROV_STATUS_SUCCESS = 0,
    VAULT_PROV_STATUS_WIFI_FAILED,
    VAULT_PROV_STATUS_MQTT_FAILED,
    VAULT_PROV_STATUS_PARSE_ERROR,
    VAULT_PROV_STATUS_MEMORY_ERROR,
    VAULT_PROV_STATUS_INVALID_CONFIG,
} vault_prov_status_t;

/**
 * @brief Provisioning manager handle
 */
typedef struct vault_provisioning_s vault_provisioning_t;

/**
 * @brief Initialize provisioning manager
 * 
 * @param mqtt MQTT client handle for staging network
 * @param memory Memory manager handle
 * @return Provisioning manager handle, NULL on failure
 */
vault_provisioning_t* vault_provisioning_init(vault_mqtt_t *mqtt, vault_memory_t *memory);

/**
 * @brief Deinitialize provisioning manager
 * 
 * @param prov Provisioning manager handle
 */
void vault_provisioning_deinit(vault_provisioning_t *prov);

/**
 * @brief Enter setup mode (suspend operational tasks)
 * 
 * This function suspends all non-essential tasks and frees PSRAM
 * to prepare for receiving large configuration payloads.
 * 
 * @param prov Provisioning manager handle
 * @return ESP_OK on success
 */
esp_err_t vault_provisioning_enter_setup_mode(vault_provisioning_t *prov);

/**
 * @brief Exit setup mode (resume operational tasks)
 * 
 * @param prov Provisioning manager handle
 * @return ESP_OK on success
 */
esp_err_t vault_provisioning_exit_setup_mode(vault_provisioning_t *prov);

/**
 * @brief Parse JSON provisioning payload
 * 
 * Parses incoming JSON configuration and validates fields.
 * Allocates certificates in PSRAM if SSL is enabled.
 * 
 * @param json_str JSON string from MQTT
 * @param len Length of JSON string
 * @param config Output configuration structure
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on parse error
 */
esp_err_t vault_provisioning_parse_config(const char *json_str, size_t len, 
                                          vault_prov_config_t *config);

/**
 * @brief Free dynamically allocated config resources
 * 
 * @param config Configuration structure to free
 */
void vault_provisioning_free_config(vault_prov_config_t *config);

/**
 * @brief Test WiFi connection with provided configuration
 * 
 * @param wifi_config WiFi configuration to test
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on successful connection
 */
esp_err_t vault_provisioning_test_wifi(const vault_wifi_config_t *wifi_config, 
                                       uint32_t timeout_ms);

/**
 * @brief Test MQTT connection with provided configuration
 * 
 * @param mqtt_config MQTT configuration to test
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on successful connection
 */
esp_err_t vault_provisioning_test_mqtt(const vault_mqtt_prov_config_t *mqtt_config, 
                                       uint32_t timeout_ms);

/**
 * @brief Apply configuration (dry-run and commit on success)
 * 
 * This function:
 * 1. Tests WiFi connection
 * 2. Tests MQTT connection
 * 3. On success: Saves to NVS and restarts
 * 4. On failure: Sends error response and stays in staging
 * 
 * @param prov Provisioning manager handle
 * @param config Configuration to apply
 * @param correlation_id Correlation ID for response
 * @return ESP_OK on success (device will restart)
 */
esp_err_t vault_provisioning_apply_config(vault_provisioning_t *prov,
                                          const vault_prov_config_t *config,
                                          const char *correlation_id);

/**
 * @brief Send provisioning response via MQTT v5
 * 
 * @param prov Provisioning manager handle
 * @param response_topic MQTT v5 Response Topic
 * @param correlation_id Correlation ID to include
 * @param status Provisioning status
 * @param details Human-readable details string
 * @return ESP_OK on success
 */
esp_err_t vault_provisioning_send_response(vault_provisioning_t *prov,
                                           const char *response_topic,
                                           const char *correlation_id,
                                           vault_prov_status_t status,
                                           const char *details);

/**
 * @brief Load default/fallback configuration from NVS
 * 
 * This ensures the device can always connect to a staging network
 * to prevent bricking.
 * 
 * @param config Output configuration structure
 * @return ESP_OK if default config exists and is loaded
 */
esp_err_t vault_provisioning_load_default_config(vault_prov_config_t *config);

/**
 * @brief Save configuration to NVS
 * 
 * @param config Configuration to save
 * @param is_default If true, save as default fallback config
 * @return ESP_OK on success
 */
esp_err_t vault_provisioning_save_config(const vault_prov_config_t *config, 
                                         bool is_default);

/**
 * @brief Get MAC address as string for topic construction
 * 
 * @param mac_str Output buffer (must be at least 18 bytes)
 * @return ESP_OK on success
 */
esp_err_t vault_provisioning_get_mac_string(char *mac_str, size_t len);

#ifdef __cplusplus
}
#endif

#endif // VAULT_PROVISIONING_H
