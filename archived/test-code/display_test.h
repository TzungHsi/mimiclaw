#ifndef DISPLAY_TEST_H
#define DISPLAY_TEST_H

#include "esp_err.h"

/**
 * @brief Initialize LCD for testing
 * 
 * @return ESP_OK on success
 */
esp_err_t display_test_init(void);

/**
 * @brief Run LCD test sequence
 * 
 * This will display various test patterns to verify LCD functionality:
 * - Solid colors (red, green, blue, white, black)
 * - Horizontal stripes
 * - Vertical stripes
 * - Checkerboard pattern
 * - Gradient
 */
void display_test_run(void);

#endif // DISPLAY_TEST_H
