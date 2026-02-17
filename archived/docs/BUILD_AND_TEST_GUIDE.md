# Build and Test Guide

## Prerequisites

### Required Software
- **ESP-IDF v5.5.2** (or compatible version)
- **Git** for version control
- **Python 3.8+** for ESP-IDF tools
- **USB Driver** for ESP32-S3 (CP210x or CH340)

### Hardware
- **LilyGO T-Display-S3** board
- **USB-C cable** for programming and power
- **Computer** with Windows, Linux, or macOS

## Step 1: Clone the Repository

```bash
git clone https://github.com/TzungHsi/mimiclaw.git
cd mimiclaw
git checkout feature/four-grid-ui
```

## Step 2: Set Up ESP-IDF Environment

### Windows (PowerShell)
```powershell
# Navigate to ESP-IDF installation directory
cd C:\Espressif\frameworks\esp-idf-v5.5.2

# Run export script
.\export.ps1

# Navigate back to project
cd path\to\mimiclaw
```

### Linux/macOS
```bash
# Source ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Navigate to project
cd ~/mimiclaw
```

## Step 3: Configure Project

```bash
# Open menuconfig (optional, if you need to change settings)
idf.py menuconfig

# Key settings to verify:
# - Component config -> ESP32S3-Specific -> Support for external, SPI-connected RAM
# - Serial flasher config -> Flash size (should be 16MB)
```

## Step 4: Build the Project

```bash
# Clean previous build (if any)
idf.py fullclean

# Build the project
idf.py build
```

**Expected Output:**
```
Project build complete. To flash, run:
 idf.py -p (PORT) flash
or run:
 idf.py -p (PORT) flash monitor
```

## Step 5: Flash to Device

### Find COM Port
**Windows:**
```powershell
# Check Device Manager -> Ports (COM & LPT)
# Look for "Silicon Labs CP210x" or "USB-SERIAL CH340"
# Example: COM3
```

**Linux:**
```bash
ls /dev/ttyUSB*
# Example: /dev/ttyUSB0
```

**macOS:**
```bash
ls /dev/cu.usbserial-*
# Example: /dev/cu.usbserial-1420
```

### Flash Command
```bash
# Replace COM3 with your actual port
idf.py -p COM3 flash

# Or flash and monitor in one command
idf.py -p COM3 flash monitor
```

**To enter download mode manually (if needed):**
1. Hold **Boot** button (GPIO 0)
2. Press and release **Reset** button
3. Release **Boot** button
4. Run flash command

## Step 6: Monitor Serial Output

```bash
idf.py -p COM3 monitor

# To exit monitor: Ctrl+]
```

**Expected Serial Output:**
```
========================================
  MimiClaw - ESP32-S3 AI Agent
========================================
Internal free: XXXXX bytes
PSRAM free:    XXXXX bytes
I (XXX) display: Initializing T-Display-S3 Display (ST7789 8-bit Parallel)...
I (XXX) button: Initializing buttons (Boot: GPIO0, User: GPIO14)
I (XXX) display: Display initialized successfully
I (XXX) button: Buttons initialized
...
```

## Step 7: Test Button Functions

### Test 1: Boot Button Short Press (Display Mode Cycling)
1. Short press **Boot** button
2. **Expected**: Display changes from Grid → Detail → Large IP → Grid
3. **Check**: Serial log shows "Display mode switched to: X"

### Test 2: Boot Button Long Press (Backlight Toggle)
1. Hold **Boot** button for 2 seconds
2. **Expected**: Backlight turns off
3. Hold **Boot** button for 2 seconds again
4. **Expected**: Backlight turns on
5. **Check**: Serial log shows "Backlight: ON/OFF"

### Test 3: User Button Short Press (Manual Refresh)
1. Short press **User** button
2. **Expected**: Display refreshes immediately
3. **Check**: Serial log shows "Manual refresh triggered"

### Test 4: User Button Long Press (WiFi Restart)
1. Hold **User** button for 2 seconds
2. **Expected**: Serial log shows "WiFi restart triggered"
3. **Note**: WiFi restart is not yet implemented (placeholder)

## Step 8: Verify Display Modes

### Mode 1: Four-Grid Dashboard (Default)
**What to check:**
- [ ] Four cells visible (WiFi, Telegram, System, Memory)
- [ ] WiFi indicator is green when connected, red when disconnected
- [ ] Telegram indicator is green when active
- [ ] System indicator shows green for "Ready"
- [ ] Memory shows percentage (e.g., "45%")

### Mode 2: Detailed Values
**What to check:**
- [ ] WiFi RSSI displayed (e.g., "-50dBm")
- [ ] IP address displayed correctly
- [ ] Uptime displayed in hours and minutes
- [ ] Memory usage displayed in KB
- [ ] System state displayed (e.g., "Ready")

### Mode 3: Large IP Display
**What to check:**
- [ ] "IP Address:" label visible
- [ ] IP address displayed in large text
- [ ] Text is readable and centered

## Step 9: Test WiFi Connection

### Check WiFi Status
1. Ensure WiFi credentials are set in `main/mimi_secrets.h`:
   ```c
   #define MIMI_SECRET_WIFI_SSID "YourSSID"
   #define MIMI_SECRET_WIFI_PASSWORD "YourPassword"
   ```
2. Rebuild and flash if credentials were changed
3. **Expected**: WiFi indicator turns green after connection
4. **Check**: Serial log shows "WiFi connected: X.X.X.X"

### Verify IP Display
1. Switch to Mode 3 (Large IP Display)
2. **Expected**: Device's IP address is displayed
3. Verify IP matches the one shown in serial log

## Step 10: Test Power Consumption

### Measure Current Draw
1. Use USB power meter or multimeter
2. **Expected values** (approximate):
   - Backlight ON, WiFi connected: ~150-200mA
   - Backlight OFF, WiFi connected: ~100-150mA
   - Backlight ON, WiFi disconnected: ~100-120mA

### Test on Power Bank
1. Connect to USB-A power bank
2. **Expected**: Device runs without issues
3. Display should be stable and responsive

## Troubleshooting

### Build Errors

**Error: "driver" component not found**
- **Solution**: Ensure `main/CMakeLists.txt` includes `REQUIRES driver`

**Error: "esp_lcd_panel_io.h: No such file"**
- **Solution**: Update ESP-IDF to v5.5.2 or later

**Error: "heap_caps_malloc undefined"**
- **Solution**: Add `#include "esp_heap_caps.h"` to display_manager.c

### Flash Errors

**Error: "Failed to connect to ESP32-S3"**
- **Solution**: Enter download mode manually (see Step 5)

**Error: "Serial port COM3 not found"**
- **Solution**: Check Device Manager, install USB drivers

### Display Issues

**Issue: Display shows nothing (black screen)**
- **Check**: Backlight is on (long press Boot button)
- **Check**: Serial log for display initialization errors
- **Solution**: Verify LCD pin definitions match hardware

**Issue: Display shows "snow" or random pixels**
- **Check**: ST7789 initialization parameters
- **Solution**: Verify color invert, swap_xy, mirror, and gap settings

**Issue: Text is unreadable**
- **Note**: Current implementation uses placeholder font
- **Solution**: Implement proper bitmap font library (future enhancement)

### Button Issues

**Issue: Buttons not responding**
- **Check**: Serial log for button events
- **Check**: GPIO configuration (pull-up enabled?)
- **Solution**: Verify button_poll() is being called every 10ms

**Issue: False button triggers**
- **Solution**: Increase debounce time in `button_driver.h`

**Issue: Long press not working**
- **Check**: Button is held for full 2 seconds
- **Solution**: Adjust BUTTON_LONG_PRESS_MS if needed

## Performance Verification

### Memory Usage
Check serial log for memory statistics:
```
Internal free: XXXXX bytes
PSRAM free:    XXXXX bytes
```

**Expected:**
- Internal RAM usage: ~100-150KB for framebuffer + system
- PSRAM available for application data

### Task Performance
Monitor task execution in serial log:
```
I (XXX) button: Button task started
I (XXX) display: Display update task started
```

**Expected:**
- Button task runs smoothly at 10ms interval
- Display update task runs every 5 seconds
- No task watchdog timeouts

## Next Steps After Testing

1. **If everything works:**
   - Document any issues or observations
   - Test for extended period (24+ hours)
   - Measure actual power consumption
   - Consider merging to main branch

2. **If issues found:**
   - Document specific problems
   - Check serial logs for errors
   - Review code for potential fixes
   - Report issues on GitHub

3. **Future enhancements:**
   - Implement proper font rendering
   - Add WiFi RSSI reading
   - Implement WiFi restart function
   - Add icons and animations

## Support

For issues or questions:
- Check serial logs for error messages
- Review documentation in `DISPLAY_UI_DESIGN.md` and `BUTTON_CONTROLS.md`
- Open an issue on GitHub: https://github.com/TzungHsi/mimiclaw/issues

---

**Good luck with testing!** 🚀
