#include "button_driver.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "button";

// Button state tracking
typedef struct {
    uint8_t gpio;
    bool last_state;        // Last stable state (true = pressed)
    bool current_state;     // Current raw state
    int64_t press_time;     // Time when button was pressed
    bool long_press_fired;  // Flag to prevent repeated long press events
} button_state_t;

static button_state_t boot_button = {.gpio = BUTTON_BOOT_GPIO};
static button_state_t user_button = {.gpio = BUTTON_USER_GPIO};

void button_init(void)
{
    ESP_LOGI(TAG, "Initializing buttons (Boot: GPIO%d, User: GPIO%d)", 
             BUTTON_BOOT_GPIO, BUTTON_USER_GPIO);

    // Configure Boot button
    gpio_config_t boot_cfg = {
        .pin_bit_mask = (1ULL << BUTTON_BOOT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&boot_cfg);

    // Configure User button
    gpio_config_t user_cfg = {
        .pin_bit_mask = (1ULL << BUTTON_USER_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&user_cfg);

    // Initialize button states
    boot_button.last_state = false;
    boot_button.current_state = false;
    boot_button.press_time = 0;
    boot_button.long_press_fired = false;

    user_button.last_state = false;
    user_button.current_state = false;
    user_button.press_time = 0;
    user_button.long_press_fired = false;

    ESP_LOGI(TAG, "Buttons initialized");
}

static button_event_t check_button(button_state_t *btn, button_event_t short_event, button_event_t long_event)
{
    // Read current GPIO state (LOW = pressed, HIGH = released)
    int level = gpio_get_level(btn->gpio);
    bool pressed = (level == 0);
    
    int64_t now = esp_timer_get_time() / 1000; // Convert to milliseconds
    
    // Detect press (transition from released to pressed)
    if (pressed && !btn->last_state) {
        btn->press_time = now;
        btn->long_press_fired = false;
        btn->current_state = true;
    }
    
    // Detect release (transition from pressed to released)
    if (!pressed && btn->last_state) {
        int64_t press_duration = now - btn->press_time;
        btn->current_state = false;
        
        // Only fire short press if long press wasn't already fired
        if (!btn->long_press_fired && press_duration < BUTTON_LONG_PRESS_MS) {
            btn->last_state = false;
            ESP_LOGI(TAG, "Button GPIO%d: SHORT press (%lld ms)", btn->gpio, press_duration);
            return short_event;
        }
        
        btn->last_state = false;
        return BUTTON_EVENT_NONE;
    }
    
    // Check for long press (button still held down)
    if (pressed && btn->current_state && !btn->long_press_fired) {
        int64_t press_duration = now - btn->press_time;
        
        if (press_duration >= BUTTON_LONG_PRESS_MS) {
            btn->long_press_fired = true;
            ESP_LOGI(TAG, "Button GPIO%d: LONG press (%lld ms)", btn->gpio, press_duration);
            return long_event;
        }
    }
    
    btn->last_state = pressed;
    return BUTTON_EVENT_NONE;
}

button_event_t button_poll(void)
{
    // Check boot button first
    button_event_t event = check_button(&boot_button, BUTTON_EVENT_BOOT_SHORT, BUTTON_EVENT_BOOT_LONG);
    if (event != BUTTON_EVENT_NONE) {
        return event;
    }
    
    // Check user button
    event = check_button(&user_button, BUTTON_EVENT_USER_SHORT, BUTTON_EVENT_USER_LONG);
    return event;
}
