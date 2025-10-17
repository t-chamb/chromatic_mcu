#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the WiFi file server service
 */
void wifi_file_server_init(void);

/**
 * @brief Start the WiFi AP and HTTP file server
 * @return ESP_OK on success
 */
esp_err_t wifi_file_server_start(void);

/**
 * @brief Stop the WiFi AP and HTTP file server
 */
void wifi_file_server_stop(void);

/**
 * @brief Check if the file server is running
 * @return true if running, false otherwise
 */
bool wifi_file_server_is_running(void);

/**
 * @brief Get the current IP address
 * @return IP address string (e.g., "192.168.4.1")
 */
const char* wifi_file_server_get_ip(void);

#ifdef __cplusplus
}
#endif
