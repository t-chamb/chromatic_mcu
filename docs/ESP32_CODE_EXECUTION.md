# ESP32 Code Execution Architecture

## The Problem
ESP32 has a Harvard architecture with separate instruction and data memory spaces. You can't just allocate heap memory and execute code from it.

## Memory Regions

### Executable Regions:
- **IRAM** (0x40080000-0x400A0000): ~128KB, truly executable
- **Instruction Cache**: Maps flash to 0x40000000-0x40400000 (read-only)
- **ROM** (0x40000000-0x40070000): Built-in ROM code

### Data Regions:
- **DRAM** (0x3FFB0000-0x3FFF0000): ~256KB, NOT executable
- **External RAM/PSRAM** (0x3F800000-0x3FC00000): 4-8MB, NOT directly executable

## Solutions for Dynamic Modules

### Option 1: IRAM Allocation (Current Attempt - BROKEN)
```c
// This WON'T work:
void *code = malloc(size);  // Gets DRAM
// Can't execute from DRAM!

// This MIGHT work:
void *code = heap_caps_malloc(size, MALLOC_CAP_EXEC);
// But MALLOC_CAP_EXEC gives instruction cache space which is READ-ONLY!
```

### Option 2: Copy to IRAM (RECOMMENDED)
```c
// Allocate in IRAM
void *code = heap_caps_malloc(size, MALLOC_CAP_IRAM_8BIT);
// Copy code
memcpy(code, source, size);
// Execute - this works!
```

**Pros**: Actually executable
**Cons**: Limited to ~128KB total IRAM

### Option 3: PSRAM with Cache Mapping (COMPLEX)
Your 7.8MB PSRAM could theoretically be used, but requires:
- Configuring MMU to map PSRAM to instruction space
- Cache management
- ESP-IDF doesn't make this easy

### Option 4: Embedded in Flash (SIMPLEST)
```c
// Module is already in flash at compile time
extern const uint8_t module_start[];
// Flash is already mapped to instruction cache
// Just call it directly!
```

**Pros**: No runtime loading needed, works immediately
**Cons**: Not truly "dynamic", requires reflash to update

## Recommended Approach

For your use case (WiFi server, optional features):

1. **Embed critical modules in flash** - They're always available, no loading needed
2. **Use IRAM for small dynamic code** - < 100KB modules
3. **Use PSRAM for module DATA** - Your 7.8MB is perfect for this
4. **Keep code in flash/IRAM, data in PSRAM**

## Implementation Strategy

```c
typedef struct {
    void *code;      // In IRAM or flash (executable)
    void *data;      // In PSRAM (large data structures)
    size_t code_size;
    size_t data_size;
} module_t;

// Load module
esp_err_t load_module(const uint8_t *flash_module) {
    // Code goes to IRAM (small, executable)
    void *code = heap_caps_malloc(code_size, MALLOC_CAP_IRAM_8BIT);
    memcpy(code, flash_module + header_size, code_size);
    
    // Data goes to PSRAM (large, not executable)
    void *data = heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM);
    memcpy(data, flash_module + header_size + code_size, data_size);
    
    // Now code is executable, data is in huge PSRAM!
}
```

## Next Steps

1. Change `MALLOC_CAP_EXEC` to `MALLOC_CAP_IRAM_8BIT`
2. Add PSRAM support for module data sections
3. Test with small modules first
4. Consider if "embedded" modules even need dynamic loading
   - They're already in flash
   - Just call them directly!
