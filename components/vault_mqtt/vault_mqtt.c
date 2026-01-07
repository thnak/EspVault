/**
 * @file vault_mqtt.c
 * @brief MQTT 5.0 Client Implementation for EspVault Universal Node
 */

#include "vault_mqtt.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>

static const char *TAG = "vault_mqtt";

/**
 * @brief MQTT event handler
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    vault_mqtt_t *mqtt = (vault_mqtt_t *)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected to broker");
        mqtt->connected = true;
        
        // Subscribe to command topic
        esp_mqtt_client_subscribe(mqtt->client, VAULT_MQTT_TOPIC_COMMAND, 1);
        ESP_LOGI(TAG, "Subscribed to %s", VAULT_MQTT_TOPIC_COMMAND);
        
        // Subscribe to provisioning topic if MAC is available
        if (mqtt->device_mac) {
            vault_mqtt_subscribe_provisioning(mqtt);
        }
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected from broker");
        mqtt->connected = false;
        break;
        
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT data received on topic: %.*s", 
                 event->topic_len, event->topic);
        
        // Check if this is a provisioning message
        if (mqtt->device_mac && 
            strncmp(event->topic, VAULT_MQTT_TOPIC_PROV_CFG, 
                    strlen(VAULT_MQTT_TOPIC_PROV_CFG)) == 0) {
            
            // Extract MQTT v5 properties if available
            // Note: Response topic and correlation data extraction requires
            // proper MQTT v5 property parsing. For now, we construct the
            // response topic from the device MAC.
            const char *response_topic = NULL;
            const char *correlation_data = NULL;
            
            // TODO: Extract MQTT v5 properties from event->property
            // This requires proper implementation based on ESP-IDF MQTT v5 API
            
            ESP_LOGI(TAG, "Provisioning message received");
            if (response_topic) {
                ESP_LOGI(TAG, "Response Topic: %s", response_topic);
            }
            if (correlation_data) {
                ESP_LOGI(TAG, "Correlation Data: %s", correlation_data);
            }
            
            // Call provisioning callback
            if (mqtt->prov_callback) {
                mqtt->prov_callback(event->data, event->data_len,
                                  response_topic, correlation_data,
                                  mqtt->prov_user_data);
            }
        }
        // Parse command packet
        else if (strncmp(event->topic, VAULT_MQTT_TOPIC_COMMAND, event->topic_len) == 0) {
            vault_packet_t packet;
            if (vault_protocol_parse((uint8_t *)event->data, event->data_len, &packet)) {
                // Handle replay command
                if (packet.cmd == VAULT_CMD_REPLAY) {
                    uint32_t seq_start = packet.seq;
                    uint32_t seq_end = packet.val;  // End sequence in val field
                    ESP_LOGI(TAG, "Replay command: %lu to %lu", seq_start, seq_end);
                    vault_mqtt_handle_replay(mqtt, seq_start, seq_end);
                }
                
                // Call user callback if registered
                if (mqtt->command_callback != NULL) {
                    mqtt->command_callback(&packet, mqtt->command_user_data);
                }
            }
        }
        break;
        
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error occurred");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", 
                     event->error_handle->esp_tls_last_esp_err);
            ESP_LOGE(TAG, "Last tls stack error number: 0x%x", 
                     event->error_handle->esp_tls_stack_err);
        }
        break;
        
    default:
        ESP_LOGD(TAG, "MQTT event id: %d", event_id);
        break;
    }
}

vault_mqtt_t* vault_mqtt_init(const vault_mqtt_config_t *config, vault_memory_t *memory)
{
    if (config == NULL || memory == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return NULL;
    }
    
    vault_mqtt_t *mqtt = malloc(sizeof(vault_mqtt_t));
    if (mqtt == NULL) {
        ESP_LOGE(TAG, "Failed to allocate MQTT handle");
        return NULL;
    }
    
    memset(mqtt, 0, sizeof(vault_mqtt_t));
    mqtt->memory = memory;
    
    // Get device MAC address for provisioning topics
    mqtt->device_mac = malloc(18);
    if (mqtt->device_mac) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(mqtt->device_mac, 18, "%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        ESP_LOGI(TAG, "Device MAC: %s", mqtt->device_mac);
    }
    
    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = config->broker_uri,
        .credentials.client_id = config->client_id,
        .credentials.username = config->username,
        .credentials.authentication.password = config->password,
        .session.protocol_ver = MQTT_PROTOCOL_V_5,  // MQTT 5.0
    };
    
    // Add TLS configuration if enabled
    if (config->use_tls && config->ca_cert != NULL) {
        mqtt_cfg.broker.verification.certificate = config->ca_cert;
        mqtt_cfg.broker.verification.skip_cert_common_name_check = false;
    }
    
    mqtt->client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt->client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        if (mqtt->device_mac) free(mqtt->device_mac);
        free(mqtt);
        return NULL;
    }
    
    // Register event handler
    esp_mqtt_client_register_event(mqtt->client, ESP_EVENT_ANY_ID, 
                                   mqtt_event_handler, mqtt);
    
    mqtt->initialized = true;
    ESP_LOGI(TAG, "MQTT client initialized");
    
    return mqtt;
}

void vault_mqtt_deinit(vault_mqtt_t *mqtt)
{
    if (mqtt == NULL) {
        return;
    }
    
    if (mqtt->client != NULL) {
        esp_mqtt_client_destroy(mqtt->client);
    }
    
    if (mqtt->device_mac != NULL) {
        free(mqtt->device_mac);
    }
    
    free(mqtt);
    ESP_LOGI(TAG, "MQTT client deinitialized");
}

bool vault_mqtt_start(vault_mqtt_t *mqtt)
{
    if (mqtt == NULL || !mqtt->initialized) {
        return false;
    }
    
    esp_err_t err = esp_mqtt_client_start(mqtt->client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "MQTT client started");
    return true;
}

void vault_mqtt_stop(vault_mqtt_t *mqtt)
{
    if (mqtt == NULL || !mqtt->initialized) {
        return;
    }
    
    esp_mqtt_client_stop(mqtt->client);
    mqtt->connected = false;
    ESP_LOGI(TAG, "MQTT client stopped");
}

bool vault_mqtt_publish_event(vault_mqtt_t *mqtt, const vault_packet_t *packet)
{
    if (mqtt == NULL || !mqtt->initialized || !mqtt->connected || packet == NULL) {
        return false;
    }
    
    // Serialize packet to binary
    uint8_t data[VAULT_PROTO_PACKET_SIZE];
    size_t len = vault_protocol_serialize(packet, data);
    
    // Publish to event topic
    int msg_id = esp_mqtt_client_publish(mqtt->client, VAULT_MQTT_TOPIC_EVENT,
                                         (const char *)data, len, 1, 0);
    
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish event");
        return false;
    }
    
    ESP_LOGD(TAG, "Published event, seq=%lu, msg_id=%d", packet->seq, msg_id);
    return true;
}

bool vault_mqtt_publish_heartbeat(vault_mqtt_t *mqtt)
{
    if (mqtt == NULL || !mqtt->initialized || !mqtt->connected) {
        return false;
    }
    
    // Create heartbeat packet
    vault_packet_t packet;
    vault_protocol_init_packet(&packet, VAULT_CMD_HEARTBEAT, 
                               vault_memory_get_next_seq(mqtt->memory));
    vault_protocol_finalize_packet(&packet);
    
    // Serialize and publish
    uint8_t data[VAULT_PROTO_PACKET_SIZE];
    vault_protocol_serialize(&packet, data);
    
    int msg_id = esp_mqtt_client_publish(mqtt->client, VAULT_MQTT_TOPIC_HEARTBEAT,
                                         (const char *)data, VAULT_PROTO_PACKET_SIZE, 
                                         0, 0);
    
    return (msg_id >= 0);
}

void vault_mqtt_register_command_cb(vault_mqtt_t *mqtt, 
                                     vault_mqtt_command_cb_t callback,
                                     void *user_data)
{
    if (mqtt == NULL || !mqtt->initialized) {
        return;
    }
    
    mqtt->command_callback = callback;
    mqtt->command_user_data = user_data;
    
    ESP_LOGI(TAG, "Command callback registered");
}

size_t vault_mqtt_handle_replay(vault_mqtt_t *mqtt, uint32_t seq_start, uint32_t seq_end)
{
    if (mqtt == NULL || !mqtt->initialized || !mqtt->connected) {
        return 0;
    }
    
    // Allocate buffer for replay packets
    const size_t max_replay = 100;  // Replay up to 100 packets at a time
    vault_packet_t *packets = malloc(max_replay * sizeof(vault_packet_t));
    if (packets == NULL) {
        ESP_LOGE(TAG, "Failed to allocate replay buffer");
        return 0;
    }
    
    // Retrieve packets from history
    size_t count = vault_memory_get_range(mqtt->memory, seq_start, seq_end, 
                                          packets, max_replay);
    
    ESP_LOGI(TAG, "Replaying %d packets from %lu to %lu", count, seq_start, seq_end);
    
    // Mark packets as replayed and republish
    for (size_t i = 0; i < count; i++) {
        packets[i].flags |= VAULT_FLAG_IS_REPLAY;
        vault_protocol_finalize_packet(&packets[i]);  // Recalculate CRC
        vault_mqtt_publish_event(mqtt, &packets[i]);
    }
    
    free(packets);
    return count;
}

bool vault_mqtt_is_connected(vault_mqtt_t *mqtt)
{
    return (mqtt != NULL && mqtt->initialized && mqtt->connected);
}

void vault_mqtt_register_provisioning_cb(vault_mqtt_t *mqtt,
                                         vault_mqtt_provisioning_cb_t callback,
                                         void *user_data)
{
    if (mqtt == NULL || !mqtt->initialized) {
        return;
    }
    
    mqtt->prov_callback = callback;
    mqtt->prov_user_data = user_data;
    
    ESP_LOGI(TAG, "Provisioning callback registered");
}

bool vault_mqtt_publish_response(vault_mqtt_t *mqtt,
                                 const char *response_topic,
                                 const char *correlation_data,
                                 const char *payload,
                                 int qos)
{
    if (mqtt == NULL || !mqtt->initialized || !mqtt->connected) {
        return false;
    }
    
    if (payload == NULL) {
        return false;
    }
    
    // Use provided response topic or construct default
    char topic[128];
    if (response_topic != NULL) {
        strncpy(topic, response_topic, sizeof(topic) - 1);
    } else if (mqtt->device_mac) {
        snprintf(topic, sizeof(topic), "%s%s", 
                 VAULT_MQTT_TOPIC_PROV_RES, mqtt->device_mac);
    } else {
        ESP_LOGE(TAG, "No response topic available");
        return false;
    }
    
    // TODO: Add MQTT v5 properties for correlation data
    // This requires proper implementation based on ESP-IDF MQTT v5 API
    
    // Publish response
    int msg_id = esp_mqtt_client_publish(mqtt->client, topic,
                                         payload, strlen(payload),
                                         qos, 0);
    
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish response");
        return false;
    }
    
    ESP_LOGI(TAG, "Published response to %s, msg_id=%d", topic, msg_id);
    return true;
}

bool vault_mqtt_subscribe_provisioning(vault_mqtt_t *mqtt)
{
    if (mqtt == NULL || !mqtt->initialized || !mqtt->connected) {
        return false;
    }
    
    if (mqtt->device_mac == NULL) {
        ESP_LOGE(TAG, "Device MAC not available");
        return false;
    }
    
    // Construct provisioning topic: dev/cfg/[MAC]
    char topic[64];
    snprintf(topic, sizeof(topic), "%s%s", 
             VAULT_MQTT_TOPIC_PROV_CFG, mqtt->device_mac);
    
    int msg_id = esp_mqtt_client_subscribe(mqtt->client, topic, 1);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to provisioning topic");
        return false;
    }
    
    ESP_LOGI(TAG, "Subscribed to provisioning topic: %s", topic);
    return true;
}
