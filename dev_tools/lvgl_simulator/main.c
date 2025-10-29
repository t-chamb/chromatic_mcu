/**
 * @file main
 * LVGL 8.4 SDL Simulator
 */

/*********************
 *      INCLUDES
 *********************/
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"
#include "esp_stubs.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>

/*********************
 *      DEFINES
 *********************/
// Chromatic screen is 160x128
#define WINDOW_HOR_RES    160
#define WINDOW_VER_RES    128
// Scale up for visibility
#define WINDOW_SCALE      4
#define DISPLAY_HOR_RES   (WINDOW_HOR_RES * WINDOW_SCALE)
#define DISPLAY_VER_RES   (WINDOW_VER_RES * WINDOW_SCALE)

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void hal_init(void);
static void monitor_sdl_init(void);
static void monitor_sdl_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);
static void sdl_mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static uint32_t custom_tick_get(void);
static void handle_keyboard_input(SDL_KeyboardEvent *key_event, bool is_pressed);

/**********************
 *  STATIC VARIABLES
 **********************/
static SDL_Window * window;
static SDL_Renderer * renderer;
static SDL_Texture * texture;
static uint32_t * tft_fb;
static int32_t mouse_x = 0;
static int32_t mouse_y = 0;
static bool mouse_pressed = false;

/* Button debounce tracking - track which keys are currently held */
static bool key_is_down[512] = {false};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  printf("=== LVGL Simulator Starting ===\n");
  fflush(stdout);

  /* Initialize LVGL */
  printf("1. Initializing LVGL...\n");
  fflush(stdout);
  lv_init();
  
  /* Set default theme to dark with black background */
  lv_theme_t * theme = lv_theme_default_init(NULL, lv_color_hex(0x000000), lv_color_hex(0xFFFFFF), true, LV_FONT_DEFAULT);
  lv_disp_set_theme(NULL, theme);
  
  printf("   LVGL initialized OK\n");
  fflush(stdout);

  /* Initialize HAL (display and input devices) */
  printf("2. Initializing HAL...\n");
  fflush(stdout);
  hal_init();
  printf("   HAL initialized OK\n");
  fflush(stdout);

  /* Initialize OSD system first */
  printf("3. Calling OSD_Initialize...\n");
  fflush(stdout);
  extern int OSD_Initialize(void);
  OSD_Initialize();
  printf("   OSD_Initialize OK\n");
  fflush(stdout);
  
  /* Initialize Chromatic OSD */
  printf("4. Initializing Chromatic OSD...\n");
  fflush(stdout);
  extern void chromatic_osd_init(void);
  chromatic_osd_init();
  printf("   Chromatic OSD initialized OK\n");
  fflush(stdout);
  
  /* Draw OSD once */
  printf("5. Drawing OSD...\n");
  fflush(stdout);
  extern void OSD_Draw(void* arg);
  extern lv_obj_t * get_osd_container(void);
  lv_obj_t * osd_container = get_osd_container();
  OSD_Draw(osd_container);
  printf("   OSD_Draw OK\n");
  fflush(stdout);
  
  /* Force an initial render before entering main loop */
  printf("6. Forcing initial render...\n");
  fflush(stdout);
  lv_timer_handler();
  lv_timer_handler();
  lv_timer_handler();
  printf("   Render OK\n");
  fflush(stdout);
  printf("=== OSD initialization complete, entering main loop ===\n");
  fflush(stdout);
  
  /* Test button system by simulating a button press */
  printf("Testing button system - simulating A button press...\n");
  Button_SetState(kButton_A, kButtonState_Pressed);
  usleep(100000); /* 100ms */
  printf("A button test complete\n");

  /* Main loop */
  int frame_count = 0;
  while(1) {
    frame_count++;
    /* Handle SDL events */
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        goto cleanup;
      }
      else if(event.type == SDL_MOUSEMOTION) {
        mouse_x = event.motion.x;
        mouse_y = event.motion.y;
      }
      else if(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        mouse_pressed = (event.type == SDL_MOUSEBUTTONDOWN);
      }
      else if(event.type == SDL_KEYDOWN) {
        /* Keyboard controls for OSD navigation 
         * Arrow keys: Navigate menus
         * Z/A: A button (confirm/select)
         * X/S: B button (back/cancel)
         * Enter: Start
         * Right Shift: Select
         */
        SDL_Keycode key = event.key.keysym.sym;
        
        printf("Key detected: %s (code: %d)\n", SDL_GetKeyName(key), key);
        
        /* Only trigger on actual key press, not repeat - track key state manually */
        if (!key_is_down[key & 0x1FF]) {
          key_is_down[key & 0x1FF] = true;
          
          switch(key) {
            case SDLK_UP:    
              printf("Button: UP pressed\n");
              Button_SetState(kButton_Up, kButtonState_Pressed); 
              break;
            case SDLK_DOWN:  
              printf("Button: DOWN pressed\n");
              Button_SetState(kButton_Down, kButtonState_Pressed); 
              break;
            case SDLK_LEFT:  
              printf("Button: LEFT pressed\n");
              Button_SetState(kButton_Left, kButtonState_Pressed); 
              break;
            case SDLK_RIGHT: 
              printf("Button: RIGHT pressed\n");
              Button_SetState(kButton_Right, kButtonState_Pressed); 
              break;
            case SDLK_z:
            case SDLK_a:     
              printf("Button: A pressed\n");
              Button_SetState(kButton_A, kButtonState_Pressed); 
              break;
            case SDLK_x:
            case SDLK_b:
            case SDLK_s:     
              printf("Button: B pressed\n");
              Button_SetState(kButton_B, kButtonState_Pressed); 
              break;
            case SDLK_RETURN: 
              printf("Button: START pressed\n");
              Button_SetState(kButton_Start, kButtonState_Pressed); 
              break;
            case SDLK_RSHIFT: 
              printf("Button: SELECT pressed\n");
              Button_SetState(kButton_Select, kButtonState_Pressed); 
              break;
            default:
              printf("Unknown key pressed: %s (code: %d)\n", SDL_GetKeyName(key), key);
              break;
          }
        }
      }
      else if(event.type == SDL_KEYUP) {
        /* Release buttons */
        SDL_Keycode key = event.key.keysym.sym;
        key_is_down[key & 0x1FF] = false;
        
        switch(key) {
          case SDLK_UP:    Button_SetState(kButton_Up, kButtonState_None); break;
          case SDLK_DOWN:  Button_SetState(kButton_Down, kButtonState_None); break;
          case SDLK_LEFT:  Button_SetState(kButton_Left, kButtonState_None); break;
          case SDLK_RIGHT: Button_SetState(kButton_Right, kButtonState_None); break;
          case SDLK_z:
          case SDLK_a:     Button_SetState(kButton_A, kButtonState_None); break;
          case SDLK_x:
          case SDLK_b:
          case SDLK_s:     Button_SetState(kButton_B, kButtonState_None); break;
          case SDLK_RETURN: Button_SetState(kButton_Start, kButtonState_None); break;
          case SDLK_RSHIFT: Button_SetState(kButton_Select, kButtonState_None); break;
        }
      }
    }

    /* Update tick - 16ms per frame for ~60 FPS (closer to hardware) */
    lv_tick_inc(16);

    /* Handle OSD button inputs */
    extern void OSD_HandleInputs(void);
    OSD_HandleInputs();

    /* Redraw OSD */
    extern void OSD_Draw(void* arg);
    extern lv_obj_t * get_osd_container(void);
    OSD_Draw(get_osd_container());

    /* Handle LVGL tasks (lv_timer_handler in LVGL 9.x) */
    lv_timer_handler();
    
    /* Clear button states at end of frame after everything has processed */
    extern void Button_UpdateFrame(void);
    Button_UpdateFrame();
    
    /* Frame delay - 16ms for ~60 FPS to match hardware timing */
    usleep(16000);
  }

cleanup:
  lv_deinit();
  free(tft_fb);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void hal_init(void)
{
  /* Use SDL to double buffer the screen */
  monitor_sdl_init();

  /* Initialize display buffer */
  static lv_disp_draw_buf_t disp_buf;
  static lv_color_t buf1[WINDOW_HOR_RES * 10];
  static lv_color_t buf2[WINDOW_HOR_RES * 10];
  lv_disp_draw_buf_init(&disp_buf, buf1, buf2, WINDOW_HOR_RES * 10);

  /* Initialize display driver */
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.draw_buf = &disp_buf;
  disp_drv.flush_cb = monitor_sdl_flush;
  disp_drv.hor_res = WINDOW_HOR_RES;
  disp_drv.ver_res = WINDOW_VER_RES;
  lv_disp_drv_register(&disp_drv);

  /* Initialize input device (mouse) */
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = sdl_mouse_read;
  lv_indev_drv_register(&indev_drv);
}

static void monitor_sdl_init(void)
{
  /* Initialize SDL */
  SDL_Init(SDL_INIT_VIDEO);
  
  /* Disable text input mode so letter keys work as game buttons */
  SDL_StopTextInput();

  window = SDL_CreateWindow("Chromatic OSD Simulator - 160x128",
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            DISPLAY_HOR_RES, DISPLAY_VER_RES,
                            0);

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  
  /* Set renderer to black background */
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  
  texture = SDL_CreateTexture(renderer,
                              SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STATIC,
                              WINDOW_HOR_RES, WINDOW_VER_RES);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

  /* Create framebuffer - initialize to black (0xFF000000 = opaque black in ARGB) */
  tft_fb = (uint32_t *)malloc(sizeof(uint32_t) * WINDOW_HOR_RES * WINDOW_VER_RES);
  for(int i = 0; i < WINDOW_HOR_RES * WINDOW_VER_RES; i++) {
    tft_fb[i] = 0xFF000000;  /* Opaque black */
  }
}

static void monitor_sdl_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
  int32_t x, y;
  for(y = area->y1; y <= area->y2; y++) {
    for(x = area->x1; x <= area->x2; x++) {
      tft_fb[y * WINDOW_HOR_RES + x] = lv_color_to32(*color_p);
      color_p++;
    }
  }

  /* Update the texture and scale it up */
  SDL_UpdateTexture(texture, NULL, tft_fb, WINDOW_HOR_RES * sizeof(uint32_t));
  SDL_RenderClear(renderer);
  /* Scale the texture to fill the window */
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);

  lv_disp_flush_ready(disp_drv);
}

static void sdl_mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
  (void)indev_drv;

  /* Scale mouse coordinates back to logical resolution */
  data->point.x = mouse_x / WINDOW_SCALE;
  data->point.y = mouse_y / WINDOW_SCALE;
  data->state = mouse_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

static uint32_t custom_tick_get(void)
{
  static uint64_t start_ms = 0;
  if(start_ms == 0) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    start_ms = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
  }

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t now_ms = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);

  return (uint32_t)(now_ms - start_ms);
}
