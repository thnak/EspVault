# Known Issues and TODOs

## MQTT v5 Property Extraction (Critical for Full Functionality)

### Issue
The current implementation has placeholder TODOs for MQTT v5 Response Topic and Correlation Data extraction. This functionality is essential for proper request-response workflow.

### Location
`components/vault_mqtt/vault_mqtt.c`:
- Lines ~57-60: Response Topic extraction from incoming messages
- Lines ~352-355: Correlation Data inclusion in response messages

### Impact
Without proper MQTT v5 property handling:
- Response Topic must be constructed from device MAC (works but not optimal)
- Correlation Data won't be included in responses (breaks correlation)

### Solution Required
Implement proper MQTT v5 property extraction using ESP-IDF MQTT v5 API:

```c
// In MQTT_EVENT_DATA handler
if (event->property) {
    // Extract Response Topic
    mqtt5_user_property_item_t *property_item = 
        esp_mqtt5_client_get_user_property(event->property);
    
    // Extract Correlation Data
    // Note: Actual API depends on ESP-IDF version
}

// In vault_mqtt_publish_response
esp_mqtt5_publish_property_config_t properties;
memset(&properties, 0, sizeof(properties));

if (correlation_data) {
    properties.correlation_data = (uint8_t *)correlation_data;
    properties.correlation_data_len = strlen(correlation_data);
}

// Publish with properties
esp_mqtt_client_enqueue(mqtt->client, topic, payload, 
                       strlen(payload), qos, 0, true, &properties);
```

### Testing Status
- [ ] Test with actual ESP-IDF MQTT v5 implementation
- [ ] Verify property extraction works with Mosquitto 2.x
- [ ] Test correlation data round-trip

### Priority
**HIGH** - Required for production use, but current fallback implementation allows basic functionality.

## Task Handle Export

### Issue
Task handles are exported as global variables for provisioning to suspend/resume them.

### Location
`main/main.c`: Lines 28-31

### Impact
- Allows provisioning to free resources during setup mode
- May cause issues if tasks are not properly initialized

### Solution
Already implemented with NULL checks in vault_provisioning.c

### Testing Status
- [ ] Test setup mode with active tasks
- [ ] Verify task suspension/resumption
- [ ] Check memory freed during setup mode

## Certificate Size Limits

### Issue
Certificates are limited to 2KB each due to PSRAM allocation constraints.

### Location
`components/vault_provisioning/vault_provisioning.h`: Line 18

### Impact
- Some certificate chains may exceed 2KB
- Limits SSL/TLS configuration options

### Solution (Future Enhancement)
Implement payload chunking:
- Split large certificates across multiple MQTT messages
- Reassemble on device before parsing
- Use sequence numbers to ensure order

### Testing Status
- [ ] Test with various certificate sizes
- [ ] Document actual size limits based on PSRAM availability
- [ ] Implement chunking if needed

## NVS Storage

### Issue
Configuration saved to NVS without encryption by default.

### Location
`components/vault_provisioning/vault_provisioning.c`: vault_provisioning_save_config()

### Impact
- Credentials stored in plaintext
- Security risk for production deployments

### Solution
Enable NVS encryption in production:
```
CONFIG_NVS_ENCRYPTION=y
```

Already documented in `sdkconfig.defaults` and `docs/REMOTE_PROVISIONING.md`

### Testing Status
- [ ] Test with NVS encryption enabled
- [ ] Verify encrypted partitions work correctly

## WiFi Connection Testing

### Issue
WiFi validation is format-only, doesn't actually test connection.

### Location
`components/vault_provisioning/vault_provisioning.c`: vault_provisioning_test_wifi()

### Impact
- Can't verify credentials before committing
- May require second provisioning attempt if wrong

### Solution (Future Enhancement)
Implement actual connection test:
- Temporarily connect to test network
- Verify connectivity
- Disconnect and return to staging network
- Only commit if successful

**Complexity**: High - requires careful WiFi state management

### Testing Status
- [ ] Design safe WiFi switching mechanism
- [ ] Implement connection test
- [ ] Add timeout and retry logic

## MQTT Connection Testing

### Issue
Similar to WiFi - validation is format-only.

### Location
`components/vault_provisioning/vault_provisioning.c`: vault_provisioning_test_mqtt()

### Impact
- Can't verify broker connectivity before committing

### Solution (Future Enhancement)
- Create temporary MQTT client
- Test connection to production broker
- Destroy client if successful
- Requires careful resource management

### Testing Status
- [ ] Implement MQTT test connection
- [ ] Handle SSL/TLS in test client
- [ ] Add proper cleanup

## Memory Management

### Issue
Large JSON payloads and certificates allocated in PSRAM.

### Location
Throughout `vault_provisioning.c`

### Current Status
- ✅ Using heap_caps_malloc with MALLOC_CAP_SPIRAM
- ✅ Proper free() calls in vault_provisioning_free_config()
- ✅ Memory reporting in responses

### Testing Needed
- [ ] Test with maximum payload sizes
- [ ] Monitor PSRAM fragmentation
- [ ] Test multiple provisioning cycles

## Python Script Dependencies

### Issue
Requires paho-mqtt library.

### Location
`examples/provisioning/provision_device.py`

### Impact
- Users need to install dependencies
- May have version compatibility issues

### Solution
Already documented in README with installation instructions.

### Enhancement Ideas
- Add requirements.txt file
- Create Docker container for provisioner
- Add Node.js alternative

## Documentation

### Status
- ✅ Complete provisioning workflow documented
- ✅ Payload format examples
- ✅ Python script with usage examples
- ✅ Security considerations
- ❌ Hardware testing results pending

### Needed
- [ ] Add screenshots/logs from actual device provisioning
- [ ] Document tested MQTT broker versions
- [ ] Add troubleshooting section with common errors
- [ ] Create video tutorial

## Next Steps for Production Readiness

1. **Implement MQTT v5 Properties** (Critical)
   - Response Topic extraction
   - Correlation Data handling
   
2. **Hardware Testing** (Critical)
   - Test full workflow on ESP32
   - Verify memory management
   - Test with various payloads
   
3. **Connection Testing** (Important)
   - Implement WiFi dry-run
   - Implement MQTT dry-run
   
4. **Security Hardening** (Important)
   - Enable NVS encryption
   - Test with secure boot
   - Audit credential handling
   
5. **Enhanced Features** (Nice to Have)
   - Physical button trigger for setup mode
   - Payload chunking for large certs
   - Batch provisioning support
   - Web-based provisioning portal

## Contributing

When addressing these TODOs:
1. Update this file with findings
2. Add tests where possible
3. Update documentation with actual behavior
4. Submit PR with detailed description

## Questions?

Open an issue on GitHub: https://github.com/thnak/EspVault/issues
