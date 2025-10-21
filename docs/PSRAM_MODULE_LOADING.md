# PSRAM Module Loading Strategy

## PSRAM Memory Map (Official from FPGA Team)
```
┌─────────────────────────────────────────────────────────┐
│ Address Range          │ Size      │ Purpose            │
├─────────────────────────────────────────────────────────┤
│ 0x000000 - 0x00FFFF    │ 64 KB     │ OSD Framebuffer    │
│ 0x010000 - 0x01FFFF    │ 64 KB     │ GB Framebuffer     │
│ 0x020000 - 0x1FFFFF    │ 1.875 MB  │ Module Heap        │
│ 0x200000 - 0x7FFFFF    │ 6 MB      │ Asset Storage      │
└─────────────────────────────────────────────────────────┘
Total: 8 MB (0x800000 bytes)
```

### Region Details
- **OSD Framebuffer**: 160×144 pixels, RGB565, ~45KB used
- **GB Framebuffer**: 160×144 pixels, RGB565, ~45KB used
- **Module Heap**: 1.875 MB for dynamic module allocations
- **Asset Storage**: 6 MB for game ROMs, graphics, assets

## Module Loading Strategy

### Option 1: Load to PSRAM (RECOMMENDED)
- Load module code/data directly to PSRAM starting at 0x20000
- Pros:
  - Huge space available (7.8 MB)
  - No heap fragmentation
  - Can load very large modules
  - Direct QSPI access from MCU
- Cons:
  - Code in PSRAM may not be executable directly
  - Need to copy executable code to IRAM/DRAM

### Option 2: Hybrid Approach (BEST)
- Load module **data** to PSRAM (0x20000+)
- Load module **code** to executable heap (MALLOC_CAP_EXEC)
- This gives you:
  - Executable code in fast memory
  - Large data structures in PSRAM
  - Best of both worlds

## Implementation Plan

### Phase 1: Fix Current Embedded Loading
- Debug the flash memory access issue
- Get basic embedded module working

### Phase 2: Add PSRAM Data Support
- Extend module format to separate code/data
- Load data sections to PSRAM
- Keep code in executable memory

### Phase 3: PSRAM Module Storage
- Store compiled modules in PSRAM
- Load from PSRAM on demand
- Use PSRAM as a "module cache"

## Memory Allocation Strategy (Aligned with FPGA Protocol)

```
PSRAM Layout (via QSPI @ 40MHz):
0x000000 - 0x00FFFF: OSD Framebuffer (64 KB) - FPGA reads for display
0x010000 - 0x01FFFF: GB Framebuffer (64 KB) - FPGA reads for display
0x020000 - 0x1FFFFF: Module Heap (1.875 MB) - MCU read/write via QSPI
0x200000 - 0x7FFFFF: Asset Storage (6 MB) - MCU read/write via QSPI

MCU Heap (DRAM/IRAM):
- Module code (executable) - Must be in MALLOC_CAP_EXEC
- Small data structures
- Stack/heap for module execution

QSPI Protocol:
- 48-bit command header: [cmd(1)][length(10)][address(32)]
- Max transfer: 1023 bytes per transaction
- Bandwidth: ~15 MB/s practical
```

## QSPI PSRAM Integration

### Module Loading Flow
1. **Load module from SD card** to MCU DRAM buffer
2. **Parse module header** to determine code/data sizes
3. **Allocate code memory** in MCU heap (MALLOC_CAP_EXEC)
4. **Allocate data memory** in PSRAM Module Heap (0x020000+)
5. **Copy code** to MCU executable memory
6. **Write data** to PSRAM via QSPI (fpga_psram_write)
7. **Initialize module** with PSRAM data pointer

### PSRAM Access Functions (from fpga_psram.h)
```c
// Write data to PSRAM via QSPI
esp_err_t fpga_psram_write(uint32_t address, const uint8_t *data, size_t length);

// Read data from PSRAM via QSPI
esp_err_t fpga_psram_read(uint32_t address, uint8_t *data, size_t length);

// Burst write (up to 1023 bytes per transaction)
esp_err_t fpga_psram_burst_write(uint32_t address, const uint8_t *data, size_t length);
```

### Module Data Structure Extension
```c
typedef struct {
    void *code_mem;           // MCU executable memory
    void *data_mem;           // MCU heap or PSRAM pointer
    uint32_t psram_address;   // PSRAM address if data in PSRAM
    bool data_in_psram;       // Flag: data stored in PSRAM
    size_t psram_size;        // Size of data in PSRAM
} module_t;
```

### Benefits
- **1.875 MB** available for module data (vs limited MCU heap)
- **6 MB** for asset storage (ROMs, graphics)
- **No heap fragmentation** for large data structures
- **Persistent storage** across module load/unload cycles

## Next Steps
1. ✅ Fix embedded module crash (flash memory alignment)
2. ⏳ Test basic embedded module loading
3. 🔜 Integrate fpga_psram driver
4. 🔜 Add PSRAM data section support
5. 🔜 Implement PSRAM-backed module heap allocator
