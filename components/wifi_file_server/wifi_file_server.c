// WiFi File Server Component
#include "wifi_file_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "cJSON.h"
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#define WIFI_SSID       "Chromatic_MCU"
#define WIFI_PASS       "chromatic123"
#define WIFI_CHANNEL    1
#define MAX_STA_CONN    4

static const char* TAG = "WiFiFileServer";
static httpd_handle_t server = NULL;
static bool wifi_started = false;
static bool service_initialized = false;
static char ip_address[16] = "0.0.0.0";
static esp_netif_t *ap_netif = NULL;

// Embedded files
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");

// Serve index.html
static esp_err_t index_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Heap before index: %lu", esp_get_free_heap_size());
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)index_html_start, index_html_end - index_html_start);
    ESP_LOGI(TAG, "Heap after index: %lu", esp_get_free_heap_size());
    return ESP_OK;
}

// Serve style.css
static esp_err_t css_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char*)style_css_start, style_css_end - style_css_start);
    return ESP_OK;
}

// Serve app.js
static esp_err_t js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char*)app_js_start, app_js_end - app_js_start);
    return ESP_OK;
}

// API: Get system info
static esp_err_t api_info_handler(httpd_req_t *req)
{
    // Get ESP32 MAC address as serial number
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char serial[32];
    snprintf(serial, sizeof(serial), "CHR-%02X%02X%02X%02X%02X%02X", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Get free heap
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_heap = esp_get_minimum_free_heap_size();
    
    char response[512];
    snprintf(response, sizeof(response), 
        "{\"serial\":\"%s\",\"fw_version\":\"%s\",\"free_heap\":%lu,\"min_heap\":%lu,\"ssid\":\"%s\"}", 
        serial, CONFIG_CHROMATIC_FW_VER_STR, free_heap, min_heap, WIFI_SSID);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

// API: Get settings
static esp_err_t api_settings_get_handler(httpd_req_t *req)
{
    // Get ESP32 MAC address as serial number
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char serial[32];
    snprintf(serial, sizeof(serial), "CHR-%02X%02X%02X%02X%02X%02X", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    char response[256];
    snprintf(response, sizeof(response), 
        "{\"ssid\":\"%s\",\"password\":\"%s\",\"serial\":\"%s\"}", 
        WIFI_SSID, WIFI_PASS, serial);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

// API: Save settings
static esp_err_t api_settings_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse JSON (simple parsing for SSID and password)
    char *ssid_start = strstr(buf, "\"ssid\":\"");
    char *pass_start = strstr(buf, "\"password\":\"");
    
    if (!ssid_start || !pass_start) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Extract SSID
    ssid_start += 8;  // Skip "ssid":"
    char *ssid_end = strchr(ssid_start, '"');
    if (!ssid_end || (ssid_end - ssid_start) > 32) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid SSID");
        return ESP_FAIL;
    }
    
    // Extract password
    pass_start += 12;  // Skip "password":"
    char *pass_end = strchr(pass_start, '"');
    if (!pass_end || (pass_end - pass_start) > 64 || (pass_end - pass_start) < 8) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid password");
        return ESP_FAIL;
    }
    
    char new_ssid[33];
    char new_pass[65];
    memcpy(new_ssid, ssid_start, ssid_end - ssid_start);
    new_ssid[ssid_end - ssid_start] = '\0';
    memcpy(new_pass, pass_start, pass_end - pass_start);
    new_pass[pass_end - pass_start] = '\0';
    
    ESP_LOGI(TAG, "Settings update requested: SSID='%s'", new_ssid);
    
    // TODO: Save to NVS and restart WiFi
    // For now, just acknowledge
    httpd_resp_sendstr(req, "OK");
    
    ESP_LOGW(TAG, "Settings saved but not applied (restart required)");
    return ESP_OK;
}

// API: List files
static esp_err_t api_list_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "List handler called, heap: %lu", esp_get_free_heap_size());
    
    // Get query parameters
    char query[128] = {0};
    char path_param[100] = "/";
    int page = 1;
    
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "path", path_param, sizeof(path_param));
        char page_str[10];
        if (httpd_query_key_value(query, "page", page_str, sizeof(page_str)) == ESP_OK) {
            page = atoi(page_str);
            if (page < 1) page = 1;
        }
    }
    
    // Build full path
    char full_path[120];
    snprintf(full_path, sizeof(full_path), "/sdcard%s", path_param);
    
    ESP_LOGI(TAG, "Attempting to list directory: %s", full_path);
    
    // Open directory
    DIR *dir = opendir(full_path);
    if (!dir) {
        ESP_LOGW(TAG, "Failed to open directory: %s (errno: %d - %s)", full_path, errno, strerror(errno));
        
        // Return error message in JSON
        char error_response[256];
        snprintf(error_response, sizeof(error_response),
            "{\"files\":[],\"page\":1,\"total_pages\":1,\"total_files\":0,\"error\":\"Cannot open %s: %s\"}",
            full_path, strerror(errno));
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, error_response);
        return ESP_OK;
    }
    
    // Create JSON response manually to avoid cJSON memory issues
    char *response = malloc(4096);
    if (!response) {
        closedir(dir);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }
    
    int pos = snprintf(response, 4096, "{\"files\":[");
    struct dirent *entry;
    int file_count = 0;
    bool first = true;
    
    while ((entry = readdir(dir)) != NULL && pos < 3800) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Get file stats
        char file_path[400];
        snprintf(file_path, sizeof(file_path), "%s/%s", full_path, entry->d_name);
        struct stat st;
        if (stat(file_path, &st) != 0) {
            continue;
        }
        
        // Add comma if not first
        if (!first) {
            pos += snprintf(response + pos, 4096 - pos, ",");
        }
        first = false;
        
        // Add file entry
        const char *type = S_ISDIR(st.st_mode) ? "dir" : "file";
        pos += snprintf(response + pos, 4096 - pos, 
            "{\"name\":\"%s\",\"type\":\"%s\",\"size\":%ld}",
            entry->d_name, type, (long)st.st_size);
        
        file_count++;
    }
    
    closedir(dir);
    
    // Complete JSON
    snprintf(response + pos, 4096 - pos, 
        "],\"page\":%d,\"total_pages\":1,\"total_files\":%d,\"ip\":\"%s\"}",
        page, file_count, ip_address);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    free(response);
    
    ESP_LOGI(TAG, "Listed %d files from %s, heap: %lu", file_count, full_path, esp_get_free_heap_size());
    return ESP_OK;
}

// API: Download file
static esp_err_t api_download_handler(httpd_req_t *req)
{
    char path_param[100];
    char query[128];
    
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing path");
        return ESP_FAIL;
    }
    
    if (httpd_query_key_value(query, "path", path_param, sizeof(path_param)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing path");
        return ESP_FAIL;
    }
    
    char full_path[120];
    snprintf(full_path, sizeof(full_path), "/sdcard%.100s", path_param);
    
    FILE *f = fopen(full_path, "rb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/octet-stream");
    
    // Use smaller buffer to conserve heap
    char *buf = malloc(256);
    if (!buf) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }
    
    size_t n;
    while ((n = fread(buf, 1, 256, f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) {
            fclose(f);
            free(buf);
            return ESP_FAIL;
        }
    }
    
    fclose(f);
    free(buf);
    httpd_resp_send_chunk(req, NULL, 0);
    
    return ESP_OK;
}

// API: Delete file
static esp_err_t api_delete_handler(httpd_req_t *req)
{
    char path_param[100];
    char query[128];
    
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing path");
        return ESP_FAIL;
    }
    
    if (httpd_query_key_value(query, "path", path_param, sizeof(path_param)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing path");
        return ESP_FAIL;
    }
    
    char full_path[120];
    snprintf(full_path, sizeof(full_path), "/sdcard%.100s", path_param);
    
    if (unlink(full_path) == 0) {
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }
}

// API: Upload file
static esp_err_t api_upload_handler(httpd_req_t *req)
{
    // Use smaller buffer to conserve heap
    char *buf = malloc(256);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }
    
    // Read first chunk to get filename and path
    int ret = httpd_req_recv(req, buf, 256);
    if (ret <= 0) {
        free(buf);
        return ESP_FAIL;
    }
    
    // Extract filename
    char fname[64] = "upload.bin";
    char *p = strstr(buf, "filename=\"");
    if (p && p < buf + ret - 10) {
        p += 10;
        char *e = strchr(p, '"');
        if (e && e < buf + ret && (e - p) < 60) {
            memcpy(fname, p, e - p);
            fname[e - p] = 0;
        }
    }
    
    // Find data start
    char *data = strstr(buf, "\r\n\r\n");
    if (!data || data >= buf + ret - 4) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad format");
        return ESP_FAIL;
    }
    data += 4;
    
    // Open file (always in /sdcard root for now)
    char path[100];
    snprintf(path, sizeof(path), "/sdcard/%.60s", fname);
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File error");
        return ESP_FAIL;
    }
    
    // Write first chunk
    int first_len = ret - (data - buf);
    if (first_len > 0) {
        fwrite(data, 1, first_len, f);
    }
    
    // Read and write rest
    int remaining = req->content_len - ret;
    while (remaining > 0) {
        int to_read = (remaining > 256) ? 256 : remaining;
        ret = httpd_req_recv(req, buf, to_read);
        if (ret <= 0) break;
        
        if (ret > 4 && buf[0] == '\r' && buf[1] == '\n' && buf[2] == '-') {
            break;
        }
        
        fwrite(buf, 1, ret, f);
        remaining -= ret;
    }
    
    fclose(f);
    free(buf);
    
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Station connected");
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "Station disconnected");
    }
}

void wifi_file_server_init(void)
{
    if (service_initialized) return;
    
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize netif");
        return;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop");
        return;
    }
    
    service_initialized = true;
    ESP_LOGI(TAG, "WiFi File Server initialized");
}

esp_err_t wifi_file_server_start(void)
{
    if (!service_initialized) {
        wifi_file_server_init();
    }
    
    if (wifi_started) {
        ESP_LOGW(TAG, "Already started");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting WiFi file server...");
    ESP_LOGI(TAG, "Free heap before WiFi init: %lu", esp_get_free_heap_size());
    
    // Create WiFi AP
    if (!ap_netif) {
        ap_netif = esp_netif_create_default_wifi_ap();
        if (!ap_netif) {
            ESP_LOGE(TAG, "Failed to create AP netif");
            return ESP_FAIL;
        }
    }
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Event handler register failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Free heap after WiFi init: %lu", esp_get_free_heap_size());

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi set mode failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi set config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Starting WiFi AP...");
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_started = true;
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(ap_netif, &ip_info);
    snprintf(ip_address, sizeof(ip_address), IPSTR, IP2STR(&ip_info.ip));
    
    ESP_LOGI(TAG, "WiFi AP ready. SSID: %s, IP: %s", WIFI_SSID, ip_address);

    // Start HTTP server
    ESP_LOGI(TAG, "Free heap before HTTP server: %lu", esp_get_free_heap_size());
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 6144;  // Smaller stack to save memory
    config.max_open_sockets = 1;  // Only 1 connection at a time
    config.lru_purge_enable = true;
    config.core_id = 1;  // Run on core 1 if available
    config.recv_wait_timeout = 5;  // Shorter timeout
    config.send_wait_timeout = 5;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Static files
        httpd_uri_t uri_index = {.uri = "/", .method = HTTP_GET, .handler = index_handler};
        httpd_uri_t uri_css = {.uri = "/style.css", .method = HTTP_GET, .handler = css_handler};
        httpd_uri_t uri_js = {.uri = "/app.js", .method = HTTP_GET, .handler = js_handler};
        
        // API endpoints
        httpd_uri_t uri_info = {.uri = "/api/info", .method = HTTP_GET, .handler = api_info_handler};
        httpd_uri_t uri_list = {.uri = "/api/list", .method = HTTP_GET, .handler = api_list_handler};
        httpd_uri_t uri_download = {.uri = "/api/download", .method = HTTP_GET, .handler = api_download_handler};
        httpd_uri_t uri_delete = {.uri = "/api/delete", .method = HTTP_DELETE, .handler = api_delete_handler};
        httpd_uri_t uri_upload = {.uri = "/api/upload", .method = HTTP_POST, .handler = api_upload_handler};
        httpd_uri_t uri_settings_get = {.uri = "/api/settings", .method = HTTP_GET, .handler = api_settings_get_handler};
        httpd_uri_t uri_settings_post = {.uri = "/api/settings", .method = HTTP_POST, .handler = api_settings_post_handler};
        
        httpd_register_uri_handler(server, &uri_index);
        httpd_register_uri_handler(server, &uri_css);
        httpd_register_uri_handler(server, &uri_js);
        httpd_register_uri_handler(server, &uri_info);
        httpd_register_uri_handler(server, &uri_list);
        httpd_register_uri_handler(server, &uri_download);
        httpd_register_uri_handler(server, &uri_delete);
        httpd_register_uri_handler(server, &uri_upload);
        httpd_register_uri_handler(server, &uri_settings_get);
        httpd_register_uri_handler(server, &uri_settings_post);
        
        ESP_LOGI(TAG, "HTTP server started");
        ESP_LOGI(TAG, "Access at: http://%s/", ip_address);
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        esp_wifi_stop();
        wifi_started = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

void wifi_file_server_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
    }

    if (wifi_started) {
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
        esp_wifi_stop();
        esp_wifi_deinit();
        wifi_started = false;
        strcpy(ip_address, "0.0.0.0");
    }
}

bool wifi_file_server_is_running(void)
{
    return wifi_started && (server != NULL);
}

const char* wifi_file_server_get_ip(void)
{
    return ip_address;
}
