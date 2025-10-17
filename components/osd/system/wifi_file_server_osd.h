#pragma once

#include "osd_shared.h"
#include "button.h"
#include "settings.h"
#include "wifi_file_server.h"  // Component API

#ifdef __cplusplus
extern "C" {
#endif

// Menu system functions
OSD_Result_t WiFiFileServer_Draw(void* arg);
OSD_Result_t WiFiFileServer_OnButton(const Button_t Button, const ButtonState_t State, void *arg);
OSD_Result_t WiFiFileServer_OnTransition(void* arg);

// State management functions
void WiFiFileServer_Update(const bool bEnabled);
bool WiFiFileServer_IsEnabled(void);
void WiFiFileServer_RegisterOnUpdateCb(fnOnUpdateCb_t fnOnUpdate);

// Settings integration (called from main)
void WiFiFileServer_Initialize(void);
OSD_Result_t WiFiFileServer_ApplySetting(SettingValue_t const *const pValue);

#ifdef __cplusplus
}
#endif