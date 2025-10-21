#include "fpga_psram.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "fpga_psram";

// QSPI pins from board.h
#define QSPI_CS   GPIO_NUM_5
#define QSPI_CLK  GPIO_NUM_18
#define QSPI_MOSI GPIO_NUM_23
#define QSPI_MISO GPIO_NUM_19
#define QSPI_WP   GPIO_NUM_22
#define QSPI_HD   GPIO_NUM_21

static spi_device_handle_t spi_handle = NULL;

// Initialize QSPI for PSRAM access
esp_err_t fpga_psram_init(void)
{
    if (spi_handle != NULL) {
        ESP_LOGI(TAG, "Already initialized");
        return ESP_OK;
    }

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = QSPI_MOSI,
        .miso_io_num = QSPI_MISO,
        .sclk_io_num = QSPI_CLK,
        .quadwp_io_num = QSPI_WP,
        .quadhd_io_num = QSPI_HD,
        .max_transfer_sz = 4096,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_QUAD
    };

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 40 * 1000 * 1000,  // 40 MHz
        .mode = 0,
        .spics_io_num = QSPI_CS,
        .queue_size = 3,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .command_bits = 11,   // [Cmd:1][Length:10]
        .address_bits = 32,   // 32-bit address
        .dummy_bits = 0       // NO dummy bits (LVGL has 3 but causes corruption)
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to init SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "FPGA PSRAM interface initialized (8MB @ 40MHz QSPI)");
    ESP_LOGI(TAG, "  OSD FB:      0x%06X - 0x%06X (%d KB)", 
             PSRAM_OSD_FB_ADDR, PSRAM_OSD_FB_ADDR + PSRAM_OSD_FB_SIZE - 1, 
             PSRAM_OSD_FB_SIZE / 1024);
    ESP_LOGI(TAG, "  GB FB:       0x%06X - 0x%06X (%d KB)", 
             PSRAM_GB_FB_ADDR, PSRAM_GB_FB_ADDR + PSRAM_GB_FB_SIZE - 1, 
             PSRAM_GB_FB_SIZE / 1024);
    ESP_LOGI(TAG, "  Module Heap: 0x%06X - 0x%06X (%d KB)", 
             PSRAM_MODULE_HEAP_ADDR, PSRAM_MODULE_HEAP_ADDR + PSRAM_MODULE_HEAP_SIZE - 1, 
             PSRAM_MODULE_HEAP_SIZE / 1024);
    ESP_LOGI(TAG, "  Assets:      0x%06X - 0x%06X (%d KB)", 
             PSRAM_ASSET_ADDR, PSRAM_ASSET_ADDR + PSRAM_ASSET_SIZE - 1, 
             PSRAM_ASSET_SIZE / 1024);
    
    return ESP_OK;
}

// Write to PSRAM via FPGA
// Uses EXACT same format as LVGL send_lines()
esp_err_t fpga_psram_write(uint32_t address, const uint8_t *data, size_t length)
{
    if (spi_handle == NULL) {
        ESP_LOGE(TAG, "PSRAM not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (length > 1024) {
        ESP_LOGE(TAG, "Length too large: %zu (max 1024)", length);
        return ESP_ERR_INVALID_SIZE;
    }

    // EXACT same format as LVGL: 0x400 | 1023
    // This is [Cmd:1][Length:10] where Cmd=1 (write), Length=1023
    uint16_t cmd = 0x400 | 1023;
    
    spi_transaction_t trans = {
        .flags = SPI_TRANS_MODE_QIO,
        .cmd = cmd,
        .addr = address,
        .length = length * 8,
        .tx_buffer = data
    };

    esp_err_t ret = spi_device_transmit(spi_handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// Read from PSRAM via FPGA
esp_err_t fpga_psram_read(uint32_t address, uint8_t *data, size_t length)
{
    if (spi_handle == NULL) {
        ESP_LOGE(TAG, "PSRAM not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (length > 1024) {
        ESP_LOGE(TAG, "Length too large: %zu (max 1024)", length);
        return ESP_ERR_INVALID_SIZE;
    }

    // Read command: Cmd=0 (read), Length=1023
    uint16_t cmd = 1023;  // No write bit
    
    spi_transaction_t trans = {
        .flags = SPI_TRANS_MODE_QIO,
        .cmd = cmd,
        .addr = address,
        .length = 0,
        .rxlength = length * 8,
        .rx_buffer = data
    };

    esp_err_t ret = spi_device_transmit(spi_handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// Burst write for large transfers
esp_err_t fpga_psram_write_burst(uint32_t address, const uint8_t *data, size_t total_length)
{
    size_t offset = 0;
    
    while (offset < total_length) {
        size_t chunk = (total_length - offset > 1024) ? 1024 : (total_length - offset);
        
        esp_err_t ret = fpga_psram_write(address + offset, &data[offset], chunk);
        if (ret != ESP_OK) {
            return ret;
        }
        
        offset += chunk;
    }
    
    return ESP_OK;
}

// Burst read for large transfers
esp_err_t fpga_psram_read_burst(uint32_t address, uint8_t *data, size_t total_length)
{
    size_t offset = 0;
    
    while (offset < total_length) {
        size_t chunk = (total_length - offset > 1024) ? 1024 : (total_length - offset);
        
        esp_err_t ret = fpga_psram_read(address + offset, &data[offset], chunk);
        if (ret != ESP_OK) {
            return ret;
        }
        
        offset += chunk;
    }
    
    return ESP_OK;
}

// Get PSRAM stats
esp_err_t fpga_psram_get_stats(fpga_psram_stats_t *stats)
{
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    stats->total_size = 8 * 1024 * 1024;  // 8MB
    stats->module_heap_free = PSRAM_MODULE_HEAP_SIZE;  // TODO: track allocations
    stats->asset_storage_used = 0;  // TODO: track usage

    return ESP_OK;
}
