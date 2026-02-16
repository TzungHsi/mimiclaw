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

/* ── Internal State ── */
static lv_disp_t *s_disp = NULL;

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

/* ── LCD Backlight Init (simple on/off, no PWM) ── */
static void lcd_backlight_init(void)
{
    ESP_LOGI(TAG, "Turning on backlight (GPIO %d)...", LCD_PIN_NUM_BK_LIGHT);
    gpio_config_t bk_cfg = {
        .pin_bit_mask = 1ULL << LCD_PIN_NUM_BK_LIGHT,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bk_cfg);
    gpio_set_level(LCD_PIN_NUM_BK_LIGHT, 1);
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
