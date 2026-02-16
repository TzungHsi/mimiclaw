#pragma once

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the MimiClaw status UI on the given LVGL display.
 * Call once after LVGL and display driver are fully initialized.
 */
void display_ui_init(lv_disp_t *disp);

/**
 * Update the UI elements with current system state.
 * Thread-safe: acquires the LVGL lock internally.
 */
void display_ui_update(bool wifi, bool tg, const char *status);

#ifdef __cplusplus
}
#endif
