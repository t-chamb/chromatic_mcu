#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "esp_console.h"
#include "esp_log.h"
#include "esp_err.h"
#include "sd_spi.h"


static int do_sd_spi_init(int argc, char **argv)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║        SPI SD Card Initialization          ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    printf("This command will:\n");
    printf("  1. Initialize SPI bus (tries SPI3_HOST, fallback HSPI_HOST)\n");
    printf("  2. Configure SD card in SPI mode\n");
    printf("  3. Mount FAT32 filesystem\n");
    printf("  4. Display card information\n\n");

    // Initialize SPI
    printf("═══ Step 1: Initialize SPI Bus ═══\n");
    esp_err_t ret = sd_spi_init();
    if (ret != ESP_OK) {
        printf("✗ SPI initialization failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    printf("✓ SPI bus initialized successfully\n\n");

    // Mount SD card
    printf("═══ Step 2: Mount SD Card ═══\n");
    printf("DEBUG: Attempting to mount SD card at /sdcard...\n");
    printf("DEBUG: This may take several seconds for card detection...\n");
    ret = sd_spi_mount("/sdcard");
    if (ret != ESP_OK) {
        printf("✗ SD card mount failed: %s\n", esp_err_to_name(ret));
        printf("DEBUG: Mount failure details:\n");
        if (ret == ESP_ERR_TIMEOUT) {
            printf("  - SD card not responding (check if card is inserted)\n");
            printf("  - Try a different SD card\n");
            printf("  - Check if card needs slower SPI speed\n");
        } else {
            printf("  - Error code: 0x%x\n", ret);
        }
        printf("General checks:\n");
        printf("  - SD card is inserted properly\n");
        printf("  - Card is formatted as FAT32\n");
        printf("  - Wiring connections are correct\n");
        return 1;
    }
    printf("✓ SD card mounted at /sdcard\n\n");

    printf("╔════════════════════════════════════════════╗\n");
    printf("║          ✅ SUCCESS! SD CARD READY!         ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    printf("Next steps:\n");
    printf("  1. Run: sd_spi_test (test read/write)\n");
    printf("  2. Run: sd_spi_info (detailed card info)\n");
    printf("  3. Run: sd_spi_unmount (safely remove)\n\n");

    return 0;
}

static int do_sd_spi_test(int argc, char **argv)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║           SPI SD Card Test                  ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    if (!sd_spi_is_mounted()) {
        printf("✗ SD card not mounted. Run 'sd_spi_init' first.\n");
        return 1;
    }

    const char *test_file = "/sdcard/test.txt";
    const char *test_content = "ESP32 SPI SD Card Test - Hello World!";

    printf("═══ Step 1: Write Test File ═══\n");
    FILE *f = fopen(test_file, "w");
    if (f == NULL) {
        printf("✗ Failed to open file for writing\n");
        return 1;
    }

    fprintf(f, "%s\n", test_content);
    fclose(f);
    printf("✓ Written: %s\n", test_content);

    printf("\n═══ Step 2: Read Test File ═══\n");
    f = fopen(test_file, "r");
    if (f == NULL) {
        printf("✗ Failed to open file for reading\n");
        return 1;
    }

    char line[128];
    if (fgets(line, sizeof(line), f) != NULL) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        printf("✓ Read: %s\n", line);
        
        if (strcmp(line, test_content) == 0) {
            printf("✓ Content matches!\n");
        } else {
            printf("✗ Content mismatch!\n");
        }
    } else {
        printf("✗ Failed to read file content\n");
    }
    fclose(f);

    printf("\n═══ Step 3: File System Info ═══\n");
    struct stat st;
    if (stat(test_file, &st) == 0) {
        printf("✓ File size: %ld bytes\n", st.st_size);
    }

    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║           ✅ TEST COMPLETED!                ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    return 0;
}

static int do_sd_spi_info(int argc, char **argv)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║           SPI SD Card Info                  ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    if (!sd_spi_is_mounted()) {
        printf("✗ SD card not mounted. Run 'sd_spi_init' first.\n");
        return 1;
    }

    sdmmc_card_t *card = sd_spi_get_card_info();
    if (card == NULL) {
        printf("✗ Cannot get card information\n");
        return 1;
    }

    printf("Card Information:\n");
    printf("  Name: %s\n", card->cid.name);
    printf("  Type: %s\n", (card->ocr & (1<<30)) ? "SDHC/SDXC" : "SDSC");
    printf("  Speed: %s\n", (card->csd.tr_speed > 25000000) ? "High Speed" : "Default Speed");
    printf("  Capacity: %llu MB\n", ((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    printf("  Sector Size: %d bytes\n", card->csd.sector_size);
    printf("  Frequency: %d kHz\n", card->real_freq_khz);

    return 0;
}

static int do_sd_spi_unmount(int argc, char **argv)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║         SPI SD Card Unmount                 ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    if (!sd_spi_is_mounted()) {
        printf("✓ SD card already unmounted\n");
        return 0;
    }

    esp_err_t ret = sd_spi_unmount();
    if (ret != ESP_OK) {
        printf("✗ Failed to unmount: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("✓ SD card unmounted successfully\n");
    printf("✓ Safe to remove card\n\n");

    return 0;
}

static int do_sd_spi_init_alt(int argc, char **argv)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║    SPI SD Card Init (ESP32 Built-in Pins)  ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    printf("This command will:\n");
    printf("  1. Force use ESP32 built-in SD pins (GPIO 6,7,8,11)\n");
    printf("  2. Initialize SPI bus\n");
    printf("  3. Mount FAT32 filesystem\n");
    printf("  4. Display card information\n\n");

    // First unmount if already mounted
    if (sd_spi_is_mounted()) {
        printf("═══ Step 0: Unmount Current SD Card ═══\n");
        esp_err_t ret = sd_spi_unmount();
        if (ret != ESP_OK) {
            printf("✗ Failed to unmount: %s\n", esp_err_to_name(ret));
            return 1;
        }
        printf("✓ Previous SD card unmounted\n\n");
    }

    // Initialize SPI with alternative pins
    printf("═══ Step 1: Initialize SPI Bus (Built-in Pins) ═══\n");
    esp_err_t ret = sd_spi_init_alt_pins();
    if (ret != ESP_OK) {
        printf("✗ SPI initialization with built-in pins failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    printf("✓ SPI bus initialized successfully with built-in pins\n\n");

    // Mount SD card
    printf("═══ Step 2: Mount SD Card ═══\n");
    printf("DEBUG: Attempting to mount SD card at /sdcard...\n");
    printf("DEBUG: Using ESP32 built-in SD pins\n");
    ret = sd_spi_mount("/sdcard");
    if (ret != ESP_OK) {
        printf("✗ SD card mount failed: %s\n", esp_err_to_name(ret));
        printf("DEBUG: Mount failure with built-in pins\n");
        if (ret == ESP_ERR_TIMEOUT) {
            printf("  - SD card not responding\n");
            printf("  - Check if card is properly wired to built-in pins:\n");
            printf("    ESP32 GPIO11 (CS)   → SD Card DAT3/CS\n");
            printf("    ESP32 GPIO8 (MOSI)  → SD Card CMD/DI\n");
            printf("    ESP32 GPIO7 (MISO)  → SD Card DAT0/DO\n");
            printf("    ESP32 GPIO6 (CLK)   → SD Card CLK/SCLK\n");
        } else {
            printf("  - Error code: 0x%x\n", ret);
        }
        return 1;
    }

    printf("✓ SD card mounted successfully!\n\n");

    // Show card info if available
    printf("═══ Step 3: SD Card Information ═══\n");
    sdmmc_card_t* card = sd_spi_get_card_info();
    if (card) {
        printf("✓ Card Name: %s\n", card->cid.name);
        printf("✓ Card Type: %s\n", (card->ocr & (1<<30)) ? "SDHC/SDXC" : "SDSC");
        printf("✓ Card Speed: %s\n", (card->csd.tr_speed > 25000000) ? "high speed" : "default speed");
        printf("✓ Card Size: %llu MB\n", ((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    }

    printf("\n✓ SD card ready for use at /sdcard\n");
    return 0;
}

void register_sd_spi_commands(void)
{
    const esp_console_cmd_t init_cmd = {
        .command = "sd_spi_init",
        .help = "Initialize SPI SD card and mount FAT32 filesystem",
        .hint = NULL,
        .func = &do_sd_spi_init,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&init_cmd));

    const esp_console_cmd_t test_cmd = {
        .command = "sd_spi_test", 
        .help = "Test SPI SD card read/write functionality",
        .hint = NULL,
        .func = &do_sd_spi_test,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_cmd));

    const esp_console_cmd_t info_cmd = {
        .command = "sd_spi_info",
        .help = "Display SPI SD card information",
        .hint = NULL,
        .func = &do_sd_spi_info,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&info_cmd));

    const esp_console_cmd_t unmount_cmd = {
        .command = "sd_spi_unmount",
        .help = "Safely unmount SPI SD card",
        .hint = NULL,
        .func = &do_sd_spi_unmount,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&unmount_cmd));

    const esp_console_cmd_t init_alt_cmd = {
        .command = "sd_spi_init_alt",
        .help = "Initialize SPI SD card using ESP32 built-in pins (GPIO 6,7,8,11)",
        .hint = NULL,
        .func = &do_sd_spi_init_alt,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&init_alt_cmd));
}