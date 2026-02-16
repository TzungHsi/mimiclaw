#include "display_test.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "display_test";

/**
 * T-Display-S3 Pin Definitions (8-bit Parallel)
 */
#define LCD_PIN_BK_LIGHT       38
#define LCD_PIN_RST            5
#define LCD_PIN_CS             6
#define LCD_PIN_DC             7
#define LCD_PIN_PCLK           8

#define LCD_H_RES              170
#define LCD_V_RES              320

// Color definitions (RGB565)
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_YELLOW    0xFFE0
#define COLOR_BLUE      0x001F
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F

static esp_lcd_panel_handle_t panel_handle = NULL;
static uint16_t *test_buffer = NULL;

// Fill entire screen with a single color
static void fill_screen(uint16_t color)
{
    if (!test_buffer) return;
    
    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        test_buffer[i] = color;
    }
    
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, test_buffer);
}

// Draw a horizontal stripe pattern
static void draw_stripes_horizontal(void)
{
    if (!test_buffer) return;
    
    uint16_t colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA};
    int stripe_height = LCD_V_RES / 6;
    
    for (int y = 0; y < LCD_V_RES; y++) {
        int color_index = y / stripe_height;
        if (color_index >= 6) color_index = 5;
        
        for (int x = 0; x < LCD_H_RES; x++) {
            test_buffer[y * LCD_H_RES + x] = colors[color_index];
        }
    }
    
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, test_buffer);
}

// Draw a vertical stripe pattern
static void draw_stripes_vertical(void)
{
    if (!test_buffer) return;
    
    uint16_t colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA};
    int stripe_width = LCD_H_RES / 6;
    
    for (int y = 0; y < LCD_V_RES; y++) {
        for (int x = 0; x < LCD_H_RES; x++) {
            int color_index = x / stripe_width;
            if (color_index >= 6) color_index = 5;
            test_buffer[y * LCD_H_RES + x] = colors[color_index];
        }
    }
    
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, test_buffer);
}

// Draw a checkerboard pattern
static void draw_checkerboard(void)
{
    if (!test_buffer) return;
    
    int square_size = 20;
    
    for (int y = 0; y < LCD_V_RES; y++) {
        for (int x = 0; x < LCD_H_RES; x++) {
            bool is_white = ((x / square_size) + (y / square_size)) % 2 == 0;
            test_buffer[y * LCD_H_RES + x] = is_white ? COLOR_WHITE : COLOR_BLACK;
        }
    }
    
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, test_buffer);
}

// Draw a gradient
static void draw_gradient(void)
{
    if (!test_buffer) return;
    
    for (int y = 0; y < LCD_V_RES; y++) {
        // Calculate color based on Y position
        uint8_t intensity = (y * 255) / LCD_V_RES;
        uint16_t color = (intensity >> 3) << 11 | (intensity >> 2) << 5 | (intensity >> 3);
        
        for (int x = 0; x < LCD_H_RES; x++) {
            test_buffer[y * LCD_H_RES + x] = color;
        }
    }
    
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, test_buffer);
}

esp_err_t display_test_init(void)
{
    ESP_LOGI(TAG, "=== T-Display-S3 LCD Test ===");
    ESP_LOGI(TAG, "Resolution: %dx%d", LCD_H_RES, LCD_V_RES);
    
    // Allocate test buffer
    test_buffer = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!test_buffer) {
        ESP_LOGE(TAG, "Failed to allocate test buffer");
        return ESP_ERR_NO_MEM;
    }
    memset(test_buffer, 0, LCD_H_RES * LCD_V_RES * sizeof(uint16_t));
    
    // 1. Backlight Init
    ESP_LOGI(TAG, "Step 1: Initializing backlight (GPIO %d)", LCD_PIN_BK_LIGHT);
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_BK_LIGHT,
    };
    gpio_config(&bk_gpio_config);
    gpio_set_level(LCD_PIN_BK_LIGHT, 1);
    ESP_LOGI(TAG, "Backlight ON");
    
    // 2. LCD Reset
    ESP_LOGI(TAG, "Step 2: Resetting LCD (GPIO %d)", LCD_PIN_RST);
    gpio_config_t rst_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_RST,
    };
    gpio_config(&rst_gpio_config);
    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "LCD reset complete");
    
    // 3. Initialize I80 bus
    ESP_LOGI(TAG, "Step 3: Initializing I80 bus");
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
    ESP_LOGI(TAG, "I80 bus initialized");
    
    // 4. Initialize LCD panel IO
    ESP_LOGI(TAG, "Step 4: Initializing LCD panel IO");
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
    ESP_LOGI(TAG, "LCD panel IO initialized");
    
    // 5. Initialize ST7789 panel
    ESP_LOGI(TAG, "Step 5: Initializing ST7789 panel");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_LOGI(TAG, "ST7789 panel initialized");
    
    // 6. Configure ST7789
    ESP_LOGI(TAG, "Step 6: Configuring ST7789");
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, false, true);
    esp_lcd_panel_set_gap(panel_handle, 0, 35);
    esp_lcd_panel_disp_on_off(panel_handle, true);
    ESP_LOGI(TAG, "ST7789 configured");
    
    ESP_LOGI(TAG, "=== LCD Test Initialization Complete ===");
    
    return ESP_OK;
}

void display_test_run(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Starting LCD Test Sequence ===");
    
    // Test 1: Solid colors
    ESP_LOGI(TAG, "Test 1: RED screen");
    fill_screen(COLOR_RED);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "Test 1: GREEN screen");
    fill_screen(COLOR_GREEN);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "Test 1: BLUE screen");
    fill_screen(COLOR_BLUE);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "Test 1: WHITE screen");
    fill_screen(COLOR_WHITE);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "Test 1: BLACK screen");
    fill_screen(COLOR_BLACK);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Test 2: Patterns
    ESP_LOGI(TAG, "Test 2: Horizontal stripes");
    draw_stripes_horizontal();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "Test 2: Vertical stripes");
    draw_stripes_vertical();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "Test 2: Checkerboard");
    draw_checkerboard();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "Test 2: Gradient");
    draw_gradient();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "=== LCD Test Sequence Complete ===");
    ESP_LOGI(TAG, "Repeating tests...");
}
