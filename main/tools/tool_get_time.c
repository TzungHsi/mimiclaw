#include "tool_get_time.h"
#include "mimi_config.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_sntp.h"

static const char *TAG = "tool_time";

/* SNTP sync callback */
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized via SNTP");
}

/* Initialize and sync time via SNTP */
static esp_err_t sync_time_sntp(char *out, size_t out_size)
{
    ESP_LOGI(TAG, "Initializing SNTP time synchronization...");
    
    /* Stop SNTP if already running */
    if (esp_sntp_enabled()) {
        ESP_LOGI(TAG, "SNTP already running, restarting...");
        esp_sntp_stop();
    }
    
    /* Configure SNTP */
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    /* Use Taiwan NTP servers for best performance */
    esp_sntp_setservername(0, "time.stdtime.gov.tw");  // Taiwan National Time Server
    esp_sntp_setservername(1, "tock.stdtime.gov.tw");  // Taiwan backup server
    esp_sntp_setservername(2, "watch.stdtime.gov.tw"); // Taiwan backup server
    esp_sntp_setservername(3, "tick.stdtime.gov.tw");  // Taiwan backup server
    
    /* Set notification callback */
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    
    /* Start SNTP */
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP initialized, waiting for time sync...");
    
    /* Wait for time to be set (max 10 seconds) */
    int retry = 0;
    const int max_retry = 100;  // 100 * 100ms = 10 seconds
    
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < max_retry) {
        ESP_LOGD(TAG, "Waiting for system time to be set... (%d/%d)", retry + 1, max_retry);
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    
    if (esp_sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        ESP_LOGE(TAG, "Failed to sync time via SNTP (timeout)");
        return ESP_ERR_TIMEOUT;
    }
    
    /* Get current time */
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    /* Check if time is valid (after 2020) */
    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGE(TAG, "Time not set correctly");
        return ESP_FAIL;
    }
    
    /* Format time string */
    strftime(out, out_size, "%Y-%m-%d %H:%M:%S %Z (%A)", &timeinfo);
    
    ESP_LOGI(TAG, "System time set successfully: %s", out);
    return ESP_OK;
}

esp_err_t tool_get_time_execute(const char *input_json, char *output, size_t output_size)
{
    ESP_LOGI(TAG, "Fetching current time via SNTP...");
    
    esp_err_t err = sync_time_sntp(output, output_size);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Time: %s", output);
    } else {
        snprintf(output, output_size, "Error: failed to sync time via SNTP (%s)", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", output);
    }
    
    return err;
}
