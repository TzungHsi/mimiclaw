#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// Button GPIO definitions
#define BUTTON_BOOT_GPIO    0   // Boot button (GPIO 0)
#define BUTTON_USER_GPIO    14  // User button (GPIO 14)

// Button timing definitions (in milliseconds)
#define BUTTON_DEBOUNCE_MS      50      // Debounce time
#define BUTTON_LONG_PRESS_MS    2000    // Long press threshold

// Button event types
typedef enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_BOOT_SHORT,
    BUTTON_EVENT_BOOT_LONG,
    BUTTON_EVENT_USER_SHORT,
    BUTTON_EVENT_USER_LONG
} button_event_t;

/**
 * @brief Initialize button driver
 * 
 * Sets up GPIO pins with internal pull-up resistors
 */
void button_init(void);

/**
 * @brief Poll buttons and detect events
 * 
 * Should be called periodically (e.g., every 10ms)
 * 
 * @return button_event_t The detected button event
 */
button_event_t button_poll(void);

#endif // BUTTON_DRIVER_H
