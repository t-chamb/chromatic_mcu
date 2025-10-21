# Module Memory Isolation Strategy

## Problem
Modules allocating from main heap can cause:
- Heap fragmentation
- Out of memory crashes
- System instability
- Hard to debug memory leaks

## Solution: Dedicated PSRAM Application Space

### Memory Layout
```
PSRAM (8MB Total):
├── 0x00000-0x0FFFF: OSD Framebuffer (64KB) - System
├── 0x10000-0x1FFFF: GB Framebuffer (64KB) - System
├── 0x20000-0x7FFFFF: Application Space (7.875MB) - Modules
    ├── 0x20000-0x1FFFFF: Module Heap (1.875MB)
    │   └── Isolated heap for module allocations
    └── 0x200000-0x7FFFFF: Asset Storage (6MB)
        ├── Game ROMs
        ├── Graphics assets
        └── Module data
```

## Implementation

### 1. Create Module-Specific Heap in PSRAM

```c
// In module_loader.c
#include "esp_heap_caps.h"

#define MODULE_HEAP_START 0x3F820000  // PSRAM 0x20000
#define MODULE_HEAP_SIZE  (1875 * 1024)  // 1.875MB

static heap_t module_heap = NULL;

esp_err_t module_loader_init(void) {
    // Create isolated heap in PSRAM for modules
    module_heap = heap_caps_init_region(
        (void*)MODULE_HEAP_START,
        MODULE_HEAP_SIZE,
        MALLOC_CAP_SPIRAM
    );
    
    if (module_heap == NULL) {
        ESP_LOGE(TAG, "Failed to create module heap");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Module heap created: %d KB at 0x%08x",
             MODULE_HEAP_SIZE / 1024, MODULE_HEAP_START);
    
    return ESP_OK;
}
```

### 2. Module Allocation Functions

```c
// Modules use these instead of malloc/free
void* module_malloc(size_t size) {
    return heap_caps_malloc_prefer(size, 2, 
                                    MALLOC_CAP_SPIRAM,
                                    MALLOC_CAP_8BIT);
}

void module_free(void* ptr) {
    heap_caps_free(ptr);
}

// Get module heap stats
void module_heap_stats(size_t *free, size_t *used) {
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    *free = info.total_free_bytes;
    *used = info.total_allocated_bytes;
}
```

### 3. Module API

Modules get access to isolated allocator:

```c
// In module header
typedef struct {
    void* (*malloc)(size_t size);
    void (*free)(void* ptr);
    void* (*realloc)(void* ptr, size_t size);
    void* (*calloc)(size_t n, size_t size);
} module_allocator_t;

// Pass to module init
typedef struct {
    module_allocator_t allocator;
    // ... other APIs
} module_context_t;

// Module uses it
void* my_buffer = ctx->allocator.malloc(1024 * 1024);  // 1MB in PSRAM
```

## Benefits

### ✅ Memory Isolation
- Modules can't exhaust system heap
- Module crashes don't corrupt system memory
- Easy to track per-module memory usage

### ✅ Large Allocations
- 1.875MB dedicated heap for modules
- Perfect for large buffers, caches, assets
- No competition with system allocations

### ✅ Easy Cleanup
- Unload module = free all its allocations
- No memory leaks between module loads
- Clear memory accounting

### ✅ PSRAM Utilization
- Uses your 7.8MB PSRAM effectively
- Keeps fast DRAM for system use
- Slower PSRAM is fine for module data

## Asset Storage Region

### Direct PSRAM Access (0x200000-0x7FFFFF)

For large assets that don't need heap management:

```c
// Load ROM directly to PSRAM
#define ROM_STORAGE_START 0x3FA00000  // PSRAM 0x200000
#define ROM_STORAGE_SIZE  (6 * 1024 * 1024)  // 6MB

esp_err_t load_rom_to_psram(const char *path, uint32_t *out_addr) {
    FILE *f = fopen(path, "rb");
    
    // Get ROM size
    fseek(f, 0, SEEK_END);
    size_t rom_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Allocate in ROM storage region
    static uint32_t next_rom_addr = ROM_STORAGE_START;
    uint32_t rom_addr = next_rom_addr;
    
    // Read directly to PSRAM
    uint8_t *psram_ptr = (uint8_t*)rom_addr;
    fread(psram_ptr, 1, rom_size, f);
    fclose(f);
    
    next_rom_addr += (rom_size + 0xFFF) & ~0xFFF;  // 4KB align
    *out_addr = rom_addr;
    
    ESP_LOGI(TAG, "ROM loaded: %zu bytes at 0x%08x", rom_size, rom_addr);
    return ESP_OK;
}
```

## Memory Protection

### Optional: Add Bounds Checking

```c
// Wrap allocations with guards
typedef struct {
    uint32_t magic_start;  // 0xDEADBEEF
    size_t size;
    uint8_t data[];
    // magic_end at data[size]
} guarded_alloc_t;

void* module_malloc_guarded(size_t size) {
    guarded_alloc_t *alloc = module_malloc(
        sizeof(guarded_alloc_t) + size + sizeof(uint32_t)
    );
    
    alloc->magic_start = 0xDEADBEEF;
    alloc->size = size;
    *(uint32_t*)(alloc->data + size) = 0xDEADBEEF;
    
    return alloc->data;
}

void module_free_guarded(void* ptr) {
    guarded_alloc_t *alloc = (guarded_alloc_t*)
        ((uint8_t*)ptr - offsetof(guarded_alloc_t, data));
    
    // Check guards
    assert(alloc->magic_start == 0xDEADBEEF);
    assert(*(uint32_t*)(alloc->data + alloc->size) == 0xDEADBEEF);
    
    module_free(alloc);
}
```

## Implementation Priority

### Phase 1: Basic Isolation ✅
- [x] Use PSRAM for module data
- [ ] Create module heap in PSRAM
- [ ] Provide module allocator API

### Phase 2: Asset Loading
- [ ] Direct ROM loading to PSRAM
- [ ] Asset manager for 6MB storage region
- [ ] Cache management

### Phase 3: Protection (Optional)
- [ ] Guard bytes for overflow detection
- [ ] Per-module memory limits
- [ ] Memory usage tracking

## Usage Example

```c
// WiFi server module
esp_err_t wifi_server_init(module_context_t *ctx) {
    // Allocate 512KB buffer in PSRAM (isolated)
    void *buffer = ctx->allocator.malloc(512 * 1024);
    
    // Load assets to PSRAM storage
    uint32_t html_addr;
    load_asset_to_psram("/sdcard/www/index.html", &html_addr);
    
    // Module has 1.875MB heap + 6MB storage available
    // System heap is protected
    
    return ESP_OK;
}
```

## Next Steps

1. Implement module heap in PSRAM
2. Add allocator API to module context
3. Test with WiFi server module
4. Add ROM loading to storage region
