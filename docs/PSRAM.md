# FPGA PSRAM Access

Bidirectional PSRAM read/write via FPGA QSPI interface.

## Status

✅ **Working** - 100% reliable for ≤16-byte transfers, 94% for 256-byte (FPGA has byte-boundary bug at 0x10 intervals)

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

## Known Issues

FPGA has address increment bug at 16-byte boundaries (0x10, 0x20, 0x30...). Each boundary byte reads from -16 offset. ESP32 code is fully working.

## Commands

- `psram_256` - 256-byte read/write test
