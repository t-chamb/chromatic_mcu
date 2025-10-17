#include "embedded_modules.h"

// Embedded module data
extern const uint8_t hello_mod_start[] asm("_binary_hello_mod_start");
extern const uint8_t hello_mod_end[] asm("_binary_hello_mod_end");

const embedded_module_t g_embedded_modules[] = {
    {
        .name = "hello",
        .data = hello_mod_start,
        .size = 0  // Will be calculated
    }
};

const size_t g_num_embedded_modules = sizeof(g_embedded_modules) / sizeof(embedded_module_t);

size_t embedded_module_get_size(size_t index)
{
    if (index >= g_num_embedded_modules) {
        return 0;
    }
    
    if (index == 0) {
        return hello_mod_end - hello_mod_start;
    }
    
    return 0;
}
