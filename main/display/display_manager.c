#include "display_manager.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "display";

// T-Display-S3 pin definitions
#define LCD_BK_LIGHT_PIN    38
#define LCD_RST_PIN         5
#define LCD_CS_PIN          6
#define LCD_DC_PIN          7
#define LCD_WR_PIN          8
#define LCD_RD_PIN          9
#define LCD_D0_PIN          39
#define LCD_D1_PIN          40
#define LCD_D2_PIN          41
#define LCD_D3_PIN          42
#define LCD_D4_PIN          45
#define LCD_D5_PIN          46
#define LCD_D6_PIN          47
#define LCD_D7_PIN          48

// Screen dimensions
#define LCD_H_RES           320
#define LCD_V_RES           170

// Simple 16-bit color definitions (RGB565)
#define COLOR_BLACK         0x0000
#define COLOR_WHITE         0xFFFF
#define COLOR_RED           0xF800
#define COLOR_GREEN         0x07E0
#define COLOR_BLUE          0x001F
#define COLOR_YELLOW        0xFFE0
#define COLOR_DARK_BLUE     0x0010

// Global LCD handle
static esp_lcd_panel_handle_t panel_handle = NULL;

// State variables
static bool wifi_connected = false;
static bool tg_connected = false;
static char status_text[64] = "Initializing...";

// Simple framebuffer (single buffer for low memory usage)
static uint16_t *framebuffer = NULL;

/**
 * @brief Fill rectangle in framebuffer
 */
static void fb_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (!framebuffer) return;
    
    for (int row = y; row < y + h && row < LCD_V_RES; row++) {
        for (int col = x; col < x + w && col < LCD_H_RES; col++) {
            framebuffer[row * LCD_H_RES + col] = color;
        }
    }
}

/**
 * @brief Draw a simple filled circle (for status icons)
 */
static void fb_fill_circle(int cx, int cy, int radius, uint16_t color) {
    if (!framebuffer) return;
    
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < LCD_H_RES && py >= 0 && py < LCD_V_RES) {
                    framebuffer[py * LCD_H_RES + px] = color;
                }
            }
        }
    }
}

/**
 * @brief Simple text rendering (8x8 bitmap font, minimal implementation)
 * Note: This is a placeholder. For production, use a proper font library.
 */
static void fb_draw_char(int x, int y, char c, uint16_t color) {
    // Simplified: just draw a small rectangle as placeholder
    // In production, you would use a bitmap font here
    fb_fill_rect(x, y, 6, 8, color);
}

static void fb_draw_string(int x, int y, const char *str, uint16_t color) {
    int cx = x;
    while (*str) {
        fb_draw_char(cx, y, *str, color);
        cx += 8;
        str++;
    }
}

/**
 * @brief Render the complete UI to framebuffer
 */
static void render_ui(void) {
    if (!framebuffer) return;
    
    // Clear screen
    fb_fill_rect(0, 0, LCD_H_RES, LCD_V_RES, COLOR_BLACK);
    
    // Header bar (dark blue)
    fb_fill_rect(0, 0, LCD_H_RES, 30, COLOR_DARK_BLUE);
    fb_draw_string(10, 10, "MIMICLAW", COLOR_WHITE);
    
    // WiFi status icon (top right)
    uint16_t wifi_color = wifi_connected ? COLOR_GREEN : COLOR_RED;
    fb_fill_circle(LCD_H_RES - 60, 15, 10, wifi_color);
    
    // Telegram status icon (top right)
    uint16_t tg_color = tg_connected ? COLOR_GREEN : COLOR_RED;
    fb_fill_circle(LCD_H_RES - 30, 15, 10, tg_color);
    
    // Main status text (center)
    fb_draw_string(LCD_H_RES / 2 - 40, LCD_V_RES / 2, status_text, COLOR_WHITE);
    
    // Footer line
    fb_fill_rect(0, LCD_V_RES - 30, LCD_H_RES, 1, COLOR_WHITE);
    
    // Footer text
    const char *wifi_text = wifi_connected ? "WiFi: OK" : "WiFi: --";
    const char *tg_text = tg_connected ? "TG: Active" : "TG: Offline";
    fb_draw_string(10, LCD_V_RES - 20, wifi_text, COLOR_WHITE);
    fb_draw_string(LCD_H_RES / 2, LCD_V_RES - 20, tg_text, COLOR_WHITE);
}

/**
 * @brief Push framebuffer to LCD
 */
static void push_to_lcd(void) {
    if (!panel_handle || !framebuffer) return;
    
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, framebuffer);
}

void display_manager_init(void) {
    ESP_LOGI(TAG, "Initializing T-Display-S3 (lightweight mode)");
    
    // Allocate framebuffer (single buffer, ~109KB)
    framebuffer = (uint16_t *)heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!framebuffer) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer");
        return;
    }
    
    // Initialize backlight GPIO
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << LCD_BK_LIGHT_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bk_gpio_config);
    gpio_set_level(LCD_BK_LIGHT_PIN, 1);  // Turn on backlight
    
    // Initialize reset GPIO
    gpio_config_t rst_gpio_config = {
        .pin_bit_mask = 1ULL << LCD_RST_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst_gpio_config);
    gpio_set_level(LCD_RST_PIN, 1);
    
    // Configure i80 bus
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = LCD_DC_PIN,
        .wr_gpio_num = LCD_WR_PIN,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = {
            LCD_D0_PIN, LCD_D1_PIN, LCD_D2_PIN, LCD_D3_PIN,
            LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN,
        },
        .bus_width = 8,
        .max_transfer_bytes = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
    
    // Configure panel IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_CS_PIN,
        .pclk_hz = 10000000,  // 10MHz for stability
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));
    
    // Create ST7789 panel
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    
    // Initialize panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    
    // Initial render
    render_ui();
    push_to_lcd();
    
    ESP_LOGI(TAG, "Display initialized (low power mode)");
}

void display_manager_update(bool wifi_status, bool tg_status, const char *status) {
    wifi_connected = wifi_status;
    tg_connected = tg_status;
    
    if (status) {
        strncpy(status_text, status, sizeof(status_text) - 1);
        status_text[sizeof(status_text) - 1] = '\0';
    }
    
    render_ui();
    push_to_lcd();
    
    ESP_LOGI(TAG, "Display updated: WiFi=%d, TG=%d, Status=%s", wifi_status, tg_status, status_text);
}

void display_manager_set_status(const char *status) {
    if (status) {
        strncpy(status_text, status, sizeof(status_text) - 1);
        status_text[sizeof(status_text) - 1] = '\0';
        render_ui();
        push_to_lcd();
    }
}
