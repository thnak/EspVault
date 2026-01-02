# Security Configuration Guide for EspVault

This document provides detailed instructions for enabling security features in production deployments.

## Overview

EspVault supports three levels of security:
1. **Flash Encryption** - Protects firmware and NVS data from physical readout
2. **Secure Boot V2** - Ensures only signed firmware can run
3. **NVS Encryption** - Protects sensitive configuration data

## Prerequisites

- ESP-IDF v5.0 or later
- Python 3.7+
- espsecure.py and espefuse.py tools (included in ESP-IDF)

## Flash Encryption

### Generate Encryption Key

```bash
# Generate a random 256-bit key
espsecure.py generate_flash_encryption_key flash_encryption_key.bin
```

⚠️ **IMPORTANT**: Store this key securely! If lost, the device cannot be updated.

### Enable Flash Encryption in sdkconfig

```
CONFIG_SECURE_FLASH_ENC_ENABLED=y
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y
CONFIG_SECURE_FLASH_REQUIRE_ALREADY_ENABLED=n
CONFIG_SECURE_BOOT_ALLOW_ROM_BASIC=n
CONFIG_SECURE_BOOT_ALLOW_JTAG=n
```

### Burn Encryption Key to eFuse

```bash
espefuse.py --port /dev/ttyUSB0 burn_key \
  BLOCK1 flash_encryption_key.bin FLASH_ENCRYPTION
```

### First Encrypted Flash

```bash
idf.py flash monitor
```

The bootloader will automatically encrypt the flash on first boot.

## Secure Boot V2

### Generate Signing Key

```bash
espsecure.py generate_signing_key --version 2 secure_boot_signing_key.pem
```

⚠️ **IMPORTANT**: This private key must be kept secret. Anyone with access can sign firmware.

### Enable Secure Boot in sdkconfig

```
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_BOOT_ENABLE_AGGRESSIVE_KEY_REVOKE=y
CONFIG_SECURE_BOOT_SIGNING_KEY="secure_boot_signing_key.pem"
```

### Build and Sign Firmware

```bash
# Build with secure boot enabled
idf.py build

# The build process automatically signs the bootloader and app
# Output: build/bootloader/bootloader.bin (signed)
# Output: build/EspVault.bin (signed)
```

### Burn Secure Boot Key to eFuse

```bash
espsecure.py burn_key_digest --port /dev/ttyUSB0 \
  BLOCK2 secure_boot_signing_key.pem
```

### Flash Signed Firmware

```bash
esptool.py --port /dev/ttyUSB0 write_flash \
  0x1000 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x20000 build/EspVault.bin
```

## NVS Encryption

### Enable NVS Encryption in sdkconfig

```
CONFIG_NVS_ENCRYPTION=y
CONFIG_NVS_SEC_KEY_PROTECTION_SCHEME=1  # Flash encryption based
```

### Generate NVS Encryption Key

```bash
# Generate encryption key partition
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py \
  generate-key --keyfile nvs_encryption_key.bin
```

### Create Encrypted Factory Partition

```bash
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py \
  encrypt factory/nvs_config.csv factory_nvs_encrypted.bin 0x6000 \
  --keygen --keyfile nvs_encryption_key.bin
```

### Flash Encrypted NVS Partition

```bash
esptool.py --port /dev/ttyUSB0 write_flash 0x10000 factory_nvs_encrypted.bin
```

## Combined Setup (All Security Features)

### Step 1: Configure sdkconfig

```bash
idf.py menuconfig
```

Enable:
- Security features → Enable flash encryption on boot
- Security features → Enable hardware Secure Boot in bootloader
- Component config → NVS → Enable NVS encryption

### Step 2: Generate Keys

```bash
# Flash encryption key
espsecure.py generate_flash_encryption_key flash_encryption_key.bin

# Secure boot signing key
espsecure.py generate_signing_key --version 2 secure_boot_signing_key.pem

# NVS encryption key (automatically generated during build)
```

### Step 3: Build Firmware

```bash
idf.py build
```

### Step 4: Burn Keys to eFuse (ONE-TIME OPERATION!)

⚠️ **WARNING**: This is irreversible! Ensure you have backups of all keys.

```bash
# Burn flash encryption key
espefuse.py --port /dev/ttyUSB0 burn_key \
  BLOCK1 flash_encryption_key.bin FLASH_ENCRYPTION

# Burn secure boot key
espsecure.py burn_key_digest --port /dev/ttyUSB0 \
  BLOCK2 secure_boot_signing_key.pem

# Enable secure boot and flash encryption
espefuse.py --port /dev/ttyUSB0 burn_efuse FLASH_CRYPT_CNT
espefuse.py --port /dev/ttyUSB0 burn_efuse FLASH_CRYPT_CONFIG 0x0F
```

### Step 5: Flash Firmware

```bash
idf.py flash monitor
```

## Verification

### Check eFuse Status

```bash
espefuse.py --port /dev/ttyUSB0 summary
```

Look for:
- `FLASH_CRYPT_CNT`: Should be non-zero (flash encryption enabled)
- `SECURE_BOOT_KEY_DIGEST`: Should show digest (secure boot enabled)

### Verify Encrypted Flash

Try to read flash:
```bash
esptool.py --port /dev/ttyUSB0 read_flash 0x20000 0x100000 firmware_dump.bin
```

The dumped firmware should be encrypted (random-looking data).

## OTA Updates with Security

### Sign OTA Update

```bash
espsecure.py sign_data --version 2 \
  --keyfile secure_boot_signing_key.pem \
  --output EspVault_signed.bin \
  build/EspVault.bin
```

### Verify Signature

```bash
espsecure.py verify_signature --version 2 \
  --keyfile secure_boot_signing_key.pem \
  EspVault_signed.bin
```

## Key Management Best Practices

1. **Store Keys Securely**
   - Use hardware security modules (HSM) for production keys
   - Never commit keys to version control
   - Use key rotation policies

2. **Key Backup**
   - Maintain encrypted backups of all keys
   - Store in multiple secure locations
   - Document key recovery procedures

3. **Access Control**
   - Limit key access to authorized personnel only
   - Implement multi-person authorization for key operations
   - Audit all key usage

4. **Development vs Production**
   - Use separate keys for development and production
   - Never use development keys in production devices
   - Implement key lifecycle management

## Troubleshooting

### Device Won't Boot After Flash Encryption

- Check if FLASH_CRYPT_CNT eFuse is set correctly
- Verify partition table matches encrypted layout
- Ensure bootloader is properly signed (if secure boot enabled)

### Secure Boot Verification Failed

- Verify signing key matches burned key digest
- Check if firmware was signed correctly
- Ensure secure boot version matches (V1 vs V2)

### NVS Read Errors

- Check if NVS encryption key is accessible
- Verify partition is formatted correctly
- Ensure flash encryption is not interfering

## References

- [ESP-IDF Flash Encryption](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/flash-encryption.html)
- [ESP-IDF Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html)
- [NVS Encryption](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#nvs-encryption)
