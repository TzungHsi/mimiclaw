# T-Display-S3 Button Controls

## Button Layout

The T-Display-S3 has two physical buttons:

```
┌─────────────────────┐
│                     │
│   T-Display-S3      │
│                     │
│   [Boot]  [User]    │ ← Buttons at bottom
│    GPIO0   GPIO14   │
└─────────────────────┘
```

## Button Functions

### Boot Button (GPIO 0)

| Press Type | Duration | Function |
|------------|----------|----------|
| **Short Press** | < 2s | Cycle through display modes |
| **Long Press** | ≥ 2s | Toggle backlight on/off |

**Display Mode Cycle:**
1. Four-Grid Dashboard (default)
2. Detailed Values
3. Large IP Display
4. (back to Four-Grid Dashboard)

### User Button (GPIO 14)

| Press Type | Duration | Function |
|------------|----------|----------|
| **Short Press** | < 2s | Manual refresh display |
| **Long Press** | ≥ 2s | Restart WiFi connection |

## Usage Examples

### Checking System Status
1. Look at the default Four-Grid Dashboard
2. Green circles = everything OK
3. Red circles = problem detected

### Viewing Detailed Information
1. Short press **Boot** button once
2. Screen shows detailed metrics (WiFi RSSI, uptime, memory)
3. Short press **Boot** again to return to grid view

### Finding IP Address
1. Short press **Boot** button twice
2. Screen shows large IP address
3. Short press **Boot** again to return to grid view

### Saving Power
1. Long press **Boot** button (hold for 2 seconds)
2. Backlight turns off
3. Long press **Boot** again to turn backlight back on

### Refreshing Display
1. Short press **User** button
2. Display immediately updates with latest status

### Fixing WiFi Connection
1. Long press **User** button (hold for 2 seconds)
2. WiFi connection restarts
3. Wait for WiFi to reconnect (watch WiFi indicator)

## Technical Details

### Button Hardware
- Both buttons are active-low (pressed = LOW, released = HIGH)
- Internal pull-up resistors are enabled
- No external components required

### Debouncing
- Software debounce time: 50ms
- Prevents false triggers from mechanical bounce

### Long Press Detection
- Long press threshold: 2000ms (2 seconds)
- Long press event fires immediately when threshold is reached
- Short press event fires on button release (if < 2s)

### Button Polling
- Buttons are polled every 10ms
- Non-blocking design (no interrupts)
- Low CPU overhead

## Implementation

### Button Driver Files
- `main/button_driver.h` - API definitions
- `main/button_driver.c` - Implementation with debouncing

### Integration
- Button task runs on Core 1 at priority 5
- Calls `button_poll()` every 10ms
- Handles events immediately

### Example Code

```c
#include "button_driver.h"

// Initialize buttons
button_init();

// In main loop (every 10ms)
button_event_t event = button_poll();

switch (event) {
    case BUTTON_EVENT_BOOT_SHORT:
        // Cycle display mode
        break;
    case BUTTON_EVENT_BOOT_LONG:
        // Toggle backlight
        break;
    case BUTTON_EVENT_USER_SHORT:
        // Refresh display
        break;
    case BUTTON_EVENT_USER_LONG:
        // Restart WiFi
        break;
    default:
        // No event
        break;
}
```

## Troubleshooting

### Button Not Responding
- Check GPIO configuration (pull-up enabled?)
- Verify button_poll() is being called regularly
- Check serial logs for button events

### False Triggers
- Increase debounce time in `button_driver.h`
- Check for electrical noise on GPIO pins

### Long Press Not Working
- Ensure button is held for full 2 seconds
- Check BUTTON_LONG_PRESS_MS definition

### Display Not Updating
- Verify display_manager is initialized
- Check display update task is running
- Try manual refresh (short press User button)

## Safety Notes

⚠️ **Boot Button Warning**: GPIO 0 is also used for entering download mode. To enter download mode:
1. Hold Boot button
2. Press and release Reset button
3. Release Boot button

This is normal ESP32 behavior and will not damage the device.

## Future Improvements

Potential enhancements for button controls:

1. **Double-Click Detection**: Add double-click support for more functions
2. **Button Combinations**: Press both buttons simultaneously for special functions
3. **Haptic Feedback**: Add buzzer feedback on button press
4. **Configurable Actions**: Allow users to customize button functions
5. **Button Disable**: Option to disable buttons to prevent accidental presses
