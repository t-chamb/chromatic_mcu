# Module Development Guide

## Overview

The Chromatic MCU supports dynamic loading of modules at runtime. Modules are compiled as position-independent code (PIC) and can be loaded into any available memory slot.

## Memory Architecture

### Slot Sizes

Modules are allocated into size-based slots:

- **64K**: Lightweight utilities, simple features
- **128K**: Moderate features, basic drivers
- **256K**: Complex features
- **512K**: Heavy applications
- **1024K (1MB)**: Very large apps like WiFi server with full stack

The system automatically selects the appropriate slot size based on your module's requirements.

### Dynamic Allocation

Modules are allocated dynamically from available RAM. The loader tracks:
- Which slots are in use
- Where each module is loaded (address)
- How much memory each module uses

## Creating a Module

### 1. Write Your Module Code

Create a C file with init and exit functions:

```c
#include <stdint.h>

// Called when module loads
int module_init(void) {
    // Initialize your module
    return 0;  // 0 = success
}

// Called when module unloads
void module_exit(void) {
    // Clean up resources
}

// Your module functions
void my_feature(void) {
    // Implementation
}
```

### 2. Build the Module

Use the module builder tool:

```bash
./tools/build_module.py \
    my_module.c \
    -o my_module.mod \
    -n "MyModule" \
    -d "Description of my module"
```

### 3. Deploy the Module

Copy to SD card:
```bash
cp my_module.mod /Volumes/SDCARD/modules/
```

Or flash to modules partition (advanced).

### 4. Load the Module

From the console:
```
modload /sdcard/modules/my_module.mod
```

## Module Constraints

### What You CAN Do

✅ Use basic C operations (math, logic, loops)
✅ Call registered core functions
✅ Allocate memory (carefully)
✅ Access hardware through core APIs
✅ Store module-local state

### What You CANNOT Do

❌ Use standard library functions (printf, malloc, etc.) directly
❌ Access hardware registers directly
❌ Assume specific memory addresses
❌ Use global variables from main firmware
❌ Call functions not in the core function registry

### Core Function Registry

The main firmware exports functions that modules can call. Register them with:

```c
// In main firmware
module_register_core_function("log_info", (void*)esp_log_write);
module_register_core_function("delay_ms", (void*)vTaskDelay);
```

Modules can then call these functions (implementation TBD - needs symbol resolution).

## Module Format

### Binary Structure

```
[Header: 160 bytes]
[Code Section: variable]
[Data Section: variable]
```

### Header Format

```c
struct module_header {
    uint32_t magic;           // 0x4D4F4455 "MODU"
    uint32_t version;         // Format version
    uint32_t code_size;       // Bytes of code
    uint32_t data_size;       // Bytes of initialized data
    uint32_t bss_size;        // Bytes of zero-initialized data
    uint32_t entry_offset;    // Offset to module_init()
    uint32_t exit_offset;     // Offset to module_exit()
    uint32_t reserved;
    char name[32];            // Module name
    char description[64];     // Description
    uint32_t reserved[8];     // Future use
};
```

## Console Commands

### List Loaded Modules
```
modlist
```

Shows all loaded modules with:
- Name and version
- Size class (64K, 128K, 256K, 512K, 1024K)
- Memory usage
- Load address
- Description

### Load a Module
```
modload /sdcard/modules/wifi.mod
```

### Unload a Module
```
modunload wifi
```

### Show Statistics
```
modstats
```

Displays:
- Number of loaded modules
- Total memory used
- Free heap
- Minimum free heap

## Best Practices

### Memory Management

1. **Keep modules small** - Aim for 64K or 128K slots when possible
2. **Free resources** - Always clean up in `module_exit()`
3. **Check allocations** - Handle malloc failures gracefully
4. **Avoid leaks** - Every malloc needs a free

### Code Organization

1. **Single responsibility** - One module, one feature
2. **Minimal dependencies** - Reduce coupling to core
3. **Error handling** - Return error codes, don't crash
4. **Documentation** - Comment your module's purpose

### Testing

1. **Test loading** - Verify module loads successfully
2. **Test unloading** - Ensure clean shutdown
3. **Test memory** - Check for leaks with `modstats`
4. **Test interaction** - Verify module works with core

## Example: WiFi Module

See `components/module_wifi/` for a complete example of converting the WiFi file server into a loadable module.

## Troubleshooting

### Module Won't Load

- Check magic number in header
- Verify file isn't corrupted
- Ensure enough free memory
- Check console for error messages

### Module Crashes

- Likely calling unavailable function
- Check for null pointer dereferences
- Verify all dependencies are registered
- Use `modstats` to check memory

### Memory Leaks

- Run `modstats` before and after loading
- Check minimum free heap
- Ensure `module_exit()` frees everything
- Use heap tracing in development

## Future Enhancements

- Symbol resolution for calling core functions
- Module dependencies and versioning
- Signed modules for security
- Hot-reload support
- Module marketplace

## Advanced Topics

### Position-Independent Code (PIC)

Modules are compiled with `-fPIC` which generates code that works at any address. This adds slight overhead but enables flexible loading.

### Relocation (Future)

Currently modules must be fully position-independent. Future versions may support relocation tables for better performance.

### Shared Libraries (Future)

Multiple modules could share common code to reduce memory usage.
