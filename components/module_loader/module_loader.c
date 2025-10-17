#include "module_loader.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "module_loader";

#define MAX_MODULES 8
#define MAX_CORE_FUNCTIONS 64

// Internal module structure
typedef struct {
    bool in_use;
    module_header_t header;
    void *code_mem;           // Allocated memory for code
    void *data_mem;           // Allocated memory for data
    module_init_fn_t init_fn;
    module_exit_fn_t exit_fn;
} module_t;

// Core function registry entry
typedef struct {
    char name[32];
    void *func;
} core_function_t;

// Module loader state
static struct {
    bool initialized;
    module_t modules[MAX_MODULES];
    core_function_t core_functions[MAX_CORE_FUNCTIONS];
    size_t num_core_functions;
    size_t total_memory_allocated;
} g_loader = {0};

esp_err_t module_loader_init(void)
{
    if (g_loader.initialized) {
        ESP_LOGW(TAG, "Module loader already initialized");
        return ESP_OK;
    }

    memset(&g_loader, 0, sizeof(g_loader));
    g_loader.initialized = true;

    ESP_LOGI(TAG, "Module loader initialized");
    ESP_LOGI(TAG, "Max modules: %d, Max core functions: %d", MAX_MODULES, MAX_CORE_FUNCTIONS);

    return ESP_OK;
}

static int find_free_slot(void)
{
    for (int i = 0; i < MAX_MODULES; i++) {
        if (!g_loader.modules[i].in_use) {
            return i;
        }
    }
    return -1;
}

static esp_err_t validate_module_header(const module_header_t *header)
{
    if (header->magic != MODULE_MAGIC) {
        ESP_LOGE(TAG, "Invalid module magic: 0x%08lx", header->magic);
        return ESP_ERR_INVALID_ARG;
    }

    if (header->code_size == 0) {
        ESP_LOGE(TAG, "Module has no code");
        return ESP_ERR_INVALID_SIZE;
    }

    if (header->code_size > 2 * 1024 * 1024) {  // 2MB max
        ESP_LOGE(TAG, "Module code too large: %lu bytes", header->code_size);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "Module: %s", header->name);
    ESP_LOGI(TAG, "  Version: %lu", header->version);
    ESP_LOGI(TAG, "  Code: %lu bytes", header->code_size);
    ESP_LOGI(TAG, "  Data: %lu bytes", header->data_size);
    ESP_LOGI(TAG, "  BSS: %lu bytes", header->bss_size);

    return ESP_OK;
}

esp_err_t module_load_from_sd(const char *filepath, module_handle_t *handle)
{
    if (!g_loader.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (filepath == NULL || handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int slot = find_free_slot();
    if (slot < 0) {
        ESP_LOGE(TAG, "No free module slots");
        return ESP_ERR_NO_MEM;
    }

    module_t *mod = &g_loader.modules[slot];

    // Open module file
    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open module file: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }

    // Read header
    if (fread(&mod->header, sizeof(module_header_t), 1, f) != 1) {
        ESP_LOGE(TAG, "Failed to read module header");
        fclose(f);
        return ESP_FAIL;
    }

    // Validate header
    esp_err_t ret = validate_module_header(&mod->header);
    if (ret != ESP_OK) {
        fclose(f);
        return ret;
    }

    // Allocate and read code
    mod->code_mem = heap_caps_malloc(mod->header.code_size, MALLOC_CAP_EXEC);
    if (mod->code_mem == NULL) {
        ESP_LOGE(TAG, "Failed to allocate code memory");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    if (fread(mod->code_mem, mod->header.code_size, 1, f) != 1) {
        ESP_LOGE(TAG, "Failed to read module code");
        free(mod->code_mem);
        fclose(f);
        return ESP_FAIL;
    }

    // Allocate and read data if needed
    if (mod->header.data_size > 0) {
        mod->data_mem = malloc(mod->header.data_size + mod->header.bss_size);
        if (mod->data_mem == NULL) {
            ESP_LOGE(TAG, "Failed to allocate data memory");
            free(mod->code_mem);
            fclose(f);
            return ESP_ERR_NO_MEM;
        }

        if (fread(mod->data_mem, mod->header.data_size, 1, f) != 1) {
            ESP_LOGE(TAG, "Failed to read module data");
            free(mod->code_mem);
            free(mod->data_mem);
            fclose(f);
            return ESP_FAIL;
        }

        // Zero BSS
        if (mod->header.bss_size > 0) {
            memset((uint8_t*)mod->data_mem + mod->header.data_size, 0, mod->header.bss_size);
        }
    }

    fclose(f);

    // Set function pointers
    mod->init_fn = (module_init_fn_t)((uint8_t*)mod->code_mem + mod->header.entry_offset);
    mod->exit_fn = (module_exit_fn_t)((uint8_t*)mod->code_mem + mod->header.exit_offset);

    mod->in_use = true;
    g_loader.total_memory_allocated += mod->header.code_size + mod->header.data_size + mod->header.bss_size;

    ESP_LOGI(TAG, "Module '%s' loaded from SD", mod->header.name);

    // Call init
    if (mod->init_fn != NULL) {
        ret = mod->init_fn();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Module init failed");
            free(mod->code_mem);
            if (mod->data_mem) free(mod->data_mem);
            mod->in_use = false;
            return ret;
        }
    }

    *handle = (module_handle_t)(intptr_t)slot;
    return ESP_OK;
}

esp_err_t module_unload(module_handle_t handle)
{
    int slot = (int)(intptr_t)handle;
    
    if (slot < 0 || slot >= MAX_MODULES) {
        return ESP_ERR_INVALID_ARG;
    }

    module_t *mod = &g_loader.modules[slot];
    
    if (!mod->in_use) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Unloading module '%s'", mod->header.name);

    // Call exit function
    if (mod->exit_fn != NULL) {
        ESP_LOGI(TAG, "Calling module exit function...");
        mod->exit_fn();
    }

    // Free memory
    size_t freed = mod->header.code_size + mod->header.data_size + mod->header.bss_size;
    
    if (mod->code_mem) {
        free(mod->code_mem);
        mod->code_mem = NULL;
    }
    
    if (mod->data_mem) {
        free(mod->data_mem);
        mod->data_mem = NULL;
    }

    g_loader.total_memory_allocated -= freed;
    mod->in_use = false;

    ESP_LOGI(TAG, "Module unloaded, freed %zu bytes", freed);
    ESP_LOGI(TAG, "Total memory allocated: %zu bytes", g_loader.total_memory_allocated);

    return ESP_OK;
}

esp_err_t module_get_info(module_handle_t handle, module_info_t *info)
{
    int slot = (int)(intptr_t)handle;
    
    if (slot < 0 || slot >= MAX_MODULES || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    module_t *mod = &g_loader.modules[slot];
    
    if (!mod->in_use) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(info->name, mod->header.name, sizeof(info->name) - 1);
    strncpy(info->description, mod->header.description, sizeof(info->description) - 1);
    info->version = mod->header.version;
    info->memory_used = mod->header.code_size + mod->header.data_size + mod->header.bss_size;
    info->load_address = mod->code_mem;
    
    // Determine size class
    if (info->memory_used <= MODULE_SLOT_64K) {
        info->size_class = MODULE_SLOT_64K;
    } else if (info->memory_used <= MODULE_SLOT_128K) {
        info->size_class = MODULE_SLOT_128K;
    } else if (info->memory_used <= MODULE_SLOT_256K) {
        info->size_class = MODULE_SLOT_256K;
    } else if (info->memory_used <= MODULE_SLOT_512K) {
        info->size_class = MODULE_SLOT_512K;
    } else {
        info->size_class = MODULE_SLOT_1024K;
    }
    
    info->is_loaded = true;

    return ESP_OK;
}

esp_err_t module_list_loaded(module_info_t *info_array, size_t max_modules, size_t *num_modules)
{
    if (info_array == NULL || num_modules == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t count = 0;
    for (int i = 0; i < MAX_MODULES && count < max_modules; i++) {
        if (g_loader.modules[i].in_use) {
            module_get_info((module_handle_t)(intptr_t)i, &info_array[count]);
            count++;
        }
    }

    *num_modules = count;
    return ESP_OK;
}

esp_err_t module_register_core_function(const char *name, void *func)
{
    if (name == NULL || func == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_loader.num_core_functions >= MAX_CORE_FUNCTIONS) {
        ESP_LOGE(TAG, "Core function registry full");
        return ESP_ERR_NO_MEM;
    }

    core_function_t *entry = &g_loader.core_functions[g_loader.num_core_functions];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->func = func;
    g_loader.num_core_functions++;

    ESP_LOGD(TAG, "Registered core function: %s at %p", name, func);

    return ESP_OK;
}

esp_err_t module_load_from_memory(const uint8_t *data, size_t size, module_handle_t *handle)
{
    if (!g_loader.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || handle == NULL || size < sizeof(module_header_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    int slot = find_free_slot();
    if (slot < 0) {
        ESP_LOGE(TAG, "No free module slots");
        return ESP_ERR_NO_MEM;
    }

    module_t *mod = &g_loader.modules[slot];

    ESP_LOGI(TAG, "Loading from memory: data=%p, size=%zu", data, size);
    
    // Check if data is in flash (ROM) - addresses 0x40000000-0x40400000
    bool is_flash = ((uintptr_t)data >= 0x40000000 && (uintptr_t)data < 0x40400000) ||
                    ((uintptr_t)data >= 0x3F400000 && (uintptr_t)data < 0x3F800000);
    
    if (is_flash) {
        ESP_LOGI(TAG, "Data is in flash, using byte-by-byte copy");
    }

    // Copy header from memory - use byte copy for safety with flash
    // Flash memory can have alignment issues with word access
    uint8_t *dst = (uint8_t *)&mod->header;
    for (size_t i = 0; i < sizeof(module_header_t); i++) {
        dst[i] = data[i];
    }

    // Validate header
    esp_err_t ret = validate_module_header(&mod->header);
    if (ret != ESP_OK) {
        return ret;
    }

    // For embedded modules in flash, use the code directly (flash is executable)
    // For SD card modules, we'd need IRAM but that's limited
    // Solution: For now, embedded modules execute from flash, SD modules are for data only
    if (is_flash) {
        ESP_LOGI(TAG, "Module code is in flash (executable), using direct pointer");
        mod->code_mem = (void *)(data + sizeof(module_header_t));
    } else {
        // SD card modules: allocate in regular DRAM
        // Note: Code won't be executable, but data/assets can still be loaded
        ESP_LOGW(TAG, "Allocating code in DRAM (not executable)");
        mod->code_mem = malloc(mod->header.code_size);
        if (mod->code_mem == NULL) {
            ESP_LOGE(TAG, "Failed to allocate %lu bytes for code", mod->header.code_size);
            return ESP_ERR_NO_MEM;
        }
        
        const uint8_t *code_src = data + sizeof(module_header_t);
        uint8_t *code_dst = (uint8_t *)mod->code_mem;
        for (size_t i = 0; i < mod->header.code_size; i++) {
            code_dst[i] = code_src[i];
        }
    }
    const uint8_t *code_src = data + sizeof(module_header_t);
    uint8_t *code_dst = (uint8_t *)mod->code_mem;
    for (size_t i = 0; i < mod->header.code_size; i++) {
        code_dst[i] = code_src[i];
    }

    // Allocate and copy data if needed
    if (mod->header.data_size > 0) {
        mod->data_mem = malloc(mod->header.data_size + mod->header.bss_size);
        if (mod->data_mem == NULL) {
            ESP_LOGE(TAG, "Failed to allocate data memory");
            free(mod->code_mem);
            return ESP_ERR_NO_MEM;
        }

        ESP_LOGI(TAG, "Copying %lu bytes of data", mod->header.data_size);
        const uint8_t *data_src = data + sizeof(module_header_t) + mod->header.code_size;
        uint8_t *data_dst = (uint8_t *)mod->data_mem;
        for (size_t i = 0; i < mod->header.data_size; i++) {
            data_dst[i] = data_src[i];
        }

        // Zero BSS
        if (mod->header.bss_size > 0) {
            memset((uint8_t*)mod->data_mem + mod->header.data_size, 0, mod->header.bss_size);
        }
    }

    // Set function pointers
    mod->init_fn = (module_init_fn_t)((uint8_t*)mod->code_mem + mod->header.entry_offset);
    mod->exit_fn = (module_exit_fn_t)((uint8_t*)mod->code_mem + mod->header.exit_offset);

    mod->in_use = true;
    g_loader.total_memory_allocated += mod->header.code_size + mod->header.data_size + mod->header.bss_size;

    ESP_LOGI(TAG, "Module '%s' loaded from memory", mod->header.name);

    // Call init
    if (mod->init_fn != NULL) {
        ret = mod->init_fn();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Module init failed");
            free(mod->code_mem);
            if (mod->data_mem) free(mod->data_mem);
            mod->in_use = false;
            return ret;
        }
    }

    *handle = (module_handle_t)(intptr_t)slot;
    return ESP_OK;
}

esp_err_t module_get_memory_stats(size_t *total_allocated, size_t *num_modules)
{
    if (total_allocated == NULL || num_modules == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *total_allocated = g_loader.total_memory_allocated;
    
    size_t count = 0;
    for (int i = 0; i < MAX_MODULES; i++) {
        if (g_loader.modules[i].in_use) {
            count++;
        }
    }
    *num_modules = count;

    return ESP_OK;
}
