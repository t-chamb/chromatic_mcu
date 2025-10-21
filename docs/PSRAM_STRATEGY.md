# ESP32 PSRAM Module Loading Strategy

## Research Summary

Based on ESP-IDF documentation and community findings:

### ESP32 (Original) Limitations

1. **PSRAM is NOT executable** - Mapped to data bus only (0x3F800000-0x3FC00000)
2. **IRAM is limited** - Only ~90-128KB available for dynamic allocation
3. **No MMU for PSRAM execution** - Unlike ESP32-S3, original ESP32 cannot map PSRAM to instruction bus

### Available Memory Regions

#### Executable Memory:

- **IRAM**: ~90-128KB, use `MALLOC_CAP_IRAM_8BIT`
- **D/IRAM**: Dual-use RAM, can be instruction or data
- **Flash (via cache)**: Already mapped, read-only

#### Data Memory:

- **DRAM**: ~180-256KB, use `MALLOC_CAP_8BIT`
- **PSRAM**: 7.8MB available (your hardware!), use `MALLOC_CAP_SPIRAM`

## Recommended Implementation

### Hybrid Approach: Code in IRAM, Data in PSRAM

```c
typedef struct {
    void *code_iram;      // Small executable code in IRAM
    void *data_psram;     // Large data in PSRAM (7.8MB!)
    size_t code_size;
    size_t data_size;
} module_t;

esp_err_t load_module(const uint8_t *source) {
    // 1. Allocate code in IRAM (executable, limited space)
    void *code = heap_caps_malloc(code_size, MALLOC_CAP_IRAM_8BIT);

    // 2. Allocate data in PSRAM (huge space, not executable)
    void *data = heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM);

    // 3. Copy from source
    memcpy(code, source + header_size, code_size);
    memcpy(data, source + header_size + code_size, data_size);

    // 4. Code is now executable from IRAM!
    // 5. Data is in your 7.8MB PSRAM!
}
```

### Module Format

```
[Header: 128 bytes]
[Code Section: < 100KB] -> Goes to IRAM
[Data Section: up to 7.8MB] -> Goes to PSRAM
```

### Use Cases

1. **WiFi File Server Module**

   - Code: ~50KB in IRAM
   - Data: HTTP buffers, file cache in PSRAM

2. **Game ROM Loader**

   - Code: ~20KB ROM emulator in IRAM
   - Data: ROM data (up to 7.8MB!) in PSRAM

3. **Graphics/UI Module**
   - Code: ~30KB rendering engine in IRAM
   - Data: Framebuffers, sprites in PSRAM

## Implementation Steps

### Phase 1: Fix Current Module Loader ✅

- [x] Change `MALLOC_CAP_EXEC` to `MALLOC_CAP_IRAM_8BIT`
- [x] Fix flash memory reading (byte-by-byte copy)
- [ ] Test with small embedded module

### Phase 2: Add PSRAM Data Support

- [ ] Extend module format to separate code/data sections
- [ ] Add PSRAM allocation for data sections
- [ ] Update module_loader.h with new structures

### Phase 3: PSRAM Asset Loading

- [ ] Load game ROMs to PSRAM (0x20000+)
- [ ] Load graphics assets to PSRAM
- [ ] Implement PSRAM cache management

## Memory Map Strategy

```
IRAM (128KB):
├── System code
├── WiFi stack
├── Module code (dynamic, ~50-80KB available)
└── Reserve

PSRAM (7.8MB):
├── 0x00000-0x0FFFF: OSD Framebuffer (64KB)
├── 0x10000-0x1FFFF: GB Framebuffer (64KB)
├── 0x20000-0x1FFFFF: Module Data (1.875MB)
└── 0x200000-0x7FFFFF: Asset Storage / ROM Data (6MB)
```

## Key Takeaways

1. **Keep code small** - IRAM is limited, optimize for size
2. **Use PSRAM for data** - You have 7.8MB, use it!
3. **Embedded modules work best** - Already in flash, no loading needed
4. **SD card for development** - Test modules before embedding

## Next Action

Change module loader to use `MALLOC_CAP_IRAM_8BIT` and test!
