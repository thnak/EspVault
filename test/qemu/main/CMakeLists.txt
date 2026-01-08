idf_component_register(
    SRCS "test_main.c" "test_provisioning.c" "test_nvs.c" "test_memory.c"
    INCLUDE_DIRS "."
    REQUIRES unity nvs_flash esp_wifi mqtt vault_provisioning vault_memory
)
