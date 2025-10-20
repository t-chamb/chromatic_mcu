#include "fpga_psram_module.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "fpga_psram_module";

// Use existing FPGA PSRAM functions from main/fpga_psram.c
extern esp_err_t fpga_psram_write(uint32_t addr, const uint8_t *data, size_t len);
extern esp_err_t fpga_psram_read(uint32_t addr, uint8_t *data, size_t len);

static struct {
    bool initialized;
    uint32_t next_free_addr;
} g_fpga_psram = {
    .initialized = false,
    .next_free_addr = FPGA_PSRAM_MODULE_START
};

esp_err_t fpga_psram_module_init(void)
{
    if (g_fpga_psram.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing FPGA PSRAM module loader");
    ESP_LOGI(TAG, "Available PSRAM: %zu bytes (%.2f MB)", 
             FPGA_PSRAM_MODULE_SIZE, 
             FPGA_PSRAM_MODULE_SIZE / (1024.0 * 1024.0));
    ESP_LOGI(TAG, "Module region: 0x%06lX - 0x%06lX", 
             (unsigned long)FPGA_PSRAM_MODULE_START, 
             (unsigned long)FPGA_PSRAM_MODULE_END);

    g_fpga_psram.initialized = true;
    return ESP_OK;
}

// fpga_psram_write and fpga_psram_read are already defined in main/fpga_psram.c
// We just use them directly via extern declarations above

esp_err_t fpga_psram_module_load(const char *filepath, uint32_t psram_addr, module_handle_t *handle)
{
    if (!g_fpga_psram.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (filepath == NULL || handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Loading module from %s to PSRAM 0x%06lX", filepath, (unsigned long)psram_addr);

    // Open file
    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    ESP_LOGI(TAG, "Module size: %zu bytes", file_size);

    // Check if it fits
    if (psram_addr + file_size > FPGA_PSRAM_MODULE_END) {
        ESP_LOGE(TAG, "Module too large for PSRAM");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    // Read and write to PSRAM in chunks
    uint8_t buffer[256];
    size_t offset = 0;
    
    while (offset < file_size) {
        size_t chunk_size = (file_size - offset > sizeof(buffer)) ? 
                           sizeof(buffer) : (file_size - offset);
        
        if (fread(buffer, chunk_size, 1, f) != 1) {
            ESP_LOGE(TAG, "Failed to read file");
            fclose(f);
            return ESP_FAIL;
        }

        esp_err_t ret = fpga_psram_write(psram_addr + offset, buffer, chunk_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write to PSRAM");
            fclose(f);
            return ret;
        }

        offset += chunk_size;
    }

    fclose(f);

    ESP_LOGI(TAG, "Module written to PSRAM successfully");

    // Update next free address
    g_fpga_psram.next_free_addr = psram_addr + file_size;

    // TODO: Create a module handle that references PSRAM
    // For now, return a placeholder
    *handle = (module_handle_t)(intptr_t)1;

    return ESP_OK;
}

size_t fpga_psram_get_free_space(void)
{
    if (!g_fpga_psram.initialized) {
        return 0;
    }

    return FPGA_PSRAM_MODULE_END - g_fpga_psram.next_free_addr + 1;
}
