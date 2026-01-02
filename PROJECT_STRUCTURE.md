# EspVault Project Structure

This document provides an overview of the project's file organization.

```
EspVault/
├── .editorconfig              # Editor configuration for consistent code style
├── .gitignore                 # Git ignore patterns
├── CMakeLists.txt             # Top-level CMake build configuration
├── LICENSE                    # MIT License
├── README.md                  # Project overview and quick start guide
├── CONTRIBUTING.md            # Contribution guidelines
├── partitions.csv             # Flash partition table (4MB optimized)
├── sdkconfig.defaults         # Default ESP-IDF configuration
│
├── components/                # Custom ESP-IDF components
│   ├── vault_protocol/        # Binary protocol implementation
│   │   ├── CMakeLists.txt
│   │   ├── vault_protocol.h   # Protocol definitions and API
│   │   └── vault_protocol.c   # CRC8, parsing, serialization
│   │
│   ├── vault_memory/          # PSRAM memory management
│   │   ├── CMakeLists.txt
│   │   ├── vault_memory.h     # Memory manager API
│   │   └── vault_memory.c     # Ring buffers, history, sequence counter
│   │
│   └── vault_mqtt/            # MQTT 5.0 client
│       ├── CMakeLists.txt
│       ├── vault_mqtt.h       # MQTT client API
│       └── vault_mqtt.c       # Connection, pub/sub, replay logic
│
├── docs/                      # Documentation
│   ├── ARCHITECTURE.md        # System architecture details
│   ├── SECURITY.md            # Security configuration guide
│   ├── PROVISIONING.md        # Factory provisioning guide
│   └── TESTING.md             # Testing strategy and examples
│
├── examples/                  # Usage examples
│   └── basic_example.md       # Basic event capture example
│
├── factory/                   # Factory provisioning resources
│   ├── certs/                 # SSL/TLS certificates
│   │   └── README.md          # Certificate documentation
│   ├── nvs_config.csv         # NVS configuration template
│   └── generate_nvs.sh        # Helper script for NVS generation
│
└── main/                      # Main application
    ├── CMakeLists.txt         # Main component configuration
    ├── Kconfig.projbuild      # Custom configuration options
    └── main.c                 # Application entry point, dual-core tasks

```

## Component Details

### vault_protocol
- **Purpose**: Binary packet protocol implementation
- **Size**: ~7KB code
- **Features**: CRC8 validation, packed structs, 13-byte packets
- **Dependencies**: None

### vault_memory
- **Purpose**: PSRAM management and cross-core communication
- **Size**: ~10KB code + 3MB PSRAM
- **Features**: Ring buffers, history storage, sequence counters, NVS sync
- **Dependencies**: nvs_flash, vault_protocol

### vault_mqtt
- **Purpose**: MQTT 5.0 client with replay support
- **Size**: ~9KB code
- **Features**: Event publishing, command handling, replay logic
- **Dependencies**: mqtt, vault_protocol, vault_memory

## Build Artifacts (Generated)

```
build/
├── bootloader/
│   └── bootloader.bin         # Bootloader binary
├── partition_table/
│   └── partition-table.bin    # Partition table binary
├── EspVault.bin               # Main application binary
├── EspVault.elf               # ELF file with debug symbols
└── EspVault.map               # Memory map file
```

## Configuration Files

- **sdkconfig.defaults**: Default ESP-IDF settings (PSRAM, dual-core, MQTT)
- **partitions.csv**: Flash layout (NVS, factory, OTA, storage)
- **Kconfig.projbuild**: Custom menuconfig options
- **.editorconfig**: Code style configuration

## Documentation Files

- **README.md**: Project overview, quick start, features
- **CONTRIBUTING.md**: Development guidelines, coding standards
- **LICENSE**: MIT License
- **docs/ARCHITECTURE.md**: Detailed system architecture
- **docs/SECURITY.md**: Security configuration (Secure Boot, Flash Encryption)
- **docs/PROVISIONING.md**: Factory provisioning process
- **docs/TESTING.md**: Testing strategy and examples

## Total Project Size

- **Source Code**: ~1,500 lines (C)
- **Documentation**: ~4,000 lines (Markdown)
- **Headers**: ~500 lines
- **Configuration**: ~200 lines

## Memory Usage

- **Flash**: ~150KB (application code)
- **SRAM**: ~50KB (stacks, heap)
- **PSRAM**: 3MB (2MB history + 1MB queue)

## File Naming Conventions

- **Components**: `vault_<name>/vault_<name>.{c,h}`
- **Documentation**: `UPPERCASE.md` or `docs/*.md`
- **Scripts**: `<action>_<object>.sh`
- **Configuration**: `<name>.csv` or `<name>.defaults`
