# GitHub Actions Workflows

This directory contains all CI/CD workflows for the EspVault project.

## Available Workflows

### 1. Complete Test Suite (`test-suite.yml`)

**Primary testing workflow** that runs comprehensive tests across multiple test suites.

**Triggers:**
- Automatic on push to `main` or `develop` branches
- Automatic on pull requests to `main` or `develop` branches
- **Manual via GitHub Actions UI** with options to select test suite

**Manual Trigger Options:**
- **Test Suite Selection:**
  - `all` - Run all test suites (default)
  - `basic` - Run only basic tests (NVS, Memory, Provisioning)
  - `network` - Run only network tests with MQTT broker
  - `integration` - Run only integration tests
- **Debug Logging:**
  - Enable/disable verbose debug output

**Test Suites:**

1. **Basic Tests** (8 tests)
   - NVS operations (read/write, default config)
   - Memory management (heap, PSRAM allocation)
   - Provisioning configuration (JSON parsing, validation)

2. **Network Tests** (10 tests)
   - Ethernet initialization
   - MQTT broker connectivity
   - Publish/Subscribe operations
   - Network failure scenarios

3. **Integration Tests** (10 tests)
   - End-to-end provisioning workflow
   - SSL/TLS certificate handling
   - Error recovery and fallback
   - Complete system validation

**Features:**
- ✅ Parallel job execution for faster results
- ✅ MQTT broker service container (mosquitto)
- ✅ Automatic artifact uploads (logs, builds)
- ✅ Test result summaries in GitHub UI
- ✅ 7-day artifact retention
- ✅ Comprehensive error reporting

**How to Run Manually:**

1. Navigate to **Actions** tab in GitHub
2. Select **Complete Test Suite** workflow
3. Click **Run workflow** button
4. Select test suite to run (or leave as "all")
5. Enable debug logging if needed
6. Click **Run workflow**

### 2. QEMU Tests (`qemu-tests.yml`)

**Legacy workflow** for basic QEMU testing without network features.

**Triggers:**
- Automatic on push to `main` or `develop` branches (component changes)
- Automatic on pull requests
- Manual via GitHub Actions UI (`workflow_dispatch`)

**Test Coverage:**
- Basic NVS operations
- Memory management
- Configuration parsing

**Note:** This workflow is being superseded by `test-suite.yml` but maintained for backward compatibility.

### 3. Copilot Setup Steps (`copilot-setup-steps.yml`)

**Setup workflow** for GitHub Copilot workspace integration.

**Purpose:**
- Prepares the development environment for Copilot
- Installs ESP-IDF toolchain
- Caches dependencies
- Sets up build tools

**Triggers:**
- Manual via GitHub Actions UI
- Automatic on changes to this workflow file

**Not used for regular CI/CD** - only for Copilot workspace setup.

## Workflow Architecture

```
┌─────────────────────────────────────────┐
│       Complete Test Suite (Recommended)  │
│         test-suite.yml                   │
└─────────────────────────────────────────┘
                    │
        ┌───────────┴───────────┐
        │                       │
        ▼                       ▼
┌───────────────┐      ┌────────────────┐
│  Basic Tests  │      │ Network Tests  │
│  (8 tests)    │      │ (10 tests)     │
│               │      │ + MQTT Broker  │
└───────┬───────┘      └────────┬───────┘
        │                       │
        └───────────┬───────────┘
                    ▼
        ┌───────────────────────┐
        │  Integration Tests    │
        │  (10 tests)           │
        │  + End-to-End         │
        └───────────────────────┘
                    │
                    ▼
        ┌───────────────────────┐
        │   Test Summary        │
        │   (Report)            │
        └───────────────────────┘
```

## Test Execution Times

| Suite | Estimated Duration |
|-------|-------------------|
| Basic Tests | ~2-3 minutes |
| Network Tests | ~3-4 minutes |
| Integration Tests | ~3-4 minutes |
| **Total (All)** | **~8-11 minutes** |

Times may vary based on GitHub runner availability and network conditions.

## Artifacts Generated

Each workflow execution generates artifacts available for 7 days:

### Basic Tests
- `basic-test-logs` - QEMU execution logs
- `basic-test-build` - Built firmware binaries (.bin, .elf, .map)

### Network Tests
- `network-test-logs` - QEMU network test logs
- `mqtt-broker-logs` - Mosquitto broker logs

### Integration Tests
- `integration-test-logs` - End-to-end test logs

## Environment Requirements

All workflows run on:
- **Runner**: `ubuntu-latest`
- **ESP-IDF**: v5.4
- **Target**: ESP32 (dual-core)
- **QEMU**: ESP32 emulator
- **MQTT Broker**: Mosquitto (Docker container)

## Monitoring Test Results

### Via GitHub UI
1. Navigate to **Actions** tab
2. Select workflow run
3. View job status (✅/❌)
4. Check **Summary** for detailed report
5. Download artifacts for logs

### Via API
```bash
# Get latest workflow run status
gh run list --workflow=test-suite.yml --limit=1

# Download artifacts
gh run download <run-id>
```

## Troubleshooting

### Test Timeouts
- Basic tests: 120 seconds timeout
- Network tests: 180 seconds timeout
- Integration tests: 180 seconds timeout

If tests consistently timeout, check:
1. QEMU installation
2. ESP-IDF environment setup
3. MQTT broker availability (network tests)

### Failed Tests
1. Check job logs in GitHub Actions UI
2. Download artifacts for detailed logs
3. Review test output for specific failures
4. Re-run failed jobs individually

### MQTT Connection Issues
For network/integration tests:
1. Verify mosquitto service container is healthy
2. Check network configuration (10.0.2.2:1883)
3. Review mosquitto broker logs artifact

## Adding New Tests

To add new test cases:

1. **Basic Tests**: Add to `test/qemu/main/test_*.c`
2. **Network Tests**: Add to `test/qemu/main/test_network.c`
3. **Integration Tests**: Add to `test/qemu/main/test_integration.c`

Tests will be automatically picked up by the workflow.

## CI/CD Best Practices

✅ **Do:**
- Run `test-suite.yml` before merging PRs
- Test manually with specific suites during development
- Review artifacts for detailed debugging
- Monitor test execution times

❌ **Don't:**
- Skip tests for "minor" changes
- Ignore intermittent failures
- Commit without local testing first
- Merge with failing tests

## Status Badges

Add to your README.md:

```markdown
[![Complete Test Suite](https://github.com/thnak/EspVault/actions/workflows/test-suite.yml/badge.svg)](https://github.com/thnak/EspVault/actions/workflows/test-suite.yml)
```

## Support

For issues with workflows:
1. Check workflow logs in Actions tab
2. Review this documentation
3. See `test/qemu/README.md` for test details
4. See `test/qemu/NETWORK_TESTING.md` for network test specifics
