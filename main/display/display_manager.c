#include "display_manager.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "display";

/**
 * T-Display-S3 Pin Definitions (8-bit Parallel)
 */
#define LCD_PIN_BK_LIGHT       38
#define LCD_PIN_RST            5
#define LCD_PIN_CS             6
#define LCD_PIN_DC             7
#define LCD_PIN_PCLK           8
#define LCD_PIN_RD             9

#define LCD_H_RES              320
#define LCD_V_RES              170

// Color definitions (RGB565)
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_YELLOW    0xFFE0
#define COLOR_BLUE      0x001F
#define COLOR_GRAY      0x8410

static esp_lcd_panel_handle_t panel_handle = NULL;
static display_mode_t current_mode = DISPLAY_MODE_GRID;
static display_status_t last_status = {0};
static bool backlight_on = true;
static uint16_t *framebuffer = NULL;

// Simple 8x16 font (ASCII 32-127)
// Each character is 8 pixels wide, 16 pixels tall
// Stored as 16 bytes per character (1 byte per row)
static const uint8_t font_8x16[][16] = {
    // Space (32)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // ! (33)
    {0x00, 0x00, 0x18, 0x3C, 0x3C, 0x3C, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00},
    // Add more characters as needed...
    // For brevity, we'll implement a minimal set
};

// Helper: Draw a filled rectangle
static void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (!framebuffer) return;
    
    for (uint16_t j = 0; j < h; j++) {
        for (uint16_t i = 0; i < w; i++) {
            uint16_t px = x + i;
            uint16_t py = y + j;
            if (px < LCD_H_RES && py < LCD_V_RES) {
                framebuffer[py * LCD_H_RES + px] = color;
            }
        }
    }
}

// Helper: Draw a filled circle (simple algorithm)
static void draw_circle(uint16_t cx, uint16_t cy, uint16_t radius, uint16_t color)
{
    if (!framebuffer) return;
    
    for (int16_t y = -radius; y <= radius; y++) {
        for (int16_t x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                uint16_t px = cx + x;
                uint16_t py = cy + y;
                if (px < LCD_H_RES && py < LCD_V_RES) {
                    framebuffer[py * LCD_H_RES + px] = color;
                }
            }
        }
    }
}

// Helper: Draw text (simple implementation)
static void draw_text(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t bg_color, uint8_t scale)
{
    if (!framebuffer || !text) return;
    
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    
    while (*text) {
        char c = *text++;
        
        // Simple character rendering (just draw rectangles for now)
        // In a real implementation, use a proper font
        for (uint8_t row = 0; row < 16 * scale; row++) {
            for (uint8_t col = 0; col < 8 * scale; col++) {
                uint16_t px = cursor_x + col;
                uint16_t py = cursor_y + row;
                
                // Simple pattern: draw some pixels for readable text
                bool pixel_on = ((row / scale) % 2 == 0 && (col / scale) % 2 == 0);
                
                if (px < LCD_H_RES && py < LCD_V_RES) {
                    framebuffer[py * LCD_H_RES + px] = pixel_on ? color : bg_color;
                }
            }
        }
        
        cursor_x += 8 * scale + 2;
        if (cursor_x >= LCD_H_RES) break;
    }
}

// Render Mode: Four-grid dashboard
static void render_grid_mode(const display_status_t *status)
{
    // Clear screen
    draw_rect(0, 0, LCD_H_RES, LCD_V_RES, COLOR_BLACK);
    
    // Grid layout: 2x2
    // Each cell: 160x85 pixels
    uint16_t cell_w = LCD_H_RES / 2;
    uint16_t cell_h = LCD_V_RES / 2;
    
    // Draw grid lines
    draw_rect(cell_w - 1, 0, 2, LCD_V_RES, COLOR_GRAY);  // Vertical line
    draw_rect(0, cell_h - 1, LCD_H_RES, 2, COLOR_GRAY);  // Horizontal line
    
    // Cell 1: WiFi (top-left)
    uint16_t wifi_color = status->wifi_connected ? COLOR_GREEN : COLOR_RED;
    draw_circle(cell_w / 2, cell_h / 2 - 10, 20, wifi_color);
    draw_text(cell_w / 2 - 20, cell_h - 25, "WiFi", COLOR_WHITE, COLOR_BLACK, 1);
    
    // Cell 2: Telegram (top-right)
    uint16_t tg_color = status->telegram_connected ? COLOR_GREEN : COLOR_RED;
    draw_circle(cell_w + cell_w / 2, cell_h / 2 - 10, 20, tg_color);
    draw_text(cell_w + cell_w / 2 - 15, cell_h - 25, "TG", COLOR_WHITE, COLOR_BLACK, 1);
    
    // Cell 3: System (bottom-left)
    uint16_t sys_color = COLOR_GREEN;  // Default to green
    if (status->system_state && strstr(status->system_state, "Error")) {
        sys_color = COLOR_RED;
    }
    draw_circle(cell_w / 2, cell_h + cell_h / 2 - 10, 20, sys_color);
    draw_text(cell_w / 2 - 25, LCD_V_RES - 25, "System", COLOR_WHITE, COLOR_BLACK, 1);
    
    // Cell 4: Memory (bottom-right)
    uint32_t mem_percent = (status->total_heap > 0) ? 
        ((status->total_heap - status->free_heap) * 100 / status->total_heap) : 0;
    uint16_t mem_color = (mem_percent > 80) ? COLOR_RED : 
                         (mem_percent > 60) ? COLOR_YELLOW : COLOR_GREEN;
    
    char mem_text[16];
    snprintf(mem_text, sizeof(mem_text), "%lu%%", (unsigned long)mem_percent);
    draw_circle(cell_w + cell_w / 2, cell_h + cell_h / 2 - 10, 20, mem_color);
    draw_text(cell_w + cell_w / 2 - 15, LCD_V_RES - 25, mem_text, COLOR_WHITE, COLOR_BLACK, 1);
}

// Render Mode: Detailed values
static void render_detail_mode(const display_status_t *status)
{
    // Clear screen
    draw_rect(0, 0, LCD_H_RES, LCD_V_RES, COLOR_BLACK);
    
    // Display detailed information
    char line[64];
    uint16_t y = 10;
    uint16_t line_height = 20;
    
    // WiFi RSSI
    snprintf(line, sizeof(line), "WiFi: %ddBm", status->wifi_rssi);
    draw_text(10, y, line, COLOR_WHITE, COLOR_BLACK, 1);
    y += line_height;
    
    // IP Address
    snprintf(line, sizeof(line), "IP: %s", status->ip_address);
    draw_text(10, y, line, COLOR_WHITE, COLOR_BLACK, 1);
    y += line_height;
    
    // Uptime
    uint32_t hours = status->uptime_seconds / 3600;
    uint32_t minutes = (status->uptime_seconds % 3600) / 60;
    snprintf(line, sizeof(line), "Uptime: %luh %lum", (unsigned long)hours, (unsigned long)minutes);
    draw_text(10, y, line, COLOR_WHITE, COLOR_BLACK, 1);
    y += line_height;
    
    // Memory
    snprintf(line, sizeof(line), "Mem: %lu/%lu KB", 
             (unsigned long)(status->free_heap / 1024), 
             (unsigned long)(status->total_heap / 1024));
    draw_text(10, y, line, COLOR_WHITE, COLOR_BLACK, 1);
    y += line_height;
    
    // System State
    snprintf(line, sizeof(line), "State: %s", status->system_state ? status->system_state : "Unknown");
    draw_text(10, y, line, COLOR_WHITE, COLOR_BLACK, 1);
}

// Render Mode: Large IP display
static void render_large_ip_mode(const display_status_t *status)
{
    // Clear screen
    draw_rect(0, 0, LCD_H_RES, LCD_V_RES, COLOR_BLACK);
    
    // Display large IP address
    draw_text(10, LCD_V_RES / 2 - 20, "IP Address:", COLOR_WHITE, COLOR_BLACK, 1);
    draw_text(10, LCD_V_RES / 2, status->ip_address, COLOR_GREEN, COLOR_BLACK, 2);
}

// Main render function
static void render_display(const display_status_t *status)
{
    if (!panel_handle || !framebuffer) return;
    
    switch (current_mode) {
        case DISPLAY_MODE_GRID:
            render_grid_mode(status);
            break;
        case DISPLAY_MODE_DETAIL:
            render_detail_mode(status);
            break;
        case DISPLAY_MODE_LARGE_IP:
            render_large_ip_mode(status);
            break;
        default:
            break;
    }
    
    // Send framebuffer to display
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, framebuffer);
}

esp_err_t display_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing T-Display-S3 Display (ST7789 8-bit Parallel)...");
    
    // Allocate framebuffer
    framebuffer = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!framebuffer) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer");
        return ESP_ERR_NO_MEM;
    }
    memset(framebuffer, 0, LCD_H_RES * LCD_V_RES * sizeof(uint16_t));
    
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
    
    // 3. Initialize I80 bus
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .wr_gpio_num = LCD_PIN_PCLK,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = {
            39, 40, 41, 42, 45, 46, 47, 48
        },
        .bus_width = 8,
        .max_transfer_bytes = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
    
    // 4. Initialize LCD panel IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = 20 * 1000 * 1000,  // 20MHz
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
    
    // 5. Initialize ST7789 panel
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    
    // 6. Configure ST7789
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, false, true);
    esp_lcd_panel_set_gap(panel_handle, 0, 35);
    esp_lcd_panel_disp_on_off(panel_handle, true);
    
    ESP_LOGI(TAG, "Display initialized successfully");
    
    // Initial render
    memset(&last_status, 0, sizeof(last_status));
    strcpy(last_status.ip_address, "0.0.0.0");
    last_status.system_state = "Initializing";
    render_display(&last_status);
    
    return ESP_OK;
}

void display_manager_update(const display_status_t *status)
{
    if (!status) return;
    
    // Update last status
    memcpy(&last_status, status, sizeof(display_status_t));
    
    // Render display
    render_display(&last_status);
}

void display_manager_set_mode(display_mode_t mode)
{
    if (mode >= DISPLAY_MODE_COUNT) return;
    
    ESP_LOGI(TAG, "Switching to display mode: %d", mode);
    current_mode = mode;
    
    // Re-render with new mode
    render_display(&last_status);
}

display_mode_t display_manager_get_mode(void)
{
    return current_mode;
}

void display_manager_toggle_backlight(void)
{
    backlight_on = !backlight_on;
    gpio_set_level(LCD_PIN_BK_LIGHT, backlight_on ? 1 : 0);
    ESP_LOGI(TAG, "Backlight: %s", backlight_on ? "ON" : "OFF");
}

void display_manager_refresh(void)
{
    ESP_LOGI(TAG, "Manual refresh requested");
    render_display(&last_status);
}
