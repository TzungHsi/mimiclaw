#include "display_manager.h"
#include <TFT_eSPI.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "display";

// T-Display-S3 specifications
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 170

// Color scheme (dark theme like NerdMiner)
#define COLOR_BG      0x0000  // Black
#define COLOR_HEADER  0x001F  // Dark Blue
#define COLOR_TEXT    0xFFFF  // White
#define COLOR_ONLINE  0x07E0  // Green
#define COLOR_OFFLINE 0xF800  // Red
#define COLOR_WARN    0xFFE0  // Yellow

// Global instances
static TFT_eSPI tft = TFT_eSPI();
static TFT_eSprite sprite = TFT_eSprite(&tft);

// State variables
static bool wifi_status = false;
static bool tg_status = false;
static char current_status[64] = "Initializing...";

/**
 * @brief Draw the static background (header + footer)
 */
static void draw_background(void) {
    sprite.fillSprite(COLOR_BG);
    
    // Header bar
    sprite.fillRect(0, 0, SCREEN_WIDTH, 30, COLOR_HEADER);
    sprite.setTextColor(COLOR_TEXT, COLOR_HEADER);
    sprite.setTextDatum(TL_DATUM);
    sprite.setTextSize(2);
    sprite.drawString("MIMICLAW", 10, 5);
    
    // Footer bar
    sprite.drawFastHLine(0, SCREEN_HEIGHT - 30, SCREEN_WIDTH, COLOR_TEXT);
}

/**
 * @brief Draw WiFi and Telegram status icons
 */
static void draw_status_icons(void) {
    // WiFi icon (top right)
    uint16_t wifi_color = wifi_status ? COLOR_ONLINE : COLOR_OFFLINE;
    sprite.fillCircle(SCREEN_WIDTH - 60, 15, 8, wifi_color);
    sprite.setTextColor(COLOR_BG, wifi_color);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextSize(1);
    sprite.drawString("W", SCREEN_WIDTH - 60, 15);
    
    // Telegram icon (top right)
    uint16_t tg_color = tg_status ? COLOR_ONLINE : COLOR_OFFLINE;
    sprite.fillCircle(SCREEN_WIDTH - 30, 15, 8, tg_color);
    sprite.setTextColor(COLOR_BG, tg_color);
    sprite.drawString("T", SCREEN_WIDTH - 30, 15);
}

/**
 * @brief Draw main status text
 */
static void draw_status_text(void) {
    sprite.setTextColor(COLOR_TEXT, COLOR_BG);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextSize(2);
    sprite.drawString(current_status, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
}

/**
 * @brief Draw footer info
 */
static void draw_footer(void) {
    sprite.setTextColor(COLOR_TEXT, COLOR_BG);
    sprite.setTextDatum(BL_DATUM);
    sprite.setTextSize(1);
    
    const char *wifi_text = wifi_status ? "WiFi: OK" : "WiFi: --";
    const char *tg_text = tg_status ? "TG: Active" : "TG: Offline";
    
    sprite.drawString(wifi_text, 10, SCREEN_HEIGHT - 5);
    sprite.drawString(tg_text, SCREEN_WIDTH / 2 + 10, SCREEN_HEIGHT - 5);
}

/**
 * @brief Refresh the entire display
 */
static void refresh_display(void) {
    draw_background();
    draw_status_icons();
    draw_status_text();
    draw_footer();
    sprite.pushSprite(0, 0);
}

void display_manager_init(void) {
    ESP_LOGI(TAG, "Initializing T-Display-S3 with TFT_eSPI (low power mode)");
    
    // Initialize TFT
    tft.init();
    tft.setRotation(1);  // Landscape
    tft.fillScreen(COLOR_BG);
    
    // Create sprite buffer (single buffer, low memory)
    sprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
    sprite.setSwapBytes(true);
    
    // Initial display
    refresh_display();
    
    ESP_LOGI(TAG, "Display initialized successfully");
}

void display_manager_update(bool wifi_connected, bool telegram_active, const char *status_text) {
    wifi_status = wifi_connected;
    tg_status = telegram_active;
    
    if (status_text) {
        strncpy(current_status, status_text, sizeof(current_status) - 1);
        current_status[sizeof(current_status) - 1] = '\0';
    }
    
    refresh_display();
    ESP_LOGI(TAG, "Display updated: WiFi=%d, TG=%d, Status=%s", wifi_connected, telegram_active, current_status);
}

void display_manager_set_status(const char *status_text) {
    if (status_text) {
        strncpy(current_status, status_text, sizeof(current_status) - 1);
        current_status[sizeof(current_status) - 1] = '\0';
        refresh_display();
    }
}
