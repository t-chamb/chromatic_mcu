#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_console.h"
#include "esp_log.h"
#include "fpga_psram.h"

static int do_psram_test(int argc, char **argv)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║   FPGA PSRAM Test                          ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Initialize
    printf("Step 1: Initializing FPGA PSRAM interface...\n");
    esp_err_t ret = fpga_psram_init();
    if (ret != ESP_OK) {
        printf("  ✗ Init failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    printf("  ✓ Initialized\n\n");

    // Test pattern
    uint8_t write_data[256];
    uint8_t read_data[256];
    
    for (int i = 0; i < 256; i++) {
        write_data[i] = i;
    }

    // Write test to module heap area
    uint32_t test_addr = PSRAM_MODULE_HEAP_ADDR + 0x1000;
    printf("Step 2: Writing 256 bytes to address 0x%06lX...\n", (unsigned long)test_addr);
    ret = fpga_psram_write(test_addr, write_data, 256);
    if (ret != ESP_OK) {
        printf("  ✗ Write failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    printf("  ✓ Write complete\n\n");

    // Read test
    printf("Step 3: Reading 256 bytes from address 0x%06lX...\n", (unsigned long)test_addr);
    memset(read_data, 0, 256);
    ret = fpga_psram_read(test_addr, read_data, 256);
    if (ret != ESP_OK) {
        printf("  ✗ Read failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    printf("  ✓ Read complete\n\n");

    // Verify
    printf("Step 4: Verifying data...\n");
    int errors = 0;
    for (int i = 0; i < 256; i++) {
        if (write_data[i] != read_data[i]) {
            printf("  ✗ Mismatch at offset %d: wrote 0x%02X, read 0x%02X\n",
                   i, write_data[i], read_data[i]);
            errors++;
            if (errors >= 10) {
                printf("  ... (stopping after 10 errors)\n");
                break;
            }
        }
    }

    if (errors == 0) {
        printf("  ✓ All data verified!\n\n");
        
        // Test burst write/read
        printf("Step 5: Testing burst transfer (2KB)...\n");
        uint8_t *large_buf = malloc(2048);
        uint8_t *read_buf = malloc(2048);
        
        if (large_buf && read_buf) {
            for (int i = 0; i < 2048; i++) {
                large_buf[i] = (i & 0xFF);
            }
            
            ret = fpga_psram_write_burst(test_addr, large_buf, 2048);
            if (ret == ESP_OK) {
                ret = fpga_psram_read_burst(test_addr, read_buf, 2048);
                if (ret == ESP_OK) {
                    if (memcmp(large_buf, read_buf, 2048) == 0) {
                        printf("  ✓ Burst transfer verified!\n\n");
                    } else {
                        printf("  ✗ Burst data mismatch\n\n");
                        errors++;
                    }
                } else {
                    printf("  ✗ Burst read failed\n\n");
                    errors++;
                }
            } else {
                printf("  ✗ Burst write failed\n\n");
                errors++;
            }
            
            free(large_buf);
            free(read_buf);
        }
        
        if (errors == 0) {
            printf("✅ PSRAM test PASSED\n\n");
            return 0;
        }
    }
    
    printf("\n❌ PSRAM test FAILED (%d errors)\n\n", errors);
    return 1;
}

static int do_psram_stats(int argc, char **argv)
{
    fpga_psram_stats_t stats;
    esp_err_t ret = fpga_psram_get_stats(&stats);
    
    if (ret != ESP_OK) {
        printf("Failed to get stats: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    printf("\nFPGA PSRAM Statistics:\n");
    printf("  Total Size:        %zu MB\n", stats.total_size / (1024 * 1024));
    printf("  Module Heap Free:  %zu KB\n", stats.module_heap_free / 1024);
    printf("  Asset Storage Used: %zu KB\n\n", stats.asset_storage_used / 1024);
    
    return 0;
}

void register_psram_commands(void)
{
    const esp_console_cmd_t test_cmd = {
        .command = "psram_test",
        .help = "Test FPGA PSRAM read/write",
        .hint = NULL,
        .func = &do_psram_test,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_cmd));
    
    const esp_console_cmd_t stats_cmd = {
        .command = "psram_stats",
        .help = "Show FPGA PSRAM statistics",
        .hint = NULL,
        .func = &do_psram_stats,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&stats_cmd));
}
