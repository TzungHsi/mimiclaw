#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

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
#include "proxy/http_proxy.h"
#include "tools/tool_registry.h"

static const char *TAG = "mimi";

/* --- T-Display-S3 Display Pins (8-bit Parallel) --- */
#define LCD_PIN_BK_LIGHT       38
#define LCD_PIN_RST            5
#define LCD_PIN_CS             6
#define LCD_PIN_DC             7
#define LCD_PIN_WR             8
#define LCD_PIN_RD             9

// Data pins for 8-bit parallel bus
static const int lcd_data_pins[] = {39, 40, 41, 42, 45, 46, 47, 48};

/**
 * T-Display-S3 Hardware Init
 * This function turns on the backlight and sets the control pins to idle state.
 * To actually draw, one would need to use the 'esp_lcd' component with the ST7789 driver.
 */
void display_init(void) {
    ESP_LOGI(TAG, "Initializing T-Display-S3 Display Hardware...");
    
    // 1. Backlight
    gpio_config_t bk_conf = {
        .pin_bit_mask = 1ULL << LCD_PIN_BK_LIGHT,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bk_conf);
    gpio_set_level(LCD_PIN_BK_LIGHT, 1);

    // 2. Control Pins
    uint64_t ctrl_mask = (1ULL << LCD_PIN_RST) | (1ULL << LCD_PIN_CS) | 
                         (1ULL << LCD_PIN_DC) | (1ULL << LCD_PIN_WR) | (1ULL << LCD_PIN_RD);
    gpio_config_t ctrl_conf = {
        .pin_bit_mask = ctrl_mask,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&ctrl_conf);

    // 3. Reset Sequence
    gpio_set_level(LCD_PIN_CS, 1);
    gpio_set_level(LCD_PIN_RD, 1);
    gpio_set_level(LCD_PIN_WR, 1);
    
    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "LCD Hardware Ready (Backlight ON).");
}

void display_update_status(bool wifi_connected, bool tg_active, const char *status_text) {
    ESP_LOGI("DISPLAY", "WiFi: %s | Telegram: %s | Status: %s", 
             wifi_connected ? "ON" : "OFF", 
             tg_active ? "ON" : "OFF", 
             status_text);
}

/* --- Core Infrastructure --- */

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
    esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  MimiClaw - ESP32-S3 AI Agent");
    ESP_LOGI(TAG, "========================================");

    display_init();

    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(init_spiffs());

    ESP_ERROR_CHECK(message_bus_init());
    ESP_ERROR_CHECK(memory_store_init());
    ESP_ERROR_CHECK(session_mgr_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(http_proxy_init());
    ESP_ERROR_CHECK(telegram_bot_init());
    ESP_ERROR_CHECK(llm_proxy_init());
    ESP_ERROR_CHECK(tool_registry_init());
    ESP_ERROR_CHECK(agent_loop_init());

    ESP_ERROR_CHECK(serial_cli_init());
    display_update_status(false, false, "Waiting for WiFi...");

    esp_err_t wifi_err = wifi_manager_start();
    if (wifi_err == ESP_OK) {
        if (wifi_manager_wait_connected(30000) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi connected: %s", wifi_manager_get_ip());
            display_update_status(true, false, "WiFi Connected");

            ESP_ERROR_CHECK(telegram_bot_start());
            ESP_ERROR_CHECK(agent_loop_start());
            ESP_ERROR_CHECK(ws_server_start());
            display_update_status(true, true, "System Ready");

            xTaskCreatePinnedToCore(
                outbound_dispatch_task, "outbound",
                MIMI_OUTBOUND_STACK, NULL,
                MIMI_OUTBOUND_PRIO, NULL, MIMI_OUTBOUND_CORE);

            ESP_LOGI(TAG, "All services started!");
        } else {
            display_update_status(false, false, "WiFi Timeout");
        }
    } else {
        display_update_status(false, false, "WiFi Config Missing");
    }

    ESP_LOGI(TAG, "MimiClaw ready. Type 'help' for CLI commands.");
}
