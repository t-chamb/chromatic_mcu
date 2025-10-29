# FPGA PSRAM Access

Bidirectional PSRAM read/write via FPGA QSPI interface.

## Status

❌ **BLOCKED - FPGA Read Data Path Broken**

**ESP32 Implementation**: ✅ **Complete and Working**
- SPI communication verified correct
- Write operations confirmed working
- Separate device handle prevents LVGL interference
- Transaction queuing and timing correct

**FPGA Issue**: ❌ **Critical Bug Found**
- FPGA is NOT reading from PSRAM
- All read operations return debug register value (`0F 18 FF 18`)
- FPGA receives commands correctly but data path is disconnected
- See "Debug Findings" section below for details

## Usage

```c
#include "fpga_psram.h"

// Write data
uint8_t data[16] = {0x00, 0x01, 0x02, ...};
fpga_psram_write(0x200000, data, 16);

// Read data
uint8_t buffer[16];
fpga_psram_read(0x200000, buffer, 16);
```

## Memory Map

| Region | Address | Size | Usage |
|--------|---------|------|-------|
| OSD FB | 0x000000 | 64KB | LVGL framebuffer |
| GB FB | 0x010000 | 64KB | Future use |
| Heap | 0x020000 | 1.9MB | Dynamic allocation |
| Assets | 0x200000 | 6MB | Static data |

## Implementation

- Separate SPI device handle (shares VSPI_HOST bus with LVGL)
- ESP-IDF handles bus arbitration automatically
- Static DMA buffers for reliable transfers
- CS pin shared with LVGL (GPIO5)

## Debug Findings (2025-10-29)

### Critical Discovery: FPGA Returns Debug Register Instead of PSRAM Data

**Evidence:**
1. FPGA debug register at 0xFF00 reads as: `0F 18 FF 18`
2. ALL PSRAM read operations return identical pattern: `0F 18 FF 18 FF 18...` (repeating)
3. Pattern matches debug register value exactly
4. Debug register changes after reads (`0D AE DD AE`), proving FPGA processes commands
5. Write operations succeed (no errors returned)

**Analysis:**
- ✅ ESP32 sends correct SPI commands (verified with logging)
- ✅ FPGA receives and decodes commands (debug register responds)
- ❌ FPGA data mux routes debug register to SPI output instead of PSRAM data
- ❌ PSRAM read data path not connected to SPI MISO

**For FPGA Team:**
Check the data multiplexer that selects between:
- PSRAM read data (`mm_burst_read.qDataOut`)
- Debug register data
- Default/status data

The mux is currently always outputting debug register or a fixed pattern.

### Issues Investigated and Resolved

1. **FIFO Stale Data Between Consecutive Reads** (ESP32)
   - **Problem**: Back-to-back reads showed stale FIFO data
   - **Root Cause**: FPGA FIFO not clearing when CS goes HIGH
   - **Workaround**: 10ms delay after each read (`vTaskDelay(pdMS_TO_TICKS(10))`)
   - **Proper Fix**: FPGA should clear FIFO on CS rising edge
   - **Status**: Workaround in place, but masked by data path bug

2. **Transaction Queue Interference with LVGL** (ESP32)
   - **Problem**: Wrong transactions returned from queue
   - **Root Cause**: Sharing SPI device handle with LVGL
   - **Fix**: Created separate device handle using `spi_bus_add_device()`
   - **Status**: ✅ Fixed

3. **Power Manager Sleep During SPI Operations** (ESP32)
   - **Problem**: ESP32 entering light sleep after 100ms idle
   - **Root Cause**: `PwrMgr_Task` auto-sleep
   - **Fix**: Disabled power manager task during debugging
   - **Status**: ✅ Disabled (commented out in `main.c`)

## Commands

- `psram_256` - 256-byte read/write test
- `psram_diag` - Comprehensive diagnostic with FPGA debug register monitoring

## Known Issues

### FPGA-Side
1. **Critical: Read data path disconnected** - Returns debug register instead of PSRAM data
2. Previous 16-byte boundary bug (0x10, 0x20...) - Cannot verify until data path fixed

### ESP32-Side
None - all ESP32 code working correctly
