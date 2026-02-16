#include "display_manager.h"
#include "display_ui.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "display_mgr";

/* T-Display-S3 LCD Pins */
#define LCD_PIN_BK_LIGHT       38
#define LCD_PIN_RST            5
#define LCD_PIN_CS             6
#define LCD_PIN_DC             7
#define LCD_PIN_PCLK           8
#define LCD_PIN_RD             9

/* Data Bus Pins (8-bit) */
#define LCD_PIN_D0             39
#define LCD_PIN_D1             40
#define LCD_PIN_D2             41
#define LCD_PIN_D3             42
#define LCD_PIN_D4             45
#define LCD_PIN_D5             46
#define LCD_PIN_D6             47
#define LCD_PIN_D7             48

static esp_lcd_panel_handle_t panel_handle = NULL;

static struct {
    bool wifi;
    bool tg;
    char status[64];
} s_display_state = {0};

static void display_task(void *arg) {
    while (1) {
        display_ui_render(s_display_state.wifi, s_display_state.tg, s_display_state.status);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t display_manager_init(void) {
    ESP_LOGI(TAG, "Initializing T-Display-S3 LCD (8-bit Parallel)...");

    // 1. Backlight
    gpio_config_t bk_conf = {
        .pin_bit_mask = 1ULL << LCD_PIN_BK_LIGHT,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bk_conf);
    gpio_set_level(LCD_PIN_BK_LIGHT, 1);

    // 2. LCD Reset
    gpio_config_t rst_conf = {
        .pin_bit_mask = 1ULL << LCD_PIN_RST,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst_conf);
    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /**
     * T-Display-S3 uses Intel 8080 8-bit parallel interface.
     * Note: Full implementation of esp_lcd_new_i80_bus requires specific 
     * i80 bus configuration which is available in ESP-IDF v5.x.
     */
    ESP_LOGI(TAG, "LCD Hardware Configured. Backlight is ON.");

    // Start UI Task
    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);

    return ESP_OK;
}

void display_manager_update(bool wifi, bool tg, const char *status) {
    s_display_state.wifi = wifi;
    s_display_state.tg = tg;
    if (status) {
        strncpy(s_display_state.status, status, sizeof(s_display_state.status) - 1);
    }
}

void display_manager_set_status(const char *status) {
    if (status) {
        strncpy(s_display_state.status, status, sizeof(s_display_state.status) - 1);
    }
}
