#include "system/wifi_file_server_osd.h"
#include "wifi_file_server.h"  // Component API
#include "esp_log.h"
#include "esp_err.h"
#include "osd_shared.h"
#include "lvgl.h"
#include "mutex.h"
#include "settings.h"

LV_IMG_DECLARE(img_toggle_on);
LV_IMG_DECLARE(img_toggle_off);

enum {
    kToggleBtnX_px = 77,
    kToggleBtnY_px = 55,

    kMsgStateOnX_px = kToggleBtnX_px + 14,
    kMsgStateOffX_px = kToggleBtnX_px + 44,
    kMsgStateY_px = kToggleBtnY_px + 12,
    
    kInfoTextX_px = kToggleBtnX_px + 33,  // Centered under toggle (66px wide / 2)
    kInfoTextY_px = kToggleBtnY_px + 24,  // Reduced padding
};

typedef struct WiFiFileServer {
    lv_obj_t* pMsgStateObj;
    lv_obj_t* pImgToggleOnObj;
    lv_obj_t* pImgToggleOffObj;
    lv_obj_t* pInfoTextObj;
    bool bEnabled;
    fnOnUpdateCb_t fnOnUpdateCb;
} WiFiFileServer_t;

static const char* TAG = "WiFiFileServer";
static WiFiFileServer_t _Ctx = {
    .bEnabled = false,
};

static void SaveToSettings(const bool bEnabled);

OSD_Result_t WiFiFileServer_Draw(void* arg)
{
    if (arg == NULL)
    {
        return kOSD_Result_Err_NullDataPtr;
    }

    lv_obj_t *const pScreen = (lv_obj_t *const) arg;

    // Status text
    if (_Ctx.pMsgStateObj == NULL)
    {
        _Ctx.pMsgStateObj = lv_label_create(pScreen);
        lv_obj_add_style(_Ctx.pMsgStateObj, OSD_GetStyleTextBlack(), 0);
        lv_obj_set_align(_Ctx.pMsgStateObj, LV_ALIGN_TOP_LEFT);
    }

    // Create IP address text (always created, visibility controlled below)
    if (_Ctx.pInfoTextObj == NULL)
    {
        _Ctx.pInfoTextObj = lv_label_create(pScreen);
        lv_obj_add_style(_Ctx.pInfoTextObj, OSD_GetStyleTextWhite(), 0);
        const char *ip = wifi_file_server_get_ip();
        lv_label_set_text(_Ctx.pInfoTextObj, ip);
        lv_obj_set_style_text_align(_Ctx.pInfoTextObj, LV_TEXT_ALIGN_CENTER, 0);
    }

    // Create toggle button images
    if (_Ctx.bEnabled)
    {
        if (_Ctx.pImgToggleOnObj == NULL)
        {
            _Ctx.pImgToggleOnObj = lv_img_create(pScreen);
            lv_obj_align(_Ctx.pImgToggleOnObj, LV_ALIGN_TOP_LEFT, kToggleBtnX_px, kToggleBtnY_px);
            lv_img_set_src(_Ctx.pImgToggleOnObj, &img_toggle_on);

            lv_obj_set_pos(_Ctx.pMsgStateObj, kMsgStateOnX_px, kMsgStateY_px);
            lv_label_set_text_static(_Ctx.pMsgStateObj, "ON");
        }
        
        // Align IP text centered below toggle button
        lv_obj_align_to(_Ctx.pInfoTextObj, _Ctx.pImgToggleOnObj, LV_ALIGN_OUT_BOTTOM_MID, 0, -7);
    }
    else
    {
        if (_Ctx.pImgToggleOffObj == NULL)
        {
            _Ctx.pImgToggleOffObj = lv_img_create(pScreen);
            lv_obj_align(_Ctx.pImgToggleOffObj, LV_ALIGN_TOP_LEFT, kToggleBtnX_px, kToggleBtnY_px);
            lv_img_set_src(_Ctx.pImgToggleOffObj, &img_toggle_off);

            lv_obj_set_pos(_Ctx.pMsgStateObj, kMsgStateOffX_px, kMsgStateY_px);
            lv_label_set_text_static(_Ctx.pMsgStateObj, "OFF");
        }
        
        // Align IP text centered below toggle button
        lv_obj_align_to(_Ctx.pInfoTextObj, _Ctx.pImgToggleOffObj, LV_ALIGN_OUT_BOTTOM_MID, 0, -7);
    }
    
    // Control IP address visibility based on enabled state
    if (_Ctx.pInfoTextObj != NULL)
    {
        if (_Ctx.bEnabled)
        {
            lv_obj_clear_flag(_Ctx.pInfoTextObj, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(_Ctx.pInfoTextObj, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Move all objects to foreground
    lv_obj_move_foreground(_Ctx.pMsgStateObj);
    if (_Ctx.pInfoTextObj != NULL)
    {
        lv_obj_move_foreground(_Ctx.pInfoTextObj);
    }

    return kOSD_Result_Ok;
}

void WiFiFileServer_Update(const bool bEnabled)
{
    // Clean up old UI elements before changing state
    WiFiFileServer_OnTransition(NULL);

    if (Mutex_Take(kMutexKey_WiFiFileServer) == kMutexResult_Ok)
    {
        const bool bPrevEnabled = _Ctx.bEnabled;
        _Ctx.bEnabled = bEnabled;

        if (bEnabled != bPrevEnabled)
        {
            SaveToSettings(bEnabled);
            
            // Start or stop the WiFi file server
            if (bEnabled)
            {
                if (wifi_file_server_start() == ESP_OK) {
                    ESP_LOGI(TAG, "WiFi file server enabled and started");
                } else {
                    ESP_LOGE(TAG, "Failed to start WiFi file server");
                }
            }
            else
            {
                wifi_file_server_stop();
                ESP_LOGI(TAG, "WiFi file server disabled and stopped");
            }
        }

        (void) Mutex_Give(kMutexKey_WiFiFileServer);
    }

    if (_Ctx.fnOnUpdateCb != NULL)
    {
        _Ctx.fnOnUpdateCb();
    }
}

OSD_Result_t WiFiFileServer_OnButton(const Button_t Button, const ButtonState_t State, void* arg)
{
    (void)arg;

    switch (Button)
    {
        case kButton_A:
        {
            if (State == kButtonState_Pressed)
            {
                bool NewState = !_Ctx.bEnabled;
                WiFiFileServer_Update(NewState);
            }
            break;
        }
        case kButton_B:
        {
            if (State == kButtonState_Pressed)
            {
                bool NewState = !_Ctx.bEnabled;
                WiFiFileServer_Update(NewState);
            }
            break;
        }
        default:
            break;
    }
    return kOSD_Result_Ok;
}

OSD_Result_t WiFiFileServer_OnTransition(void* arg)
{
    (void)arg;

    lv_obj_t** ToDelete[] = {
        &_Ctx.pImgToggleOnObj,
        &_Ctx.pImgToggleOffObj,
        &_Ctx.pMsgStateObj,
        &_Ctx.pInfoTextObj,
    };

    for (size_t i = 0; i < ARRAY_SIZE(ToDelete); i++)
    {
        if (*ToDelete[i] != NULL)
        {
            lv_obj_del(*ToDelete[i]);
            *(ToDelete[i]) = NULL;
        }
    }

    return kOSD_Result_Ok;
}

bool WiFiFileServer_IsEnabled(void)
{
    return _Ctx.bEnabled;
}


void WiFiFileServer_RegisterOnUpdateCb(fnOnUpdateCb_t fnOnUpdate)
{
    _Ctx.fnOnUpdateCb = fnOnUpdate;
}

static void SaveToSettings(const bool bEnabled)
{
    OSD_Result_t eResult;
    if ((eResult = Settings_Update(kSettingKey_WiFiFileServer, bEnabled ? 1 : 0)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "WiFi file server setting save failed: %d", eResult);
    }
}

void WiFiFileServer_Initialize(void)
{
    // Initialize the underlying component
    wifi_file_server_init();
    ESP_LOGI(TAG, "WiFi File Server initialized");
}

OSD_Result_t WiFiFileServer_ApplySetting(SettingValue_t const *const pValue)
{
    if (pValue == NULL)
    {
        return kOSD_Result_Err_NullDataPtr;
    }

    if (pValue->eType != kSettingDataType_U8)
    {
        return kOSD_Result_Err_UnexpectedSettingDataType;
    }

    // Update the state through the normal Update function
    bool bEnabled = (pValue->U8 != 0);
    WiFiFileServer_Update(bEnabled);

    return kOSD_Result_Ok;
}