#include "bt_audio.h"
#include "esp_console.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"
#include "linenoise/linenoise.h"
#include "driver/spi_master.h"
#include <string.h>

static const char *TAG = "cmd_bt";

// bt_audio start - Start Bluetooth audio streaming
static int cmd_bt_start(int argc, char **argv)
{
    esp_err_t ret = bt_audio_init();
    if (ret != ESP_OK) {
        printf("Failed to initialize Bluetooth: %s\n", esp_err_to_name(ret));
        return 1;
    }

    ret = bt_audio_start();
    if (ret != ESP_OK) {
        printf("Failed to start audio streaming: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("Bluetooth audio started\n");
    printf("Device name: Chromatic Audio\n");
    printf("Status: %s\n", bt_audio_is_connected() ? "Connected" : "Discoverable");

    char mac_str[18];
    if (bt_audio_get_paired_device(mac_str, sizeof(mac_str)) == ESP_OK) {
        printf("Paired device: %s\n", mac_str);
    }

    return 0;
}

// bt_audio stop - Stop Bluetooth audio streaming
static int cmd_bt_stop(int argc, char **argv)
{
    esp_err_t ret = bt_audio_stop();
    if (ret != ESP_OK) {
        printf("Failed to stop audio: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("Bluetooth audio stopped\n");
    return 0;
}

// bt_audio status - Show Bluetooth status
static int cmd_bt_status(int argc, char **argv)
{
    printf("Bluetooth Audio Status:\n");
    printf("  Connected: %s\n", bt_audio_is_connected() ? "Yes" : "No");

    char mac_str[18];
    if (bt_audio_get_paired_device(mac_str, sizeof(mac_str)) == ESP_OK) {
        printf("  Paired device: %s\n", mac_str);
    } else {
        printf("  Paired device: None\n");
    }

    return 0;
}

// bt_audio clear - Clear pairing and enter discoverable mode
static int cmd_bt_clear(int argc, char **argv)
{
    esp_err_t ret = bt_audio_clear_pairing();
    if (ret != ESP_OK) {
        printf("Failed to clear pairing: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("Pairing cleared - device is now discoverable\n");
    return 0;
}

// bt_audio scan - Scan for nearby Bluetooth devices
static int cmd_bt_scan(int argc, char **argv)
{
    uint32_t duration = 10;  // Default 10 seconds
    esp_bt_inq_mode_t mode = ESP_BT_INQ_MODE_GENERAL_INQUIRY;
    
    if (argc > 1) {
        duration = atoi(argv[1]);
        if (duration < 1 || duration > 60) {
            printf("Duration must be between 1 and 60 seconds\n");
            return 1;
        }
    }
    
    // Check for mode flag
    if (argc > 2 && strcmp(argv[2], "limited") == 0) {
        mode = ESP_BT_INQ_MODE_LIMITED_INQUIRY;
        printf("Using LIMITED inquiry mode\n");
    }

    printf("Scanning for %lu seconds...\n", duration);
    
    esp_err_t ret = bt_audio_scan_devices_mode(duration, mode);
    if (ret != ESP_OK) {
        printf("Failed to start scan: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    return 0;
}

// bt_audio connect - Connect to a specific device
static int cmd_bt_connect(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: bt_connect <MAC_ADDRESS>\n");
        printf("Example: bt_connect 1a:2b:3c:4d:5e:6f\n");
        return 1;
    }

    esp_err_t ret = bt_audio_connect_to_device(argv[1]);
    if (ret != ESP_OK) {
        printf("Failed to connect: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("Connecting to %s...\n", argv[1]);
    printf("Check logs for connection status.\n");
    
    return 0;
}

// bt_audio list - List discovered devices
static int cmd_bt_list(int argc, char **argv)
{
    bt_audio_list_devices();
    return 0;
}

// bt_audio mem - Show memory status
static int cmd_bt_mem(int argc, char **argv)
{
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    
    printf("\n=== Memory Status ===\n");
    printf("Free heap:        %d bytes (%.1f KB)\n", free_heap, free_heap / 1024.0);
    printf("Minimum free:     %d bytes (%.1f KB)\n", min_free, min_free / 1024.0);
    printf("Largest block:    %d bytes (%.1f KB)\n", largest_block, largest_block / 1024.0);
    printf("Heap used:        ~%d KB\n", (200 - free_heap / 1024));
    
    if (free_heap < 5000) {
        printf("\n⚠️  WARNING: Low memory! Discovery may fail.\n");
        printf("Try: bt_stop (to free BT memory) or reboot\n");
    }
    
    return 0;
}

// bt_audio filter - Toggle audio device filter
static int cmd_bt_filter(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: bt_filter <on|off>\n");
        printf("  on  - Show only audio devices (default)\n");
        printf("  off - Show ALL Bluetooth devices (debug mode)\n");
        return 1;
    }

    if (strcmp(argv[1], "on") == 0) {
        bt_audio_set_filter(true);
        printf("Filter enabled: Will show only audio devices\n");
    } else if (strcmp(argv[1], "off") == 0) {
        bt_audio_set_filter(false);
        printf("Filter disabled: Will show ALL devices (debug mode)\n");
    } else {
        printf("Invalid option. Use 'on' or 'off'\n");
        return 1;
    }

    return 0;
}

// bt_audio pair - Interactive pairing (scan + list + connect)
static int cmd_bt_pair(int argc, char **argv)
{
    uint32_t duration = 10;  // Default 10 seconds
    
    if (argc > 1) {
        duration = atoi(argv[1]);
        if (duration < 1 || duration > 60) {
            printf("Duration must be between 1 and 60 seconds\n");
            return 1;
        }
    }

    printf("Put your headphones in pairing mode...\n");

    // Start scan (will handle disconnect/cleanup internally)
    esp_err_t ret = bt_audio_scan_devices(duration);
    if (ret != ESP_OK) {
        printf("Failed to start scan: %s\n", esp_err_to_name(ret));
        return 1;
    }

    // Wait for scan to complete (duration + 2 seconds buffer)
    vTaskDelay(pdMS_TO_TICKS((duration + 2) * 1000));

    // Show results (sorted by signal strength)
    bt_audio_list_devices();
    
    if (discovered_count == 0) {
        printf("\nNo devices found. Make sure your device is in pairing mode.\n");
        return 1;
    }

    // Prompt for selection
    printf("\nEnter device number (1-%d) or 0 to cancel: ", discovered_count);
    fflush(stdout);

    // Read user input using linenoise (ESP console library)
    char *line = linenoise("");
    if (line == NULL) {
        printf("\nCancelled\n");
        return 0;
    }

    int selection = atoi(line);
    linenoiseFree(line);
    
    if (selection == 0) {
        printf("Cancelled\n");
        return 0;
    }

    if (selection < 1 || selection > discovered_count) {
        printf("Invalid selection\n");
        return 1;
    }

    // Connect to selected device
    int idx = selection - 1;
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             discovered_devices[idx].bda[0], discovered_devices[idx].bda[1],
             discovered_devices[idx].bda[2], discovered_devices[idx].bda[3],
             discovered_devices[idx].bda[4], discovered_devices[idx].bda[5]);

    printf("Connecting to %s...\n", discovered_devices[idx].name);

    ret = bt_audio_connect_to_device(mac_str);
    if (ret != ESP_OK) {
        printf("Failed to connect: %s\n", esp_err_to_name(ret));
        return 1;
    }

    return 0;
}

// bt_audio_test - Test FPGA audio read with timeout
static int cmd_bt_audio_test(int argc, char **argv)
{
    printf("Testing FPGA audio read (512 bytes)...\n");
    printf("This will test if the FPGA audio FIFO responds to SPI reads\n\n");
    
    // Use the audio SPI handle (will auto-initialize)
    extern esp_err_t read_audio_from_fpga(uint8_t *buffer, size_t length);
    
    // Allocate buffer
    uint8_t *buffer = malloc(512);
    if (buffer == NULL) {
        printf("ERROR: Failed to allocate buffer\n");
        return 1;
    }
    
    printf("Attempting SPI read with 5 second timeout...\n");
    
    // Try reading with timeout monitoring
    TickType_t start = xTaskGetTickCount();
    esp_err_t ret = read_audio_from_fpga(buffer, 512);
    TickType_t elapsed = xTaskGetTickCount() - start;
    
    printf("SPI transaction completed in %lu ms\n", (unsigned long)(elapsed * portTICK_PERIOD_MS));
    
    if (ret != ESP_OK) {
        printf("ERROR: SPI transaction failed: %s\n", esp_err_to_name(ret));
        free(buffer);
        return 1;
    }
    
    // Count non-zero bytes
    int non_zero = 0;
    for (int i = 0; i < 512; i++) {
        if (buffer[i] != 0 && buffer[i] != 0xCC) non_zero++;
    }
    
    printf("✓ Read 512 bytes successfully\n");
    printf("  Non-zero bytes: %d/512\n", non_zero);
    
    // Show first 64 bytes regardless (to see pattern)
    printf("  First 64 bytes:\n  ");
    for (int i = 0; i < 64; i++) {
        printf("%02X ", buffer[i]);
        if ((i + 1) % 16 == 0) printf("\n  ");
    }
    printf("\n");

    if (non_zero > 0) {
        printf("✓✓ Audio data is flowing!\n");
    } else {
        printf("⚠ All zeros/0xCC (FIFO empty or not implemented)\n");
        printf("  Check: FPGA audio FIFO enabled?\n");
        printf("  Check: CMD_AUDIO (0x015) handler implemented?\n");
        printf("  Check: Address 0xA0D10000 routed correctly?\n");
    }
    
    free(buffer);
    return 0;
}

void register_bt_audio_commands(void)
{
    const esp_console_cmd_t start_cmd = {
        .command = "bt_start",
        .help = "Start Bluetooth audio streaming",
        .hint = NULL,
        .func = &cmd_bt_start,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&start_cmd));

    const esp_console_cmd_t stop_cmd = {
        .command = "bt_stop",
        .help = "Stop Bluetooth audio streaming",
        .hint = NULL,
        .func = &cmd_bt_stop,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&stop_cmd));

    const esp_console_cmd_t status_cmd = {
        .command = "bt_status",
        .help = "Show Bluetooth audio status",
        .hint = NULL,
        .func = &cmd_bt_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status_cmd));

    const esp_console_cmd_t clear_cmd = {
        .command = "bt_clear",
        .help = "Clear Bluetooth pairing",
        .hint = NULL,
        .func = &cmd_bt_clear,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&clear_cmd));

    const esp_console_cmd_t scan_cmd = {
        .command = "bt_scan",
        .help = "Scan for nearby Bluetooth devices",
        .hint = "[duration_seconds] [limited]",
        .func = &cmd_bt_scan,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&scan_cmd));

    const esp_console_cmd_t connect_cmd = {
        .command = "bt_connect",
        .help = "Connect to a Bluetooth device",
        .hint = "<MAC_ADDRESS>",
        .func = &cmd_bt_connect,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&connect_cmd));

    const esp_console_cmd_t list_cmd = {
        .command = "bt_list",
        .help = "List discovered Bluetooth devices",
        .hint = NULL,
        .func = &cmd_bt_list,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&list_cmd));

    const esp_console_cmd_t pair_cmd = {
        .command = "bt_pair",
        .help = "Interactive pairing (scan + select + connect)",
        .hint = "[duration_seconds]",
        .func = &cmd_bt_pair,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&pair_cmd));

    const esp_console_cmd_t mem_cmd = {
        .command = "bt_mem",
        .help = "Show Bluetooth memory status",
        .hint = NULL,
        .func = &cmd_bt_mem,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&mem_cmd));

    const esp_console_cmd_t filter_cmd = {
        .command = "bt_filter",
        .help = "Toggle audio device filter (on|off)",
        .hint = "<on|off>",
        .func = &cmd_bt_filter,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&filter_cmd));

    const esp_console_cmd_t test_cmd = {
        .command = "bt_audio_test",
        .help = "Test FPGA audio read (512 bytes)",
        .hint = NULL,
        .func = &cmd_bt_audio_test,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_cmd));
}
