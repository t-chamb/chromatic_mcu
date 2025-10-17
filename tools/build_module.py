#!/usr/bin/env python3
"""
Module Builder for Chromatic MCU
Compiles C code into loadable modules with proper headers
"""

import argparse
import struct
import subprocess
import os
import sys

MODULE_MAGIC = 0x4D4F4455  # "MODU"
MODULE_VERSION = 1

def build_module(source_file, output_file, name, description, entry_func="module_init", exit_func="module_exit"):
    """Build a loadable module from C source"""
    
    print(f"Building module: {name}")
    print(f"  Source: {source_file}")
    print(f"  Output: {output_file}")
    
    # Compile with position-independent code
    obj_file = source_file.replace('.c', '.o')
    compile_cmd = [
        'xtensa-esp32-elf-gcc',
        '-c',
        '-fPIC',                    # Position-independent code
        '-ffunction-sections',      # Separate functions
        '-fdata-sections',          # Separate data
        '-Os',                      # Optimize for size
        '-mlongcalls',              # Required for ESP32
        '-nostdlib',                # No standard library
        '-I', os.environ.get('IDF_PATH', '') + '/components/esp_common/include',
        '-I', os.environ.get('IDF_PATH', '') + '/components/esp_system/include',
        '-I', os.environ.get('IDF_PATH', '') + '/components/freertos/include',
        source_file,
        '-o', obj_file
    ]
    
    print(f"  Compiling: {' '.join(compile_cmd)}")
    result = subprocess.run(compile_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Compilation failed:\n{result.stderr}")
        return False
    
    # Link into flat binary
    bin_file = source_file.replace('.c', '.bin')
    link_cmd = [
        'xtensa-esp32-elf-ld',
        '-r',                       # Relocatable output
        '-e', entry_func,           # Entry point
        obj_file,
        '-o', bin_file
    ]
    
    print(f"  Linking: {' '.join(link_cmd)}")
    result = subprocess.run(link_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Linking failed:\n{result.stderr}")
        return False
    
    # Extract sections
    objcopy_cmd = [
        'xtensa-esp32-elf-objcopy',
        '-O', 'binary',
        bin_file,
        bin_file + '.raw'
    ]
    
    print(f"  Extracting binary: {' '.join(objcopy_cmd)}")
    result = subprocess.run(objcopy_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Binary extraction failed:\n{result.stderr}")
        return False
    
    # Read the binary
    with open(bin_file + '.raw', 'rb') as f:
        code_data = f.read()
    
    code_size = len(code_data)
    data_size = 0  # For now, assume no separate data section
    bss_size = 0   # For now, assume no BSS
    
    # Find entry and exit function offsets (simplified - would need symbol table parsing)
    entry_offset = 0  # Assume entry is at start
    exit_offset = 0   # Assume exit is at start (or 0 if none)
    
    print(f"  Code size: {code_size} bytes")
    
    # Build module header
    header = struct.pack(
        '<IIIIIIII32s64s8I',
        MODULE_MAGIC,
        MODULE_VERSION,
        code_size,
        data_size,
        bss_size,
        entry_offset,
        exit_offset,
        0,  # Reserved
        name.encode('utf-8').ljust(32, b'\0'),
        description.encode('utf-8').ljust(64, b'\0'),
        0, 0, 0, 0, 0, 0, 0, 0  # Reserved fields
    )
    
    # Write module file
    with open(output_file, 'wb') as f:
        f.write(header)
        f.write(code_data)
    
    print(f"✓ Module built successfully: {output_file}")
    print(f"  Total size: {len(header) + code_size} bytes")
    
    # Cleanup
    for temp_file in [obj_file, bin_file, bin_file + '.raw']:
        if os.path.exists(temp_file):
            os.remove(temp_file)
    
    return True

def main():
    parser = argparse.ArgumentParser(description='Build loadable modules for Chromatic MCU')
    parser.add_argument('source', help='Source C file')
    parser.add_argument('-o', '--output', required=True, help='Output module file')
    parser.add_argument('-n', '--name', required=True, help='Module name')
    parser.add_argument('-d', '--description', default='', help='Module description')
    parser.add_argument('--entry', default='module_init', help='Entry function name')
    parser.add_argument('--exit', default='module_exit', help='Exit function name')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.source):
        print(f"Error: Source file not found: {args.source}")
        return 1
    
    success = build_module(
        args.source,
        args.output,
        args.name,
        args.description,
        args.entry,
        args.exit
    )
    
    return 0 if success else 1

if __name__ == '__main__':
    sys.exit(main())
