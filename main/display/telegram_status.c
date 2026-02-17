/**
 * telegram_status.c
 * 
 * Telegram bot status tracking for display
 */

#include "telegram_status.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static telegram_status_t s_status = TG_STATUS_OFFLINE;
static SemaphoreHandle_t s_mutex = NULL;

void telegram_status_set(telegram_status_t status)
{
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY)) {
        s_status = status;
        xSemaphoreGive(s_mutex);
    }
}

telegram_status_t telegram_status_get(void)
{
    if (!s_mutex) {
        return TG_STATUS_OFFLINE;
    }
    
    telegram_status_t status = TG_STATUS_OFFLINE;
    if (xSemaphoreTake(s_mutex, portMAX_DELAY)) {
        status = s_status;
        xSemaphoreGive(s_mutex);
    }
    return status;
}

const char* telegram_status_get_text(void)
{
    telegram_status_t status = telegram_status_get();
    
    switch (status) {
        case TG_STATUS_OFFLINE:
            return "Offline";
        case TG_STATUS_READY:
            return "Ready";
        case TG_STATUS_INCOMING:
            return "Incoming...";
        case TG_STATUS_RESPONDING:
            return "Responding...";
        case TG_STATUS_SENDING:
            return "Sending...";
        default:
            return "Unknown";
    }
}
