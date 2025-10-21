# WiFi File Server - Recommended Improvements

Based on the Chromatic MCU codebase review, here are recommended improvements before submitting the PR.

## High Priority

### 1. Add Console Command for Testing
Following the pattern in `cmd_sd_test.c`, add a console command for WiFi testing:

**File**: `main/cmd_wifi_test.c`
```c
#include <stdio.h>
#include "esp_console.h"
#include "esp_log.h"
#include "wifi_file_server.h"

static int do_wifi_status(int argc, char **argv)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║        WiFi File Server Status             ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    
    if (wifi_file_server_is_running()) {
        printf("Status: ✓ RUNNING\n");
        printf("IP Address: %s\n", wifi_file_server_get_ip());
        printf("Access at: http://%s/\n", wifi_file_server_get_ip());
    } else {
        printf("Status: ✗ STOPPED\n");
    }
    
    return 0;
}

void register_wifi_test(void)
{
    const esp_console_cmd_t cmd = {
        .command = "wifi_status",
        .help = "Show WiFi file server status",
        .hint = NULL,
        .func = &do_wifi_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
```

### 2. Make WiFi Credentials Configurable
Add to `components/settings/settings.h`:

```c
typedef enum SettingKey {
    // ... existing settings ...
    kSettingKey_WiFiSSID,
    kSettingKey_WiFiPassword,
    // ...
} SettingKey_t;
```

Update component to read from settings instead of hardcoded values.

### 3. Add Heap Monitoring
Add heap usage logging to detect memory leaks:

```c
void wifi_file_server_start(void)
{
    ESP_LOGI(TAG, "Free heap before start: %lu bytes", esp_get_free_heap_size());
    
    // ... existing code ...
    
    ESP_LOGI(TAG, "Free heap after start: %lu bytes", esp_get_free_heap_size());
}
```

### 4. Add Upload Progress Indicator
Update `www/app.js` to show upload progress:

```javascript
async function uploadFile() {
    const fileInput = document.getElementById('fileInput');
    const file = fileInput.files[0];
    if (!file) return;
    
    const formData = new FormData();
    formData.append('file', file);
    
    // Show progress
    const progressDiv = document.createElement('div');
    progressDiv.textContent = 'Uploading...';
    document.body.appendChild(progressDiv);
    
    try {
        const response = await fetch('/api/upload', {
            method: 'POST',
            body: formData
        });
        
        if (response.ok) {
            progressDiv.textContent = 'Upload complete!';
            setTimeout(() => {
                fileInput.value = '';
                loadFiles();
                progressDiv.remove();
            }, 1000);
        } else {
            progressDiv.textContent = 'Upload failed';
        }
    } catch (error) {
        progressDiv.textContent = 'Error: ' + error.message;
    }
}
```

## Medium Priority

### 5. Add File Size Limits
Prevent uploads that are too large:

```c
#define MAX_UPLOAD_SIZE (10 * 1024 * 1024)  // 10MB

static esp_err_t api_upload_handler(httpd_req_t *req)
{
    if (req->content_len > MAX_UPLOAD_SIZE) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File too large");
        return ESP_FAIL;
    }
    
    // ... rest of implementation ...
}
```

### 6. Add Connection Limit
Prevent too many simultaneous connections:

```c
httpd_config_t config = HTTPD_DEFAULT_CONFIG();
config.max_open_sockets = 4;  // Limit concurrent connections
config.stack_size = 8192;
config.lru_purge_enable = true;
```

### 7. Improve Error Messages
Make error messages more user-friendly in the web UI:

```javascript
async function deleteFile(path) {
    if (!confirm('Delete this file?')) return;
    
    try {
        const response = await fetch(`/api/delete?path=${encodeURIComponent(path)}`, {
            method: 'DELETE'
        });
        if (response.ok) {
            loadFiles();
        } else {
            const error = await response.text();
            alert(`Delete failed: ${error}`);
        }
    } catch (error) {
        alert('Network error: ' + error.message);
    }
}
```

### 8. Add Watchdog Timer
Prevent hangs during long operations:

```c
#include "esp_task_wdt.h"

static esp_err_t api_upload_handler(httpd_req_t *req)
{
    // Add to watchdog
    esp_task_wdt_add(NULL);
    
    // ... upload logic ...
    
    // Remove from watchdog
    esp_task_wdt_delete(NULL);
    
    return ESP_OK;
}
```

## Low Priority (Future Enhancements)

### 9. Add File Search
Allow searching for files by name:

```javascript
function searchFiles(query) {
    const files = document.querySelectorAll('.file');
    files.forEach(file => {
        const name = file.textContent.toLowerCase();
        file.style.display = name.includes(query.toLowerCase()) ? '' : 'none';
    });
}
```

### 10. Add Folder Creation
Allow creating new directories:

```c
static esp_err_t api_mkdir_handler(httpd_req_t *req)
{
    char path_param[100];
    // ... parse path ...
    
    char full_path[120];
    snprintf(full_path, sizeof(full_path), "/sdcard%.100s", path_param);
    
    if (mkdir(full_path, 0755) == 0) {
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed");
        return ESP_FAIL;
    }
}
```

### 11. Add File Rename
Allow renaming files:

```c
static esp_err_t api_rename_handler(httpd_req_t *req)
{
    char old_path[100], new_path[100];
    // ... parse paths ...
    
    char full_old[120], full_new[120];
    snprintf(full_old, sizeof(full_old), "/sdcard%.100s", old_path);
    snprintf(full_new, sizeof(full_new), "/sdcard%.100s", new_path);
    
    if (rename(full_old, full_new) == 0) {
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed");
        return ESP_FAIL;
    }
}
```

### 12. Add Authentication (Optional)
Simple password protection:

```c
#define WIFI_AUTH_PASSWORD "chromatic"

static bool check_auth(httpd_req_t *req)
{
    char auth_header[128];
    size_t len = httpd_req_get_hdr_value_len(req, "Authorization");
    
    if (len > 0 && len < sizeof(auth_header)) {
        httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header));
        // Check Basic Auth
        // ... implementation ...
        return true;
    }
    
    return false;
}
```

## Code Quality Improvements

### 13. Add Doxygen Comments
Document all public functions:

```c
/**
 * @brief Initialize the WiFi file server service
 * 
 * This function initializes the network interface and event loop.
 * Must be called before wifi_file_server_start().
 * 
 * @note This function is idempotent - calling it multiple times is safe.
 */
void wifi_file_server_init(void);

/**
 * @brief Start the WiFi AP and HTTP file server
 * 
 * Creates a WiFi Access Point and starts an HTTP server for file management.
 * The server will be accessible at the IP address returned by 
 * wifi_file_server_get_ip().
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 * 
 * @note Ensure SD card is mounted before calling this function.
 */
esp_err_t wifi_file_server_start(void);
```

### 14. Add Input Validation
Validate all user inputs:

```c
static bool is_valid_filename(const char *name)
{
    if (!name || strlen(name) == 0 || strlen(name) > 255) {
        return false;
    }
    
    // Check for invalid characters
    const char *invalid = "\\/:*?\"<>|";
    for (const char *p = name; *p; p++) {
        if (strchr(invalid, *p)) {
            return false;
        }
    }
    
    return true;
}
```

### 15. Add Unit Tests
Create basic unit tests:

**File**: `components/wifi_file_server/test/test_wifi_file_server.c`
```c
#include "unity.h"
#include "wifi_file_server.h"

TEST_CASE("wifi_file_server_init is idempotent", "[wifi_file_server]")
{
    wifi_file_server_init();
    wifi_file_server_init();  // Should not crash
    TEST_ASSERT_TRUE(true);
}

TEST_CASE("wifi_file_server_get_ip returns valid IP", "[wifi_file_server]")
{
    const char *ip = wifi_file_server_get_ip();
    TEST_ASSERT_NOT_NULL(ip);
    TEST_ASSERT_GREATER_THAN(0, strlen(ip));
}
```

## Performance Optimizations

### 16. Cache Directory Listings
Reduce SD card reads:

```c
static struct {
    char path[100];
    time_t timestamp;
    cJSON *cached_list;
} dir_cache = {0};

#define CACHE_TIMEOUT_SEC 5

static cJSON* get_cached_dir_list(const char *path)
{
    time_t now = time(NULL);
    
    if (strcmp(dir_cache.path, path) == 0 &&
        (now - dir_cache.timestamp) < CACHE_TIMEOUT_SEC) {
        return dir_cache.cached_list;
    }
    
    return NULL;
}
```

### 17. Optimize JSON Generation
Use cJSON_CreateObjectReference for repeated data:

```c
cJSON *root = cJSON_CreateObject();
cJSON_AddStringToObject(root, "ip", ip_address);  // Reuse ip_address
```

## Summary

**Must-Have Before PR:**
1. Console command for testing
2. Heap monitoring
3. File size limits
4. Better error messages

**Nice-to-Have:**
5. Configurable WiFi credentials
6. Upload progress indicator
7. Connection limits
8. Doxygen comments

**Future Enhancements:**
9. File search
10. Folder creation
11. File rename
12. Authentication
13. Unit tests
14. Performance optimizations

These improvements will make the component more robust, user-friendly, and aligned with the Chromatic MCU project standards.
