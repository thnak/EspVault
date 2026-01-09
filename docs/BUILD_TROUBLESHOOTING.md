# Build Troubleshooting Guide

## Common Build Issues

### Issue 1: Flash Size Configuration Not Applied

**Symptom**: Build fails with error:
```
Partitions tables occupies 3.9MB of flash (4128768 bytes) which does not fit in configured flash size 2MB.
```

**Cause**: Cached `sdkconfig` file overrides `sdkconfig.defaults` settings.

**Solution**: Clean the build directory and regenerate configuration:

```bash
# For ESP-IDF build system
idf.py fullclean
rm -f sdkconfig sdkconfig.old

# Then rebuild
idf.py build
```

Or on Windows:
```cmd
idf.py fullclean
del sdkconfig sdkconfig.old
idf.py build
```

### Issue 2: Partition Table Warning

**Symptom**: Warning during build:
```
WARNING: Partition has name 'factory' which is a partition subtype...
```

**Solution**: This has been fixed in the latest version. The partition was renamed from `factory` to `factory_cfg` to avoid confusion with the app factory subtype.

If you still see this warning after updating, clean and rebuild:
```bash
idf.py fullclean
idf.py build
```

## Build Requirements

### Hardware
- ESP32 or ESP32-S3 with **4MB Flash** minimum
- **4MB PSRAM** required for provisioning feature

### Flash Size Configuration

The project is configured for 4MB flash in `sdkconfig.defaults`:
```
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
```

### Partition Table Layout

The partition table (`partitions.csv`) allocates:
- **NVS**: 24KB (nvs partition)
- **PHY Init**: 4KB
- **Factory Config**: 24KB (encrypted, for default provisioning config)
- **OTA Data**: 8KB
- **OTA 0**: 1.5MB (primary app)
- **OTA 1**: 1.5MB (backup app for OTA updates)
- **Storage**: 832KB (SPIFFS for logs/data)

Total: ~3.9MB (fits in 4MB flash)

## Step-by-Step Build Process

### 1. Set Target

```bash
idf.py set-target esp32
```

Or for ESP32-S3:
```bash
idf.py set-target esp32s3
```

### 2. Configure (Optional)

```bash
idf.py menuconfig
```

Verify under "Serial flasher config" → "Flash size" is set to **4 MB**.

### 3. Clean Build (Recommended for First Build)

```bash
idf.py fullclean
rm -f sdkconfig sdkconfig.old
idf.py build
```

### 4. Flash

```bash
idf.py -p /dev/ttyUSB0 flash
```

Replace `/dev/ttyUSB0` with your serial port (e.g., `COM3` on Windows).

### 5. Monitor

```bash
idf.py -p /dev/ttyUSB0 monitor
```

## Verification

After successful build, verify partition table:

```bash
idf.py partition-table
```

Expected output should show:
```
# ESP-IDF Partition Table
# Name,       Type, SubType,  Offset,   Size,    Flags
nvs,          data, nvs,      0x9000,   24K,
phy_init,     data, phy,      0xf000,   4K,
factory_cfg,  data, nvs,      0x10000,  24K,     encrypted
otadata,      data, ota,      0x16000,  8K,
ota_0,        app,  ota_0,    0x20000,  1536K,
ota_1,        app,  ota_1,    0x1a0000, 1536K,
storage,      data, spiffs,   0x320000, 832K,
```

## Advanced Troubleshooting

### Check Flash Size Detection

```bash
esptool.py --port /dev/ttyUSB0 flash_id
```

This should report actual flash chip size. If it shows 2MB but you expect 4MB, you may have the wrong hardware.

### Force Configuration Update

If configuration seems stuck:

```bash
# Backup your custom settings (if any)
cp sdkconfig sdkconfig.backup

# Remove all generated files
idf.py fullclean
rm -rf build/
rm -f sdkconfig sdkconfig.old

# Regenerate from defaults
idf.py reconfigure

# Restore custom settings if needed
# Then rebuild
idf.py build
```

### CMake Cache Issues

If using CMake directly or having persistent issues:

```bash
rm -rf build/ CMakeCache.txt CMakeFiles/
idf.py reconfigure
```

## Docker/Container Builds

If building in a container (devcontainer), ensure the container has access to:
1. ESP-IDF v5.4 or later
2. USB device passthrough (for flashing)
3. Sufficient disk space (~2GB for build artifacts)

For devcontainer setup, see `.devcontainer/devcontainer.json`.

## Getting Help

If issues persist after trying these steps:

1. Check ESP-IDF version: `idf.py --version`
   - Required: v5.4 or later
   
2. Verify partition table matches flash size:
   ```bash
   python $IDF_PATH/components/partition_table/gen_esp32part.py partitions.csv
   ```

3. Check for hardware mismatch:
   - Ensure ESP32 module has 4MB flash
   - Verify PSRAM is present and detected

4. Report issue with:
   - ESP-IDF version
   - Target chip (ESP32/ESP32-S3)
   - Full build log
   - Output of `esptool.py flash_id`

## References

- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html)
- [Partition Tables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html)
- [Flash Size Configuration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#config-esptoolpy-flashsize)
