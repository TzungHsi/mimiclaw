file(GLOB_RECURSE SOURCES "*.c")

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "." "bus" "wifi" "telegram" "llm" "agent" "memory" "gateway" "cli" "ota" "proxy" "tools"
    REQUIRES nvs_flash esp_wifi esp_netif esp_http_client esp_http_server
             esp_https_ota esp_event json spiffs console vfs app_update esp-tls
)
