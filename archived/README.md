# Archived Files

This directory contains files that are no longer actively used in the project but are preserved for historical reference.

## 📁 Directory Structure

### `docs/`
Development documentation created during the implementation of T-Display-S3 support and four-grid UI:

- **BUILD_AND_TEST_GUIDE.md** - Build and test guide for the project
- **BUTTON_CONTROLS.md** - Button control documentation
- **DISPLAY_UI_DESIGN.md** - Display UI design specifications
- **IMPLEMENTATION_SUMMARY.md** - Implementation summary and progress
- **LCD_TEST_GUIDE.md** - LCD testing guide for T-Display-S3
- **T-DISPLAY-S3-SUPPORT.md** - T-Display-S3 hardware support documentation

### `test-code/`
Test code used during LCD and display development:

- **mimi_test.c** - LCD basic test main program
- **CMakeLists.txt.test** - CMake configuration for test build
- **display_test.c** - LCD display test implementation
- **display_test.h** - LCD display test header

## 📝 Notes

These files were moved to `archived/` on 2026-02-17 after the successful completion of:
- Four-grid dashboard UI implementation
- T-Display-S3 screen support
- Cron scheduling system integration
- SNTP time synchronization

The current project uses:
- `display_manager.c/h` - Production display manager
- `display_ui.c/h` - Four-grid dashboard UI
- `FOUR_GRID_UI.md` - Current UI documentation

## 🔄 Restoration

If you need to restore any of these files, you can:

```bash
# Restore a specific file
git mv archived/docs/BUILD_AND_TEST_GUIDE.md .

# Or restore test code
git mv archived/test-code/mimi_test.c main/
```

---

**Archived by**: Manus AI Agent  
**Date**: 2026-02-17
