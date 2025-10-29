#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "fpga_psram.h"
#include "board.h"

static const char *TAG = "psram_256";

static int do_psram_256_test(int argc, char **argv)
{
    printf("\n=== 256-Byte PSRAM Test ===\n\n");

    // Pause LVGL to prevent bus contention
    lvgl_pause_updates();

    // Use address 0x220000 (completely different from other tests)
    uint32_t test_addr = 0x220000;

    printf("Test: Write 256-byte pattern at 0x%06lX\n", (unsigned long)test_addr);

    // Allocate DMA-capable buffers
    uint8_t *write_buf = heap_caps_malloc(256, MALLOC_CAP_DMA);
    uint8_t *read_buf = heap_caps_malloc(256, MALLOC_CAP_DMA);

    if (!write_buf || !read_buf) {
        printf("  ✗ Failed to allocate DMA buffers!\n");
        if (write_buf) heap_caps_free(write_buf);
        if (read_buf) heap_caps_free(read_buf);
        return 1;
    }

    printf("DMA buffer addresses: write=%p read=%p\n", (void*)write_buf, (void*)read_buf);

    // Initialize write pattern: ascending bytes 0x00-0xFF
    for (int i = 0; i < 256; i++) {
        write_buf[i] = i;
    }
    memset(read_buf, 0, 256);

    printf("  Writing 256 bytes...\n");
    printf("  First 16 bytes: ");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", write_buf[i]);
    }
    printf("\n");
    printf("  Last 16 bytes:  ");
    for (int i = 240; i < 256; i++) {
        printf("%02X ", write_buf[i]);
    }
    printf("\n");

    printf("  Command will be: 0x%03X (256-1=255=0xFF)\n", 255);

    esp_err_t ret = fpga_psram_write(test_addr, write_buf, 256);
    if (ret != ESP_OK) {
        printf("  ✗ Write failed: %s\n", esp_err_to_name(ret));
        heap_caps_free(write_buf);
        heap_caps_free(read_buf);
        return 1;
    }
    printf("  ✓ Write complete\n");

    // Add a small delay to ensure write completes
    vTaskDelay(pdMS_TO_TICKS(10));

    printf("  Reading 256 bytes...\n");
    ret = fpga_psram_read(test_addr, read_buf, 256);
    if (ret != ESP_OK) {
        printf("  ✗ Read failed: %s\n", esp_err_to_name(ret));
        heap_caps_free(write_buf);
        heap_caps_free(read_buf);
        return 1;
    }
    printf("  ✓ Read complete\n\n");

    printf("  First 16 bytes read: ");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", read_buf[i]);
    }
    printf("\n");

    printf("  Last 16 bytes read:  ");
    for (int i = 240; i < 256; i++) {
        printf("%02X ", read_buf[i]);
    }
    printf("\n\n");

    // Check for matches
    int matches = 0;
    int first_mismatch = -1;
    for (int i = 0; i < 256; i++) {
        if (write_buf[i] == read_buf[i]) {
            matches++;
        } else if (first_mismatch < 0) {
            first_mismatch = i;
        }
    }

    printf("Result: %d / 256 bytes match\n", matches);
    if (matches == 256) {
        printf("✅ PERFECT MATCH - 256-byte PSRAM write/read is working!\n\n");
    } else {
        printf("❌ MISMATCH - First error at byte 0x%02X\n", first_mismatch);
        printf("   Expected: 0x%02X, Got: 0x%02X\n\n",
               write_buf[first_mismatch], read_buf[first_mismatch]);
    }

    heap_caps_free(write_buf);
    heap_caps_free(read_buf);
    lvgl_resume_updates();
    return (matches == 256) ? 0 : 1;
}

void register_psram_256_test(void)
{
    const esp_console_cmd_t cmd = {
        .command = "psram_256",
        .help = "Test 256-byte PSRAM read/write",
        .hint = NULL,
        .func = &do_psram_256_test,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
