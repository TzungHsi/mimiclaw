#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "mimi_config.h"
#include "bus/message_bus.h"
#include "wifi/wifi_manager.h"
#include "telegram/telegram_bot.h"
#include "llm/llm_proxy.h"
#include "agent/agent_loop.h"
#include "memory/memory_store.h"
#include "memory/session_mgr.h"
#include "gateway/ws_server.h"
#include "cli/serial_cli.h"
#include "display/display_manager.h"
#include "proxy/http_proxy.h"
#include "tools/tool_registry.h"
#include "button_driver.h"

static const char *TAG = "mimi";

static int64_t boot_time_us = 0;

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = MIMI_SPIFFS_BASE,
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS: total=%d, used=%d", (int)total, (int)used);

    return ESP_OK;
}

/* Update display with current system status */
static void update_display_status(void)
{
    display_status_t status = {0};
    
    // WiFi status
    status.wifi_connected = wifi_manager_is_connected();
    status.wifi_rssi = -50;  // TODO: Implement RSSI reading
    const char *ip = wifi_manager_get_ip();
    if (ip) {
        strncpy(status.ip_address, ip, sizeof(status.ip_address) - 1);
    } else {
        strcpy(status.ip_address, "0.0.0.0");
    }
    
    // Telegram status (assume running if WiFi connected)
    status.telegram_connected = status.wifi_connected;
    
    // System uptime
    int64_t now_us = esp_timer_get_time();
    status.uptime_seconds = (uint32_t)((now_us - boot_time_us) / 1000000);
    
    // Memory status
    status.free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    status.total_heap = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    
    // System state
    if (status.wifi_connected && status.telegram_connected) {
        status.system_state = "Ready";
    } else if (status.wifi_connected) {
        status.system_state = "WiFi OK";
    } else {
        status.system_state = "Starting";
    }
    
    display_manager_update(&status);
}

/* Button polling task */
static void button_task(void *arg)
{
    ESP_LOGI(TAG, "Button task started");
    
    while (1) {
        button_event_t event = button_poll();
        
        switch (event) {
            case BUTTON_EVENT_BOOT_SHORT:
                // Cycle display mode
                {
                    display_mode_t current = display_manager_get_mode();
                    display_mode_t next = (current + 1) % DISPLAY_MODE_COUNT;
                    display_manager_set_mode(next);
                    ESP_LOGI(TAG, "Display mode switched to: %d", next);
                }
                break;
                
            case BUTTON_EVENT_BOOT_LONG:
                // Toggle backlight
                display_manager_toggle_backlight();
                break;
                
            case BUTTON_EVENT_USER_SHORT:
                // Manual refresh
                ESP_LOGI(TAG, "Manual refresh triggered");
                update_display_status();
                break;
                
            case BUTTON_EVENT_USER_LONG:
                // Restart WiFi
                ESP_LOGI(TAG, "WiFi restart triggered");
                // TODO: Implement WiFi restart
                ESP_LOGW(TAG, "WiFi restart not yet implemented");
                break;
                
            default:
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // Poll every 10ms
    }
}

/* Periodic display update task */
static void display_update_task(void *arg)
{
    ESP_LOGI(TAG, "Display update task started");
    
    while (1) {
        update_display_status();
        vTaskDelay(pdMS_TO_TICKS(5000));  // Update every 5 seconds
    }
}

/* Outbound dispatch task: reads from outbound queue and routes to channels */
static void outbound_dispatch_task(void *arg)
{
    ESP_LOGI(TAG, "Outbound dispatch started");

    while (1) {
        mimi_msg_t msg;
        if (message_bus_pop_outbound(&msg, UINT32_MAX) != ESP_OK) continue;

        ESP_LOGI(TAG, "Dispatching response to %s:%s", msg.channel, msg.chat_id);

        if (strcmp(msg.channel, MIMI_CHAN_TELEGRAM) == 0) {
            telegram_send_message(msg.chat_id, msg.content);
        } else if (strcmp(msg.channel, MIMI_CHAN_WEBSOCKET) == 0) {
            ws_server_send(msg.chat_id, msg.content);
        } else {
            ESP_LOGW(TAG, "Unknown channel: %s", msg.channel);
        }

        free(msg.content);
    }
}

void app_main(void)
{
    /* Record boot time */
    boot_time_us = esp_timer_get_time();
    
    /* Silence noisy components */
    esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  MimiClaw - ESP32-S3 AI Agent");
    ESP_LOGI(TAG, "========================================");

    /* Print memory info */
    ESP_LOGI(TAG, "Internal free: %d bytes",
             (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "PSRAM free:    %d bytes",
             (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    /* Phase 1: Core infrastructure */
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(display_manager_init());
    button_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(init_spiffs());

    /* Initialize subsystems */
    ESP_ERROR_CHECK(message_bus_init());
    ESP_ERROR_CHECK(memory_store_init());
    ESP_ERROR_CHECK(session_mgr_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(http_proxy_init());
    ESP_ERROR_CHECK(telegram_bot_init());
    ESP_ERROR_CHECK(llm_proxy_init());
    ESP_ERROR_CHECK(tool_registry_init());
    ESP_ERROR_CHECK(agent_loop_init());

    /* Start Serial CLI first (works without WiFi) */
    ESP_ERROR_CHECK(serial_cli_init());

    /* Start button task */
    xTaskCreatePinnedToCore(
        button_task, "button",
        4096, NULL, 5, NULL, 1);

    /* Start display update task */
    xTaskCreatePinnedToCore(
        display_update_task, "display_update",
        4096, NULL, 4, NULL, 1);

    /* Start WiFi */
    esp_err_t wifi_err = wifi_manager_start();
    if (wifi_err == ESP_OK) {
        ESP_LOGI(TAG, "Scanning nearby APs on boot...");
        wifi_manager_scan_and_print();
        ESP_LOGI(TAG, "Waiting for WiFi connection...");
        if (wifi_manager_wait_connected(30000) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi connected: %s", wifi_manager_get_ip());
            update_display_status();

            /* Start network-dependent services */
            ESP_ERROR_CHECK(telegram_bot_start());
            ESP_ERROR_CHECK(agent_loop_start());
            ESP_ERROR_CHECK(ws_server_start());
            update_display_status();

            /* Outbound dispatch task */
            xTaskCreatePinnedToCore(
                outbound_dispatch_task, "outbound",
                MIMI_OUTBOUND_STACK, NULL,
                MIMI_OUTBOUND_PRIO, NULL, MIMI_OUTBOUND_CORE);

            ESP_LOGI(TAG, "All services started!");
        } else {
            ESP_LOGW(TAG, "WiFi connection timeout. Check MIMI_SECRET_WIFI_SSID in mimi_secrets.h");
        }
    } else {
        ESP_LOGW(TAG, "No WiFi credentials. Set MIMI_SECRET_WIFI_SSID in mimi_secrets.h");
    }

    ESP_LOGI(TAG, "MimiClaw ready. Type 'help' for CLI commands.");
}
