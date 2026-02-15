#include "display_manager.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "display";

/**
 * T-Display-S3 Pin Definitions (8-bit Parallel)
 * Data Bus: GPIO 39, 40, 41, 42, 45, 46, 47, 48
 * Control:
 *   PCLK: 8, CS: 6, DC: 7, RST: 5, WR: 8 (PCLK), RD: 9
 * Backlight: 38
 */
#define LCD_PIN_BK_LIGHT       38
#define LCD_PIN_RST            5
#define LCD_PIN_CS             6
#define LCD_PIN_DC             7
#define LCD_PIN_PCLK           8
#define LCD_PIN_RD             9

#define LCD_H_RES              320
#define LCD_V_RES              170

static esp_lcd_panel_handle_t panel_handle = NULL;

esp_err_t display_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing T-Display-S3 Display (ST7789 8-bit Parallel)...");
    
    // 1. Backlight Init
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_BK_LIGHT,
    };
    gpio_config(&bk_gpio_config);
    gpio_set_level(LCD_PIN_BK_LIGHT, 1); 

    // 2. LCD Reset
    gpio_config_t rst_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_RST,
    };
    gpio_config(&rst_gpio_config);
    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /**
     * Note for User: 
     * To fully implement the 8-bit parallel driver, you should add the 'esp_lcd_panel_io_i80' 
     * and 'esp_lcd_st7789' components to your project.
     * 
     * Example configuration for i80 bus:
     * esp_lcd_i80_bus_handle_t i80_bus = NULL;
     * esp_lcd_i80_bus_config_t bus_config = { ... pins 39-48 ... };
     * esp_lcd_new_i80_bus(&bus_config, &i80_bus);
     */
    
    ESP_LOGI(TAG, "Display Hardware Configured (Pins Set)");
    return ESP_OK;
}

void display_manager_update(bool wifi_connected, bool tg_connected, const char *status_text)
{
    ESP_LOGI(TAG, "Display Refresh -> WiFi: %s | Telegram: %s | Status: %s", 
             wifi_connected ? "Connected" : "Disconnected", 
             tg_connected ? "Active" : "Inactive", 
             status_text);
    
    /**
     * Logic for drawing on screen:
     * 1. Clear status area
     * 2. Draw WiFi icon (Green if connected, Red/Grey if not)
     * 3. Draw Telegram icon (Green if active)
     * 4. Render status_text string
     */
}

void display_manager_set_status(const char *status_text)
{
    ESP_LOGI(TAG, "Display Status Update: %s", status_text);
    // In a real implementation, this would trigger a partial screen update
}
