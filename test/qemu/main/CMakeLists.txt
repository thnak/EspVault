idf_component_register(
    SRCS "test_main.c" "test_provisioning.c" "test_nvs.c" "test_memory.c" "test_network.c" "test_integration.c"
    INCLUDE_DIRS "."
    REQUIRES unity nvs_flash esp_wifi esp_eth esp_netif mqtt vault_provisioning vault_memory
)
