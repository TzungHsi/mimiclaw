#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * Display modes for T-Display-S3
 */
typedef enum {
    DISPLAY_MODE_GRID = 0,      // Four-grid dashboard (WiFi, Telegram, System, Memory)
    DISPLAY_MODE_DETAIL,        // Detailed values (signal strength, uptime, etc.)
    DISPLAY_MODE_LARGE_IP,      // Large IP address display
    DISPLAY_MODE_COUNT          // Total number of modes
} display_mode_t;

/**
 * System status structure for display
 */
typedef struct {
    bool wifi_connected;
    int8_t wifi_rssi;           // WiFi signal strength in dBm
    char ip_address[16];        // IP address string
    bool telegram_connected;
    uint32_t uptime_seconds;    // System uptime
    uint32_t free_heap;         // Free heap memory in bytes
    uint32_t total_heap;        // Total heap memory in bytes
    const char *system_state;   // System state string (e.g., "Ready", "Error")
} display_status_t;

/**
 * Initialize the display subsystem (T-Display-S3 ST7789).
 */
esp_err_t display_manager_init(void);

/**
 * Update the display with current system status.
 * @param status  Pointer to display_status_t structure
 */
void display_manager_update(const display_status_t *status);

/**
 * Set display mode (grid, detail, large IP).
 * @param mode  Display mode to set
 */
void display_manager_set_mode(display_mode_t mode);

/**
 * Get current display mode.
 * @return Current display mode
 */
display_mode_t display_manager_get_mode(void);

/**
 * Toggle backlight on/off.
 */
void display_manager_toggle_backlight(void);

/**
 * Force refresh the display.
 */
void display_manager_refresh(void);
