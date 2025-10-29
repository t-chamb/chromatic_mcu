/**
 * @file chromatic_test.c
 * Chromatic OSD initialization for simulator
 */

#include "lvgl/lvgl.h"
#include "esp_stubs.h"
#include <stdio.h>

/* Include actual OSD code */
#include "osd_default.h"

/* Global container for OSD widgets */
static lv_obj_t * g_osd_container = NULL;

lv_obj_t * get_osd_container(void)
{
    return g_osd_container;
}

/**
 * Initialize the real Chromatic OSD
 */
void chromatic_osd_init(void)
{
    lv_obj_t * scr = lv_scr_act();
    
    printf("Initializing Chromatic OSD...\n");
    printf("Screen size: %dx%d\n", lv_obj_get_width(scr), lv_obj_get_height(scr));
    
    /* Set background to black for OSD */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    /* Use the screen directly as OSD container */
    g_osd_container = scr;
    
    printf("Using screen directly as OSD container\n");
    
    /* Initialize serial number first */
    printf("Calling SerialNum_Initialize...\n");
    extern int SerialNum_Initialize(void);
    SerialNum_Initialize();
    printf("SerialNum_Initialize complete\n");
    
    /* Call the actual OSD initialization */
    printf("Calling OSD_Default_Init...\n");
    OSD_Default_Init(scr);
    printf("OSD_Default_Init complete\n");
    
    printf("chromatic_osd_init complete\n");
}
