idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_netif esp_event vault_protocol vault_memory vault_mqtt vault_provisioning
)
