/**
 * telegram_status.h
 * 
 * Telegram bot status tracking for display
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Telegram bot status */
typedef enum {
    TG_STATUS_OFFLINE = 0,
    TG_STATUS_READY,
    TG_STATUS_INCOMING,
    TG_STATUS_RESPONDING,
    TG_STATUS_SENDING
} telegram_status_t;

/**
 * Set Telegram bot status
 */
void telegram_status_set(telegram_status_t status);

/**
 * Get Telegram bot status
 */
telegram_status_t telegram_status_get(void);

/**
 * Get status text for display
 */
const char* telegram_status_get_text(void);

#ifdef __cplusplus
}
#endif
