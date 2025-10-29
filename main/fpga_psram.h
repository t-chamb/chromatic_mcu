#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

// PSRAM Memory Map (8MB total)
#define PSRAM_OSD_FB_ADDR       0x00000   // OSD Framebuffer (64KB)
#define PSRAM_OSD_FB_SIZE       0x10000
#define PSRAM_GB_FB_ADDR        0x10000   // GB Framebuffer (64KB)
#define PSRAM_GB_FB_SIZE        0x10000
#define PSRAM_MODULE_HEAP_ADDR  0x20000   // Module Heap (1.875MB)
#define PSRAM_MODULE_HEAP_SIZE  0x1E0000
#define PSRAM_ASSET_ADDR        0x200000  // Asset Storage (6MB)
#define PSRAM_ASSET_SIZE        0x600000

// Initialize FPGA PSRAM interface
esp_err_t fpga_psram_init(void);

// Write data to PSRAM via FPGA
// Max length per call: 1023 bytes
esp_err_t fpga_psram_write(uint32_t address, const uint8_t *data, size_t length);

// Read data from PSRAM via FPGA
// Max length per call: 1023 bytes
esp_err_t fpga_psram_read(uint32_t address, uint8_t *data, size_t length);

// Burst write (handles chunking automatically)
esp_err_t fpga_psram_write_burst(uint32_t address, const uint8_t *data, size_t total_length);

// Burst read (handles chunking automatically)
esp_err_t fpga_psram_read_burst(uint32_t address, uint8_t *data, size_t total_length);

// Get PSRAM stats
typedef struct {
    size_t total_size;
    size_t module_heap_free;
    size_t asset_storage_used;
} fpga_psram_stats_t;

esp_err_t fpga_psram_get_stats(fpga_psram_stats_t *stats);
