#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize display hardware using TFT_eSPI (low power)
 * 
 * Initializes T-Display-S3 with single sprite buffer
 * Similar to NerdMiner's lightweight approach
 */
void display_manager_init(void);

/**
 * @brief Update display with current system status
 * 
 * @param wifi_connected WiFi connection status
 * @param telegram_active Telegram bot status
 * @param status_text Status message to display
 */
void display_manager_update(bool wifi_connected, bool telegram_active, const char *status_text);

/**
 * @brief Set status text only (lightweight update)
 * 
 * @param status_text Status message to display
 */
void display_manager_set_status(const char *status_text);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_MANAGER_H
