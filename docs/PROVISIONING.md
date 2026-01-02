# Factory Provisioning Guide for EspVault

This document describes the factory provisioning process for EspVault Universal Node devices during manufacturing.

## Overview

Factory provisioning allows you to pre-configure devices with:
- WiFi credentials
- MQTT broker settings
- Device-specific identifiers
- SSL/TLS certificates
- Security keys

## NVS Partition Generation

### Step 1: Prepare Configuration CSV

Edit `factory/nvs_config.csv` with device-specific values:

```csv
key,type,encoding,value
vault,namespace,,
wifi_ssid,data,string,ProductionWiFi
wifi_password,data,string,SecurePassword123
device_id,data,string,VAULT-00001
mqtt_broker,data,string,mqtts://prod.broker.com:8883
mqtt_username,data,string,device_vault_00001
mqtt_password,data,string,DeviceSecretKey
```

### Step 2: Generate NVS Binary

```bash
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py \
  generate factory/nvs_config.csv factory_nvs.bin 0x6000
```

### Step 3: Flash Factory Partition

```bash
esptool.py --port /dev/ttyUSB0 write_flash 0x10000 factory_nvs.bin
```

## Certificate Provisioning

### Embed Certificates in Firmware

For small-scale deployments, embed certificates directly in the binary.

### Store Certificates in NVS

For device-specific certificates, use NVS with base64 encoding.

## WiFi Provisioning Alternative

For field deployment without factory access, use WiFi provisioning via BLE or SoftAP.

## Quality Control Checks

### Post-Provisioning Tests

1. Flash Check - Verify flash integrity
2. NVS Check - Verify NVS partition is readable
3. WiFi Connect - Test WiFi connectivity
4. MQTT Connect - Test MQTT connection

## Production Workflow

### Recommended Manufacturing Process

1. Flash Bootloader & Partition Table
2. Flash Application
3. Provision Device-Specific Data
4. Enable Security Features (if required)
5. Run Quality Tests
6. Label & Package

## References

- [NVS Partition Generator](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_partition_gen.html)
- [WiFi Provisioning](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/provisioning/provisioning.html)
