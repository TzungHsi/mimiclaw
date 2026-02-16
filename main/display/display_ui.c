/**
 * display_ui.c
 *
 * MimiClaw AI Agent – Four-grid dashboard for T-Display-S3.
 * Resolution: 320x170 (landscape), RGB565.
 *
 * Layout:
 * ┌────────────────┬────────────────┐
 * │  📶 WiFi       │  📱 Telegram   │  <- Top row (85px each)
 * │  Connected     │  Active        │
 * │  192.168.1.100 │  Ready         │
 * ├────────────────┼────────────────┤
 * │  ⚙️ System     │  💾 Memory     │  <- Bottom row (85px each)
 * │  Ready         │  245KB Free    │
 * │  Uptime: 1h23m │  78% Used      │
 * └────────────────┴────────────────┘
 */

#include "display_ui.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "display_ui";

/* ── Color Palette (NerdMiner-inspired dark theme) ── */
#define COLOR_BG_DARK      lv_color_hex(0x1A1A2E)
#define COLOR_BG_CARD      lv_color_hex(0x16213E)
#define COLOR_ACCENT       lv_color_hex(0xE94560)
#define COLOR_GREEN        lv_color_hex(0x00FF88)
#define COLOR_RED          lv_color_hex(0xFF4444)
#define COLOR_YELLOW       lv_color_hex(0xFFD700)
#define COLOR_BLUE         lv_color_hex(0x4A9EFF)
#define COLOR_WHITE        lv_color_hex(0xEEEEEE)
#define COLOR_GRAY         lv_color_hex(0x888888)

/* ── UI Element Handles ── */
static struct {
    lv_obj_t *wifi_title;
    lv_obj_t *wifi_status;
    lv_obj_t *wifi_detail;
    
    lv_obj_t *tg_title;
    lv_obj_t *tg_status;
    lv_obj_t *tg_detail;
    
    lv_obj_t *sys_title;
    lv_obj_t *sys_status;
    lv_obj_t *sys_detail;
    
    lv_obj_t *mem_title;
    lv_obj_t *mem_status;
    lv_obj_t *mem_detail;
} ui_elements = {0};

/* ── Helper: create a card container ── */
static lv_obj_t *create_card(lv_obj_t *parent, int16_t x, int16_t y, int16_t w, int16_t h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COLOR_GRAY, 0);
    lv_obj_set_style_radius(card, 4, 0);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

/* ── Helper: create a styled label ── */
static lv_obj_t *create_label(lv_obj_t *parent, const char *text, lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    return lbl;
}

void display_ui_init(lv_disp_t *disp)
{
    ESP_LOGI(TAG, "Creating four-grid dashboard UI...");

    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_disp_get_scr_act(disp);

        /* ── Screen background ── */
        lv_obj_set_style_bg_color(scr, COLOR_BG_DARK, 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

        /* ══════════════════════════════════════
         *  FOUR-GRID LAYOUT (160x85 each card)
         * ══════════════════════════════════════ */
        
        /* WiFi Card (top-left) */
        lv_obj_t *wifi_card = create_card(scr, 0, 0, 160, 85);
        ui_elements.wifi_title = create_label(wifi_card, LV_SYMBOL_WIFI " WiFi", COLOR_BLUE);
        lv_obj_align(ui_elements.wifi_title, LV_ALIGN_TOP_LEFT, 0, 0);
        
        ui_elements.wifi_status = create_label(wifi_card, "Disconnected", COLOR_RED);
        lv_obj_align(ui_elements.wifi_status, LV_ALIGN_TOP_LEFT, 0, 20);
        
        ui_elements.wifi_detail = create_label(wifi_card, "0.0.0.0", COLOR_GRAY);
        lv_obj_align(ui_elements.wifi_detail, LV_ALIGN_TOP_LEFT, 0, 40);
        
        /* Telegram Card (top-right) */
        lv_obj_t *tg_card = create_card(scr, 160, 0, 160, 85);
        ui_elements.tg_title = create_label(tg_card, LV_SYMBOL_ENVELOPE " Telegram", COLOR_BLUE);
        lv_obj_align(ui_elements.tg_title, LV_ALIGN_TOP_LEFT, 0, 0);
        
        ui_elements.tg_status = create_label(tg_card, "Offline", COLOR_RED);
        lv_obj_align(ui_elements.tg_status, LV_ALIGN_TOP_LEFT, 0, 20);
        
        ui_elements.tg_detail = create_label(tg_card, "Waiting...", COLOR_GRAY);
        lv_obj_align(ui_elements.tg_detail, LV_ALIGN_TOP_LEFT, 0, 40);
        
        /* System Card (bottom-left) */
        lv_obj_t *sys_card = create_card(scr, 0, 85, 160, 85);
        ui_elements.sys_title = create_label(sys_card, LV_SYMBOL_SETTINGS " System", COLOR_BLUE);
        lv_obj_align(ui_elements.sys_title, LV_ALIGN_TOP_LEFT, 0, 0);
        
        ui_elements.sys_status = create_label(sys_card, "Starting", COLOR_YELLOW);
        lv_obj_align(ui_elements.sys_status, LV_ALIGN_TOP_LEFT, 0, 20);
        
        ui_elements.sys_detail = create_label(sys_card, "Uptime: 0s", COLOR_GRAY);
        lv_obj_align(ui_elements.sys_detail, LV_ALIGN_TOP_LEFT, 0, 40);
        
        /* Memory Card (bottom-right) */
        lv_obj_t *mem_card = create_card(scr, 160, 85, 160, 85);
        ui_elements.mem_title = create_label(mem_card, LV_SYMBOL_SD_CARD " Memory", COLOR_BLUE);
        lv_obj_align(ui_elements.mem_title, LV_ALIGN_TOP_LEFT, 0, 0);
        
        ui_elements.mem_status = create_label(mem_card, "0 KB Free", COLOR_GRAY);
        lv_obj_align(ui_elements.mem_status, LV_ALIGN_TOP_LEFT, 0, 20);
        
        ui_elements.mem_detail = create_label(mem_card, "0% Used", COLOR_GRAY);
        lv_obj_align(ui_elements.mem_detail, LV_ALIGN_TOP_LEFT, 0, 40);

        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "Four-grid dashboard UI created.");
}

void display_ui_update(bool wifi, bool tg, const char *status)
{
    if (lvgl_port_lock(0)) {
        /* Update WiFi status */
        if (ui_elements.wifi_status) {
            lv_label_set_text(ui_elements.wifi_status, wifi ? "Connected" : "Disconnected");
            lv_obj_set_style_text_color(ui_elements.wifi_status, wifi ? COLOR_GREEN : COLOR_RED, 0);
        }
        
        /* Update Telegram status */
        if (ui_elements.tg_status) {
            lv_label_set_text(ui_elements.tg_status, tg ? "Active" : "Offline");
            lv_obj_set_style_text_color(ui_elements.tg_status, tg ? COLOR_GREEN : COLOR_RED, 0);
        }
        
        /* Update System status */
        if (ui_elements.sys_status && status) {
            lv_label_set_text(ui_elements.sys_status, status);
            lv_color_t color = COLOR_YELLOW;
            if (wifi && tg) color = COLOR_GREEN;
            else if (!wifi) color = COLOR_RED;
            lv_obj_set_style_text_color(ui_elements.sys_status, color, 0);
        }

        lvgl_port_unlock();
    }
}

void display_ui_update_dashboard(const ui_status_t *status)
{
    if (!status) return;
    
    if (lvgl_port_lock(0)) {
        char buf[64];
        
        /* ── WiFi Card ── */
        if (ui_elements.wifi_status) {
            lv_label_set_text(ui_elements.wifi_status, 
                status->wifi_connected ? "Connected" : "Disconnected");
            lv_obj_set_style_text_color(ui_elements.wifi_status, 
                status->wifi_connected ? COLOR_GREEN : COLOR_RED, 0);
        }
        if (ui_elements.wifi_detail && status->ip_address) {
            snprintf(buf, sizeof(buf), "%s", status->ip_address);
            lv_label_set_text(ui_elements.wifi_detail, buf);
        }
        
        /* ── Telegram Card ── */
        if (ui_elements.tg_status) {
            lv_label_set_text(ui_elements.tg_status, 
                status->telegram_connected ? "Active" : "Offline");
            lv_obj_set_style_text_color(ui_elements.tg_status, 
                status->telegram_connected ? COLOR_GREEN : COLOR_RED, 0);
        }
        if (ui_elements.tg_detail) {
            lv_label_set_text(ui_elements.tg_detail, 
                status->telegram_connected ? "Ready" : "Waiting...");
        }
        
        /* ── System Card ── */
        if (ui_elements.sys_status && status->system_state) {
            lv_label_set_text(ui_elements.sys_status, status->system_state);
            lv_color_t color = COLOR_YELLOW;
            if (status->wifi_connected && status->telegram_connected) color = COLOR_GREEN;
            else if (!status->wifi_connected) color = COLOR_RED;
            lv_obj_set_style_text_color(ui_elements.sys_status, color, 0);
        }
        if (ui_elements.sys_detail) {
            uint32_t hours = status->uptime_seconds / 3600;
            uint32_t mins = (status->uptime_seconds % 3600) / 60;
            snprintf(buf, sizeof(buf), "Up: %uh%um", hours, mins);
            lv_label_set_text(ui_elements.sys_detail, buf);
        }
        
        /* ── Memory Card ── */
        if (ui_elements.mem_status) {
            snprintf(buf, sizeof(buf), "%u KB Free", status->free_heap / 1024);
            lv_label_set_text(ui_elements.mem_status, buf);
        }
        if (ui_elements.mem_detail && status->total_heap > 0) {
            uint32_t used_pct = ((status->total_heap - status->free_heap) * 100) / status->total_heap;
            snprintf(buf, sizeof(buf), "%u%% Used", used_pct);
            lv_label_set_text(ui_elements.mem_detail, buf);
            
            lv_color_t color = COLOR_GREEN;
            if (used_pct > 80) color = COLOR_RED;
            else if (used_pct > 60) color = COLOR_YELLOW;
            lv_obj_set_style_text_color(ui_elements.mem_detail, color, 0);
        }

        lvgl_port_unlock();
    }
}
