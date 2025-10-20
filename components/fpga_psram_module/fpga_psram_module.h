#pragma once

#include "esp_err.h"
#include "module_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FPGA PSRAM Module Loader
 * 
 * Uses the 7.8MB of PSRAM accessible via FPGA (0x20000-0x7FFFFF)
 * for module storage and execution.
 */

// PSRAM memory map
#define FPGA_PSRAM_MODULE_START  0x020000  // Start after framebuffers
#define FPGA_PSRAM_MODULE_END    0x7FFFFF  // End of PSRAM
#define FPGA_PSRAM_MODULE_SIZE   (FPGA_PSRAM_MODULE_END - FPGA_PSRAM_MODULE_START + 1)

/**
 * @brief Initialize FPGA PSRAM module loader
 * 
 * @return ESP_OK on success
 */
esp_err_t fpga_psram_module_init(void);

/**
 * @brief Load a module from SD card to FPGA PSRAM
 * 
 * @param filepath Path to module file on SD card
 * @param psram_addr PSRAM address to load to (0x20000+)
 * @param handle Output handle to loaded module
 * @return ESP_OK on success
 */
esp_err_t fpga_psram_module_load(const char *filepath, uint32_t psram_addr, module_handle_t *handle);

// Note: fpga_psram_write() and fpga_psram_read() are defined in main/fpga_psram.c
// and are used directly by this module

/**
 * @brief Get available PSRAM space
 * 
 * @return Number of bytes available
 */
size_t fpga_psram_get_free_space(void);

#ifdef __cplusplus
}
#endif
