/**
 * display_ui.c
 *
 * MimiClaw AI Agent – NerdMiner-style status dashboard for T-Display-S3.
 * Resolution: 320x170 (landscape), RGB565.
 *
 * Layout:
 * ┌──────────────────────────────────────────┐
 * │  MIMICLAW AI AGENT     [WiFi] [Telegram] │  <- Header bar (30px)
 * ├──────────────────────────────────────────┤
 * │                                          │
 * │          ● System Ready                  │  <- Status area (110px)
 * │                                          │
 * ├──────────────────────────────────────────┤
 * │  WiFi: Connected  │  TG: Active          │  <- Footer bar (30px)
 * └──────────────────────────────────────────┘
 */

#include "display_ui.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "display_ui";

/* ── Color Palette (NerdMiner-inspired dark theme) ── */
#define COLOR_BG_DARK      lv_color_hex(0x1A1A2E)
#define COLOR_BG_HEADER    lv_color_hex(0x16213E)
#define COLOR_BG_FOOTER    lv_color_hex(0x16213E)
#define COLOR_ACCENT       lv_color_hex(0xE94560)
#define COLOR_GREEN         lv_color_hex(0x00FF88)
#define COLOR_RED           lv_color_hex(0xFF4444)
#define COLOR_YELLOW        lv_color_hex(0xFFD700)
#define COLOR_WHITE         lv_color_hex(0xEEEEEE)
#define COLOR_GRAY          lv_color_hex(0x888888)

/* ── UI Element Handles ── */
static lv_obj_t *lbl_title       = NULL;
static lv_obj_t *lbl_wifi_icon   = NULL;
static lv_obj_t *lbl_tg_icon     = NULL;
static lv_obj_t *lbl_status      = NULL;
static lv_obj_t *lbl_status_dot  = NULL;
static lv_obj_t *lbl_wifi_detail = NULL;
static lv_obj_t *lbl_tg_detail   = NULL;

/* ── Helper: create a styled label ── */
static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              lv_color_t color, const lv_font_t *font)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, 0);
    if (font) {
        lv_obj_set_style_text_font(lbl, font, 0);
    }
    return lbl;
}

void display_ui_init(lv_disp_t *disp)
{
    ESP_LOGI(TAG, "Creating MimiClaw dashboard UI...");

    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_disp_get_scr_act(disp);

        /* ── Screen background ── */
        lv_obj_set_style_bg_color(scr, COLOR_BG_DARK, 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

        /* ══════════════════════════════════════
         *  HEADER BAR (top 32px)
         * ══════════════════════════════════════ */
        lv_obj_t *header = lv_obj_create(scr);
        lv_obj_set_size(header, 320, 32);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, COLOR_BG_HEADER, 0);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_radius(header, 0, 0);
        lv_obj_set_style_pad_all(header, 4, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        /* Title */
        lbl_title = create_label(header, "MIMICLAW", COLOR_ACCENT, &lv_font_montserrat_14);
        lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 4, 0);

        /* WiFi icon */
        lbl_wifi_icon = create_label(header, LV_SYMBOL_WIFI, COLOR_RED, &lv_font_montserrat_14);
        lv_obj_align(lbl_wifi_icon, LV_ALIGN_RIGHT_MID, -36, 0);

        /* Telegram icon (use envelope as proxy) */
        lbl_tg_icon = create_label(header, LV_SYMBOL_ENVELOPE, COLOR_RED, &lv_font_montserrat_14);
        lv_obj_align(lbl_tg_icon, LV_ALIGN_RIGHT_MID, -4, 0);

        /* ══════════════════════════════════════
         *  MAIN STATUS AREA (middle 106px)
         * ══════════════════════════════════════ */
        lv_obj_t *body = lv_obj_create(scr);
        lv_obj_set_size(body, 320, 106);
        lv_obj_set_pos(body, 0, 32);
        lv_obj_set_style_bg_color(body, COLOR_BG_DARK, 0);
        lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(body, 0, 0);
        lv_obj_set_style_radius(body, 0, 0);
        lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

        /* Status dot (● indicator) */
        lbl_status_dot = create_label(body, "●", COLOR_YELLOW, &lv_font_montserrat_18);
        lv_obj_align(lbl_status_dot, LV_ALIGN_CENTER, -80, -8);

        /* Status text */
        lbl_status = create_label(body, "Booting...", COLOR_WHITE, &lv_font_montserrat_18);
        lv_obj_align(lbl_status, LV_ALIGN_CENTER, 20, -8);

        /* Sub-label: "AI Agent" */
        lv_obj_t *lbl_sub = create_label(body, "ESP32-S3 AI Agent", COLOR_GRAY, &lv_font_montserrat_12);
        lv_obj_align(lbl_sub, LV_ALIGN_CENTER, 0, 24);

        /* ══════════════════════════════════════
         *  FOOTER BAR (bottom 32px)
         * ══════════════════════════════════════ */
        lv_obj_t *footer = lv_obj_create(scr);
        lv_obj_set_size(footer, 320, 32);
        lv_obj_set_pos(footer, 0, 138);
        lv_obj_set_style_bg_color(footer, COLOR_BG_FOOTER, 0);
        lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(footer, 0, 0);
        lv_obj_set_style_radius(footer, 0, 0);
        lv_obj_set_style_pad_all(footer, 4, 0);
        lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

        /* WiFi detail */
        lbl_wifi_detail = create_label(footer, "WiFi: ---", COLOR_GRAY, &lv_font_montserrat_12);
        lv_obj_align(lbl_wifi_detail, LV_ALIGN_LEFT_MID, 8, 0);

        /* Telegram detail */
        lbl_tg_detail = create_label(footer, "TG: ---", COLOR_GRAY, &lv_font_montserrat_12);
        lv_obj_align(lbl_tg_detail, LV_ALIGN_RIGHT_MID, -8, 0);

        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "Dashboard UI created.");
}

void display_ui_update(bool wifi, bool tg, const char *status)
{
    if (lvgl_port_lock(0)) {

        /* ── Update header icons ── */
        if (lbl_wifi_icon) {
            lv_obj_set_style_text_color(lbl_wifi_icon,
                wifi ? COLOR_GREEN : COLOR_RED, 0);
        }
        if (lbl_tg_icon) {
            lv_obj_set_style_text_color(lbl_tg_icon,
                tg ? COLOR_GREEN : COLOR_RED, 0);
        }

        /* ── Update status dot color ── */
        if (lbl_status_dot) {
            lv_color_t dot_color;
            if (wifi && tg) {
                dot_color = COLOR_GREEN;   /* All systems go */
            } else if (wifi) {
                dot_color = COLOR_YELLOW;  /* Partial */
            } else {
                dot_color = COLOR_RED;     /* Offline */
            }
            lv_obj_set_style_text_color(lbl_status_dot, dot_color, 0);
        }

        /* ── Update main status text ── */
        if (lbl_status && status) {
            lv_label_set_text(lbl_status, status);
        }

        /* ── Update footer details ── */
        if (lbl_wifi_detail) {
            lv_label_set_text(lbl_wifi_detail,
                wifi ? "WiFi: Connected" : "WiFi: Disconnected");
            lv_obj_set_style_text_color(lbl_wifi_detail,
                wifi ? COLOR_GREEN : COLOR_RED, 0);
        }
        if (lbl_tg_detail) {
            lv_label_set_text(lbl_tg_detail,
                tg ? "TG: Active" : "TG: Offline");
            lv_obj_set_style_text_color(lbl_tg_detail,
                tg ? COLOR_GREEN : COLOR_RED, 0);
        }

        lvgl_port_unlock();
    }
}
