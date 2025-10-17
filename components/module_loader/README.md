# Dynamic Module Loader

A runtime module loading system for ESP32 that allows loading and unloading code modules dynamically from flash or SD card.

## Features

- Load modules from flash partitions or SD card
- Unload modules to free memory
- Module isolation with separate code/data sections
- Init/exit callbacks for module lifecycle
- Memory statistics and module listing
- Core function registry for module-to-core communication

## Module Format

Modules consist of a header followed by code and data sections:

```
[Module Header] [Code Section] [Data Section]
```

### Module Header Structure

```c
typedef struct {
    uint32_t magic;           // 0x4D4F4455 "MODU"
    uint32_t version;         // Module format version
    uint32_t code_size;       // Size of code section
    uint32_t data_size;       // Size of initialized data
    uint32_t bss_size;        // Size of uninitialized data
    uint32_t entry_offset;    // Offset to init function
    uint32_t exit_offset;     // Offset to cleanup function
    char name[32];            // Module name
    char description[64];     // Module description
} module_header_t;
```

## Usage

### Initialize the Module System

```c
#include "module_loader.h"

esp_err_t ret = module_loader_init();
```

### Load a Module from SD Card

```c
module_handle_t handle;
esp_err_t ret = module_load_from_sd("/sdcard/modules/wifi.mod", &handle);
if (ret == ESP_OK) {
    printf("Module loaded successfully\n");
}
```

### Load a Module from Flash

```c
module_handle_t handle;
// Load from offset 0x0 in the "modules" partition
esp_err_t ret = module_load_from_flash(0x0, &handle);
```

### Unload a Module

```c
esp_err_t ret = module_unload(handle);
```

### List Loaded Modules

```c
module_info_t modules[8];
size_t num_modules;

module_list_loaded(modules, 8, &num_modules);
for (size_t i = 0; i < num_modules; i++) {
    printf("Module: %s (v%lu) - %zu bytes\n",
           modules[i].name,
           modules[i].version,
           modules[i].memory_used);
}
```

## Console Commands

- `modlist` - List all loaded modules
- `modload <path>` - Load a module from SD card
- `modunload <name>` - Unload a module by name
- `modstats` - Show memory statistics

## Creating Modules

### Module Structure

A module must implement init and exit functions:

```c
#include "esp_err.h"

// Module init - called when loaded
esp_err_t module_init(void) {
    // Initialize your module
    return ESP_OK;
}

// Module exit - called when unloaded
void module_exit(void) {
    // Cleanup your module
}
```

### Building Modules

Modules need to be compiled as position-independent code and packaged with the proper header. A build tool will be provided to automate this process.

## Memory Considerations

- Code sections are allocated with `MALLOC_CAP_EXEC` (executable memory)
- Data sections use regular heap allocation
- BSS sections are zero-initialized
- Maximum 8 modules can be loaded simultaneously
- Each module can be up to 2MB in size

## Flash Partition Layout

Add a "modules" partition to your `partitions.csv`:

```csv
# Name,     Type, SubType, Offset,  Size
modules,    data, 0x81,    0x400000, 2M
```

## Future Enhancements

- Symbol resolution and relocation
- Module dependencies
- Versioning and compatibility checks
- Signed modules for security
- Hot-reload support
- Module marketplace/repository

## Example: WiFi Module

The WiFi file server has been converted to a loadable module as a reference implementation. See `components/module_wifi/` for details.
