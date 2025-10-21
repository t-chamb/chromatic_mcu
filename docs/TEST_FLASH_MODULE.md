# Testing Flash Module Loading

## Prerequisites

1. Build and flash the firmware: `idf.py build flash`
2. Copy hello.mod to SD card: `/sdcard/modules/hello.mod`

## Test Steps

### 1. Connect to the device

```bash
idf.py monitor
```

### 2. Write module to flash partition

```
mcu> modwriteflash /sdcard/modules/hello.mod
```

Expected output:

```
Writing module to flash (174 bytes)...
Partition size: 65536 bytes
Erasing flash partition...
Writing to flash...
✓ Module written to flash successfully
  Size: 174 bytes
  Use 'modloadflash' to load it
```

### 3. Load module from flash

```
mcu> modloadflash
```

Expected output:

```
Loading module from flash partition...
Module: Hello
  Version: 1
  Code: 46 bytes
  Data: 0 bytes
  BSS: 0 bytes
Module 'Hello' loaded from flash
Calling module init function...
Hello from dynamically loaded module!
Module init successful
✓ Module loaded successfully
  Name: Hello
  Version: 1
  Memory: 46 bytes
  Description: Simple hello world module
```

### 4. List loaded modules

```
mcu> modlist
```

Expected output:

```
Loaded Modules:
Name                 Version  Size Class  Memory       Address      Description
────────────────────────────────────────────────────────────────────────────
Hello                1        64K        46           0x3ffb1234   Simple hello world module
─────────────────────────────────────────────────────────────────
Total: 1 modules, 46 bytes
```

### 5. Test embedded module loading (alternative)

```
mcu> modloadem hello
```

Expected output:

```
Loading embedded module 'hello' (174 bytes)
Module: Hello
  Version: 1
  Code: 46 bytes
  Data: 0 bytes
  BSS: 0 bytes
Module 'Hello' loaded from memory
Calling module init function...
Hello from dynamically loaded module!
Module init successful
✓ Module loaded successfully
  Name: Hello
  Version: 1
  Memory: 46 bytes
  Description: Simple hello world module
```

## Available Commands

- `modlist` - List all loaded modules
- `modload <path>` - Load module from SD card
- `modloadem <name>` - Load embedded module
- `modloadflash` - Load module from flash partition
- `modwriteflash <path>` - Write module from SD to flash
- `modunload <name>` - Unload a module
- `modstats` - Show memory statistics

## Notes

- The flash partition is 64KB (defined in partitions.csv)
- Modules are loaded into executable memory (MALLOC_CAP_EXEC)
- Each module has init/exit callbacks
- Up to 8 modules can be loaded simultaneously
