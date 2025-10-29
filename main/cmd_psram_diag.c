#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "esp_log.h"
#include "fpga_psram.h"
#include "board.h"

static const char *TAG = "psram_diag";

// FPGA Debug Register at 0xFF00
#define FPGA_DEBUG_REG_ADDR 0xFF00

// Read FPGA debug register
static void read_fpga_debug(const char *label)
{
    uint8_t debug[4] = {0};
    esp_err_t ret = fpga_psram_read(FPGA_DEBUG_REG_ADDR, debug, 4);
    if (ret == ESP_OK) {
        printf("  [DEBUG %s] 0xFF00: %02X %02X %02X %02X\n",
               label, debug[0], debug[1], debug[2], debug[3]);
    } else {
        printf("  [DEBUG %s] Failed to read debug register\n", label);
    }
}

// Diagnostic command to help FPGA team debug read issues
static int do_psram_diag(int argc, char **argv)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║   FPGA PSRAM Read Diagnostics              ║\n");
    printf("║   For FPGA Team Debug                      ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Pause LVGL to prevent bus contention
    lvgl_pause_updates();

    printf("=== FPGA Debug Register Check ===\n");
    read_fpga_debug("INITIAL");
    printf("\n");

    // Test 1: Write ascending bytes 0x00-0xFF, read back
    printf("=== Test 1: Ascending Byte Pattern (0x00-0xFF) ===\n");
    uint32_t test_addr = 0x200000;  // Asset Storage - KNOWN WORKING

    // Write ascending pattern
    uint8_t write_pattern[256];
    for (int i = 0; i < 256; i++) {
        write_pattern[i] = i;
    }

    printf("Writing 256 bytes: 00 01 02 03 04 05 ... FD FE FF\n");
    printf("Address: 0x%06lX\n", (unsigned long)test_addr);

    read_fpga_debug("BEFORE_WRITE");
    esp_err_t ret = fpga_psram_write(test_addr, write_pattern, 256);
    if (ret != ESP_OK) {
        printf("❌ Write failed!\n\n");
        lvgl_resume_updates();
        return 1;
    }
    read_fpga_debug("AFTER_WRITE");
    printf("✓ Write complete\n\n");

    // Read back
    uint8_t read_pattern[256] = {0};
    read_fpga_debug("BEFORE_READ");
    ret = fpga_psram_read(test_addr, read_pattern, 256);
    if (ret != ESP_OK) {
        printf("❌ Read failed!\n\n");
        lvgl_resume_updates();
        return 1;
    }
    read_fpga_debug("AFTER_READ");
    printf("✓ Read complete\n\n");

    // Show detailed comparison
    printf("Offset | Wrote | Read  | Match? | Notes\n");
    printf("-------|-------|-------|--------|------------------\n");

    int mismatches = 0;
    int first_mismatch = -1;

    for (int i = 0; i < 256; i += 4) {
        for (int j = 0; j < 4 && (i+j) < 256; j++) {
            int idx = i + j;
            uint8_t wrote = write_pattern[idx];
            uint8_t read = read_pattern[idx];
            bool match = (wrote == read);

            if (!match) {
                mismatches++;
                if (first_mismatch < 0) first_mismatch = idx;
            }

            // Only print first 32 and any mismatches
            if (idx < 32 || !match) {
                printf("  0x%02X | 0x%02X | 0x%02X | %s   | ",
                       idx, wrote, read, match ? "✓" : "✗");

                if (!match) {
                    // Analyze the mismatch
                    if (read == wrote + 1) printf("Off by +1");
                    else if (read == wrote - 1) printf("Off by -1");
                    else if ((read & 0xF0) == (wrote & 0x0F) << 4) printf("Nibble swap?");
                    else if (read == (wrote >> 4)) printf("Upper nibble only?");
                    else if (read == (wrote & 0x0F)) printf("Lower nibble only?");
                    else printf("Pattern: 0x%02X", read);
                }
                printf("\n");
            }
        }

        if (i == 28 && mismatches > 8) {
            printf("  ... (showing only mismatches after this) ...\n");
        }
    }

    printf("\n");
    printf("Total mismatches: %d / 256 bytes\n", mismatches);

    if (mismatches == 0) {
        printf("✅ PERFECT MATCH - Read/Write working correctly!\n\n");
    } else if (first_mismatch >= 0) {
        printf("\nFirst mismatch at offset 0x%02X:\n", first_mismatch);
        printf("  Expected: 0x%02X\n", write_pattern[first_mismatch]);
        printf("  Got:      0x%02X\n\n", read_pattern[first_mismatch]);
    }

    // Analyze the pattern
    printf("Bit Analysis:\n");
    for (int i = 0; i < 8; i++) {
        uint8_t wrote = write_pattern[i];
        uint8_t read = read_pattern[i];
        printf("  [%d] Wrote 0x%02X (", i, wrote);
        for (int b = 7; b >= 0; b--) {
            printf("%d", (wrote >> b) & 1);
        }
        printf(") Read 0x%02X (", read);
        for (int b = 7; b >= 0; b--) {
            printf("%d", (read >> b) & 1);
        }
        printf(")\n");
    }
    printf("\n");

    // Test 2: Different read lengths to find timing issues
    printf("=== Test 2: Read Length Analysis (Consecutive Reads) ===\n");
    printf("NOTE: This tests FIFO clearing between consecutive reads\n\n");
    uint8_t lengths[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};

    for (int i = 0; i < sizeof(lengths); i++) {
        uint8_t len = lengths[i];
        uint8_t buf[256] = {0};

        if (i == 0) read_fpga_debug("BEFORE_1ST_READ");
        ret = fpga_psram_read(test_addr, buf, len);
        if (i == 0) read_fpga_debug("AFTER_1ST_READ");

        if (ret == ESP_OK) {
            printf("  Length %3d: First 4 bytes = %02X %02X %02X %02X",
                   len, buf[0], buf[1], buf[2], buf[3]);

            // Check if this looks like stale FIFO data
            if (buf[0] == 0x0E || buf[0] == 0x0D) {
                printf(" <-- STALE FIFO?");
            } else if (buf[0] == 0x00 && buf[1] == 0x01) {
                printf(" <-- GOOD");
            }
            printf("\n");
        }
    }
    read_fpga_debug("AFTER_ALL_READS");
    printf("\n");

    // Test 3: Different addresses - find the working boundary
    printf("=== Test 3: Address Analysis ===\n");
    uint32_t addrs[] = {0x000000, 0x010000, 0x020000, 0x030000, 0x100000,
                        0x1F0000, 0x1FFFF0, 0x200000, 0x300000, 0x400000};

    for (int i = 0; i < sizeof(addrs)/sizeof(addrs[0]); i++) {
        uint8_t buf[4] = {0};
        ret = fpga_psram_read(addrs[i], buf, 4);
        if (ret == ESP_OK) {
            printf("  Addr 0x%06lX: %02X %02X %02X %02X\n",
                   (unsigned long)addrs[i], buf[0], buf[1], buf[2], buf[3]);
        }
    }
    printf("\n");

    // Test 4: Write/Read alternating pattern
    printf("=== Test 4: Alternating Pattern (0xAA / 0x55) ===\n");
    uint8_t alt_pattern[8];
    for (int i = 0; i < 8; i++) {
        alt_pattern[i] = (i % 2) ? 0x55 : 0xAA;
    }

    printf("Writing: ");
    for (int i = 0; i < 8; i++) printf("%02X ", alt_pattern[i]);
    printf("\n");

    fpga_psram_write(test_addr, alt_pattern, 8);

    uint8_t alt_read[8] = {0};
    fpga_psram_read(test_addr, alt_read, 8);

    printf("Read:    ");
    for (int i = 0; i < 8; i++) printf("%02X ", alt_read[i]);
    printf("\n\n");

    // Test 5: All zeros, all ones
    printf("=== Test 5: All Zeros / All Ones ===\n");
    uint8_t zeros[4] = {0x00, 0x00, 0x00, 0x00};
    uint8_t ones[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t buf[4];

    fpga_psram_write(test_addr, zeros, 4);
    fpga_psram_read(test_addr, buf, 4);
    printf("Wrote all 0x00, Read: %02X %02X %02X %02X\n", buf[0], buf[1], buf[2], buf[3]);

    fpga_psram_write(test_addr, ones, 4);
    fpga_psram_read(test_addr, buf, 4);
    printf("Wrote all 0xFF, Read: %02X %02X %02X %02X\n", buf[0], buf[1], buf[2], buf[3]);
    printf("\n");

    // Test 6: Write/Read test pattern to multiple regions
    printf("=== Test 6: Memory Region Write/Read Test ===\n");
    uint8_t test_pattern[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    uint8_t read_buf[16];

    uint32_t test_addrs[] = {0x020000, 0x030000, 0x100000, 0x1FFFF0, 0x200000, 0x300000};
    const char* region_names[] = {"Module Heap Start", "Module Heap (0x30000)",
                                  "Module Heap Mid", "Module Heap End",
                                  "Asset Storage Start", "Asset Storage Mid"};

    for (int i = 0; i < sizeof(test_addrs)/sizeof(test_addrs[0]); i++) {
        memset(read_buf, 0, 16);

        fpga_psram_write(test_addrs[i], test_pattern, 16);
        fpga_psram_read(test_addrs[i], read_buf, 16);

        bool match = (memcmp(test_pattern, read_buf, 16) == 0);

        printf("  0x%06lX (%s): %s\n",
               (unsigned long)test_addrs[i], region_names[i],
               match ? "✅ PASS" : "❌ FAIL");

        if (!match) {
            printf("    Expected: %02X %02X %02X %02X...\n",
                   test_pattern[0], test_pattern[1], test_pattern[2], test_pattern[3]);
            printf("    Got:      %02X %02X %02X %02X...\n",
                   read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
        }
    }
    printf("\n");

    // Summary for FPGA team
    printf("╔════════════════════════════════════════════╗\n");
    printf("║   CLUES FOR FPGA TEAM:                     ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    printf("1. Check if the read pattern is ALWAYS the same\n");
    printf("   - If YES: FPGA not driving data, or read module not connected\n");
    printf("   - If NO: FPGA IS responding, but data is shifted/corrupted\n\n");

    printf("2. The consistent 0xEE EE EE pattern suggests:\n");
    printf("   - FPGA IS driving the pins (not floating at 0xFF)\n");
    printf("   - But data is NOT from PSRAM\n");
    printf("   - Check: Is mm_burst_read module instantiated in top.v?\n");
    printf("   - Check: Is qCommand=0 (read) triggering PSRAM read?\n\n");

    printf("3. ESP32 is sending READ commands correctly:\n");
    printf("   - Cmd bit = 0 (read)\n");
    printf("   - Address in QIO mode\n");
    printf("   - Dummy cycles = 3 bits\n");
    printf("   - Data phase expecting QIO input\n\n");

    printf("4. Next FPGA debug steps:\n");
    printf("   - Add ILA probe on qCommand signal\n");
    printf("   - Verify qCommand=0 triggers read request\n");
    printf("   - Check if mm_burst_read.qDataOut has valid data\n");
    printf("   - Verify QSPI pins switch to output during read\n\n");

    lvgl_resume_updates();
    return 0;
}

void register_psram_diag(void)
{
    const esp_console_cmd_t cmd = {
        .command = "psram_diag",
        .help = "PSRAM diagnostic tests for FPGA team",
        .hint = NULL,
        .func = &do_psram_diag,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
