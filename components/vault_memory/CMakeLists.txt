idf_component_register(
    SRCS "vault_memory.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash vault_protocol
)
