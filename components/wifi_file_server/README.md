# WiFi File Server Component

A self-contained ESP-IDF component that provides a WiFi Access Point with an HTTP file server for browsing, uploading, downloading, and deleting files on an SD card.

## Features

- WiFi Access Point (SSID: `Chromatic_MCU`, Password: `chromatic123`)
- Web-based file manager with modern UI
- Browse directories and files
- Upload files to SD card
- Download files from SD card
- Delete files
- Pagination for large file lists
- JSON API for programmatic access

## Usage

### Initialization

```c
#include "wifi_file_server.h"

// Initialize the component
wifi_file_server_init();

// Start the WiFi AP and HTTP server
esp_err_t result = wifi_file_server_start();
if (result == ESP_OK) {
    printf("Server started at: http://%s/\n", wifi_file_server_get_ip());
}
```

### API Functions

- `void wifi_file_server_init(void)` - Initialize the service
- `esp_err_t wifi_file_server_start(void)` - Start WiFi AP and HTTP server
- `void wifi_file_server_stop(void)` - Stop WiFi AP and HTTP server
- `bool wifi_file_server_is_running(void)` - Check if server is running
- `const char* wifi_file_server_get_ip(void)` - Get current IP address

### Web Interface

Access the file manager by connecting to the WiFi network and navigating to:
```
http://192.168.4.1/
```

### API Endpoints

- `GET /api/list?path=/&page=1` - List files in directory (JSON)
- `GET /api/download?path=/file.txt` - Download a file
- `DELETE /api/delete?path=/file.txt` - Delete a file
- `POST /api/upload` - Upload a file (multipart/form-data)

## File Structure

```
components/wifi_file_server/
├── CMakeLists.txt              # Component build configuration
├── wifi_file_server.c          # Implementation
├── include/
│   └── wifi_file_server.h      # Public API
└── www/
    ├── index.html              # Web UI
    ├── style.css               # Styling
    └── app.js                  # Client-side logic
```

## Dependencies

- `esp_wifi` - WiFi functionality
- `esp_netif` - Network interface
- `esp_event` - Event handling
- `esp_http_server` - HTTP server
- `json` - JSON parsing (cJSON)

## Configuration

Edit the WiFi credentials in `wifi_file_server.c`:

```c
#define WIFI_SSID       "Chromatic_MCU"
#define WIFI_PASS       "chromatic123"
#define WIFI_CHANNEL    1
#define MAX_STA_CONN    4
```

## Customization

The web interface can be customized by editing the files in the `www/` directory:
- `index.html` - HTML structure
- `style.css` - Visual styling
- `app.js` - JavaScript functionality

Changes will be embedded into the binary on next build.
