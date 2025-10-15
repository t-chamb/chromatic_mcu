#include "sd_spi.h"
#include "board.h"
#include "esp_log.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include <string.h>

static const char *TAG = "sd_spi";
static sd_spi_context_t g_sd_ctx = {0};
static spi_host_device_t g_spi_host = 2; // Track which SPI host is being used (numeric for compatibility)
static bool g_using_alt_pins = false; // Track if we're using alternative pin configuration

esp_err_t sd_spi_init(void)
{
    esp_err_t ret = ESP_OK;

    if (g_sd_ctx.mounted) {
        ESP_LOGW(TAG, "SD card already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SPI SD card");
    ESP_LOGI(TAG, "Pin mapping: CS=%d, MOSI=%d, MISO=%d, CLK=%d", 
             PIN_NUM_SD_CS, PIN_NUM_SD_MOSI, PIN_NUM_SD_MISO, PIN_NUM_SD_CLK);
    printf("DEBUG: Initializing SPI SD card\n");
    printf("DEBUG: Pin mapping: CS=%d, MOSI=%d, MISO=%d, CLK=%d\n", 
           PIN_NUM_SD_CS, PIN_NUM_SD_MOSI, PIN_NUM_SD_MISO, PIN_NUM_SD_CLK);

    // Reset pins from any previous SDMMC configuration
    ESP_LOGI(TAG, "Resetting pins from any previous configuration...");
    printf("DEBUG: Resetting pins from any previous configuration...\n");
    gpio_reset_pin(PIN_NUM_SD_CS);
    gpio_reset_pin(PIN_NUM_SD_MOSI); 
    gpio_reset_pin(PIN_NUM_SD_MISO);
    gpio_reset_pin(PIN_NUM_SD_CLK);
    
    // Power cycle SD card if power control is available
    #ifdef PIN_NUM_SD_POWER
    printf("DEBUG: Power cycling SD card...\n");
    gpio_reset_pin(PIN_NUM_SD_POWER);
    gpio_set_direction(PIN_NUM_SD_POWER, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_SD_POWER, 0);  // Power OFF
    vTaskDelay(pdMS_TO_TICKS(100));       // Wait 100ms
    gpio_set_level(PIN_NUM_SD_POWER, 1);  // Power ON
    vTaskDelay(pdMS_TO_TICKS(250));       // Wait 250ms for power stabilization
    printf("DEBUG: SD card power cycle complete\n");
    #else
    // Software reset sequence for always-powered SD cards
    printf("DEBUG: Performing software reset sequence...\n");
    
    // Set CS high for extended period to reset SD card state
    gpio_set_direction(PIN_NUM_SD_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_SD_CS, 1);  // CS HIGH (inactive)
    vTaskDelay(pdMS_TO_TICKS(100));    // Hold CS high for 100ms
    
    // Set MOSI high and pulse CLK to send dummy clocks
    gpio_set_direction(PIN_NUM_SD_MOSI, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_SD_CLK, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_SD_MOSI, 1);  // MOSI HIGH
    
    // Send 80+ dummy clock cycles (SD card reset requirement)
    for (int i = 0; i < 100; i++) {
        gpio_set_level(PIN_NUM_SD_CLK, 0);
        esp_rom_delay_us(10);  // 10us low
        gpio_set_level(PIN_NUM_SD_CLK, 1);
        esp_rom_delay_us(10);  // 10us high
    }
    
    gpio_set_level(PIN_NUM_SD_CLK, 0);  // Leave CLK low
    printf("DEBUG: Software reset sequence complete\n");
    #endif

    // Initialize SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_SD_MOSI,
        .miso_io_num = PIN_NUM_SD_MISO,
        .sclk_io_num = PIN_NUM_SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    // ESP32 SPI host mapping: SPI1_HOST=0, SPI2_HOST=1, SPI3_HOST=2
    // But VSPI_HOST=1 and HSPI_HOST=2 in older ESP-IDF versions
    printf("DEBUG: ESP32 SPI host mapping: SPI1_HOST=0(flash), SPI2_HOST=1(VSPI/FPGA), SPI3_HOST=2(HSPI)\n");
    
    // Note: Cannot safely free SPI buses that may have devices attached
    printf("DEBUG: Skipping SPI bus cleanup to avoid device conflicts...\n");
    
    // The issue might be that ESP32 only has 3 SPI hosts total: SPI0 (flash), SPI1, SPI2
    // and VSPI_HOST might already occupy one of them. Let's try different approach.
    
    // Try hosts systematically - Espressif recommends DMA for SD cards
    int hosts_to_try[] = {SDSPI_DEFAULT_HOST, 1, 2};
    int successful_host = -1;
    
    // First try with DMA (Espressif recommendation)
    for (int i = 0; i < 3; i++) {
        int host = hosts_to_try[i];
        printf("DEBUG: Trying spi_host_device_t value %d with DMA...\n", host);
        ret = spi_bus_initialize(host, &bus_cfg, SPI_DMA_CH_AUTO);
        if (ret == ESP_OK) {
            successful_host = host;
            printf("DEBUG: SUCCESS! Host %d initialized with DMA\n", host);
            break;
        } else {
            printf("DEBUG: Host %d with DMA failed: %s\n", host, esp_err_to_name(ret));
        }
    }
    
    // Fallback to no DMA if DMA fails
    if (successful_host == -1) {
        printf("DEBUG: Falling back to no DMA...\n");
        for (int i = 0; i < 3; i++) {
            int host = hosts_to_try[i];
            printf("DEBUG: Trying spi_host_device_t value %d without DMA...\n", host);
            ret = spi_bus_initialize(host, &bus_cfg, SPI_DMA_DISABLED);
            if (ret == ESP_OK) {
                successful_host = host;
                printf("DEBUG: SUCCESS! Host %d initialized without DMA\n", host);
                break;
            } else {
                printf("DEBUG: Host %d without DMA failed: %s\n", host, esp_err_to_name(ret));
            }
        }
    }
    
    if (successful_host == -1) {
        // Try alternate pins with all hosts
        bus_cfg.mosi_io_num = PIN_NUM_SD_MOSI_ALT;
        bus_cfg.miso_io_num = PIN_NUM_SD_MISO_ALT;
        bus_cfg.sclk_io_num = PIN_NUM_SD_CLK_ALT;
        
        printf("DEBUG: Trying alternate pins: MOSI=%d, MISO=%d, CLK=%d, CS=%d\n", 
               PIN_NUM_SD_MOSI_ALT, PIN_NUM_SD_MISO_ALT, PIN_NUM_SD_CLK_ALT, PIN_NUM_SD_CS_ALT);
        
        for (int i = 0; i < 3; i++) {
            int host = hosts_to_try[i];
            printf("DEBUG: Trying host %d with alternate pins...\n", host);
            ret = spi_bus_initialize(host, &bus_cfg, SPI_DMA_DISABLED);
            if (ret == ESP_OK) {
                successful_host = host;
                g_using_alt_pins = true;
                printf("DEBUG: SUCCESS! Host %d with alternate pins\n", host);
                break;
            } else {
                printf("DEBUG: Host %d with alt pins failed: %s\n", host, esp_err_to_name(ret));
            }
        }
        
        if (successful_host == -1) {
            printf("DEBUG: All hosts failed with both pin configurations\n");
            printf("DEBUG: This suggests SPI hardware conflict or limitation\n");
            return ESP_ERR_NOT_FOUND;
        }
    }

    g_spi_host = successful_host;
    printf("DEBUG: Final success: Host %d, Alt pins: %s\n", 
           successful_host, g_using_alt_pins ? "YES" : "NO");
    return ESP_OK;
}

esp_err_t sd_spi_mount(const char *mount_point)
{
    esp_err_t ret = ESP_OK;

    if (g_sd_ctx.mounted) {
        ESP_LOGW(TAG, "SD card already mounted at %s", g_sd_ctx.mount_point);
        return ESP_OK;
    }

    if (!mount_point) {
        mount_point = "/sdcard";
    }

    ESP_LOGI(TAG, "Mounting SD card at %s", mount_point);

    // Configure SD card host with slower, more reliable settings
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = g_spi_host;
    host.max_freq_khz = 400;  // Very slow for troubleshooting (400kHz)
    
    ESP_LOGI(TAG, "Using SPI host %d with frequency %d kHz", g_spi_host, host.max_freq_khz);

    // Configure SPI device
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = g_using_alt_pins ? PIN_NUM_SD_CS_ALT : PIN_NUM_SD_CS;
    slot_config.host_id = host.slot;
    
    ESP_LOGI(TAG, "Using CS pin: %d", slot_config.gpio_cs);

    // Configure FAT filesystem with Espressif recommended settings
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,   // Don't auto-format (safer)
        .max_files = 5,                    // Reasonable number of open files
        .allocation_unit_size = 0,         // Use default allocation unit
        .disk_status_check_enable = true   // Enable status checks for reliability
    };
    
    ESP_LOGI(TAG, "Attempting to mount with retry logic...");

    // Mount filesystem with retry logic and progressive speed reduction
    const int max_retries = 3;
    const int retry_delay_ms = 1000;
    int frequencies[] = {400, 200, 100}; // Progressive speed reduction
    
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        // Adjust frequency for each attempt
        host.max_freq_khz = frequencies[attempt - 1];
        printf("DEBUG: Mount attempt %d/%d at %d kHz...\n", attempt, max_retries, host.max_freq_khz);
        
        ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &g_sd_ctx.card);
        
        if (ret == ESP_OK) {
            printf("DEBUG: Mount successful on attempt %d\n", attempt);
            break;
        }
        
        // Log the specific error
        if (ret == ESP_ERR_TIMEOUT) {
            printf("DEBUG: Attempt %d failed with ESP_ERR_TIMEOUT\n", attempt);
        } else if (ret == ESP_FAIL) {
            printf("DEBUG: Attempt %d failed with ESP_FAIL (filesystem/format issue)\n", attempt);
        } else {
            printf("DEBUG: Attempt %d failed with %s\n", attempt, esp_err_to_name(ret));
        }
        
        // Don't retry on filesystem errors (wrong format)
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. Make sure SD card is formatted as FAT32");
            break;
        }
        
        // Delay before retry (except on last attempt)
        if (attempt < max_retries) {
            printf("DEBUG: Waiting %d ms before retry...\n", retry_delay_ms);
            vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
        }
    }
    
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "SD card not responding after %d attempts", max_retries);
            printf("Hardware troubleshooting needed:\n");
            printf("  1. Check SD card is inserted properly\n");
            printf("  2. Add 10kΩ pull-up resistor: GPIO4 → 3.3V\n");
            printf("  3. Add 10kΩ pull-ups to SD card DAT1 and DAT2\n");
            printf("  4. Try a different SD card\n");
            printf("  5. Check all wire connections\n");
        } else if (ret != ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to initialize SD card after %d attempts: %s", max_retries, esp_err_to_name(ret));
        }
        return ret;
    }

    // Store mount info
    g_sd_ctx.mounted = true;
    strncpy(g_sd_ctx.mount_point, mount_point, sizeof(g_sd_ctx.mount_point) - 1);
    g_sd_ctx.mount_point[sizeof(g_sd_ctx.mount_point) - 1] = '\0';

    // Print card info
    ESP_LOGI(TAG, "SD card mounted successfully!");
    ESP_LOGI(TAG, "Card info:");
    ESP_LOGI(TAG, "  Name: %s", g_sd_ctx.card->cid.name);
    ESP_LOGI(TAG, "  Type: %s", (g_sd_ctx.card->ocr & (1<<30)) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI(TAG, "  Speed: %s", (g_sd_ctx.card->csd.tr_speed > 25000000) ? "high speed" : "default speed");
    ESP_LOGI(TAG, "  Size: %llu MB", ((uint64_t) g_sd_ctx.card->csd.capacity) * g_sd_ctx.card->csd.sector_size / (1024 * 1024));

    return ESP_OK;
}

esp_err_t sd_spi_unmount(void)
{
    if (!g_sd_ctx.mounted) {
        ESP_LOGW(TAG, "SD card not mounted");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Unmounting SD card from %s", g_sd_ctx.mount_point);

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(g_sd_ctx.mount_point, g_sd_ctx.card);
    if (ret == ESP_OK) {
        g_sd_ctx.mounted = false;
        g_sd_ctx.card = NULL;
        memset(g_sd_ctx.mount_point, 0, sizeof(g_sd_ctx.mount_point));
        ESP_LOGI(TAG, "SD card unmounted successfully");
    } else {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t sd_spi_deinit(void)
{
    esp_err_t ret = ESP_OK;

    // Unmount if still mounted
    if (g_sd_ctx.mounted) {
        ret = sd_spi_unmount();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    // Deinitialize SPI bus
    ret = spi_bus_free(g_spi_host);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to free SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD SPI deinitialized successfully");
    return ESP_OK;
}

bool sd_spi_is_mounted(void)
{
    return g_sd_ctx.mounted;
}

sdmmc_card_t* sd_spi_get_card_info(void)
{
    return g_sd_ctx.mounted ? g_sd_ctx.card : NULL;
}

esp_err_t sd_spi_init_alt_pins(void)
{
    esp_err_t ret = ESP_OK;

    if (g_sd_ctx.mounted) {
        ESP_LOGW(TAG, "SD card already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SPI SD card with ESP32 built-in pins");
    ESP_LOGI(TAG, "Pin mapping: CS=%d, MOSI=%d, MISO=%d, CLK=%d", 
             PIN_NUM_SD_CS_ALT, PIN_NUM_SD_MOSI_ALT, PIN_NUM_SD_MISO_ALT, PIN_NUM_SD_CLK_ALT);
    printf("DEBUG: Forcing ESP32 built-in SD pins\n");
    printf("DEBUG: Pin mapping: CS=%d, MOSI=%d, MISO=%d, CLK=%d\n", 
           PIN_NUM_SD_CS_ALT, PIN_NUM_SD_MOSI_ALT, PIN_NUM_SD_MISO_ALT, PIN_NUM_SD_CLK_ALT);

    // Reset pins from any previous configuration
    ESP_LOGI(TAG, "Resetting pins from any previous configuration...");
    printf("DEBUG: Resetting pins from any previous configuration...\n");
    gpio_reset_pin(PIN_NUM_SD_CS_ALT);
    gpio_reset_pin(PIN_NUM_SD_MOSI_ALT); 
    gpio_reset_pin(PIN_NUM_SD_MISO_ALT);
    gpio_reset_pin(PIN_NUM_SD_CLK_ALT);

    // Initialize SPI bus with alternative pins
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_SD_MOSI_ALT,
        .miso_io_num = PIN_NUM_SD_MISO_ALT,
        .sclk_io_num = PIN_NUM_SD_CLK_ALT,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    // Try hosts systematically with alternative pins
    int hosts_to_try[] = {0, 2, 1}; // Try SPI1_HOST first since these are built-in pins
    int successful_host = -1;
    
    for (int i = 0; i < 3; i++) {
        int host = hosts_to_try[i];
        printf("DEBUG: Trying spi_host_device_t value %d with built-in pins...\n", host);
        ret = spi_bus_initialize(host, &bus_cfg, SPI_DMA_DISABLED);
        if (ret == ESP_OK) {
            successful_host = host;
            g_using_alt_pins = true;
            printf("DEBUG: SUCCESS! Host %d initialized with built-in pins\n", host);
            break;
        } else {
            printf("DEBUG: Host %d with built-in pins failed: %s\n", host, esp_err_to_name(ret));
        }
    }
    
    if (successful_host == -1) {
        printf("DEBUG: All hosts failed with built-in pin configuration\n");
        return ESP_ERR_NOT_FOUND;
    }

    g_spi_host = successful_host;
    printf("DEBUG: Final success: Host %d with built-in pins\n", successful_host);
    return ESP_OK;
}