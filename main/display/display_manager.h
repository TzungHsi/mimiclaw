#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Display modes */
typedef enum {
    DISPLAY_MODE_DASHBOARD = 0,  // Four-grid status dashboard
    DISPLAY_MODE_MINIMAL,         // Minimal status display
    DISPLAY_MODE_COUNT            // Total number of modes
} display_mode_t;

/* System status structure for four-grid dashboard */
typedef struct {
    bool wifi_connected;
    int8_t wifi_rssi;
    char ip_address[16];
    bool telegram_connected;
    const char *system_state;
    uint32_t uptime_seconds;
    uint32_t free_heap;
    uint32_t total_heap;
} display_status_t;

/**
 * Initialize the T-Display-S3 LCD hardware (ST7789, 8-bit i80 bus)
 * and the LVGL graphics library. Starts a background task for UI refresh.
 */
esp_err_t display_manager_init(void);

/**
 * Update the system status shown on the display (four-grid dashboard).
 * @param status Pointer to display_status_t structure
 */
void display_manager_update_status(const display_status_t *status);

/**
 * Update the system status shown on the display (legacy API).
 * @param wifi_connected  WiFi connection state
 * @param tg_connected    Telegram bot connection state
 * @param status_text     Short status description (max ~30 chars)
 */
void display_manager_update(bool wifi_connected, bool tg_connected, const char *status_text);

/**
 * Convenience: update only the status text line.
 */
void display_manager_set_status(const char *status_text);

/**
 * Get current display mode
 */
display_mode_t display_manager_get_mode(void);

/**
 * Set display mode
 */
void display_manager_set_mode(display_mode_t mode);

/**
 * Toggle backlight on/off
 */
void display_manager_toggle_backlight(void);

/**
 * Force refresh display
 */
void display_manager_refresh(void);

#ifdef __cplusplus
}
#endif
