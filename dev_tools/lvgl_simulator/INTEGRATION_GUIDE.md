# Chromatic OSD Integration Guide

## Current Status

The simulator is set up with:
- ✅ LVGL 8.4 working
- ✅ SDL display and input
- ✅ Mock menu showing structure
- ✅ ESP-IDF stubs for logging

## To Integrate Real OSD Code

### Step 1: Add OSD Source Files

Edit `CMakeLists.txt` and add your OSD sources:

```cmake
# Add OSD source files
set(OSD_SOURCES
    ../chromatic_components/osd/osd.c
    ../chromatic_components/osd/palette/style.c
    # Add more source files as needed
)

add_executable(main 
    main.c 
    mouse_cursor_icon.c 
    chromatic_test.c
    esp_stubs.c
    ${OSD_SOURCES}
)
```

### Step 2: Add Include Paths

```cmake
target_include_directories(main PRIVATE
    ../chromatic_components/osd
    ../chromatic_components/fonts
    ../chromatic_components/images
    ../chromatic_components/settings
    ../chromatic_components/dlist
    # Add other component includes
)
```

### Step 3: Mock Hardware Dependencies

Create stubs for hardware-specific code:

- **Button input**: Mock button presses with keyboard
- **Settings/NVS**: Use file-based storage
- **Battery**: Return fake values
- **Display hardware**: Already handled by SDL

### Step 4: Handle Resolution

Your OSD is 160x144, simulator is 800x480. Options:

1. **Scale container** (current approach): 3x scale in center
2. **Modify OSD**: Make resolution configurable
3. **Virtual display**: Create 160x144 buffer, scale to window

### Step 5: Incremental Integration

Start small:
1. Get OSD_Initialize() working
2. Add one simple widget (e.g., battery icon)
3. Add menu navigation
4. Add full menu system
5. Add all widgets

## Example: Adding a Widget

```c
// In chromatic_test.c
#include "../chromatic_components/osd/osd.h"
#include "../chromatic_components/osd/status/brightness.h"

void chromatic_osd_init(void)
{
    // Initialize OSD system
    OSD_Initialize();
    
    // Create and add a widget
    static OSD_Widget_t brightness_widget = {
        .Name = "Brightness",
    };
    Brightness_Init(&brightness_widget);
    OSD_AddWidget(&brightness_widget);
    
    // Draw OSD
    lv_obj_t * scr = lv_scr_act();
    OSD_Draw(scr);
}
```

## Challenges

1. **ESP-IDF Dependencies**: Need stubs for all ESP functions
2. **Hardware Access**: Mock GPIO, I2C, SPI, etc.
3. **FreeRTOS**: Mock tasks, queues, semaphores
4. **NVS Storage**: Use file-based alternative
5. **Component Dependencies**: May need to mock entire components

## Recommended Approach

For fastest results:
1. Keep using the mock menu for UI layout testing
2. Copy individual widget rendering code to test specific features
3. Use actual hardware for full integration testing
4. Use simulator for rapid UI iteration and visual design

The simulator is best for:
- Testing UI layouts
- Trying color schemes
- Prototyping new widgets
- Visual design iteration

Use hardware for:
- Full system integration
- Performance testing
- Hardware-specific features
- Final validation
