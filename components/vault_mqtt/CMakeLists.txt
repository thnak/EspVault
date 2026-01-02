idf_component_register(
    SRCS "vault_mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES mqtt vault_protocol vault_memory
)
