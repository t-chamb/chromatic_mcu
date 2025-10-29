## Unreleased - SD Card & WiFi File Server Feature

### Added (In Development - Experimental Features)

#### SD Card Support
- SPI SD card driver with FAT32 filesystem support
- Auto-mount SD card during boot at `/sdcard`
- Custom pin mapping to avoid FPGA/flash conflicts
  - CS: GPIO4, MOSI: GPIO15, MISO: GPIO2, CLK: GPIO14, Power: GPIO32
- Console commands: `sd_spi_init`, `sd_spi_test`, `sd_spi_info`, `sd_spi_unmount`
- Power cycling and retry logic for reliable initialization
- DMA-enabled SPI communication for performance
- Comprehensive diagnostic tools

#### WiFi File Server
- WiFi Access Point (SSID: `Chromatic_MCU`, Password: `chromatic123`)
- Web-based file manager at http://192.168.4.1/
- Upload/download/delete files from SD card
- Browse directories with pagination
- Modern responsive web UI
- JSON REST API for programmatic access
- OSD integration - enable/disable from System menu
- Persistent settings for WiFi server state

#### Filesystem Commands
- `ls [path]` - List files and directories
- `cat <file>` - Display file contents
- Full path navigation with tab completion

### Hardware Requirements
- 10kΩ pull-up resistor on CS pin (GPIO4) to 3.3V
- 10kΩ pull-up resistors on SD card DAT1/DAT2 pins to 3.3V
- Optional: Power control circuit on GPIO32 for reliable card reset

### Documentation
- See `components/wifi_file_server/README.md` for WiFi server API and usage
- See commit messages for SD card technical details and pin configuration

### Status
- ✅ SD card functionality working and tested
- ✅ WiFi file server working with web UI
- ⚠️  Experimental features - thorough testing recommended before production use

---

## v0.13.3

### Note
The FPGA must be updated to v18.6 or newer for palette selection and updated
frame blending behavior.

### Added
- The ability to change the color palette for non-GBC titles through a Palette tab.

### Changed
- Renamed the frame blending toggle button text to "1:1" from "3:1" to clarify
the updated FPGA approach.
- Added redundancy to two-option menu widgets. Press B to toggle previous choice.
- Added up/down roll over to bottom/top for OSD navigation.

## v0.13.2

### Added
- The ability to mock button to the FPGA.

### Changed
- Simplified the firmware version reported the system tab. Press A to see details.

## v0.13.0

### Added
- Option to ignore diagonal inputs from the D-Pad
- Option to use the background palette 0 color (prevents screen transition flashing)
- Option to set low battery icon display behavior
- Added message to brightness to indicate low power mode is active
- Added hot keys to the Controls tab

### Changed
- Tuned battery voltage thresholds for 1.2V NiMH AA's
- Prevent backlight adjustments when in low power mode
- Renamed 'USB Color Correction' menu option to 'Streaming'
- Redesigned Controls tab, toggle option text

## v0.12.3

### Note
The FPGA must be updated to v17.0 or newer as the underlying communication protocol changed.

## Added
- Sleep the MCU on power up to reduce power consumption.
- Support LiPo battery thresholds and charging indication in the battery menu.

## Fixed
- Highest brightness setting is not properly restored on power up.
- Hot key brightness adjustments are saved between power cycles

### Changed
- Supports updated UART protocol.

## Removed
- The Frame Blending text hint on the Display screen.

## v0.11.2
This is the first production release.
