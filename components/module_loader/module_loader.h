#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Module magic number "MODU"
#define MODULE_MAGIC 0x4D4F4455

// Module header structure
typedef struct {
    uint32_t magic;           // Must be MODULE_MAGIC
    uint32_t version;         // Module format version
    uint32_t code_size;       // Size of code section
    uint32_t data_size;       // Size of data section
    uint32_t bss_size;        // Size of BSS section
    uint32_t entry_offset;    // Offset to init function
    uint32_t exit_offset;     // Offset to cleanup function
    char name[32];            // Module name
    char description[64];     // Module description
    uint32_t reserved[8];     // Reserved for future use
} __attribute__((packed)) module_header_t;

// Module handle (opaque)
typedef void* module_handle_t;

// Module init/exit function signatures
typedef esp_err_t (*module_init_fn_t)(void);
typedef void (*module_exit_fn_t)(void);

// Module size categories
typedef enum {
    MODULE_SLOT_64K   = 64 * 1024,     // 64KB
    MODULE_SLOT_128K  = 128 * 1024,    // 128KB
    MODULE_SLOT_256K  = 256 * 1024,    // 256KB
    MODULE_SLOT_512K  = 512 * 1024,    // 512KB
    MODULE_SLOT_1024K = 1024 * 1024    // 1MB
} module_size_t;

// Module info structure
typedef struct {
    char name[32];
    char description[64];
    uint32_t version;
    size_t memory_used;
    void *load_address;      // Where module is loaded in RAM
    module_size_t size_class; // Size category
    bool is_loaded;
} module_info_t;

/**
 * @brief Initialize the module loader system
 * 
 * @return ESP_OK on success
 */
esp_err_t module_loader_init(void);

/**
 * @brief Load a module from memory (embedded modules)
 * 
 * @param data Pointer to module data in memory
 * @param size Size of module data
 * @param handle Output handle to loaded module
 * @return ESP_OK on success
 */
esp_err_t module_load_from_memory(const uint8_t *data, size_t size, module_handle_t *handle);

/**
 * @brief Load a module from SD card
 * 
 * @param filepath Path to module file on SD card
 * @param handle Output handle to loaded module
 * @return ESP_OK on success
 */
esp_err_t module_load_from_sd(const char *filepath, module_handle_t *handle);

/**
 * @brief Unload a module and free its memory
 * 
 * @param handle Module handle
 * @return ESP_OK on success
 */
esp_err_t module_unload(module_handle_t handle);

/**
 * @brief Get information about a loaded module
 * 
 * @param handle Module handle
 * @param info Output module info
 * @return ESP_OK on success
 */
esp_err_t module_get_info(module_handle_t handle, module_info_t *info);

/**
 * @brief List all loaded modules
 * 
 * @param info_array Array to store module info
 * @param max_modules Maximum number of modules to list
 * @param num_modules Output number of modules found
 * @return ESP_OK on success
 */
esp_err_t module_list_loaded(module_info_t *info_array, size_t max_modules, size_t *num_modules);

/**
 * @brief Register a core function that modules can call
 * 
 * @param name Function name
 * @param func Function pointer
 * @return ESP_OK on success
 */
esp_err_t module_register_core_function(const char *name, void *func);

/**
 * @brief Get memory statistics for module system
 * 
 * @param total_allocated Output total memory allocated
 * @param num_modules Output number of loaded modules
 * @return ESP_OK on success
 */
esp_err_t module_get_memory_stats(size_t *total_allocated, size_t *num_modules);

#ifdef __cplusplus
}
#endif
