# T-Display-S3 Support for MimiClaw

This document outlines the additions made to support the **LilyGO T-Display-S3** hardware in MimiClaw.

## Features Added
- **Display Manager Subsystem**: A new module in `main/display/` to handle LCD initialization and status updates.
- **Boot Status Display**: Shows "Initializing...", "Starting WiFi...", etc., during the boot sequence.
- **Connectivity Status**: Real-time updates for WiFi and Telegram connection states.

## File Changes
1. `main/display/display_manager.h`: Interface for display operations.
2. `main/display/display_manager.c`: Implementation for T-Display-S3 (ST7789 8-bit Parallel).
3. `main/mimi.c`: Integrated display calls into the main lifecycle.
4. `main/CMakeLists.txt`: Added the display module to the build system.

## Hardware Pinout (T-Display-S3)
The following pins are pre-configured in `display_manager.c`:
- **LCD Backlight**: GPIO 38
- **LCD Reset**: GPIO 5
- **LCD CS**: GPIO 6
- **LCD DC**: GPIO 7
- **LCD WR/PCLK**: GPIO 8
- **LCD RD**: GPIO 9
- **Data Bus (8-bit)**: GPIO 39, 40, 41, 42, 45, 46, 47, 48

## How to Complete the Implementation
Since MimiClaw is built on ESP-IDF, you can choose between raw drawing or using a library like **LVGL**.

### 1. Using LVGL (Recommended)
To get a professional UI:
1. Add the `lvgl/lvgl` component to your `components/` directory.
2. Add the `esp_lcd_st7789` component for the driver.
3. In `display_manager_update`, use LVGL objects (labels/icons) to show status.

### 2. Manual Build
Ensure you are using **ESP-IDF v5.5+** as required by the base MimiClaw project.
```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

## Future Improvements
- Add battery level monitoring (T-Display-S3 has a battery ADC on GPIO 4).
- Add button support (GPIO 0 and GPIO 14) to toggle display or trigger voice recording.
