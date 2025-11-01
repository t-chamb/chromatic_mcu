#include "bt_audio.h"
#include "board.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include <string.h>
#include <math.h>

static const char *TAG = "bt_audio";

// Audio configuration
#define AUDIO_SAMPLE_RATE       44100
#define AUDIO_CHANNELS          2
#define AUDIO_BITS_PER_SAMPLE   16
#define AUDIO_BUFFER_SIZE       512  // samples per callback (256 stereo pairs)

// FPGA Audio FIFO access
#define CMD_AUDIO               0x015  // Audio command identifier
#define AUDIO_MAGIC_ADDR        0xA0D10000
#define AUDIO_READ_SIZE         512  // Read 512 bytes per transaction (enough for BT buffer)

// State
static bool bt_initialized = false;
static bool bt_connected = false;
static SemaphoreHandle_t audio_mutex = NULL;
static uint8_t remote_bda[ESP_BD_ADDR_LEN] = {0};
static bool has_paired_device = false;
static int reconnect_attempts = 0;
#define MAX_RECONNECT_ATTEMPTS 3

// Device discovery (exposed for cmd_bt_audio.c)
discovered_device_t discovered_devices[MAX_DISCOVERED_DEVICES];
int discovered_count = 0;
static bool is_scanning = false;
static bool filter_audio_only = true;  // Set to false to see ALL devices

// SPI handle for audio reads - create our own device handle like fpga_psram
static spi_device_handle_t audio_spi_handle = NULL;

// Static DMA-capable buffer for audio reads
static uint8_t DMA_ATTR audio_dma_buf[512];

// RING BUFFER REMOVED - Read directly from FPGA in A2DP callback to save 2KB heap!
// Critical for A2DP: Bluetooth stack needs ~4KB heap for buffer allocation
// Old implementation consumed 2KB for ring buffer + 2KB for producer task stack

// Forward declarations
static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
static int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len);
static esp_err_t audio_spi_init(void);
static esp_err_t fpga_audio_enable(bool enable);
esp_err_t read_audio_from_fpga(uint8_t *buffer, size_t length);  // Non-static for cmd_bt_audio.c

// Load/save paired device MAC from NVS
static esp_err_t load_paired_device(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("bt_audio", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t required_size = ESP_BD_ADDR_LEN;
    err = nvs_get_blob(nvs_handle, "paired_mac", remote_bda, &required_size);
    nvs_close(nvs_handle);

    if (err == ESP_OK && required_size == ESP_BD_ADDR_LEN) {
        has_paired_device = true;
        ESP_LOGI(TAG, "Loaded paired device: %02X:%02X:%02X:%02X:%02X:%02X",
                 remote_bda[0], remote_bda[1], remote_bda[2],
                 remote_bda[3], remote_bda[4], remote_bda[5]);
    }

    return err;
}

static esp_err_t save_paired_device(const uint8_t *bda)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("bt_audio", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(nvs_handle, "paired_mac", bda, ESP_BD_ADDR_LEN);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);

    return err;
}



// OUI (Organizationally Unique Identifier) lookup for common manufacturers
static const char* get_manufacturer_from_oui(const uint8_t *bda)
{
    // TODO: Add OUI database without duplicates
    // For now, return NULL to use device name only
    (void)bda;  // Unused
    return NULL;
    
    /* Disabled due to duplicate OUIs - needs cleanup
    // Extract OUI (first 3 bytes)
    uint32_t oui = (bda[0] << 16) | (bda[1] << 8) | bda[2];
    
    // Common Bluetooth device manufacturers
    switch (oui) {
        // Apple
        case 0x001451: case 0x0050E4: case 0x0056CD: case 0x0C4DE9:
        case 0x10DDB1: case 0x14109F: case 0x1499E2: case 0x1C1AC0:
        case 0x1C5CF2: case 0x1CB096: case 0x20A2E4: case 0x20EE28:
        case 0x24A074: case 0x24A2E1: case 0x28E02C: case 0x28E14C:
        case 0x2C200B: case 0x2C3361: case 0x2C61F6: case 0x2CB43A:
        case 0x2CF0A2: case 0x2CF0EE: case 0x30636B: case 0x30F7C5:
        case 0x34159E: case 0x3451C9: case 0x34C059: case 0x34E2FD:
        case 0x380F4A: case 0x3C0754: case 0x3C2EF9: case 0x3CE072:
        case 0x40331A: case 0x40A6D9: case 0x40B395: case 0x40CBC0:
        case 0x44D884: case 0x48437C: case 0x48746E: case 0x4C569D:
        case 0x4C7C5F: case 0x4C8D79: case 0x50EAD6: case 0x5433CB:
        case 0x54AE27: case 0x54E43A: case 0x54FCF0: case 0x5CF7E6:
        case 0x5CF938: case 0x60334B: case 0x60F81D: case 0x60FACD:
        case 0x64200C: case 0x64A3CB: case 0x64B9E8: case 0x68967B:
        case 0x68AE20: case 0x68D93C: case 0x6C3E6D: case 0x6C4008:
        case 0x6C709F: case 0x6C72E7: case 0x6C94F8: case 0x6CAB31:
        case 0x70DEE2: case 0x70ECE4: case 0x7073CB: case 0x7081EB:
        case 0x70CD60: case 0x70E72C: case 0x78A3E4: case 0x78CA39:
        case 0x78FD94: case 0x7C04D0: case 0x7C6D62: case 0x7CF05F:
        case 0x7C11BE: case 0x80006E: case 0x8025DB: case 0x803F5D:
        case 0x8489AD: case 0x848506: case 0x8866A5: case 0x88E87F:
        case 0x8C006D: case 0x8C2937: case 0x8C7C92: case 0x8C8590:
        case 0x8CF5A3: case 0x90840D: case 0x9027E4: case 0x907240:
        case 0x90B21F: case 0x94BF2D: case 0x94E96A: case 0x9803D8:
        case 0x98B8E3: case 0x98D6BB: case 0x98F0AB: case 0x9CE063:
        case 0x9CFC01: case 0xA01828: case 0xA04EA7: case 0xA0999B:
        case 0xA43135: case 0xA4B197: case 0xA4C361: case 0xA4D1D2:
        case 0xA85B78: case 0xA88808: case 0xAC293A: case 0xAC3C0B:
        case 0xAC61EA: case 0xAC7F3E: case 0xAC87A3: case 0xB065BD:
        case 0xB0CA68: case 0xB418D1: case 0xB48B19: case 0xB853AC:
        case 0xB8782E: case 0xB8C75D: case 0xB8E856: case 0xB8F6B1:
        case 0xBC3BAF: case 0xBC52B7: case 0xBC926B: case 0xBCEC5D:
        case 0xC06394: case 0xC0847A: case 0xC0CECD: case 0xC42C03:
        case 0xC82A14: case 0xCC08E0: case 0xCC25EF: case 0xCC29F5:
        case 0xD0034B: case 0xD023DB: case 0xD02598: case 0xD0817A:
        case 0xD0D2B0: case 0xD4909C: case 0xD4DCCD: case 0xD8004D:
        case 0xD81D72: case 0xD89695: case 0xD8A25E: case 0xD8BB2C:
        case 0xDC0C5C: case 0xDC2B2A: case 0xDC2B61: case 0xDC3714:
        case 0xDC56E7: case 0xDC86D8: case 0xDC9B9C: case 0xE0ACCB:
        case 0xE0B52D: case 0xE0C767: case 0xE0F5C6: case 0xE0F847:
        case 0xE425E7: case 0xE498D6: case 0xE4CE8F: case 0xE80688:
        case 0xE88D28: case 0xEC3586: case 0xEC852F: case 0xF0989D:
        case 0xF0B479: case 0xF0CBA1: case 0xF0D1A9: case 0xF0DBE2:
        case 0xF0DCE2: case 0xF40F24: case 0xF41BA1: case 0xF437B7:
        case 0xF45C89: case 0xF4F15A: case 0xF82793: case 0xF86214:
        case 0xF8E94E: case 0xFC253F: case 0xFC8F90:
            return "Apple";
            
        // Sony
        case 0x000A95: case 0x001D0D: case 0x0024BE: case 0x0050F2:
        case 0x00E036: case 0x04C807: case 0x08ED02: case 0x0C8BFD:
        case 0x10084F: case 0x1C48CE: case 0x1C99F3: case 0x1CB094:
        case 0x20025B: case 0x20C9D0: case 0x28A02B: case 0x30F7C5:
        case 0x34885D: case 0x38C098: case 0x3C0771: case 0x40F3A0:
        case 0x48D705: case 0x4C3488: case 0x54271E: case 0x5C96E3:
        case 0x60D819: case 0x64BC0C: case 0x68A86D: case 0x6C5939:
        case 0x74E543: case 0x7C1DD9: case 0x7C6193: case 0x80D21D:
        case 0x84C7EA: case 0x8C64A2: case 0x9000DB: case 0x9C04EB:
        case 0x9C5C8E: case 0xA0E4CB: case 0xA4B805: case 0xAC9B0A:
        case 0xB0481A: case 0xB4F0AB: case 0xC0617E: case 0xC4731E:
        case 0xCC79CF: case 0xD0176A: case 0xD4E8B2: case 0xE0469A:
        case 0xE4C483: case 0xE8B2AC: case 0xF0D7AA: case 0xF4B549:
        case 0xF8461C: case 0xFC0FE6:
            return "Sony";
            
        // Samsung
        case 0x0002FC: case 0x000F86: case 0x001632: case 0x001D25:
        case 0x002566: case 0x0026C6: case 0x0026FC: case 0x0050BF:
        case 0x0C8112: case 0x10BF48: case 0x1C5A3E: case 0x1C62B8:
        case 0x2C44FD: case 0x34AA8B: case 0x38AA3C: case 0x3C0E23:
        case 0x40F520: case 0x44D6E0: case 0x4C3C16: case 0x5C0A5B:
        case 0x5C0E8B: case 0x6C2F2C: case 0x7825AD: case 0x7C11CB:
        case 0x84A466: case 0x8C77120: case 0x9C02A0: case 0xA0821F:
        case 0xA4EB12: case 0xB4EF39: case 0xC0BD2F: case 0xC4576E:
        case 0xCC07AB: case 0xD0176A: case 0xD022BE: case 0xD0DF9A:
        case 0xD4E8B2: case 0xE8039A: case 0xE8508B: case 0xEC1F72:
        case 0xF0E77E: case 0xF4098D: case 0xF4D9FB:
            return "Samsung";
            
        // Bose
        case 0x0002EE: case 0x0004AC: case 0x0009E8: case 0x001599:
            return "Bose";
            
        // JBL (Harman)
        case 0x000D18:
            return "JBL/Harman";
            
        default:
            return NULL;
    }
    */
}

// Helper: Check if device is an audio device based on Class of Device
static bool is_audio_device(uint32_t cod)
{
    // Check if CoD is valid
    if (!esp_bt_gap_is_valid_cod(cod)) {
        // Accept invalid CoD (might be in pairing mode)
        return true;
    }
    
    // Extract major device class (bits 8-12)
    uint32_t major_class = (cod >> 8) & 0x1F;
    
    // Check for RENDERING service (like ESP-IDF example does)
    // This is the key service for audio sinks (headphones/speakers)
    if (esp_bt_gap_get_cod_srvc(cod) & ESP_BT_COD_SRVC_RENDERING) {
        return true;
    }
    
    // Major Device Classes we want:
    // 0x04 = Audio/Video (headphones, speakers, headsets)
    if (major_class == 0x04) {
        return true;  // All Audio/Video devices
    }
    
    // Also accept "Wearable" devices (smartwatches with audio)
    if (major_class == 0x07) {
        return true;
    }
    
    // Accept uncategorized devices (CoD = 0) - might be in pairing mode
    if (cod == 0 || cod == 0x1F00) {
        return true;
    }
    
    return false;
}

// Helper: Parse device name from EIR (Extended Inquiry Response) data
static const char* get_name_from_eir(uint8_t *eir, char *name, size_t maxlen)
{
    if (!eir || !name || maxlen == 0) {
        return NULL;
    }
    
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;
    
    // Use ESP-IDF helper to resolve EIR data
    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        // Try short name if complete name not found
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }
    
    if (rmt_bdname && rmt_bdname_len > 0) {
        size_t copy_len = (rmt_bdname_len > maxlen - 1) ? maxlen - 1 : rmt_bdname_len;
        memcpy(name, rmt_bdname, copy_len);
        name[copy_len] = '\0';
        return name;
    }
    
    return NULL;
}

// Helper: Get device type string from CoD
static const char* get_device_type(uint32_t cod)
{
    uint32_t major_class = (cod >> 8) & 0x1F;
    uint32_t minor_class = (cod >> 2) & 0x3F;
    
    if (major_class == 0x04) {  // Audio/Video
        switch (minor_class) {
            case 0x01: return "Headset";
            case 0x02: return "Hands-free";
            case 0x04: return "Microphone";
            case 0x05: return "Speaker";
            case 0x06: return "Headphones";
            case 0x07: return "Portable Audio";
            case 0x08: return "Car Audio";
            case 0x0A: return "HiFi Audio";
            default: return "Audio Device";
        }
    } else if (major_class == 0x07) {
        return "Wearable";
    } else if (cod == 0) {
        return "Unknown";
    }
    
    return "Other";
}

// Helper: Add or update discovered device (no console spam during scan)
static void add_discovered_device(const uint8_t *bda, const char *name, int8_t rssi, uint32_t cod)
{
    // Check if already exists
    for (int i = 0; i < discovered_count; i++) {
        if (memcmp(discovered_devices[i].bda, bda, ESP_BD_ADDR_LEN) == 0) {
            // Update existing entry
            if (name && strlen(name) > 0 && strcmp(name, "Unknown") != 0) {
                strncpy(discovered_devices[i].name, name, ESP_BT_GAP_MAX_BDNAME_LEN);
                discovered_devices[i].name[ESP_BT_GAP_MAX_BDNAME_LEN] = '\0';
                discovered_devices[i].has_name = true;
            }
            discovered_devices[i].rssi = rssi;
            return;
        }
    }
    
    // Add new device if space available
    if (discovered_count < MAX_DISCOVERED_DEVICES) {
        memcpy(discovered_devices[discovered_count].bda, bda, ESP_BD_ADDR_LEN);
        
        // Try to get name
        if (name && strlen(name) > 0 && strcmp(name, "Unknown") != 0) {
            strncpy(discovered_devices[discovered_count].name, name, ESP_BT_GAP_MAX_BDNAME_LEN);
            discovered_devices[discovered_count].name[ESP_BT_GAP_MAX_BDNAME_LEN] = '\0';
            discovered_devices[discovered_count].has_name = true;
        } else {
            // Try manufacturer lookup from OUI
            const char *manufacturer = get_manufacturer_from_oui(bda);
            if (manufacturer) {
                snprintf(discovered_devices[discovered_count].name, 
                        ESP_BT_GAP_MAX_BDNAME_LEN, 
                        "%s Device", manufacturer);
                discovered_devices[discovered_count].has_name = false;
            } else {
                strcpy(discovered_devices[discovered_count].name, "Unknown Device");
                discovered_devices[discovered_count].has_name = false;
            }
        }
        discovered_devices[discovered_count].rssi = rssi;
        discovered_devices[discovered_count].cod = cod;
        
        discovered_count++;
    }
}

// GAP callback - handles pairing and connection events
static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT:
    {
        // Device discovered during scan
        if (!is_scanning) break;
        
        // Check if we've hit our device limit
        if (discovered_count >= MAX_DISCOVERED_DEVICES) {
            ESP_LOGW(TAG, "Device limit reached (%d), stopping discovery", MAX_DISCOVERED_DEVICES);
            esp_bt_gap_cancel_discovery();
            break;
        }
        
        // Check available heap - stop if getting critically low
        size_t free_heap = esp_get_free_heap_size();
        if (free_heap < 2000) {  // Less than 2KB free - critical
            ESP_LOGE(TAG, "Critical low memory (%d bytes), stopping discovery", free_heap);
            esp_bt_gap_cancel_discovery();
            break;
        }
        
        const uint8_t *bda = param->disc_res.bda;
        char name[ESP_BT_GAP_MAX_BDNAME_LEN + 1] = {0};
        int8_t rssi = -100;
        uint32_t cod = 0;
        bool has_name = false;
        uint8_t *eir = NULL;
        
        // Safely extract properties with bounds checking
        if (param->disc_res.num_prop > 0 && param->disc_res.prop != NULL) {
            for (int i = 0; i < param->disc_res.num_prop && i < 10; i++) {  // Max 10 properties
                if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_BDNAME && 
                    param->disc_res.prop[i].val != NULL) {
                    strncpy(name, (char *)param->disc_res.prop[i].val, ESP_BT_GAP_MAX_BDNAME_LEN);
                    name[ESP_BT_GAP_MAX_BDNAME_LEN] = '\0';
                    has_name = (strlen(name) > 0);
                } else if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_RSSI &&
                           param->disc_res.prop[i].val != NULL) {
                    rssi = *(int8_t *)param->disc_res.prop[i].val;
                } else if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_COD &&
                           param->disc_res.prop[i].val != NULL) {
                    cod = *(uint32_t *)param->disc_res.prop[i].val;
                } else if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_EIR &&
                           param->disc_res.prop[i].val != NULL) {
                    eir = param->disc_res.prop[i].val;
                }
            }
        }
        
        // If no name from properties, try parsing EIR data
        if (!has_name && eir != NULL) {
            const char *eir_name = get_name_from_eir(eir, name, sizeof(name));
            if (eir_name) {
                has_name = true;
            }
        }
        
        // Debug: Log all discovered devices with their CoD
        uint32_t major_class = (cod >> 8) & 0x1F;
        uint32_t service_class = (cod >> 13) & 0x7FF;
        ESP_LOGI(TAG, "Found: %s | CoD: 0x%06lx (Major: 0x%02lx, Service: 0x%03lx) | RSSI: %d dBm", 
                 has_name ? name : "Unknown", cod, major_class, service_class, rssi);
        
        // Filter: Only add audio devices (unless filter disabled)
        if (filter_audio_only && !is_audio_device(cod)) {
            ESP_LOGD(TAG, "Filtered out non-audio device");
            break;
        }
        
        // Add to list (will only print if new)
        add_discovered_device(bda, has_name ? name : NULL, rssi, cod);
        break;
    }
        
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
            is_scanning = true;
            printf("Scanning...\n");
        } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            is_scanning = false;
            // Restore connectable mode after scan completes
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        }
        break;
        
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Authentication success: %s", param->auth_cmpl.device_name);
            memcpy(remote_bda, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
            save_paired_device(remote_bda);
            has_paired_device = true;
        } else {
            ESP_LOGE(TAG, "Authentication failed, status: 0x%02x", param->auth_cmpl.stat);
            printf("\n⚠️  Pairing failed. For AirPods:\n");
            printf("   1. Put AirPods in case\n");
            printf("   2. Hold button until light flashes WHITE\n");
            printf("   3. Try bt_pair again\n\n");
        }
        break;
        
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(TAG, "Confirm request: %lu", param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
        
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(TAG, "Passkey: %lu", param->key_notif.passkey);
        break;
        
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(TAG, "PIN key request");
        esp_bt_gap_pin_reply(param->key_req.bda, true, 0, NULL);
        break;
        
    case ESP_BT_GAP_RMT_SRVCS_EVT:
        ESP_LOGI(TAG, "Remote services: status %d", param->rmt_srvcs.stat);
        break;
        
    case ESP_BT_GAP_RMT_SRVC_REC_EVT:
        ESP_LOGI(TAG, "Remote service record");
        break;
        
    case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
        if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Remote device name: %s", param->read_rmt_name.rmt_name);
        } else {
            ESP_LOGW(TAG, "Failed to read remote name, status: %d", param->read_rmt_name.stat);
        }
        break;
        
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(TAG, "Mode changed: %d", param->mode_chg.mode);
        break;
        
    default:
        ESP_LOGD(TAG, "GAP event: %d", event);
        break;
    }
}

// A2DP callback - handles connection state
static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            ESP_LOGI(TAG, "A2DP connected");
            bt_connected = true;
            reconnect_attempts = 0;  // Reset counter on successful connection

            // Check heap memory before starting audio - critical for A2DP buffer allocation
            ESP_LOGI(TAG, "Free heap: %ld bytes, largest block: %ld bytes",
                     (long)esp_get_free_heap_size(),
                     (long)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

            memcpy(remote_bda, param->conn_stat.remote_bda, ESP_BD_ADDR_LEN);
            save_paired_device(remote_bda);
            has_paired_device = true;

            // Restore connectable mode after successful connection
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

            // Enable FPGA audio output
            fpga_audio_enable(true);

            // Check if source is ready before starting media
            ESP_LOGI(TAG, "Checking if media source is ready...");
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
            
        } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            ESP_LOGI(TAG, "A2DP disconnected");
            bt_connected = false;
            
            // Disable FPGA audio output
            fpga_audio_enable(false);
            
            // Restore connectable mode
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            
            // Auto-reconnect with retry limit
            if (has_paired_device && reconnect_attempts < MAX_RECONNECT_ATTEMPTS) {
                reconnect_attempts++;
                vTaskDelay(pdMS_TO_TICKS(2000));
                ESP_LOGI(TAG, "Attempting auto-reconnect (%d/%d)...", 
                         reconnect_attempts, MAX_RECONNECT_ATTEMPTS);
                
                // Set non-connectable before reconnect attempt
                esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
                vTaskDelay(pdMS_TO_TICKS(100));
                esp_a2d_source_connect(remote_bda);
            } else if (reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) {
                ESP_LOGW(TAG, "Max reconnect attempts reached.");
                printf("\n⚠️  Connection failed. Try:\n");
                printf("   - Make sure device is in pairing mode\n");
                printf("   - Forget/unpair from other devices first\n");
                printf("   - Try bt_clear then bt_pair again\n\n");
            }
        }
        break;
        
    case ESP_A2D_AUDIO_STATE_EVT:
        if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
            ESP_LOGI(TAG, "Audio streaming active");
        } else if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND) {
            ESP_LOGI(TAG, "Audio streaming suspended by remote");
        } else if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STOPPED) {
            ESP_LOGI(TAG, "Audio streaming stopped");
        }
        break;
        
    case ESP_A2D_AUDIO_CFG_EVT:
        ESP_LOGI(TAG, "Audio codec configured: type %d", param->audio_cfg.mcc.type);
        break;
        
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        if (param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) {
            if (param->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
                ESP_LOGI(TAG, "Media source ready, starting stream...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
            } else {
                ESP_LOGW(TAG, "Media source not ready");
            }
        } else if (param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START) {
            if (param->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
                ESP_LOGI(TAG, "Media streaming started successfully");
            } else {
                ESP_LOGE(TAG, "Failed to start media streaming");
            }
        }
        break;
        
    default:
        ESP_LOGD(TAG, "A2DP event: %d", event);
        break;
    }
}

// RING BUFFER FUNCTIONS REMOVED - No longer needed!
// A2DP callback now reads directly from FPGA to save 2KB heap memory

// A2DP data callback - called when headphones request audio data
// CRITICAL: Must be FAST and non-blocking - NO I/O operations allowed!
// This callback is invoked from BTC task context (stack size: CONFIG_BT_BTC_TASK_STACK_SIZE)
static int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len)
{
    static uint32_t callback_count = 0;
    callback_count++;

    // ALWAYS log first call to confirm callback is invoked
    if (callback_count == 1) {
        ESP_LOGI(TAG, "!!! A2DP DATA CALLBACK INVOKED !!! (data=%p, len=%ld)",
                 data, (long)len);
        ESP_LOGI(TAG, "Free heap in callback: %ld bytes", (long)esp_get_free_heap_size());
    }

    if (data == NULL || len <= 0) {
        if (callback_count <= 3) {
            ESP_LOGW(TAG, "A2DP callback called with NULL/invalid params");
        }
        return 0;
    }

    // Read audio data from FPGA in smaller chunks to reduce FIFO underrun risk
    // Instead of reading all 512 bytes at once, read in 128-byte chunks
    // This gives the FPGA FIFO time to refill between reads
    const size_t chunk_size = 128;
    size_t total_read = 0;

    while (total_read < len) {
        size_t remaining = len - total_read;
        size_t to_read = (remaining > chunk_size) ? chunk_size : remaining;

        esp_err_t ret = read_audio_from_fpga(data + total_read, to_read);
        if (ret != ESP_OK) {
            // On error, fill remainder with silence
            memset(data + total_read, 0, remaining);
            if (callback_count <= 10) {
                ESP_LOGW(TAG, "FPGA audio read failed at offset %zu: %d", total_read, ret);
            }
            break;
        }

        total_read += to_read;

        // Small delay between chunks to let FIFO refill (only if more chunks needed)
        if (total_read < len) {
            // Delay ~100us between chunks (at 44.1kHz, 128 bytes = ~1.45ms of audio)
            esp_rom_delay_us(100);
        }
    }

    // Yield to other tasks briefly after completing read
    // This gives menu/OSD operations time to access SPI bus
    // With 2048-sample FIFO (~46ms buffer), we can afford 1ms yield every 3ms callback
    vTaskDelay(pdMS_TO_TICKS(1));

    return len;
}

// Enable audio output on FPGA
static esp_err_t fpga_audio_enable(bool enable)
{
    // Auto-initialize if needed
    if (audio_spi_handle == NULL) {
        esp_err_t ret = audio_spi_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    // Send audio enable/disable command to FPGA
    uint32_t cmd_data = enable ? 1 : 0;
    
    spi_transaction_t trans = {
        .flags = SPI_TRANS_MODE_QIO,
        .cmd = CMD_AUDIO,
        .addr = AUDIO_MAGIC_ADDR,
        .length = 32,  // 32 bits
        .tx_buffer = &cmd_data,
    };

    esp_err_t ret = spi_device_polling_transmit(audio_spi_handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FPGA audio %s failed: %s", enable ? "enable" : "disable", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "FPGA audio %s", enable ? "enabled" : "disabled");
    }

    return ret;
}

// Initialize audio SPI device handle (like fpga_psram does)
static esp_err_t audio_spi_init(void)
{
    if (audio_spi_handle != NULL) {
        return ESP_OK;  // Already initialized
    }

    // Create our OWN SPI device handle on the same bus (EXACTLY like fpga_psram)
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,  // 40MHz
        .mode = 0,                            // SPI mode 0
        .spics_io_num = PIN_NUM_QSPI_CS,      // Same CS pin
        .queue_size = 3,                      // Small queue
        .flags = SPI_DEVICE_HALFDUPLEX,       // Half-duplex
        .cs_ena_pretrans = 3,                 // CS setup time
        .cs_ena_posttrans = 255,              // CS hold time
        .command_bits = 11,                   // 11-bit command
        .address_bits = 32,                   // 32-bit address
        .dummy_bits = 3,                      // 3 dummy bits
    };

    esp_err_t ret = spi_bus_add_device(VSPI_HOST, &devcfg, &audio_spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add audio SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Audio SPI device initialized");
    return ESP_OK;
}

// Read audio data from FPGA via SPI (EXACTLY like fpga_psram_read)
// Non-static so cmd_bt_audio.c can use it for testing
esp_err_t read_audio_from_fpga(uint8_t *buffer, size_t length)
{
    // Auto-initialize if needed
    if (audio_spi_handle == NULL) {
        esp_err_t ret = audio_spi_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    if (length > 512) {
        return ESP_ERR_INVALID_SIZE;
    }

    // FPGA audio read: cmd bit 10 = 1 for READ, bits 9-0 = CMD_AUDIO (0x015)
    // FPGA checks for cmd=0x415 + address=0xA0D10000 to identify audio FIFO read
    uint16_t cmd = 0x400 | CMD_AUDIO;  // 0x400 | 0x015 = 0x415

    // Debug: Log the command being sent (first 3 calls only)
    static int debug_count = 0;
    if (debug_count < 3) {
        ESP_LOGI(TAG, "FPGA audio read: cmd=0x%03x (expect 0x415), addr=0x%08lx, len=%zu",
                 cmd, (unsigned long)AUDIO_MAGIC_ADDR, length);
        debug_count++;
    }

    // Clear DMA buffer before read (like fpga_psram)
    memset(audio_dma_buf, 0xCC, sizeof(audio_dma_buf));
    
    // Transaction descriptor - Use separate static variable (like fpga_psram uses read_trans)
    static spi_transaction_t audio_read_trans;
    memset(&audio_read_trans, 0, sizeof(spi_transaction_t));
    audio_read_trans.rx_buffer = audio_dma_buf;
    audio_read_trans.length = 0;                   // Half-duplex read: NO TX data
    audio_read_trans.rxlength = length * 8;        // RX exact bytes requested
    // QIO mode: 4-bit parallel data on all pins (EXACTLY like fpga_psram_read)
    audio_read_trans.flags = SPI_TRANS_MODE_QIO;
    audio_read_trans.cmd = cmd;
    audio_read_trans.addr = AUDIO_MAGIC_ADDR;

    // Use polling transmit for immediate execution (no queue wait)
    esp_err_t ret = spi_device_polling_transmit(audio_spi_handle, &audio_read_trans);
    if (ret != ESP_OK) {
        return ret;
    }

    // Copy from DMA buffer to output
    memcpy(buffer, audio_dma_buf, length);

    return ESP_OK;
}

// AUDIO STREAMING TASK REMOVED - No longer needed!
// A2DP callback now reads directly from FPGA, saving 2KB task stack
// This frees up heap memory for Bluetooth buffer allocation

esp_err_t bt_audio_init(void)
{
    if (bt_initialized) {
        ESP_LOGI(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Bluetooth audio...");

    // Create mutex for SPI access
    audio_mutex = xSemaphoreCreateMutex();
    if (audio_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Ring buffer initialization removed - no longer needed!

    // Initialize NVS (required for BT)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Load paired device if exists
    load_paired_device();

    // Initialize Bluetooth controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize Bluedroid stack
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set device name
    esp_bt_gap_set_device_name("Chromatic Audio");

    // Set discoverable and connectable
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    // Register GAP callback
    esp_bt_gap_register_callback(bt_app_gap_cb);
    
    // Configure Class of Device as Audio/Video
    esp_bt_cod_t cod;
    cod.major = ESP_BT_COD_MAJOR_DEV_AV;  // Audio/Video device
    cod.minor = 0;
    cod.service = ESP_BT_COD_SRVC_AUDIO;
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_MAJOR_MINOR);
    
    // Configure EIR data for better device identification
    esp_bt_eir_data_t eir_data = {0};
    eir_data.fec_required = false;
    eir_data.include_txpower = false;
    eir_data.include_uuid = true;
    eir_data.flag = ESP_BT_EIR_FLAG_LIMIT_DISC | ESP_BT_EIR_FLAG_GEN_DISC;
    eir_data.manufacturer_len = 0;
    eir_data.url_len = 0;
    esp_bt_gap_config_eir_data(&eir_data);

    // Set up SSP (Secure Simple Pairing)
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;  // No input/output
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    // Skip AVRCP to save memory - it's optional for basic audio streaming
    // AVRCP is only needed for remote control (play/pause/volume)
    // We can re-enable it later after optimizing memory usage
    ESP_LOGI(TAG, "Skipping AVRCP to conserve memory");

    // Initialize A2DP source (send audio TO headphones/AirPods)
    // IMPORTANT: Must init BEFORE registering callbacks (per ESP-IDF examples)
    ret = esp_a2d_source_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "A2DP source init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register callbacks AFTER init
    esp_a2d_register_callback(bt_app_a2d_cb);
    esp_a2d_source_register_data_callback(bt_app_a2d_data_cb);

    ESP_LOGI(TAG, "A2DP source initialized");

    // For A2DP source, we don't auto-connect immediately
    // User should use bt_pair command to discover and connect
    if (has_paired_device) {
        ESP_LOGI(TAG, "Previously paired device found");
        ESP_LOGI(TAG, "Use 'bt_pair' to connect, or 'bt_clear' to forget device");
    } else {
        ESP_LOGI(TAG, "No paired device - use 'bt_pair' to discover and connect");
    }

    bt_initialized = true;
    ESP_LOGI(TAG, "Bluetooth audio initialized");

    return ESP_OK;
}

esp_err_t bt_audio_start(void)
{
    if (!bt_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // NO TASK CREATION - A2DP callback reads directly from FPGA!
    // This saves 2KB task stack, freeing heap for Bluetooth buffers
    ESP_LOGI(TAG, "Audio streaming ready (callback-driven, no producer task)");
    return ESP_OK;
}

esp_err_t bt_audio_stop(void)
{
    // Stop media streaming if connected
    if (bt_connected) {
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
        ESP_LOGI(TAG, "Audio streaming stopped");
    }

    // No task to delete - callback-driven architecture

    return ESP_OK;
}

bool bt_audio_is_connected(void)
{
    return bt_connected;
}

esp_err_t bt_audio_get_paired_device(char *mac_str, size_t max_len)
{
    if (!has_paired_device) {
        return ESP_ERR_NOT_FOUND;
    }

    snprintf(mac_str, max_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             remote_bda[0], remote_bda[1], remote_bda[2],
             remote_bda[3], remote_bda[4], remote_bda[5]);

    return ESP_OK;
}

esp_err_t bt_audio_clear_pairing(void)
{
    // Stop auto-reconnect immediately
    reconnect_attempts = MAX_RECONNECT_ATTEMPTS;
    
    // Disconnect if connected
    if (bt_connected) {
        esp_a2d_source_disconnect(remote_bda);
    }
    
    // Clear NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("bt_audio", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_key(nvs_handle, "paired_mac");
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }

    // Clear state
    memset(remote_bda, 0, ESP_BD_ADDR_LEN);
    has_paired_device = false;
    bt_connected = false;

    ESP_LOGI(TAG, "Pairing cleared - now discoverable");
    return ESP_OK;
}

esp_err_t bt_audio_scan_devices_mode(uint32_t duration_sec, esp_bt_inq_mode_t mode)
{
    if (!bt_initialized) {
        ESP_LOGE(TAG, "Bluetooth not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Check available memory before starting
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    ESP_LOGI(TAG, "Heap: %d bytes free, %d bytes minimum", free_heap, min_free);
    
    if (free_heap < 5000) {
        ESP_LOGE(TAG, "Insufficient memory for discovery (%d bytes free)", free_heap);
        ESP_LOGE(TAG, "Try running 'bt_stop' first or reboot the device");
        return ESP_ERR_NO_MEM;
    }

    // Stop any auto-reconnect attempts
    reconnect_attempts = MAX_RECONNECT_ATTEMPTS;  // Disable auto-reconnect during scan
    
    // Disconnect if currently connected
    if (bt_connected) {
        esp_a2d_source_disconnect(remote_bda);
        bt_connected = false;
        vTaskDelay(pdMS_TO_TICKS(1000));  // Wait longer for disconnect and cleanup
    }
    
    // Wait for memory to stabilize after disconnect
    for (int i = 0; i < 5; i++) {
        size_t free_heap = esp_get_free_heap_size();
        if (free_heap > 5000) {
            break;  // Good enough
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Clear previous scan results
    discovered_count = 0;
    memset(discovered_devices, 0, sizeof(discovered_devices));

    // Set scan mode to non-connectable during discovery
    // This prevents incoming connections while we're scanning
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    
    // Configure inquiry parameters for more aggressive scanning
    // inquiry_len: Length of inquiry (in 1.28s units), max 0x30 (61.44s)
    uint8_t inquiry_len = (duration_sec * 10) / 13;  // Convert seconds to 1.28s units
    if (inquiry_len > 0x30) inquiry_len = 0x30;
    if (inquiry_len < 0x01) inquiry_len = 0x01;
    
    const char *mode_str = (mode == ESP_BT_INQ_MODE_GENERAL_INQUIRY) ? "GENERAL" : "LIMITED";
    ESP_LOGI(TAG, "Starting %s inquiry for %lu seconds (inquiry_len: 0x%02x)...", 
             mode_str, duration_sec, inquiry_len);
    
    // Start inquiry with specified mode
    esp_err_t ret = esp_bt_gap_start_discovery(mode, inquiry_len, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start discovery failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t bt_audio_scan_devices(uint32_t duration_sec)
{
    return bt_audio_scan_devices_mode(duration_sec, ESP_BT_INQ_MODE_GENERAL_INQUIRY);
}

esp_err_t bt_audio_connect_to_device(const char *mac_address)
{
    if (!bt_initialized) {
        ESP_LOGE(TAG, "Bluetooth not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Parse MAC address string (format: "XX:XX:XX:XX:XX:XX")
    uint8_t bda[ESP_BD_ADDR_LEN];
    int values[ESP_BD_ADDR_LEN];
    
    if (sscanf(mac_address, "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != ESP_BD_ADDR_LEN) {
        ESP_LOGE(TAG, "Invalid MAC address format. Use XX:XX:XX:XX:XX:XX");
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < ESP_BD_ADDR_LEN; i++) {
        bda[i] = (uint8_t)values[i];
    }

    ESP_LOGI(TAG, "Connecting to %02X:%02X:%02X:%02X:%02X:%02X...",
             bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);

    // Save as paired device
    memcpy(remote_bda, bda, ESP_BD_ADDR_LEN);
    save_paired_device(remote_bda);
    has_paired_device = true;
    reconnect_attempts = 0;  // Reset counter for manual connection

    // Set scan mode to non-connectable/non-discoverable during connection
    // This prevents interference from other devices
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    
    // Small delay for scan mode to take effect
    vTaskDelay(pdMS_TO_TICKS(100));

    // Connect A2DP
    ESP_LOGI(TAG, "Connecting A2DP...");
    esp_err_t ret = esp_a2d_source_connect(bda);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "A2DP connect failed: %s", esp_err_to_name(ret));
        // Restore connectable mode
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        return ret;
    }

    return ESP_OK;
}

// Helper: Sort devices by RSSI (strongest signal first)
static void sort_devices_by_rssi(void)
{
    // Simple bubble sort - good enough for small lists
    for (int i = 0; i < discovered_count - 1; i++) {
        for (int j = 0; j < discovered_count - i - 1; j++) {
            if (discovered_devices[j].rssi < discovered_devices[j + 1].rssi) {
                // Swap
                discovered_device_t temp = discovered_devices[j];
                discovered_devices[j] = discovered_devices[j + 1];
                discovered_devices[j + 1] = temp;
            }
        }
    }
}

void bt_audio_list_devices(void)
{
    if (discovered_count == 0) {
        printf("No devices found. Run 'bt_scan' first.\n");
        return;
    }

    // Sort by signal strength before displaying
    sort_devices_by_rssi();

    printf("\n=== Found %d device(s) ===\n", discovered_count);
    printf("Sorted by signal strength (closest first):\n\n");
    
    for (int i = 0; i < discovered_count; i++) {
        const char *dev_type = get_device_type(discovered_devices[i].cod);
        const char *indicator = (i == 0) ? " <- CLOSEST" : "";
        printf("[%d] %02X:%02X:%02X:%02X:%02X:%02X - %s [%s] (CoD: 0x%06lx, %d dBm)%s\n",
               i + 1,
               discovered_devices[i].bda[0], discovered_devices[i].bda[1],
               discovered_devices[i].bda[2], discovered_devices[i].bda[3],
               discovered_devices[i].bda[4], discovered_devices[i].bda[5],
               discovered_devices[i].name,
               dev_type,
               discovered_devices[i].cod,
               discovered_devices[i].rssi,
               indicator);
    }
    
    if (discovered_count > 0) {
        printf("\nTIP: Device #1 is closest (strongest signal)\n");
        printf("To connect: bt_connect %02X:%02X:%02X:%02X:%02X:%02X\n",
               discovered_devices[0].bda[0], discovered_devices[0].bda[1],
               discovered_devices[0].bda[2], discovered_devices[0].bda[3],
               discovered_devices[0].bda[4], discovered_devices[0].bda[5]);
    }
}

void bt_audio_set_filter(bool audio_only)
{
    filter_audio_only = audio_only;
    ESP_LOGI(TAG, "Audio filter: %s", audio_only ? "ON (audio devices only)" : "OFF (all devices)");
}

// Set Bluetooth audio volume via AVRC absolute volume command
esp_err_t bt_audio_set_volume(uint8_t volume_percent)
{
    // Only send volume commands when Bluetooth is connected
    if (!bt_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    // Validate input range
    if (volume_percent > 100) {
        volume_percent = 100;
    }

    // Convert 0-100% to 0-127 AVRC range
    uint8_t avrc_volume = (volume_percent * 127) / 100;

    // Send AVRC set absolute volume command
    // Transaction label (tl) can be 0-15, we'll use 0
    esp_err_t ret = esp_avrc_ct_send_set_absolute_volume_cmd(0, avrc_volume);

    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set BT volume: %d%% (AVRC: %d/127)", volume_percent, avrc_volume);
    } else {
        ESP_LOGW(TAG, "Failed to set BT volume: %s", esp_err_to_name(ret));
    }

    return ret;
}
