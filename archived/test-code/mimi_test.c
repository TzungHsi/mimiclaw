#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "display/display_test.h"

static const char *TAG = "mimi_test";

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  T-Display-S3 LCD Basic Test");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize LCD
    ESP_LOGI(TAG, "Initializing LCD...");
    ret = display_test_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD initialization failed!");
        return;
    }
    
    ESP_LOGI(TAG, "LCD initialized successfully!");
    ESP_LOGI(TAG, "");
    
    // Run test loop
    while (1) {
        display_test_run();
    }
}
