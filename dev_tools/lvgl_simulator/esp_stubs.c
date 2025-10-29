/**
 * @file esp_stubs.c
 * Stub implementations of ESP-IDF functions for simulator
 */

#include "esp_stubs.h"
#include "button.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

/* ESP logging stubs */
void esp_log_write(int level, const char* tag, const char* format, ...)
{
    const char* level_str[] = {"NONE", "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE"};
    if(level >= 0 && level <= 5) {
        printf("[%s] %s: ", level_str[level], tag);
    }
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

uint32_t esp_log_timestamp(void)
{
    static struct timespec start = {0};
    struct timespec now;
    
    if(start.tv_sec == 0) {
        clock_gettime(CLOCK_MONOTONIC, &start);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t)((now.tv_sec - start.tv_sec) * 1000 + 
                      (now.tv_nsec - start.tv_nsec) / 1000000);
}

/* ESP timer stubs */
int64_t esp_timer_get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000);
}

/* FreeRTOS stubs */
void vTaskDelay(uint32_t ticks)
{
    usleep(ticks * 1000);
}

uint32_t xTaskGetTickCount(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* NVS stubs - use in-memory storage */
static uint8_t nvs_storage[256] = {0};

esp_err_t nvs_open(const char* name, int open_mode, nvs_handle_t *out_handle)
{
    *out_handle = 1;
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t handle, const char* key, uint8_t* out_value)
{
    /* Return default values for common settings */
    *out_value = 50; /* Default brightness, etc */
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, uint8_t value)
{
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle)
{
}

/* GPIO stub */
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level)
{
    return ESP_OK;
}

/* FreeRTOS mutex stubs - no-op for simulator */
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *pxMutexBuffer)
{
    return (SemaphoreHandle_t)1; /* Return non-null */
}

int xSemaphoreTake(SemaphoreHandle_t xSemaphore, uint32_t xBlockTime)
{
    return pdTRUE; /* Always succeed */
}

int xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    return pdTRUE; /* Always succeed */
}

/* ESP error to string */
const char* esp_err_to_name(esp_err_t code)
{
    switch(code) {
        case ESP_OK: return "ESP_OK";
        case ESP_FAIL: return "ESP_FAIL";
        case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND";
        case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
        default: return "ESP_ERR_UNKNOWN";
    }
}

/* Settings stubs - no-op for simulator */
typedef enum {
    kSettings_Key_Brightness,
    kSettings_Key_ColorCorrectLCD,
    kSettings_Key_ColorCorrectUSB,
    kSettings_Key_Frameblend,
    kSettings_Key_LowBattIcon,
    kSettings_Key_ScreenTransit,
    kSettings_Key_Style,
    kSettings_Key_PlayerNum,
    kSettings_Key_SilentMode,
    kNumSettings_Keys
} Settings_Key_t;

typedef int Settings_Result_t;
#define kSettings_Result_Ok 0

Settings_Result_t Settings_Initialize(void) { return kSettings_Result_Ok; }
Settings_Result_t Settings_Read(Settings_Key_t eKey, uint8_t *pValue) { *pValue = 50; return kSettings_Result_Ok; }
Settings_Result_t Settings_Update(Settings_Key_t eKey, uint8_t Value) { return kSettings_Result_Ok; }

/* Battery - now using real implementation from battery.c */

/* Button state tracking for simulator */
static ButtonState_t button_states[kNumButtons] = {kButtonState_None};
static int button_frame_age[kNumButtons] = {0};

ButtonState_t Button_GetState(const Button_t eButton) 
{ 
    if (eButton >= kNumButtons) return kButtonState_None;
    return button_states[eButton];
}

void Button_SetState(Button_t eButton, ButtonState_t state)
{
    if (eButton < kNumButtons) {
        button_states[eButton] = state;
        if (state == kButtonState_Pressed) {
            button_frame_age[eButton] = 0; /* Reset age when button is pressed */
        }
    }
}

/* Call this each frame to age and auto-clear button states */
void Button_UpdateFrame(void)
{
    for (int i = 0; i < kNumButtons; i++) {
        if (button_states[i] == kButtonState_Pressed) {
            button_frame_age[i]++;
            /* Clear button after it's been available for 1 frame */
            if (button_frame_age[i] >= 1) {
                button_states[i] = kButtonState_None;
                button_frame_age[i] = 0;
            }
        }
    }
}

/* Other button stubs */
void Button_Update(const uint16_t NewButtons) {}
const char* Button_GetNameStr(const Button_t b) { return "Button"; }
const char* Button_GetStateStr(const ButtonState_t s) { return "State"; }
void Button_ResetAll(void) { for(int i = 0; i < kNumButtons; i++) button_states[i] = kButtonState_None; }
void Button_RegisterOnButtonPokeCb(fnOnButtonPokeCb_t Handler) {}
uint16_t Button_GetPokedInputs(void) { return 0; }
void Button_RegisterCommands(void) {}

/* WiFi file server stubs */
esp_err_t wifi_file_server_init(void)
{
    return ESP_OK;
}

esp_err_t wifi_file_server_start(void)
{
    return ESP_OK;
}

esp_err_t wifi_file_server_stop(void)
{
    return ESP_OK;
}

const char* wifi_file_server_get_ip(void)
{
    return "192.168.4.1";
}
