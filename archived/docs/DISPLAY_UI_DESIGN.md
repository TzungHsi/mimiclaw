# T-Display-S3 UI Design

## Overview

This document describes the user interface design for the T-Display-S3 screen on the MimiClaw ESP32-S3 AI Agent.

## Hardware

- **Display**: LilyGO T-Display-S3
- **Screen**: 170x320 ST7789 LCD (8-bit parallel interface)
- **Buttons**:
  - Boot Button: GPIO 0
  - User Button: GPIO 14

## Display Modes

The UI supports three display modes that can be cycled through using the Boot button:

### Mode 1: Four-Grid Dashboard (Default)

A 2x2 grid layout showing system status at a glance:

```
┌──────────────┬──────────────┐
│    WiFi      │   Telegram   │
│     🟢       │      🟢      │
│  Connected   │    Active    │
├──────────────┼──────────────┤
│   System     │    Memory    │
│     🟢       │     45%      │
│    Ready     │   Used       │
└──────────────┴──────────────┘
```

**Status Indicators:**
- 🟢 Green: OK/Connected
- 🟡 Yellow: Warning
- 🔴 Red: Error/Disconnected

**Grid Contents:**
1. **WiFi** (Top-Left): Connection status with colored circle
2. **Telegram** (Top-Right): Bot connection status with colored circle
3. **System** (Bottom-Left): Overall system state with colored circle
4. **Memory** (Bottom-Right): Memory usage percentage with colored circle

### Mode 2: Detailed Values

Shows detailed system metrics in text format:

```
WiFi: -45dBm
IP: 192.168.1.100
Uptime: 2h 34m
Mem: 128/256 KB
State: Ready
```

**Displayed Information:**
- WiFi signal strength (RSSI in dBm)
- IP address
- System uptime (hours and minutes)
- Memory usage (free/total in KB)
- System state description

### Mode 3: Large IP Display

Shows the IP address in large text for easy reading:

```
    IP Address:
  192.168.1.100
```

Useful for quickly identifying the device on the network.

## Button Functions

| Button | Action | Function |
|--------|--------|----------|
| **Boot (GPIO 0)** | Short Press | Cycle display mode (Grid → Detail → Large IP → Grid) |
| **Boot (GPIO 0)** | Long Press (2s) | Toggle backlight on/off |
| **User (GPIO 14)** | Short Press | Manual refresh (update display immediately) |
| **User (GPIO 14)** | Long Press (2s) | Restart WiFi connection |

## Button Timing

- **Debounce Time**: 50ms
- **Long Press Threshold**: 2000ms (2 seconds)

## Power Optimization

The display driver uses a lightweight approach without LVGL to minimize power consumption:

- Single framebuffer in DMA-capable memory
- Manual refresh on status changes (no continuous refresh)
- Low-frequency updates (every 5 seconds by default)
- Backlight can be turned off to save power

## Color Scheme

- **Background**: Black (0x0000)
- **Text**: White (0xFFFF)
- **Status OK**: Green (0x07E0)
- **Status Warning**: Yellow (0xFFE0)
- **Status Error**: Red (0xF800)
- **Grid Lines**: Gray (0x8410)

## Implementation Details

### Display Manager API

```c
// Initialize display
esp_err_t display_manager_init(void);

// Update display with status
void display_manager_update(const display_status_t *status);

// Set display mode
void display_manager_set_mode(display_mode_t mode);

// Get current mode
display_mode_t display_manager_get_mode(void);

// Toggle backlight
void display_manager_toggle_backlight(void);

// Force refresh
void display_manager_refresh(void);
```

### Button Driver API

```c
// Initialize buttons
void button_init(void);

// Poll buttons (call every 10ms)
button_event_t button_poll(void);
```

### Status Structure

```c
typedef struct {
    bool wifi_connected;
    int8_t wifi_rssi;
    char ip_address[16];
    bool telegram_connected;
    uint32_t uptime_seconds;
    uint32_t free_heap;
    uint32_t total_heap;
    const char *system_state;
} display_status_t;
```

## Future Enhancements

Potential improvements for future versions:

1. **Better Font Rendering**: Implement a proper font library for clearer text
2. **Icons**: Add WiFi signal strength icons and Telegram logo
3. **Animations**: Smooth transitions between display modes
4. **QR Code**: Display QR code for easy connection to web interface
5. **Graphs**: Show memory usage or WiFi signal strength over time
6. **Custom Themes**: Allow user-selectable color schemes
7. **Touch Support**: For T-Display-S3-Touch variant

## References

- [LilyGO T-Display-S3 GitHub](https://github.com/Xinyuan-LilyGO/T-Display-S3)
- [ESP-IDF LCD Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html)
- [ST7789 Datasheet](https://www.displayfuture.com/Display/datasheet/controller/ST7789.pdf)
