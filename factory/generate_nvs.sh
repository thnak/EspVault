#!/bin/bash
# Script to generate NVS partition from CSV

set -e

if [ -z "$IDF_PATH" ]; then
    echo "Error: IDF_PATH not set. Please source ESP-IDF export.sh"
    exit 1
fi

NVS_GEN="$IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py"

if [ ! -f "$NVS_GEN" ]; then
    echo "Error: NVS partition generator not found at $NVS_GEN"
    exit 1
fi

CSV_FILE="${1:-factory/nvs_config.csv}"
OUTPUT_FILE="${2:-factory_nvs.bin}"
SIZE="${3:-0x6000}"

echo "Generating NVS partition..."
echo "  Input:  $CSV_FILE"
echo "  Output: $OUTPUT_FILE"
echo "  Size:   $SIZE"

python3 "$NVS_GEN" generate "$CSV_FILE" "$OUTPUT_FILE" "$SIZE"

echo "✓ NVS partition generated successfully"
echo ""
echo "To flash to device:"
echo "  esptool.py --port /dev/ttyUSB0 write_flash 0x10000 $OUTPUT_FILE"
