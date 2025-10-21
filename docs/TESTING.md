# Module Loading Testing Guide

## Current Status

✅ Firmware built and flashed
✅ IRAM allocation implemented
✅ Flash memory reading fixed
⏳ Ready for testing

## Test Commands

### 1. Load Embedded Module

```
mcu> modloadem hello
```

**Expected Output:**

```
Loading embedded module 'hello' (174 bytes)
Data pointer: 0x3f447c10
First 4 bytes: 55 44 4f 4d
I (xxx) module_loader: Loading from memory: data=0x3f447c10, size=174
I (xxx) module_loader: Data is in flash, using byte-by-byte copy
I (xxx) module_loader: Module: Hello
I (xxx) module_loader:   Version: 1
I (xxx) module_loader:   Code: 46 bytes
I (xxx) module_loader:   Data: 0 bytes
I (xxx) module_loader:   BSS: 0 bytes
I (xxx) module_loader: Allocated 46 bytes of code in IRAM at 0x400xxxxx
I (xxx) module_loader: Copying 46 bytes of code
I (xxx) module_loader: Module 'Hello' loaded from memory
I (xxx) module_loader: Calling module init function...
Hello from dynamically loaded module!
I (xxx) module_loader: Module init successful
✓ Module loaded successfully
  Name: Hello
  Version: 1
  Memory: 46 bytes
  Description: Simple hello world module
```

### 2. List Loaded Modules

```
mcu> modlist
```

**Expected Output:**

```
Loaded Modules:
Name                 Version  Size Class  Memory       Address      Description
────────────────────────────────────────────────────────────────────────────
Hello                1        64K        46           0x400xxxxx   Simple hello world module
─────────────────────────────────────────────────────────────────
Total: 1 modules, 46 bytes
```

### 3. Check Memory Stats

```
mcu> modstats
```

**Expected Output:**

```
Module System Statistics:
  Loaded modules: 1
  Total memory: 46 bytes (0.04 KB)
  Free heap: xxxxx bytes
  Min free heap: xxxxx bytes
```

### 4. Unload Module

```
mcu> modunload Hello
```

**Expected Output:**

```
Unloading module: Hello
I (xxx) module_loader: Unloading module 'Hello'
I (xxx) module_loader: Calling module exit function...
I (xxx) module_loader: Module unloaded, freed 46 bytes
I (xxx) module_loader: Total memory allocated: 0 bytes
✓ Module unloaded successfully
```

## Troubleshooting

### If IRAM allocation fails:

```
E (xxx) module_loader: Failed to allocate 46 bytes in IRAM
I (xxx) module_loader: Free IRAM: xxxxx bytes
```

**Solution:** Check available IRAM with `modstats`, may need to reduce module size

### If module crashes on execution:

- Check that code is position-independent
- Verify module was built with correct flags
- Check IRAM address is in valid range (0x40080000-0x400A0000)

## Next Steps After Testing

1. ✅ Verify embedded module loads and executes
2. Test SD card loading: `modload /sdcard/modules/hello.mod`
3. Add PSRAM data section support
4. Load larger modules (WiFi server, etc.)
5. Implement PSRAM asset loading for game ROMs

## Memory Limits

- **IRAM for code**: ~90-128KB available
- **PSRAM for data**: 7.8MB available (0x20000-0x7FFFFF)
- **Module code**: Keep under 100KB for safety
- **Module data**: Can be huge (MBs) in PSRAM
