#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * Initialize the display subsystem (T-Display-S3 ST7789).
 */
esp_err_t display_manager_init(void);

/**
 * Update the display with current system status.
 * @param wifi_connected  Whether WiFi is connected
 * @param tg_connected    Whether Telegram is connected (polling active)
 * @param status_text     General status description
 */
void display_manager_update(bool wifi_connected, bool tg_connected, const char *status_text);

/**
 * Update just the status text on the display.
 */
void display_manager_set_status(const char *status_text);
