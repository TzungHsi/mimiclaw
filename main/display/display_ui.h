/**
 * display_ui.h
 * 
 * MimiClaw AI Agent UI for T-Display-S3
 * Supports both minimal status display and four-grid dashboard
 */

#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Four-grid dashboard status */
typedef struct {
    bool wifi_connected;
    int8_t wifi_rssi;
    const char *ip_address;
    bool telegram_connected;
    const char *system_state;
    uint32_t uptime_seconds;
    uint32_t free_heap;
    uint32_t total_heap;
} ui_status_t;

/**
 * Initialize the UI layout (called once after LVGL display is ready)
 */
void display_ui_init(lv_disp_t *disp);

/**
 * Update the UI with new status (legacy minimal display)
 */
void display_ui_update(bool wifi_connected, bool tg_connected, const char *status_text);

/**
 * Update the four-grid dashboard with detailed status
 */
void display_ui_update_dashboard(const ui_status_t *status);

#ifdef __cplusplus
}
#endif
