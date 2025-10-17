#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_log.h"
#include "esp_partition.h"
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
    printf("  Free heap: %lu bytes\n", esp_get_free_heap_size());
    printf("  Min free heap: %lu bytes\n\n", esp_get_minimum_free_heap_size());

    return 0;
}

static int do_module_write_flash(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: modwriteflash <filepath>\n");
        printf("Write a module from SD card to flash partition\n");
        return 1;
    }

    const char *filepath = argv[1];
    
    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        printf("✗ Failed to open file: %s\n", filepath);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    printf("Writing module to flash (%zu bytes)...\n", file_size);
    
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "modules");
    
    if (partition == NULL) {
        printf("✗ Modules partition not found\n");
        fclose(f);
        return 1;
    }
    
    printf("Partition size: %lu bytes\n", partition->size);
    
    if (file_size > partition->size) {
        printf("✗ Module too large for partition\n");
        fclose(f);
        return 1;
    }
    
    uint8_t *buffer = malloc(file_size);
    if (buffer == NULL) {
        printf("✗ Failed to allocate buffer\n");
        fclose(f);
        return 1;
    }
    
    if (fread(buffer, file_size, 1, f) != 1) {
        printf("✗ Failed to read file\n");
        free(buffer);
        fclose(f);
        return 1;
    }
    fclose(f);
    
    printf("Erasing flash partition...\n");
    esp_err_t ret = esp_partition_erase_range(partition, 0, partition->size);
    if (ret != ESP_OK) {
        printf("✗ Failed to erase partition: %s\n", esp_err_to_name(ret));
        free(buffer);
        return 1;
    }
    
    printf("Writing to flash...\n");
    ret = esp_partition_write(partition, 0, buffer, file_size);
    if (ret != ESP_OK) {
        printf("✗ Failed to write to flash: %s\n", esp_err_to_name(ret));
        free(buffer);
        return 1;
    }
    
    free(buffer);
    
    printf("✓ Module written to flash successfully\n");
    printf("  Size: %zu bytes\n", file_size);
    printf("  Use 'modloadflash' to load it\n");
    
    return 0;
}

static int do_module_load_flash(int argc, char **argv)
{
    printf("Loading module from flash partition...\n");
    
    module_handle_t handle;
    esp_err_t ret = module_load_from_flash(0, &handle);
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

    const esp_console_cmd_t write_flash_cmd = {
        .command = "modwriteflash",
        .help = "Write a module from SD card to flash partition",
        .hint = NULL,
        .func = &do_module_write_flash,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&write_flash_cmd));

    const esp_console_cmd_t load_flash_cmd = {
        .command = "modloadflash",
        .help = "Load a module from flash partition",
        .hint = NULL,
        .func = &do_module_load_flash,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&load_flash_cmd));

    const esp_console_cmd_t load_embedded_cmd = {
        .command = "modloadem",
        .help = "Load an embedded module",
        .hint = NULL,
        .func = &do_module_load_embedded,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&load_embedded_cmd));

    ESP_LOGI(TAG, "Module commands registered");
}
