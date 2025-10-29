/**
 * @file esp_stubs.h
 * Stub headers for ESP-IDF functions
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ESP Log */
#define ESP_LOGE(tag, format, ...) esp_log_write(1, tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) esp_log_write(2, tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) esp_log_write(3, tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) esp_log_write(4, tag, format, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) esp_log_write(5, tag, format, ##__VA_ARGS__)

void esp_log_write(int level, const char* tag, const char* format, ...);
uint32_t esp_log_timestamp(void);

/* ESP Timer */
int64_t esp_timer_get_time(void);

/* FreeRTOS */
void vTaskDelay(uint32_t ticks);
uint32_t xTaskGetTickCount(void);

typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef struct { int dummy; } StaticSemaphore_t;

#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xFFFFFFFF

SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *pxMutexBuffer);
int xSemaphoreTake(SemaphoreHandle_t xSemaphore, uint32_t xBlockTime);
int xSemaphoreGive(SemaphoreHandle_t xSemaphore);

/* ESP Error */
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NOT_FOUND -2
#define ESP_ERR_NO_MEM -3
#define ESP_ERR_INVALID_ARG -4
#define ESP_ERR_INVALID_STATE -5
#define ESP_ERR_INVALID_SIZE -6
#define ESP_ERR_NOT_SUPPORTED -7
#define ESP_ERR_TIMEOUT -8

/* Common types */
#ifndef portTICK_PERIOD_MS
#define portTICK_PERIOD_MS 1
#endif

#ifndef pdTICKS_TO_MS
#define pdTICKS_TO_MS(ticks) ((ticks) * portTICK_PERIOD_MS)
#endif

/* NVS Stubs */
typedef uint32_t nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1

esp_err_t nvs_open(const char* name, int open_mode, nvs_handle_t *out_handle);
esp_err_t nvs_get_u8(nvs_handle_t handle, const char* key, uint8_t* out_value);
esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, uint8_t value);
esp_err_t nvs_commit(nvs_handle_t handle);
void nvs_close(nvs_handle_t handle);

/* GPIO Stubs */
typedef int gpio_num_t;
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level);

/* ESP Error to string */
const char* esp_err_to_name(esp_err_t code);

/* Button - use real button.h types */
#include "button.h"
void Button_SetState(Button_t eButton, ButtonState_t state);
