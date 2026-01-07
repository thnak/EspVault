/**
 * @file vault_mqtt.h
 * @brief MQTT 5.0 Client for EspVault Universal Node
 * 
 * This file defines the MQTT client interface for communication
 * with the broker, including replay functionality.
 */

#ifndef VAULT_MQTT_H
#define VAULT_MQTT_H

#include <stdint.h>
#include <stdbool.h>
#include "vault_protocol.h"
#include "vault_memory.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

// MQTT Topics
#define VAULT_MQTT_TOPIC_EVENT      "vault/event"
#define VAULT_MQTT_TOPIC_CONFIG     "vault/config"
#define VAULT_MQTT_TOPIC_HEARTBEAT  "vault/heartbeat"
#define VAULT_MQTT_TOPIC_COMMAND    "vault/command"

// Provisioning topics (MQTT v5)
#define VAULT_MQTT_TOPIC_PROV_CFG   "dev/cfg/"  // Append MAC address
#define VAULT_MQTT_TOPIC_PROV_RES   "dev/res/"  // Append MAC address

/**
 * @brief MQTT client configuration
 */
typedef struct {
    const char *broker_uri;         // MQTT broker URI
    const char *client_id;          // Unique client identifier
    const char *username;           // Optional username
    const char *password;           // Optional password
    const char *ca_cert;            // CA certificate for TLS
    uint16_t port;                  // Broker port (1883 or 8883)
    bool use_tls;                   // Enable TLS/SSL
} vault_mqtt_config_t;

// Forward declaration
typedef void (*vault_mqtt_command_cb_t)(const vault_packet_t *packet, void *user_data);
typedef void (*vault_mqtt_provisioning_cb_t)(const char *data, int data_len, 
                                              const char *response_topic,
                                              const char *correlation_data,
                                              void *user_data);

/**
 * @brief MQTT client handle
 */
typedef struct {
    esp_mqtt_client_handle_t client;
    vault_memory_t *memory;
    vault_mqtt_command_cb_t command_callback;
    void *command_user_data;
    vault_mqtt_provisioning_cb_t prov_callback;
    void *prov_user_data;
    char *device_mac;  // Device MAC address for topic construction
    bool connected;
    bool initialized;
} vault_mqtt_t;

/**
 * @brief Command callback function type
 * 
 * @param packet Received command packet
 * @param user_data User-provided context
 */
typedef void (*vault_mqtt_command_cb_t)(const vault_packet_t *packet, void *user_data);

/**
 * @brief Register callback for provisioning messages (MQTT v5)
 * 
 * @param mqtt MQTT client handle
 * @param callback Callback function
 * @param user_data User context passed to callback
 */
void vault_mqtt_register_provisioning_cb(vault_mqtt_t *mqtt,
                                         vault_mqtt_provisioning_cb_t callback,
                                         void *user_data);

/**
 * @brief Publish response to MQTT v5 Response Topic
 * 
 * @param mqtt MQTT client handle
 * @param response_topic Response topic from request
 * @param correlation_data Correlation data from request
 * @param payload Response payload (JSON string)
 * @param qos Quality of Service
 * @return true on success, false on failure
 */
bool vault_mqtt_publish_response(vault_mqtt_t *mqtt,
                                 const char *response_topic,
                                 const char *correlation_data,
                                 const char *payload,
                                 int qos);

/**
 * @brief Subscribe to provisioning configuration topic
 * 
 * @param mqtt MQTT client handle
 * @return true on success, false on failure
 */
bool vault_mqtt_subscribe_provisioning(vault_mqtt_t *mqtt);

/**
 * @brief Initialize MQTT client
 * 
 * @param config MQTT configuration
 * @param memory Memory manager handle
 * @return Pointer to MQTT client handle, NULL on failure
 */
vault_mqtt_t* vault_mqtt_init(const vault_mqtt_config_t *config, vault_memory_t *memory);

/**
 * @brief Deinitialize MQTT client
 * 
 * @param mqtt MQTT client handle
 */
void vault_mqtt_deinit(vault_mqtt_t *mqtt);

/**
 * @brief Start MQTT client connection
 * 
 * @param mqtt MQTT client handle
 * @return true on success, false on failure
 */
bool vault_mqtt_start(vault_mqtt_t *mqtt);

/**
 * @brief Stop MQTT client
 * 
 * @param mqtt MQTT client handle
 */
void vault_mqtt_stop(vault_mqtt_t *mqtt);

/**
 * @brief Publish event packet to broker
 * 
 * @param mqtt MQTT client handle
 * @param packet Event packet to publish
 * @return true on success, false on failure
 */
bool vault_mqtt_publish_event(vault_mqtt_t *mqtt, const vault_packet_t *packet);

/**
 * @brief Publish heartbeat to broker
 * 
 * @param mqtt MQTT client handle
 * @return true on success, false on failure
 */
bool vault_mqtt_publish_heartbeat(vault_mqtt_t *mqtt);

/**
 * @brief Register callback for command messages
 * 
 * @param mqtt MQTT client handle
 * @param callback Callback function
 * @param user_data User context passed to callback
 */
void vault_mqtt_register_command_cb(vault_mqtt_t *mqtt, 
                                     vault_mqtt_command_cb_t callback,
                                     void *user_data);

/**
 * @brief Handle replay command from broker
 * 
 * @param mqtt MQTT client handle
 * @param seq_start Starting sequence number
 * @param seq_end Ending sequence number
 * @return Number of packets replayed
 */
size_t vault_mqtt_handle_replay(vault_mqtt_t *mqtt, uint32_t seq_start, uint32_t seq_end);

/**
 * @brief Check if MQTT client is connected
 * 
 * @param mqtt MQTT client handle
 * @return true if connected, false otherwise
 */
bool vault_mqtt_is_connected(vault_mqtt_t *mqtt);

#ifdef __cplusplus
}
#endif

#endif // VAULT_MQTT_H
