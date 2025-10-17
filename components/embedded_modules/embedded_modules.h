#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    const uint8_t *data;
    size_t size;
} embedded_module_t;

extern const embedded_module_t g_embedded_modules[];
extern const size_t g_num_embedded_modules;

size_t embedded_module_get_size(size_t index);

#ifdef __cplusplus
}
#endif
