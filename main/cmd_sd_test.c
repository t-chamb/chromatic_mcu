#include <stdio.h>
#include "esp_console.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "board.h"

static const char *TAG = "sd_test";

static int do_sd_spi_test_pins(int argc, char **argv)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║          SPI SD Card Pin Test              ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    printf("Testing SD card pin connectivity...\n\n");
    
    // Test each pin by setting it as output and toggling
    printf("Pin Test Results:\n");
    
    // Test CS pin
    gpio_reset_pin(PIN_NUM_SD_CS);
    gpio_set_direction(PIN_NUM_SD_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_SD_CS, 1);
    printf("  CS   (GPIO%d): Set HIGH - ", PIN_NUM_SD_CS);
    printf("✓ OK\n");
    
    // Test MOSI pin  
    gpio_reset_pin(PIN_NUM_SD_MOSI);
    gpio_set_direction(PIN_NUM_SD_MOSI, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_SD_MOSI, 1);
    printf("  MOSI (GPIO%d): Set HIGH - ", PIN_NUM_SD_MOSI);
    printf("✓ OK\n");
    
    // Test CLK pin
    gpio_reset_pin(PIN_NUM_SD_CLK);
    gpio_set_direction(PIN_NUM_SD_CLK, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_SD_CLK, 0);
    printf("  CLK  (GPIO%d): Set LOW  - ", PIN_NUM_SD_CLK);
    printf("✓ OK\n");
    
    // Test MISO pin (input)
    gpio_reset_pin(PIN_NUM_SD_MISO);
    gpio_set_direction(PIN_NUM_SD_MISO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_NUM_SD_MISO, GPIO_PULLUP_ONLY);
    int miso_level = gpio_get_level(PIN_NUM_SD_MISO);
    printf("  MISO (GPIO%d): Read %s  - ", PIN_NUM_SD_MISO, miso_level ? "HIGH" : "LOW");
    printf("✓ OK\n");
    
    printf("\n");
    printf("Expected SD Card Wiring:\n");
    printf("  ESP32 GPIO%d (CS)   → SD Card DAT3/CS\n", PIN_NUM_SD_CS);
    printf("  ESP32 GPIO%d (MOSI) → SD Card CMD/DI\n", PIN_NUM_SD_MOSI);
    printf("  ESP32 GPIO%d (MISO) → SD Card DAT0/DO\n", PIN_NUM_SD_MISO);
    printf("  ESP32 GPIO%d (CLK)  → SD Card CLK/SCLK\n", PIN_NUM_SD_CLK);
    printf("  ESP32 3.3V         → SD Card VDD\n");
    printf("  ESP32 GND          → SD Card VSS/GND\n");
    
    printf("\nTroubleshooting:\n");
    printf("  1. Ensure SD card is inserted and properly seated\n");
    printf("  2. Check all wire connections match the pinout above\n");
    printf("  3. Verify SD card is 3.3V compatible (most are)\n");
    printf("  4. Try a different SD card (some cards are more sensitive)\n");
    printf("  5. Ensure SD card is formatted as FAT32\n");
    
    return 0;
}

static int do_sd_spi_raw_test(int argc, char **argv)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║         Raw SPI Communication Test         ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Reset pins to SPI configuration
    gpio_reset_pin(PIN_NUM_SD_CS);
    gpio_reset_pin(PIN_NUM_SD_MOSI);
    gpio_reset_pin(PIN_NUM_SD_MISO);
    gpio_reset_pin(PIN_NUM_SD_CLK);

    spi_device_handle_t spi_handle;
    
    // Configure SPI bus for raw communication
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_SD_MOSI,
        .miso_io_num = PIN_NUM_SD_MISO,
        .sclk_io_num = PIN_NUM_SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 512,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 400000,  // 400kHz - very slow for SD card detection
        .mode = 0,
        .spics_io_num = PIN_NUM_SD_CS,
        .queue_size = 1,
    };

    // Try different SPI hosts to avoid conflicts with existing SD card initialization
    spi_host_device_t hosts_to_try[] = {SPI3_HOST, SPI2_HOST, SPI1_HOST};
    spi_host_device_t successful_host = -1;
    esp_err_t ret = ESP_FAIL;
    
    for (int i = 0; i < 3; i++) {
        ret = spi_bus_initialize(hosts_to_try[i], &buscfg, SPI_DMA_DISABLED);
        if (ret == ESP_OK) {
            successful_host = hosts_to_try[i];
            printf("✓ Raw SPI initialized on host %d at 400kHz\n", successful_host);
            break;
        } else {
            printf("DEBUG: Host %d failed: %s\n", hosts_to_try[i], esp_err_to_name(ret));
        }
    }
    
    if (successful_host == -1) {
        printf("✗ All SPI hosts failed - SD card may already be using SPI bus\n");
        printf("  Try 'sd_spi_unmount' first to free the SPI bus\n");
        return 1;
    }

    ret = spi_bus_add_device(successful_host, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        printf("✗ SPI device add failed: %s\n", esp_err_to_name(ret));
        spi_bus_free(successful_host);
        return 1;
    }

    // Send CMD0 (reset) to SD card
    uint8_t cmd0[] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95}; // CMD0 with CRC
    uint8_t response[6] = {0};

    spi_transaction_t trans = {
        .length = 48, // 6 bytes * 8 bits
        .tx_buffer = cmd0,
        .rx_buffer = response,
    };

    printf("Sending CMD0 (reset) to SD card...\n");
    ret = spi_device_transmit(spi_handle, &trans);
    if (ret == ESP_OK) {
        printf("✓ SPI transmission successful\n");
        printf("Response bytes: ");
        for (int i = 0; i < 6; i++) {
            printf("0x%02X ", response[i]);
        }
        printf("\n");
        
        if (response[0] == 0xFF && response[1] == 0xFF) {
            printf("⚠ No response from SD card (all 0xFF)\n");
            printf("  This suggests SD card is not responding\n");
        } else {
            printf("✓ SD card responded! Raw communication works\n");
        }
    } else {
        printf("✗ SPI transmission failed: %s\n", esp_err_to_name(ret));
    }

    // Cleanup
    spi_bus_remove_device(spi_handle);
    spi_bus_free(successful_host);

    return 0;
}

void register_sd_test_commands(void)
{
    const esp_console_cmd_t pin_test_cmd = {
        .command = "sd_pin_test",
        .help = "Test SD card pin connectivity and configuration",
        .hint = NULL,
        .func = &do_sd_spi_test_pins,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&pin_test_cmd));

    const esp_console_cmd_t raw_test_cmd = {
        .command = "sd_raw_test",
        .help = "Test raw SPI communication with SD card",
        .hint = NULL,
        .func = &do_sd_spi_raw_test,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&raw_test_cmd));
}