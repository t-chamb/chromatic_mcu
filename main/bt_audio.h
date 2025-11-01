#pragma once

#include "esp_err.h"
#include "esp_gap_bt_api.h"
#include <stdint.h>
#include <stdbool.h>

// Discovered device info
#define MAX_DISCOVERED_DEVICES 20  // Filter by audio devices only
typedef struct {
    uint8_t bda[ESP_BD_ADDR_LEN];
    char name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    int8_t rssi;
    uint32_t cod;  // Class of Device
    bool has_name;
} discovered_device_t;

// Exposed for command interface
extern discovered_device_t discovered_devices[MAX_DISCOVERED_DEVICES];
extern int discovered_count;

// Initialize Bluetooth A2DP audio streaming
// Starts BT in discoverable mode, auto-reconnects to last paired device
esp_err_t bt_audio_init(void);

// Start/stop audio streaming task
esp_err_t bt_audio_start(void);
esp_err_t bt_audio_stop(void);

// Get Bluetooth connection status
bool bt_audio_is_connected(void);

// Get last paired device MAC address (for display)
esp_err_t bt_audio_get_paired_device(char *mac_str, size_t max_len);

// Clear pairing and enter discoverable mode
esp_err_t bt_audio_clear_pairing(void);

// Scan for nearby Bluetooth devices
esp_err_t bt_audio_scan_devices(uint32_t duration_sec);

// Scan with specific inquiry mode (for debugging)
esp_err_t bt_audio_scan_devices_mode(uint32_t duration_sec, esp_bt_inq_mode_t mode);

// Connect to a specific device by MAC address
esp_err_t bt_audio_connect_to_device(const char *mac_address);

// List discovered devices
void bt_audio_list_devices(void);

// Toggle audio-only filter (for debugging)
void bt_audio_set_filter(bool audio_only);

// Set Bluetooth audio volume (0-100%)
// This sends an AVRC absolute volume command to the connected headset
esp_err_t bt_audio_set_volume(uint8_t volume_percent);
