/**
 * display_manager.c
 *
 * T-Display-S3 LCD driver for MimiClaw AI Agent.
 * Hardware: ST7789V 170x320 via Intel 8080 8-bit parallel bus.
 * Reference: hiruna/esp-idf-t-display-s3, Xinyuan-LilyGO/T-Display-S3
 */

#include "display_manager.h"
#include "display_ui.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

static const char *TAG = "display_mgr";

/* ── T-Display-S3 Pin Definitions ── */
#define LCD_PIN_NUM_PWR        15
#define LCD_PIN_NUM_BK_LIGHT   38
#define LCD_PIN_NUM_DATA0      39
#define LCD_PIN_NUM_DATA1      40
#define LCD_PIN_NUM_DATA2      41
#define LCD_PIN_NUM_DATA3      42
#define LCD_PIN_NUM_DATA4      45
#define LCD_PIN_NUM_DATA5      46
#define LCD_PIN_NUM_DATA6      47
#define LCD_PIN_NUM_DATA7      48
#define LCD_PIN_NUM_PCLK        8   /* WR */
#define LCD_PIN_NUM_RD          9
#define LCD_PIN_NUM_DC          7
#define LCD_PIN_NUM_CS          6
#define LCD_PIN_NUM_RST         5

/* ── LCD Parameters ── */
#define LCD_H_RES              320
#define LCD_V_RES              170
#define LCD_CMD_BITS             8
#define LCD_PARAM_BITS           8
#define LCD_I80_BUS_WIDTH        8
#define LCD_PIXEL_CLOCK_HZ     (10 * 1000 * 1000)  /* 10 MHz, safe default */
#define LCD_I80_TRANS_QUEUE_SIZE 20
#define LCD_PSRAM_TRANS_ALIGN   64
#define LCD_SRAM_TRANS_ALIGN     4

/* ── LVGL Parameters ── */
#define LVGL_BUFFER_SIZE       (LCD_H_RES * LCD_V_RES / 10)
#define LVGL_TICK_PERIOD_MS      5
#define LVGL_TASK_STACK_SIZE   (8 * 1024)
#define LVGL_TASK_PRIORITY       2

/* ── Backlight PWM Configuration ── */
#define BACKLIGHT_LEDC_TIMER      LEDC_TIMER_0
#define BACKLIGHT_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_CHANNEL    LEDC_CHANNEL_0
#define BACKLIGHT_LEDC_DUTY_RES   LEDC_TIMER_8_BIT  // 0-255
#define BACKLIGHT_LEDC_FREQUENCY  5000  // 5 kHz

/* ── Internal State ── */
static lv_disp_t *s_disp = NULL;
static display_mode_t s_display_mode = DISPLAY_MODE_DASHBOARD;
static bool s_backlight_on = true;
static uint8_t s_backlight_brightness = 100;  // 0-100%
static display_status_t s_status = {0};

static struct {
    bool wifi;
    bool tg;
    char status[64];
} s_state = {0};

/* ── LCD Power Init ── */
static void lcd_power_init(void)
{
    ESP_LOGI(TAG, "Powering on LCD (GPIO %d)...", LCD_PIN_NUM_PWR);
    gpio_config_t pwr_cfg = {
        .pin_bit_mask = 1ULL << LCD_PIN_NUM_PWR,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&pwr_cfg);
    gpio_set_level(LCD_PIN_NUM_PWR, 1);
}

/* ── LCD Backlight Init (PWM controlled) ── */
static void lcd_backlight_init(void)
{
    ESP_LOGI(TAG, "Initializing PWM backlight (GPIO %d)...", LCD_PIN_NUM_BK_LIGHT);
    
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = BACKLIGHT_LEDC_MODE,
        .duty_resolution  = BACKLIGHT_LEDC_DUTY_RES,
        .timer_num        = BACKLIGHT_LEDC_TIMER,
        .freq_hz          = BACKLIGHT_LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    
    // Configure LEDC channel
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = LCD_PIN_NUM_BK_LIGHT,
        .speed_mode     = BACKLIGHT_LEDC_MODE,
        .channel        = BACKLIGHT_LEDC_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = BACKLIGHT_LEDC_TIMER,
        .duty           = 255,  // Start at full brightness
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    
    ESP_LOGI(TAG, "Backlight PWM initialized at 100%%");
}

/* ── LCD RD Pin Init (pull high, not used for write) ── */
static void lcd_rd_init(void)
{
    gpio_config_t rd_cfg = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pin_bit_mask = 1ULL << LCD_PIN_NUM_RD,
    };
    gpio_config(&rd_cfg);
}

/* ── Create I80 Bus ── */
static void init_lcd_i80_bus(esp_lcd_panel_io_handle_t *io_handle)
{
    ESP_LOGI(TAG, "Initializing Intel 8080 bus...");

    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = LCD_PIN_NUM_DC,
        .wr_gpio_num = LCD_PIN_NUM_PCLK,
        .data_gpio_nums = {
            LCD_PIN_NUM_DATA0, LCD_PIN_NUM_DATA1,
            LCD_PIN_NUM_DATA2, LCD_PIN_NUM_DATA3,
            LCD_PIN_NUM_DATA4, LCD_PIN_NUM_DATA5,
            LCD_PIN_NUM_DATA6, LCD_PIN_NUM_DATA7,
        },
        .bus_width = LCD_I80_BUS_WIDTH,
        .max_transfer_bytes = LCD_H_RES * 100 * sizeof(uint16_t),
        .psram_trans_align = LCD_PSRAM_TRANS_ALIGN,
        .sram_trans_align = LCD_SRAM_TRANS_ALIGN,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_PIN_NUM_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = LCD_I80_TRANS_QUEUE_SIZE,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, io_handle));
}

/* ── Create ST7789 Panel ── */
static void init_lcd_panel(esp_lcd_panel_io_handle_t io_handle,
                           esp_lcd_panel_handle_t *panel)
{
    ESP_LOGI(TAG, "Initializing ST7789 LCD Driver...");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, panel));

    esp_lcd_panel_reset(*panel);
    esp_lcd_panel_init(*panel);
    esp_lcd_panel_invert_color(*panel, true);

    /* Landscape: buttons on left, screen on right */
    esp_lcd_panel_swap_xy(*panel, true);
    esp_lcd_panel_mirror(*panel, false, true);
    esp_lcd_panel_set_gap(*panel, 0, 35);

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*panel, true));
}

/* ── Register Panel with LVGL via esp_lvgl_port ── */
static lv_disp_t *init_lvgl_display(esp_lcd_panel_io_handle_t io_handle,
                                     esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG, "Adding display to LVGL port...");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel,
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
            .buff_spiram = true,
        },
    };

    return lvgl_port_add_disp(&disp_cfg);
}

/* ── Public API ── */

esp_err_t display_manager_init(void)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  T-Display-S3 LCD Init               ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════╝");

    /* 1. Init LVGL port (creates the lvgl task) */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = LVGL_TASK_PRIORITY,
        .task_stack = LVGL_TASK_STACK_SIZE,
        .task_affinity = 1,
        .task_max_sleep_ms = LVGL_TICK_PERIOD_MS * 2,
        .timer_period_ms = LVGL_TICK_PERIOD_MS,
    };
    esp_err_t err = lvgl_port_init(&lvgl_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LVGL port init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* 2. Power on LCD */
    lcd_power_init();

    /* 3. Backlight on */
    lcd_backlight_init();

    /* 4. RD pin (not used for write, pull high) */
    lcd_rd_init();

    /* 5. Create i80 bus + panel IO */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    init_lcd_i80_bus(&io_handle);

    /* 6. Create ST7789 panel */
    esp_lcd_panel_handle_t panel = NULL;
    init_lcd_panel(io_handle, &panel);

    /* 7. Register with LVGL */
    s_disp = init_lvgl_display(io_handle, panel);
    if (!s_disp) {
        ESP_LOGE(TAG, "Failed to add display to LVGL");
        return ESP_FAIL;
    }

    /* 8. Create the MimiClaw UI */
    display_ui_init(s_disp);

    ESP_LOGI(TAG, "Display initialization complete!");
    return ESP_OK;
}

void display_manager_update(bool wifi, bool tg, const char *status)
{
    s_state.wifi = wifi;
    s_state.tg = tg;
    if (status) {
        strncpy(s_state.status, status, sizeof(s_state.status) - 1);
    }
    display_ui_update(wifi, tg, s_state.status);
}

void display_manager_set_status(const char *status)
{
    if (status) {
        strncpy(s_state.status, status, sizeof(s_state.status) - 1);
    }
    display_ui_update(s_state.wifi, s_state.tg, s_state.status);
}

void display_manager_update_status(const display_status_t *status)
{
    if (status) {
        memcpy(&s_status, status, sizeof(display_status_t));
        // Update legacy state for compatibility
        s_state.wifi = status->wifi_connected;
        s_state.tg = status->telegram_connected;
        snprintf(s_state.status, sizeof(s_state.status), "%s", status->system_state);
        
        // Convert display_status_t to ui_status_t and update dashboard
        ui_status_t ui_status = {
            .wifi_connected = status->wifi_connected,
            .wifi_rssi = status->wifi_rssi,
            .ip_address = status->ip_address,
            .telegram_connected = status->telegram_connected,
            .system_state = status->system_state,
            .uptime_seconds = status->uptime_seconds,
            .free_heap = status->free_heap,
            .total_heap = status->total_heap
        };
        display_ui_update_dashboard(&ui_status);
    }
}

display_mode_t display_manager_get_mode(void)
{
    return s_display_mode;
}

void display_manager_set_mode(display_mode_t mode)
{
    if (mode < DISPLAY_MODE_COUNT) {
        s_display_mode = mode;
        ESP_LOGI(TAG, "Display mode changed to %d", mode);
        // Force refresh with current status
        display_manager_update(s_state.wifi, s_state.tg, s_state.status);
    }
}

void display_manager_toggle_backlight(void)
{
    s_backlight_on = !s_backlight_on;
    if (s_backlight_on) {
        // Restore previous brightness
        uint32_t duty = (s_backlight_brightness * 255) / 100;
        ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty);
        ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
    } else {
        // Turn off
        ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, 0);
        ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
    }
    ESP_LOGI(TAG, "Backlight %s", s_backlight_on ? "ON" : "OFF");
}

void display_manager_set_backlight(uint8_t brightness_percent)
{
    if (brightness_percent > 100) brightness_percent = 100;
    s_backlight_brightness = brightness_percent;
    s_backlight_on = (brightness_percent > 0);
    
    uint32_t duty = (brightness_percent * 255) / 100;
    ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty);
    ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
    
    ESP_LOGI(TAG, "Backlight set to %d%%", brightness_percent);
}

void display_manager_refresh(void)
{
    ESP_LOGI(TAG, "Refreshing display...");
    display_manager_update(s_state.wifi, s_state.tg, s_state.status);
}
