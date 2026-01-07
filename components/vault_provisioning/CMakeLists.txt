idf_component_register(SRCS "vault_provisioning.c"
                       INCLUDE_DIRS "."
                       REQUIRES vault_protocol vault_memory vault_mqtt json nvs_flash esp_wifi mqtt)
