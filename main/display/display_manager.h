#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the T-Display-S3 LCD hardware (ST7789, 8-bit i80 bus)
 * and the LVGL graphics library. Starts a background task for UI refresh.
 */
esp_err_t display_manager_init(void);

/**
 * Update the system status shown on the display.
 * @param wifi_connected  WiFi connection state
 * @param tg_connected    Telegram bot connection state
 * @param status_text     Short status description (max ~30 chars)
 */
void display_manager_update(bool wifi_connected, bool tg_connected, const char *status_text);

/**
 * Convenience: update only the status text line.
 */
void display_manager_set_status(const char *status_text);

#ifdef __cplusplus
}
#endif
