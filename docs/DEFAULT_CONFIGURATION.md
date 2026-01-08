# Default Configuration Guide for EspVault

This document explains how to configure and manage the default (fallback) configuration for remote provisioning.

## Overview

The default configuration acts as a **fallback** to prevent device bricking. If production credentials fail or are corrupted, the device can fall back to the staging network using this default configuration.

## Purpose

- **First Boot**: Devices boot with staging network credentials to receive provisioning
- **Fallback**: If production config fails, device reverts to staging network
- **Recovery**: Allows re-provisioning without physical access

## Where Default Config is Stored

The default configuration is stored in **NVS (Non-Volatile Storage)** with key `prov_default`.

**NVS Keys**:
- `prov_default` - Default/fallback configuration (staging network)
- `prov_config` - Active configuration (production network)

## How to Set Default Configuration

### Option 1: Programmatically in Code (Recommended for Manufacturing)

Add initialization code in your application's startup (e.g., `main/main.c`):

```c
#include "vault_provisioning.h"

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Check if default config exists
    vault_prov_config_t test_config;
    ret = vault_provisioning_load_default_config(&test_config);
    
    if (ret != ESP_OK) {
        // No default config found, create one
        ESP_LOGI(TAG, "Creating default staging configuration...");
        
        vault_prov_config_t default_config = {
            .config_id = 0,
            .wifi = {
                .ssid = "Staging_Network",
                .password = "staging_password_123",
                .ip = {
                    .type = VAULT_IP_TYPE_DHCP,
                    .address = "",
                    .gateway = "",
                    .netmask = ""
                }
            },
            .mqtt = {
                .broker_uri = "mqtt://staging.broker.local",
                .port = 1883,
                .use_ssl = false,
                .ca_cert = NULL,
                .client_cert = NULL,
                .client_key = NULL,
                .username = "",
                .password = ""
            }
        };
        
        // Save as default configuration
        ret = vault_provisioning_save_config(&default_config, true);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Default configuration saved successfully");
        } else {
            ESP_LOGE(TAG, "Failed to save default configuration");
        }
    } else {
        ESP_LOGI(TAG, "Default configuration already exists");
        ESP_LOGI(TAG, "  Staging SSID: %s", test_config.wifi.ssid);
        ESP_LOGI(TAG, "  Staging Broker: %s", test_config.mqtt.broker_uri);
    }
    
    // Continue with rest of initialization...
}
```

### Option 2: Using NVS Partition Tool (Manufacturing Flash)

For mass production, you can pre-flash the default configuration:

1. **Create NVS CSV file** (`nvs_default_config.csv`):

```csv
key,type,encoding,value
prov,namespace,,
prov_has_def,data,u8,1
```

2. **Generate NVS partition binary**:

```bash
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate nvs_default_config.csv nvs_default.bin 0x6000
```

3. **Flash during manufacturing**:

```bash
esptool.py --port /dev/ttyUSB0 write_flash 0x9000 nvs_default.bin
```

### Option 3: Using menuconfig (Build-time Configuration)

Add a Kconfig file to the provisioning component for build-time defaults.

**Create** `components/vault_provisioning/Kconfig`:

```kconfig
menu "Vault Provisioning Configuration"

    config VAULT_PROV_DEFAULT_SSID
        string "Default Staging SSID"
        default "Staging_Network"
        help
            Default WiFi SSID for staging network

    config VAULT_PROV_DEFAULT_PASSWORD
        string "Default Staging Password"
        default "staging_password"
        help
            Default WiFi password for staging network

    config VAULT_PROV_DEFAULT_BROKER
        string "Default MQTT Broker URI"
        default "mqtt://staging.local"
        help
            Default MQTT broker URI for staging network

    config VAULT_PROV_DEFAULT_PORT
        int "Default MQTT Port"
        default 1883
        help
            Default MQTT broker port

    config VAULT_PROV_USE_SSL
        bool "Use SSL for Staging"
        default n
        help
            Enable SSL/TLS for staging broker connection

endmenu
```

Then modify the initialization code to use Kconfig values:

```c
vault_prov_config_t default_config = {
    .config_id = 0,
    .wifi = {
        .ssid = CONFIG_VAULT_PROV_DEFAULT_SSID,
        .password = CONFIG_VAULT_PROV_DEFAULT_PASSWORD,
        .ip = {
            .type = VAULT_IP_TYPE_DHCP
        }
    },
    .mqtt = {
        .broker_uri = CONFIG_VAULT_PROV_DEFAULT_BROKER,
        .port = CONFIG_VAULT_PROV_DEFAULT_PORT,
        .use_ssl = CONFIG_VAULT_PROV_USE_SSL,
        .ca_cert = NULL,
        .client_cert = NULL,
        .client_key = NULL
    }
};
```

Configure via menuconfig:
```bash
idf.py menuconfig
# Navigate to: Component config -> Vault Provisioning Configuration
```

## Configuration Structure

```c
typedef struct {
    uint32_t config_id;                 // Configuration ID (0 for default)
    
    vault_wifi_config_t wifi = {
        char ssid[32];                  // WiFi SSID
        char password[64];              // WiFi password
        vault_ip_config_t ip = {
            vault_ip_type_t type;       // DHCP or STATIC
            char address[16];           // IP address (if static)
            char gateway[16];           // Gateway (if static)
            char netmask[16];           // Netmask (if static)
        }
    };
    
    vault_mqtt_prov_config_t mqtt = {
        char broker_uri[128];           // MQTT broker URI
        uint16_t port;                  // MQTT port
        bool use_ssl;                   // Enable SSL/TLS
        char *ca_cert;                  // CA certificate (optional)
        char *client_cert;              // Client cert (optional)
        char *client_key;               // Client key (optional)
        char username[64];              // MQTT username (optional)
        char password[64];              // MQTT password (optional)
    }
} vault_prov_config_t;
```

## Boot Flow with Default Configuration

```
┌─────────────────────┐
│   Device Powers On  │
└──────────┬──────────┘
           │
           ▼
    ┌──────────────┐
    │ Load Active  │
    │ Config (NVS) │
    └──────┬───────┘
           │
           ▼
    ┌──────────────┐
    │ Config Valid?│──No──┐
    └──────┬───────┘      │
           │ Yes          │
           │              ▼
           │      ┌──────────────────┐
           │      │ Load Default     │
           │      │ Config (NVS)     │
           │      └────────┬─────────┘
           │               │
           │               ▼
           │      ┌──────────────────┐
           │      │ Connect to       │
           │      │ Staging Network  │
           │      └────────┬─────────┘
           │               │
           │               ▼
           │      ┌──────────────────┐
           │      │ Wait for         │
           │      │ Provisioning     │
           │      └──────────────────┘
           │
           ▼
    ┌──────────────────┐
    │ Connect to       │
    │ Production       │
    └──────────────────┘
```

## API Functions

### Save Default Configuration

```c
esp_err_t vault_provisioning_save_config(
    const vault_prov_config_t *config, 
    bool is_default
);
```

**Parameters**:
- `config`: Configuration structure to save
- `is_default`: `true` to save as default, `false` to save as active

**Returns**: ESP_OK on success

### Load Default Configuration

```c
esp_err_t vault_provisioning_load_default_config(
    vault_prov_config_t *config
);
```

**Parameters**:
- `config`: Output buffer for configuration

**Returns**: ESP_OK on success, ESP_ERR_NOT_FOUND if no default config exists

## Example: Complete Initialization

```c
#include "nvs_flash.h"
#include "vault_provisioning.h"

static const char *TAG = "app_main";

void initialize_default_config(void)
{
    vault_prov_config_t default_config = {
        .config_id = 0,
        .wifi = {
            .ssid = "MyStaging_WiFi",
            .password = "secure_staging_pass",
            .ip = {
                .type = VAULT_IP_TYPE_DHCP
            }
        },
        .mqtt = {
            .broker_uri = "mqtt://staging.example.com",
            .port = 1883,
            .use_ssl = false,
            .username = "",
            .password = ""
        }
    };
    
    // Try to load existing default
    vault_prov_config_t existing;
    esp_err_t ret = vault_provisioning_load_default_config(&existing);
    
    if (ret == ESP_ERR_NOT_FOUND) {
        // No default exists, save the hardcoded one
        ESP_LOGI(TAG, "Creating default configuration...");
        ret = vault_provisioning_save_config(&default_config, true);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Default configuration saved");
        } else {
            ESP_LOGE(TAG, "Failed to save default config: %s", 
                     esp_err_to_name(ret));
        }
    } else if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Default configuration exists:");
        ESP_LOGI(TAG, "  SSID: %s", existing.wifi.ssid);
        ESP_LOGI(TAG, "  Broker: %s:%d", 
                 existing.mqtt.broker_uri, existing.mqtt.port);
    } else {
        ESP_LOGE(TAG, "Error loading default config: %s", 
                 esp_err_to_name(ret));
    }
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || 
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize default configuration
    initialize_default_config();
    
    // Continue with provisioning and application setup...
}
```

## Security Considerations

### DO

✅ **Use DHCP for staging**: Simplifies network setup  
✅ **Keep staging credentials separate**: Never use production creds in default  
✅ **Use non-SSL for staging** (optional): Reduces certificate management  
✅ **Document staging network requirements**: Share with deployment team  
✅ **Test fallback mechanism**: Verify device can recover  

### DON'T

❌ **Don't hardcode production credentials**: Security risk  
❌ **Don't skip default config**: Device can brick without it  
❌ **Don't use weak staging passwords**: Still needs basic security  
❌ **Don't expose staging network publicly**: Keep it isolated  

## Changing Default Config After Deployment

To update the default configuration on deployed devices:

1. **Send provisioning command** with `"id": 0` to update default:

```json
{
  "id": 0,
  "wifi": {
    "s": "NewStaging_Network",
    "p": "new_staging_password"
  },
  "mqtt": {
    "u": "mqtt://new-staging.example.com",
    "port": 1883,
    "ssl": false
  }
}
```

2. **Device will update default NVS entry** when `id` is 0

## Troubleshooting

### Device won't connect after provisioning

**Cause**: Production credentials incorrect, but fallback not working  
**Solution**: Verify default config exists and is valid

```c
vault_prov_config_t config;
esp_err_t ret = vault_provisioning_load_default_config(&config);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "No default config! Device may be bricked.");
}
```

### NVS partition full

**Cause**: Too many writes to NVS  
**Solution**: Increase NVS partition size in `partitions.csv`

```
nvs,      data, nvs,     0x9000,  0x6000,
```

### Default config not persisting

**Cause**: NVS not initialized before save  
**Solution**: Always call `nvs_flash_init()` before provisioning

## References

- [Remote Provisioning Guide](REMOTE_PROVISIONING.md)
- [Provisioning Examples](PROVISIONING_EXAMPLES.md)
- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)

## Summary

**Key Points**:
1. Default config must be set before first provisioning
2. Stored in NVS with key `prov_default`
3. Acts as fallback if production config fails
4. Should contain staging network credentials
5. Can be set via code, menuconfig, or pre-flashed NVS

**Recommended Approach**: Use Option 1 (programmatic) with hardcoded staging credentials checked into version control, or Option 3 (menuconfig) for flexibility.
