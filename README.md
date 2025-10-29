# Chromatic MCU

ESP32-based microcontroller firmware for the ModRetro Chromatic handheld gaming device.

## Overview

This firmware manages the Chromatic's user interface, settings, and communication with the FPGA. It provides an on-screen display (OSD) menu system using LVGL, handles user input, manages power and battery monitoring, and coordinates with the FPGA via UART and SPI interfaces.

## Features

- **LVGL-based OSD** - Modern menu system for settings and configuration
- **FPGA Communication** - UART protocol for control and status
- **Settings Management** - Persistent storage in NVS flash
- **Battery Monitoring** - Support for NiMH and LiPo batteries
- **Button Input** - D-pad and button handling with debouncing
- **Power Management** - Sleep modes and low-power operation

## Active Development Branches

### `feature-qspi-r/w` - FPGA PSRAM Interface
QSPI-based read/write access to 8MB PSRAM via FPGA. Currently blocked pending FPGA data path fix.
- See `docs/PSRAM.md` for technical details
- See `CHANGELOG.md` for current status

### `feature/sd-card-wifi` - SD Card & WiFi File Server
SD card support with WiFi-based file manager for wireless file transfer.
- SD card via SPI with FAT32 filesystem
- WiFi Access Point with web-based file manager
- See `components/sd_spi/README.md` and `components/wifi_file_server/README.md`

## Project Structure

```
chromatic_mcu/
├── main/                  # Main application code
│   ├── main.c            # Application entry point
│   ├── gfx.c/h           # Graphics and LVGL setup
│   ├── fpga_tx/rx.*      # FPGA UART communication
│   └── cmd_*.c           # Console commands
├── components/           # ESP-IDF components
│   ├── osd/             # On-screen display menus
│   ├── settings/        # Settings management
│   ├── battery/         # Battery monitoring
│   ├── button/          # Button input handling
│   ├── sd_spi/          # SD card driver (feature branch)
│   └── wifi_file_server/ # WiFi file manager (feature branch)
├── docs/                # Documentation
│   └── PSRAM.md         # PSRAM technical documentation
└── CHANGELOG.md         # Version history and changes
```

## Toolchain Setup
The project relies on the EspressIF IoT Development Framework (IDF) for development and debugging. Please set up the ESP32 according to the instructions at https://idf.espressif.com/.

### Windows
Please run the official ESP32 installer.

### Linux
The Ubuntu 24.04 steps are summarized below:
```bash
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
cd ~/path/to/clone/esp-idf
git clone -b v5.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Installs to the default ~/.espressif path. See IDF_TOOLS_PATH for an alternative.
./install.sh esp32

# Update your path for your current session so that the `idf.py` tool can be accessed.
source ./export.sh
```

To permanently update your environment path, add the following to your shell startup script (e.g. `~/.bashrc`):
```
alias get_idf='. $HOME/esp-idf/export.sh'
```

where `$HOME/esp-idf` is where you cloned the ESP IDF toolchain. Anytime you wish to develop, run `get_idf` in your terminal first. EspressIF does not recommend always running `export.sh` for every terminal. This may have unintended side effects.

## Build, Flashing, and Debugging

Run `idf.py -p PORT build flash monitor` to build, flash and monitor the project. The default baud rate is `115200`.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

Each of these steps can be issued individually (build, flash, and monitor).

## Serial Interface
The Chromatic FPGA supports a composite USB device consisting of several device classes. One of them is a serial interface used for flashing and debugging. The setup steps depend on your operating system.

### Windows
A serial port for the ESP32 should already appear in the Device Manager as a COM device.
You can also see this by issuing `modes` within a command prompt.

### Linux
Ensure you are part of the `dialout` group. You will need to log out and log back in after issuing the following command:
```bash
sudo gpasswd --add $USER dialout
```

If everything is working, you should see `/dev/ttyACM0` appear as the communications port to the MCU.


## Contributing

### Branch Naming Convention
- `feature/<name>` - New features or enhancements
- `bugfix/<name>` - Bug fixes
- `docs/<name>` - Documentation updates

### Development Workflow
1. Create feature branch from `main`
2. Make changes and commit with clear messages
3. Update `CHANGELOG.md` with changes
4. Add/update documentation in `docs/` or component READMEs
5. Test thoroughly
6. Create pull request to `main`

### Commit Message Guidelines
- Use clear, descriptive commit messages
- Reference issue numbers when applicable
- Follow conventional commits format when possible

## Documentation

- `CHANGELOG.md` - Project changelog
- `docs/PSRAM.md` - FPGA PSRAM interface documentation
- `components/*/README.md` - Component-specific documentation

## License

See LICENSE file for details.
