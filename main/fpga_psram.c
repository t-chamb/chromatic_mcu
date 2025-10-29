#include "fpga_psram.h"
#include "board.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "fpga_psram";

static spi_device_handle_t spi_handle = NULL;

// Static DMA-capable buffers for transactions
// These must remain valid for the entire transaction duration
static uint8_t DMA_ATTR write_dma_buf[1024];
static uint8_t DMA_ATTR read_dma_buf[1024];

// Initialize QSPI for PSRAM access
esp_err_t fpga_psram_init(void)
{
    if (spi_handle != NULL) {
        ESP_LOGI(TAG, "Already initialized");
        return ESP_OK;
    }

    // Create our OWN SPI device handle on the same bus as LVGL
    // This gives us a separate transaction queue, eliminating race conditions!
    // LVGL uses VSPI_HOST with a CS pin, so we must use the SAME CS pin
    // The FPGA decodes based on command bit 10, but CS must still be asserted
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,  // 40MHz - match LVGL speed
        .mode = 0,                            // SPI mode 0
        .spics_io_num = PIN_NUM_QSPI_CS,      // Use same CS pin as LVGL
        .queue_size = 3,                      // Small queue for our transactions
        .flags = SPI_DEVICE_HALFDUPLEX,       // Half-duplex for separate TX/RX
        .cs_ena_pretrans = 3,                 // CS setup time (match LVGL)
        .cs_ena_posttrans = 255,              // CS hold time - max value
        .command_bits = 11,                   // 11-bit command
        .address_bits = 32,                   // 32-bit address
        .dummy_bits = 3,                      // 3 dummy bits
    };

    esp_err_t ret = spi_bus_add_device(VSPI_HOST, &devcfg, &spi_handle);
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
esp_err_t fpga_psram_write(uint32_t address, const uint8_t *data, size_t length)
{
    // Auto-initialize if not already done
    if (spi_handle == NULL) {
        esp_err_t ret = fpga_psram_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    if (length > 1024) {
        ESP_LOGE(TAG, "Length too large: %zu (max 1024)", length);
        return ESP_ERR_INVALID_SIZE;
    }

    // Copy data to static DMA-capable buffer
    // CRITICAL: Must use static buffer to ensure it remains valid during async SPI transfer
    memcpy(write_dma_buf, data, length);

    uint16_t cmd = (length - 1);  // Bit 10 = 0 = WRITE

    ESP_LOGD(TAG, "WRITE: addr=0x%06lX len=%zu cmd=0x%03X",
             (unsigned long)address, length, cmd);

    // Transaction descriptor - Use separate buffer from reads to avoid conflicts
    // Declared static to remain valid during ISR execution
    static spi_transaction_t write_trans;
    memset(&write_trans, 0, sizeof(spi_transaction_t));
    write_trans.tx_buffer = write_dma_buf;
    write_trans.length = length * 8;
    write_trans.flags = SPI_TRANS_MODE_QIO;
    write_trans.cmd = cmd;
    write_trans.addr = address;

    // Queue transaction
    esp_err_t ret = spi_device_queue_trans(spi_handle, &write_trans, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Queue write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for completion - since we have our own device handle,
    // we're GUARANTEED to get back OUR transaction (no LVGL interference!)
    spi_transaction_t *rtrans;
    ret = spi_device_get_trans_result(spi_handle, &rtrans, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write wait failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "Write completed successfully");
    return ESP_OK;
}

// Read from PSRAM via FPGA
esp_err_t fpga_psram_read(uint32_t address, uint8_t *data, size_t length)
{
    // Auto-initialize if not already done
    if (spi_handle == NULL) {
        esp_err_t ret = fpga_psram_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    // FPGA supports variable-length reads from 1 byte to 1024 bytes
    const size_t MAX_SIZE = 1024;

    if (length > MAX_SIZE) {
        ESP_LOGE(TAG, "Length too large: %zu (max %zu)", length, MAX_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    // Read EXACTLY the length requested - must match write length!
    size_t read_length = length;

    uint16_t cmd = 0x400 | (read_length - 1);  // Bit 10 = 1 = READ, bits 9-0 = length-1

    ESP_LOGD(TAG, "READ: addr=0x%06lX req_len=%zu read_len=%zu cmd=0x%03X",
             (unsigned long)address, length, read_length, cmd);

    // Clear DMA buffer before read to avoid stale data
    memset(read_dma_buf, 0xCC, sizeof(read_dma_buf));

    // Transaction descriptor - Use separate buffer from writes to avoid conflicts
    // Declared static to remain valid during ISR execution
    static spi_transaction_t read_trans;
    memset(&read_trans, 0, sizeof(spi_transaction_t));
    read_trans.rx_buffer = read_dma_buf;
    read_trans.length = 0;                   // Half-duplex read: NO TX data
    read_trans.rxlength = read_length * 8;   // RX exact bytes requested
    read_trans.flags = SPI_TRANS_MODE_QIO;
    read_trans.cmd = cmd;
    read_trans.addr = address;

    // Queue transaction
    esp_err_t ret = spi_device_queue_trans(spi_handle, &read_trans, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Queue read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for completion - since we have our own device handle,
    // we're GUARANTEED to get back OUR transaction (no LVGL interference!)
    spi_transaction_t *rtrans;
    ret = spi_device_get_trans_result(spi_handle, &rtrans, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read wait failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Cache sync is not needed for static DMA_ATTR buffers
    // These buffers are in DMA-capable internal RAM, not cached memory

    // Copy requested bytes to caller's buffer
    memcpy(data, read_dma_buf, length);

    // WORKAROUND: FPGA FIFO does not clear when CS goes HIGH
    // Add delay to allow FPGA time to flush FIFO before next operation
    // TODO: Fix FPGA to auto-clear FIFO on CS rising edge
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGD(TAG, "Read completed successfully");
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
