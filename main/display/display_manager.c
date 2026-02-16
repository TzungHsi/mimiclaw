#include "display_manager.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "display_manager";

// T-Display-S3 Pin definitions
#define LCD_BK_LIGHT_PIN       GPIO_NUM_38
#define LCD_RST_PIN            GPIO_NUM_5
#define LCD_CS_PIN             GPIO_NUM_6
#define LCD_DC_PIN             GPIO_NUM_7
#define LCD_WR_PIN             GPIO_NUM_8
#define LCD_RD_PIN             GPIO_NUM_9
#define LCD_D0_PIN             GPIO_NUM_39
#define LCD_D1_PIN             GPIO_NUM_40
#define LCD_D2_PIN             GPIO_NUM_41
#define LCD_D3_PIN             GPIO_NUM_42
#define LCD_D4_PIN             GPIO_NUM_45
#define LCD_D5_PIN             GPIO_NUM_46
#define LCD_D6_PIN             GPIO_NUM_47
#define LCD_D7_PIN             GPIO_NUM_48

// ST7789 display resolution (native orientation before swap_xy)
#define LCD_H_RES              320
#define LCD_V_RES              170
#define LCD_PIXEL_CLOCK_HZ     (17 * 1000 * 1000)  // 17MHz

// LVGL buffer size (1/10 of display)
#define LVGL_BUFFER_SIZE       ((LCD_H_RES * LCD_V_RES) / 10)

// Global handles
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_display_t *lvgl_disp = NULL;

// Display state
static display_mode_t current_mode = DISPLAY_MODE_GRID;
static bool backlight_on = true;

// LVGL UI elements (four-grid layout)
static lv_obj_t *grid_wifi = NULL;
static lv_obj_t *grid_telegram = NULL;
static lv_obj_t *grid_system = NULL;
static lv_obj_t *grid_memory = NULL;

static lv_obj_t *lbl_wifi_status = NULL;
static lv_obj_t *lbl_wifi_ip = NULL;
static lv_obj_t *lbl_telegram_status = NULL;
static lv_obj_t *lbl_system_uptime = NULL;
static lv_obj_t *lbl_system_state = NULL;
static lv_obj_t *lbl_memory_free = NULL;
static lv_obj_t *lbl_memory_total = NULL;

// Forward declarations
static void ui_four_grid_init(void);
static void ui_detail_init(void);
static void ui_large_ip_init(void);

/**
 * Initialize backlight GPIO
 */
static esp_err_t init_backlight(void) {
    ESP_LOGI(TAG, "Initializing backlight (GPIO %d)", LCD_BK_LIGHT_PIN);
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_BK_LIGHT_PIN
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(LCD_BK_LIGHT_PIN, backlight_on ? 1 : 0);
    return ESP_OK;
}

/**
 * Initialize LCD hardware (I80 bus + ST7789)
 */
static esp_err_t init_lcd_hardware(void) {
    ESP_LOGI(TAG, "Initializing LCD hardware");
    
    // Reset LCD
    ESP_LOGI(TAG, "Resetting LCD (GPIO %d)", LCD_RST_PIN);
    gpio_config_t rst_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_RST_PIN
    };
    ESP_ERROR_CHECK(gpio_config(&rst_gpio_config));
    gpio_set_level(LCD_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize I80 bus
    ESP_LOGI(TAG, "Initializing I80 bus");
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
        .max_transfer_bytes = LCD_H_RES * 100 * sizeof(uint16_t),  // 100 lines
        .psram_trans_align = 64,
        .sram_trans_align = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
    
    // Initialize panel IO
    ESP_LOGI(TAG, "Initializing LCD panel IO");
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_CS_PIN,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 20,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .swap_color_bytes = false,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));
    
    // Initialize ST7789 panel
    ESP_LOGI(TAG, "Initializing ST7789 panel");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,  // Already reset manually
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    
    // Configure ST7789
    ESP_LOGI(TAG, "Configuring ST7789");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 35));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    
    ESP_LOGI(TAG, "LCD hardware initialized successfully");
    return ESP_OK;
}

/**
 * Initialize LVGL port
 */
static esp_err_t init_lvgl_port(void) {
    ESP_LOGI(TAG, "Initializing LVGL port");
    
    // Initialize LVGL port
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,
        .task_stack = 8192,  // Increased from 4096 to prevent stack overflow
        .task_affinity = 1,  // Core 1 (Core 0 for WiFi/Telegram)
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5
    };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));
    
    // Add display to LVGL
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LVGL_BUFFER_SIZE,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
        }
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    
    ESP_LOGI(TAG, "LVGL port initialized successfully");
    return ESP_OK;
}

/**
 * Initialize four-grid UI
 */
static void ui_four_grid_init(void) {
    ESP_LOGI(TAG, "Initializing four-grid UI");
    
    lvgl_port_lock(0);
    
    // Clear screen
    lv_obj_clean(lv_screen_active());
    
    // Create four grid containers (170x320 divided into 4)
    // Top-left: WiFi (85x160)
    grid_wifi = lv_obj_create(lv_screen_active());
    lv_obj_set_size(grid_wifi, 85, 160);
    lv_obj_set_pos(grid_wifi, 0, 0);
    lv_obj_set_style_bg_color(grid_wifi, lv_color_hex(0x1E3A8A), 0);
    
    lv_obj_t *lbl_wifi_title = lv_label_create(grid_wifi);
    lv_label_set_text(lbl_wifi_title, LV_SYMBOL_WIFI " WiFi");
    lv_obj_align(lbl_wifi_title, LV_ALIGN_TOP_MID, 0, 5);
    
    lbl_wifi_status = lv_label_create(grid_wifi);
    lv_label_set_text(lbl_wifi_status, "---");
    lv_obj_align(lbl_wifi_status, LV_ALIGN_CENTER, 0, 0);
    
    lbl_wifi_ip = lv_label_create(grid_wifi);
    lv_label_set_text(lbl_wifi_ip, "0.0.0.0");
    lv_obj_align(lbl_wifi_ip, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_font(lbl_wifi_ip, &lv_font_montserrat_14, 0);
    
    // Top-right: Telegram (85x160)
    grid_telegram = lv_obj_create(lv_screen_active());
    lv_obj_set_size(grid_telegram, 85, 160);
    lv_obj_set_pos(grid_telegram, 85, 0);
    lv_obj_set_style_bg_color(grid_telegram, lv_color_hex(0x065F46), 0);
    
    lv_obj_t *lbl_telegram_title = lv_label_create(grid_telegram);
    lv_label_set_text(lbl_telegram_title, LV_SYMBOL_CALL " Telegram");
    lv_obj_align(lbl_telegram_title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(lbl_telegram_title, &lv_font_montserrat_14, 0);
    
    lbl_telegram_status = lv_label_create(grid_telegram);
    lv_label_set_text(lbl_telegram_status, "---");
    lv_obj_align(lbl_telegram_status, LV_ALIGN_CENTER, 0, 0);
    
    // Bottom-left: System (85x160)
    grid_system = lv_obj_create(lv_screen_active());
    lv_obj_set_size(grid_system, 85, 160);
    lv_obj_set_pos(grid_system, 0, 160);
    lv_obj_set_style_bg_color(grid_system, lv_color_hex(0x7C2D12), 0);
    
    lv_obj_t *lbl_system_title = lv_label_create(grid_system);
    lv_label_set_text(lbl_system_title, LV_SYMBOL_SETTINGS " System");
    lv_obj_align(lbl_system_title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(lbl_system_title, &lv_font_montserrat_14, 0);
    
    lbl_system_uptime = lv_label_create(grid_system);
    lv_label_set_text(lbl_system_uptime, "0s");
    lv_obj_align(lbl_system_uptime, LV_ALIGN_CENTER, 0, -10);
    
    lbl_system_state = lv_label_create(grid_system);
    lv_label_set_text(lbl_system_state, "Ready");
    lv_obj_align(lbl_system_state, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_font(lbl_system_state, &lv_font_montserrat_14, 0);
    
    // Bottom-right: Memory (85x160)
    grid_memory = lv_obj_create(lv_screen_active());
    lv_obj_set_size(grid_memory, 85, 160);
    lv_obj_set_pos(grid_memory, 85, 160);
    lv_obj_set_style_bg_color(grid_memory, lv_color_hex(0x713F12), 0);
    
    lv_obj_t *lbl_memory_title = lv_label_create(grid_memory);
    lv_label_set_text(lbl_memory_title, LV_SYMBOL_LIST " Memory");
    lv_obj_align(lbl_memory_title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(lbl_memory_title, &lv_font_montserrat_14, 0);
    
    lbl_memory_free = lv_label_create(grid_memory);
    lv_label_set_text(lbl_memory_free, "Free: 0KB");
    lv_obj_align(lbl_memory_free, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_text_font(lbl_memory_free, &lv_font_montserrat_14, 0);
    
    lbl_memory_total = lv_label_create(grid_memory);
    lv_label_set_text(lbl_memory_total, "Total: 0KB");
    lv_obj_align(lbl_memory_total, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_font(lbl_memory_total, &lv_font_montserrat_14, 0);
    
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "Four-grid UI initialized");
}

/**
 * Initialize detail UI (placeholder)
 */
static void ui_detail_init(void) {
    ESP_LOGI(TAG, "Detail UI not yet implemented");
    // TODO: Implement detail view
}

/**
 * Initialize large IP UI (placeholder)
 */
static void ui_large_ip_init(void) {
    ESP_LOGI(TAG, "Large IP UI not yet implemented");
    // TODO: Implement large IP view
}

/**
 * Public API: Initialize display manager
 */
esp_err_t display_manager_init(void) {
    ESP_LOGI(TAG, "=== Display Manager Init ===");
    
    // Initialize backlight
    ESP_ERROR_CHECK(init_backlight());
    
    // Initialize LCD hardware
    ESP_ERROR_CHECK(init_lcd_hardware());
    
    // Initialize LVGL port
    ESP_ERROR_CHECK(init_lvgl_port());
    
    // Initialize UI based on current mode
    ui_four_grid_init();
    
    ESP_LOGI(TAG, "Display manager initialized successfully");
    return ESP_OK;
}

/**
 * Public API: Update display with system status
 */
void display_manager_update(const display_status_t *status) {
    if (!status || !lvgl_disp) return;
    
    lvgl_port_lock(0);
    
    if (current_mode == DISPLAY_MODE_GRID) {
        // Update WiFi status
        if (lbl_wifi_status) {
            lv_label_set_text(lbl_wifi_status, status->wifi_connected ? "Connected" : "Disconnected");
        }
        if (lbl_wifi_ip) {
            lv_label_set_text(lbl_wifi_ip, status->ip_address);
        }
        
        // Update Telegram status
        if (lbl_telegram_status) {
            lv_label_set_text(lbl_telegram_status, status->telegram_connected ? "Online" : "Offline");
        }
        
        // Update System info
        if (lbl_system_uptime) {
            char uptime_str[32];
            uint32_t hours = status->uptime_seconds / 3600;
            uint32_t minutes = (status->uptime_seconds % 3600) / 60;
            uint32_t seconds = status->uptime_seconds % 60;
            snprintf(uptime_str, sizeof(uptime_str), "%02luh%02lum%02lus", hours, minutes, seconds);
            lv_label_set_text(lbl_system_uptime, uptime_str);
        }
        if (lbl_system_state) {
            lv_label_set_text(lbl_system_state, status->system_state);
        }
        
        // Update Memory info
        if (lbl_memory_free) {
            char mem_str[32];
            snprintf(mem_str, sizeof(mem_str), "Free: %luKB", status->free_heap / 1024);
            lv_label_set_text(lbl_memory_free, mem_str);
        }
        if (lbl_memory_total) {
            char mem_str[32];
            snprintf(mem_str, sizeof(mem_str), "Total: %luKB", status->total_heap / 1024);
            lv_label_set_text(lbl_memory_total, mem_str);
        }
    }
    
    lvgl_port_unlock();
}

/**
 * Public API: Set display mode
 */
void display_manager_set_mode(display_mode_t mode) {
    if (mode >= DISPLAY_MODE_COUNT) return;
    
    current_mode = mode;
    ESP_LOGI(TAG, "Switching to display mode %d", mode);
    
    switch (mode) {
        case DISPLAY_MODE_GRID:
            ui_four_grid_init();
            break;
        case DISPLAY_MODE_DETAIL:
            ui_detail_init();
            break;
        case DISPLAY_MODE_LARGE_IP:
            ui_large_ip_init();
            break;
        default:
            break;
    }
}

/**
 * Public API: Get current display mode
 */
display_mode_t display_manager_get_mode(void) {
    return current_mode;
}

/**
 * Public API: Toggle backlight
 */
void display_manager_toggle_backlight(void) {
    backlight_on = !backlight_on;
    gpio_set_level(LCD_BK_LIGHT_PIN, backlight_on ? 1 : 0);
    ESP_LOGI(TAG, "Backlight %s", backlight_on ? "ON" : "OFF");
}

/**
 * Public API: Force refresh
 */
void display_manager_refresh(void) {
    ESP_LOGI(TAG, "Forcing display refresh");
    // LVGL handles refresh automatically
}

/**
 * Public API: Get LVGL display handle
 */
lv_display_t* display_manager_get_lvgl_display(void) {
    return lvgl_disp;
}
