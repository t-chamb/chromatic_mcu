#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "module_loader.h"
#include "embedded_modules.h"

static const char *TAG = "cmd_modules";

static int do_module_list(int argc, char **argv)
{
    module_info_t modules[8];
    size_t num_modules = 0;
    
    esp_err_t ret = module_list_loaded(modules, 8, &num_modules);
    if (ret != ESP_OK) {
        printf("Error listing modules: %s\n", esp_err_to_name(ret));
        return 1;
    }

    if (num_modules == 0) {
        printf("No modules loaded\n");
        return 0;
    }

    printf("\nLoaded Modules:\n");
    printf("%-20s %-8s %-10s %-12s %-12s %s\n", 
           "Name", "Version", "Size Class", "Memory", "Address", "Description");
    printf("────────────────────────────────────────────────────────────────────────────\n");

    for (size_t i = 0; i < num_modules; i++) {
        const char *size_str;
        switch (modules[i].size_class) {
            case MODULE_SLOT_64K:   size_str = "64K"; break;
            case MODULE_SLOT_128K:  size_str = "128K"; break;
            case MODULE_SLOT_256K:  size_str = "256K"; break;
            case MODULE_SLOT_512K:  size_str = "512K"; break;
            case MODULE_SLOT_1024K: size_str = "1024K"; break;
            default: size_str = "???"; break;
        }
        
        printf("%-20s %-8lu %-10s %-12zu 0x%08lx   %s\n",
               modules[i].name,
               modules[i].version,
               size_str,
               modules[i].memory_used,
               (unsigned long)modules[i].load_address,
               modules[i].description);
    }

    size_t total_mem, total_mods;
    module_get_memory_stats(&total_mem, &total_mods);
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("Total: %zu modules, %zu bytes\n\n", total_mods, total_mem);

    return 0;
}

static struct {
    struct arg_str *path;
    struct arg_end *end;
} load_args;

static int do_module_load(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&load_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, load_args.end, argv[0]);
        return 1;
    }

    const char *path = load_args.path->sval[0];
    module_handle_t handle;

    printf("Loading module from: %s\n", path);

    esp_err_t ret = module_load_from_sd(path, &handle);
    if (ret != ESP_OK) {
        printf("✗ Failed to load module: %s\n", esp_err_to_name(ret));
        return 1;
    }

    module_info_t info;
    module_get_info(handle, &info);

    printf("✓ Module loaded successfully\n");
    printf("  Name: %s\n", info.name);
    printf("  Version: %lu\n", info.version);
    printf("  Memory: %zu bytes\n", info.memory_used);
    printf("  Description: %s\n", info.description);

    return 0;
}

static struct {
    struct arg_str *name;
    struct arg_end *end;
} unload_args;

static int do_module_unload(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&unload_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, unload_args.end, argv[0]);
        return 1;
    }

    const char *name = unload_args.name->sval[0];

    // Find module by name
    module_info_t modules[8];
    size_t num_modules = 0;
    module_list_loaded(modules, 8, &num_modules);

    module_handle_t handle = NULL;
    for (size_t i = 0; i < num_modules; i++) {
        if (strcmp(modules[i].name, name) == 0) {
            handle = (module_handle_t)(intptr_t)i;
            break;
        }
    }

    if (handle == NULL) {
        printf("Module '%s' not found\n", name);
        return 1;
    }

    printf("Unloading module: %s\n", name);

    esp_err_t ret = module_unload(handle);
    if (ret != ESP_OK) {
        printf("✗ Failed to unload module: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("✓ Module unloaded successfully\n");

    return 0;
}

static int do_module_stats(int argc, char **argv)
{
    size_t total_mem, num_modules;
    
    esp_err_t ret = module_get_memory_stats(&total_mem, &num_modules);
    if (ret != ESP_OK) {
        printf("Error getting stats: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("\nModule System Statistics:\n");
    printf("  Loaded modules: %zu\n", num_modules);
    printf("  Total memory: %zu bytes (%.2f KB)\n", total_mem, total_mem / 1024.0);
    printf("\nHeap Memory:\n");
    printf("  Free heap: %lu bytes (%.2f KB)\n", esp_get_free_heap_size(), esp_get_free_heap_size() / 1024.0);
    printf("  Min free heap: %lu bytes (%.2f KB)\n", esp_get_minimum_free_heap_size(), esp_get_minimum_free_heap_size() / 1024.0);
    printf("\nIRAM (Executable):\n");
    printf("  Free IRAM: %zu bytes (%.2f KB)\n", heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT), heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT) / 1024.0);
    printf("  Largest IRAM block: %zu bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_IRAM_8BIT));
    printf("\nDRAM (Data):\n");
    printf("  Free DRAM: %zu bytes (%.2f KB)\n", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024.0);
    printf("  Largest DRAM block: %zu bytes\n\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    return 0;
}

static int do_module_load_embedded(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: modloadem <name>\n");
        printf("Available embedded modules:\n");
        for (size_t i = 0; i < g_num_embedded_modules; i++) {
            printf("  %s\n", g_embedded_modules[i].name);
        }
        return 1;
    }

    const char *name = argv[1];
    
    const embedded_module_t *found = NULL;
    size_t found_index = 0;
    for (size_t i = 0; i < g_num_embedded_modules; i++) {
        if (strcmp(g_embedded_modules[i].name, name) == 0) {
            found = &g_embedded_modules[i];
            found_index = i;
            break;
        }
    }
    
    if (found == NULL) {
        printf("Embedded module '%s' not found\n", name);
        return 1;
    }
    
    size_t size = embedded_module_get_size(found_index);
    printf("Loading embedded module '%s' (%zu bytes)\n", name, size);
    printf("  Data pointer: %p\n", found->data);
    printf("  First 4 bytes: %02x %02x %02x %02x\n", 
           found->data[0], found->data[1], found->data[2], found->data[3]);
    
    module_handle_t handle;
    esp_err_t ret = module_load_from_memory(found->data, size, &handle);
    if (ret != ESP_OK) {
        printf("✗ Failed to load module: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    module_info_t info;
    module_get_info(handle, &info);
    
    printf("✓ Module loaded successfully\n");
    printf("  Name: %s\n", info.name);
    printf("  Version: %lu\n", info.version);
    printf("  Memory: %zu bytes\n", info.memory_used);
    printf("  Description: %s\n", info.description);
    
    return 0;
}

void register_module_commands(void)
{
    const esp_console_cmd_t list_cmd = {
        .command = "modlist",
        .help = "List loaded modules",
        .hint = NULL,
        .func = &do_module_list,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&list_cmd));

    load_args.path = arg_str1(NULL, NULL, "<path>", "Path to module file");
    load_args.end = arg_end(1);

    const esp_console_cmd_t load_cmd = {
        .command = "modload",
        .help = "Load a module from SD card",
        .hint = NULL,
        .func = &do_module_load,
        .argtable = &load_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&load_cmd));

    unload_args.name = arg_str1(NULL, NULL, "<name>", "Module name");
    unload_args.end = arg_end(1);

    const esp_console_cmd_t unload_cmd = {
        .command = "modunload",
        .help = "Unload a module",
        .hint = NULL,
        .func = &do_module_unload,
        .argtable = &unload_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&unload_cmd));

    const esp_console_cmd_t stats_cmd = {
        .command = "modstats",
        .help = "Show module system statistics",
        .hint = NULL,
        .func = &do_module_stats,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&stats_cmd));

    const esp_console_cmd_t load_embedded_cmd = {
        .command = "modloadem",
        .help = "Load an embedded module",
        .hint = NULL,
        .func = &do_module_load_embedded,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&load_embedded_cmd));

    ESP_LOGI(TAG, "Module commands registered");
}
