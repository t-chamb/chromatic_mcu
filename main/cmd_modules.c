#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_log.h"
#include "module_loader.h"

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
    printf("%-20s %-10s %-12s %s\n", "Name", "Version", "Memory", "Description");
    printf("─────────────────────────────────────────────────────────────────\n");

    for (size_t i = 0; i < num_modules; i++) {
        printf("%-20s %-10lu %-12zu %s\n",
               modules[i].name,
               modules[i].version,
               modules[i].memory_used,
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
    printf("  Free heap: %lu bytes\n", esp_get_free_heap_size());
    printf("  Min free heap: %lu bytes\n\n", esp_get_minimum_free_heap_size());

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

    ESP_LOGI(TAG, "Module commands registered");
}
