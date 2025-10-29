# SD Card SPI Component

SPI SD card driver with FAT32 filesystem support for ESP32.

## Features

- SPI-based SD card communication (compatible with all SD/SDHC cards)
- FAT32 filesystem support via ESP-IDF VFS
- DMA-enabled for optimal performance
- Auto-mount during boot at `/sdcard`
- Power cycling and retry logic for reliable initialization
- Progressive speed reduction for maximum compatibility
- Comprehensive diagnostic tools
- Custom pin mapping to avoid FPGA/flash conflicts

## Pin Configuration

Uses custom ESP32 GPIO pins to avoid conflicts with FPGA QSPI and flash memory:

| Function | GPIO | Notes |
|----------|------|-------|
| CS (Chip Select) | GPIO4 | Requires 10kΩ pull-up to 3.3V |
| MOSI (Master Out) | GPIO15 | Data from ESP32 to SD card |
| MISO (Master In) | GPIO2 | Data from SD card to ESP32 |
| CLK (Clock) | GPIO14 | SPI clock signal |
| Power Control | GPIO32 | Optional - for card power cycling |

### Alternative Pins

Built-in ESP32 SD pins (may conflict with flash on some boards):
- CS: GPIO11, MOSI: GPIO8, MISO: GPIO7, CLK: GPIO6

## Hardware Requirements

### Required
- 10kΩ pull-up resistor on CS pin (GPIO4) to 3.3V
- 10kΩ pull-up resistors on SD card DAT1 and DAT2 pins to 3.3V

### Optional but Recommended
- Power control circuit on GPIO32 for reliable card reset
- Without power control, software reset fallback is used

## Usage

### Auto-Initialize (Default)

The SD card auto-mounts during boot. Check console output:

```
I (xxx) main: Auto-initializing SD card...
I (xxx) main: SD card auto-mounted successfully at /sdcard
```

### Manual Initialization

If auto-mount fails or card is inserted after boot:

```c
#include "sd_spi.h"

// Initialize SPI interface
esp_err_t ret = sd_spi_init();
if (ret == ESP_OK) {
    // Mount filesystem
    ret = sd_spi_mount("/sdcard");
}
```

### Reading/Writing Files

Use standard C file I/O or ESP-IDF VFS functions:

```c
// Write a file
FILE *f = fopen("/sdcard/test.txt", "w");
fprintf(f, "Hello SD Card!\n");
fclose(f);

// Read a file
f = fopen("/sdcard/test.txt", "r");
char line[64];
fgets(line, sizeof(line), f);
fclose(f);
```

### Unmounting

```c
sd_spi_unmount();
```

## Console Commands

### sd_spi_init
Initialize and mount SD card at `/sdcard`

```
mcu> sd_spi_init
```

### sd_spi_init_alt
Try alternative ESP32 built-in pins (GPIO 6,7,8,11)

```
mcu> sd_spi_init_alt
```

### sd_spi_test
Test read/write functionality with a test file

```
mcu> sd_spi_test
```

### sd_spi_info
Display detailed card information (size, type, speed, etc.)

```
mcu> sd_spi_info
```

### sd_spi_unmount
Safely unmount the SD card

```
mcu> sd_spi_unmount
```

### sd_pin_test
Test pin connectivity and SPI communication

```
mcu> sd_pin_test
```

### sd_raw_test
Low-level SPI communication test

```
mcu> sd_raw_test
```

## Technical Implementation

- **SPI Host**: VSPI (SPI3)
- **DMA**: Enabled for optimal performance
- **Clock Speed**:
  - Initialization: 400kHz
  - Progressive fallback: 400kHz → 200kHz → 100kHz for compatibility
  - Operational: Up to 20MHz (card dependent)
- **Power Cycling**:
  - Preferred: GPIO32 power control (100ms off, 200ms on)
  - Fallback: Software reset via CMD0
- **Error Handling**: Comprehensive with retry logic and diagnostic output

## Compatibility

Tested with:
- 8GB SDHC card
- Various SD and microSD cards (with adapter)

Should work with:
- SD cards (up to 2GB)
- SDHC cards (4GB - 32GB)
- Most microSD cards with adapter

## Troubleshooting

### Card not detected
1. Check hardware connections and pull-up resistors
2. Try `sd_pin_test` to verify GPIO connectivity
3. Try `sd_spi_init_alt` for alternative pins
4. Check card is properly inserted and formatted as FAT32

### Mount fails
1. Ensure card is formatted as FAT32 (not exFAT or NTFS)
2. Try reformatting card on PC
3. Check power supply is adequate (some cards need more current)
4. Review console output for specific error codes

### Slow performance
1. Check DMA is enabled in build configuration
2. Verify clock speed in debug output
3. Some cards have slower write speeds - this is normal

## Files

```
components/sd_spi/
├── CMakeLists.txt    # Component build configuration
├── sd_spi.c          # Implementation
├── sd_spi.h          # Public API
└── README.md         # This file
```

## Dependencies

- `esp_driver_spi` - SPI peripheral driver
- `esp_driver_gpio` - GPIO configuration
- `fatfs` - FAT filesystem
- `sdmmc` - SD/MMC protocol stack (card operations)

## API Reference

### Initialization
```c
esp_err_t sd_spi_init(void);
```
Initialize SPI interface to SD card

### Mounting
```c
esp_err_t sd_spi_mount(const char *mount_point);
```
Mount FAT filesystem at specified path

### Unmounting
```c
void sd_spi_unmount(void);
```
Safely unmount filesystem

### Card Information
```c
esp_err_t sd_spi_get_info(sdmmc_card_t **out_card);
```
Get card information structure

## Integration

This component integrates with:
- **WiFi File Server**: Provides storage backend for web file manager
- **Filesystem Commands**: `ls` and `cat` commands for console file browsing
- **Module Loading**: Future support for loading game modules from SD card

## Future Enhancements

- [ ] SD card hot-swap detection
- [ ] Multiple partition support
- [ ] exFAT support for >32GB cards
- [ ] Faster SPI modes (SDR50/SDR104)
- [ ] 4-bit SD mode (not SPI) for maximum speed
