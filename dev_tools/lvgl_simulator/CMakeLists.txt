cmake_minimum_required(VERSION 3.10)
project(lvgl)

option(LV_USE_DRAW_SDL "Use SDL draw unit" OFF)
option(LV_USE_LIBPNG "Use libpng to decode PNG" OFF)
option(LV_USE_LIBJPEG_TURBO "Use libjpeg turbo to decode JPEG" OFF)
option(LV_USE_FFMPEG "Use libffmpeg to display video using lv_ffmpeg" OFF)
option(LV_USE_FREETYPE "Use freetype lib" OFF)

set(CMAKE_C_STANDARD 99)#C99 # lvgl officially support C99 and above
set(CMAKE_CXX_STANDARD 17)#C17
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

find_package(SDL2 REQUIRED SDL2)

add_compile_definitions($<$<BOOL:${LV_USE_DRAW_SDL}>:LV_USE_DRAW_SDL=1>)
add_compile_definitions($<$<BOOL:${LV_USE_LIBPNG}>:LV_USE_LIBPNG=1>)
add_compile_definitions($<$<BOOL:${LV_USE_LIBJPEG_TURBO}>:LV_USE_LIBJPEG_TURBO=1>)
add_compile_definitions($<$<BOOL:${LV_USE_FFMPEG}>:LV_USE_FFMPEG=1>)
add_compile_definitions(SIMULATOR_BUILD)

add_subdirectory(lvgl)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS})

# Chromatic component paths
set(CHROMATIC_ROOT ${PROJECT_SOURCE_DIR}/../..)
set(COMPONENTS_DIR ${CHROMATIC_ROOT}/components)
set(MAIN_DIR ${CHROMATIC_ROOT}/main)

# Chromatic OSD sources
file(GLOB_RECURSE OSD_SOURCES 
    ${COMPONENTS_DIR}/osd/*.c
    ${COMPONENTS_DIR}/menu_mgr/*.c
    ${COMPONENTS_DIR}/dlist/*.c
    ${COMPONENTS_DIR}/mutex/*.c
    ${COMPONENTS_DIR}/tab/*.c
    ${COMPONENTS_DIR}/common/*.c
    ${COMPONENTS_DIR}/battery/*.c
)

# WiFi file server sources - exclude for now, too many dependencies
# file(GLOB WIFI_SOURCES
#     ${COMPONENTS_DIR}/wifi_file_server/wifi_file_server.c
# )

# Exclude settings for now - has compilation issues
# file(GLOB SETTINGS_SOURCES ${COMPONENTS_DIR}/settings/*.c)
# list(APPEND OSD_SOURCES ${SETTINGS_SOURCES})

# Main sources
file(GLOB MAIN_SOURCES
    ${MAIN_DIR}/osd_default.c
)

# Image sources - using real images
file(GLOB IMAGE_SOURCES
    ${COMPONENTS_DIR}/images/*.c
)

# Font sources
file(GLOB FONT_SOURCES
    ${COMPONENTS_DIR}/fonts/*.c
)

add_executable(main 
    main.c 
    mouse_cursor_icon.c 
    chromatic_test.c
    esp_stubs.c
    ${OSD_SOURCES}
    ${MAIN_SOURCES}
    ${IMAGE_SOURCES}
    ${FONT_SOURCES}
    # ${WIFI_SOURCES}
)

# Include directories - add all component dirs
target_include_directories(main PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${MAIN_DIR}
    ${COMPONENTS_DIR}/battery
    ${COMPONENTS_DIR}/button
    ${COMPONENTS_DIR}/common
    ${COMPONENTS_DIR}/crc
    ${COMPONENTS_DIR}/dlist
    ${COMPONENTS_DIR}/fonts
    ${COMPONENTS_DIR}/images
    ${COMPONENTS_DIR}/menu_mgr
    ${COMPONENTS_DIR}/mutex
    ${COMPONENTS_DIR}/osd
    ${COMPONENTS_DIR}/osd/controls
    ${COMPONENTS_DIR}/osd/display
    ${COMPONENTS_DIR}/osd/palette
    ${COMPONENTS_DIR}/osd/status
    ${COMPONENTS_DIR}/osd/system
    ${COMPONENTS_DIR}/sd_spi
    ${COMPONENTS_DIR}/settings
    ${COMPONENTS_DIR}/tab
    ${COMPONENTS_DIR}/tab/tab_list
    ${COMPONENTS_DIR}/tab/tab_dots
    ${COMPONENTS_DIR}/tab/tab_table
    ${COMPONENTS_DIR}/wifi_file_server
    ${COMPONENTS_DIR}/wifi_file_server/include
)

target_compile_definitions(main PRIVATE 
    LV_CONF_INCLUDE_SIMPLE
    SIMULATOR_BUILD=1
    CONFIG_CHROMATIC_FW_VER_STR="v1.0.0-sim"
)

target_link_libraries(main lvgl::examples lvgl::demos ${SDL2_LIBRARIES} m pthread)
add_custom_target (run COMMAND ${EXECUTABLE_OUTPUT_PATH}/main DEPENDS main)

if(LV_USE_DRAW_SDL)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
    # Need to install libsdl2-image-dev
    # `sudo apt install libsdl2-image-dev`
    # `brew install sdl2_image`
    find_package(SDL2_image REQUIRED)
    target_include_directories(lvgl PUBLIC ${SDL2_IMAGE_INCLUDE_DIRS})
    target_link_libraries(main ${SDL2_IMAGE_LIBRARIES})
endif(LV_USE_DRAW_SDL)

if(LV_USE_LIBPNG)
    find_package(PNG REQUIRED)
    target_include_directories(lvgl PUBLIC ${PNG_INCLUDE_DIR})
    target_link_libraries(main ${PNG_LIBRARY})
endif(LV_USE_LIBPNG)

if(LV_USE_LIBJPEG_TURBO)
    # Need to install libjpeg-turbo8-dev
    # `sudo apt install libjpeg-turbo8-dev`
    # `brew install libjpeg-turbo`
    find_package(JPEG REQUIRED)
    target_include_directories(lvgl PUBLIC ${JPEG_INCLUDE_DIRS})
    target_link_libraries(main ${JPEG_LIBRARIES})
endif(LV_USE_LIBJPEG_TURBO)

if(LV_USE_FFMPEG)
    target_link_libraries(main avformat avcodec avutil swscale)
endif(LV_USE_FFMPEG)

if(LV_USE_FREETYPE)
    find_package(Freetype REQUIRED)
    target_link_libraries(main ${FREETYPE_LIBRARIES})
    target_include_directories(lvgl PUBLIC ${FREETYPE_INCLUDE_DIRS})
endif(LV_USE_FREETYPE)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(lvgl PRIVATE
        -pedantic-errors
        -Wall
        -Wclobbered
        -Wdeprecated
        -Wdouble-promotion
        -Wempty-body
        -Wextra
        -Wformat-security
        -Wmaybe-uninitialized
        # -Wmissing-prototypes
        -Wpointer-arith
        -Wmultichar
        -Wno-pedantic # ignored for now, we convert functions to pointers for propertis table.
        -Wreturn-type
        -Wshadow
        -Wshift-negative-value
        -Wsizeof-pointer-memaccess
        -Wtype-limits
        -Wundef
        -Wuninitialized
        -Wunreachable-code
        -Wfloat-conversion
        -Wstrict-aliasing
    )

    target_compile_options(main PRIVATE -fsanitize=address,leak,undefined)
    target_link_options(main PRIVATE -fsanitize=address,leak,undefined)
endif()
