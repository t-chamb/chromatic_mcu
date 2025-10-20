# FPGA PSRAM Protocol Specification

## Overview

The ESP32 MCU communicates with the FPGA via QSPI to access 8MB of PSRAM. The FPGA acts as a memory controller, handling all PSRAM read/write operations.

```
ESP32 MCU <--[QSPI 40MHz]--> FPGA <--[Memory Controller]--> PSRAM (8MB)
```

## Hardware Interface

### QSPI Pins

- **CS (Chip Select)**: GPIO5
- **CLK (Clock)**: GPIO18 (40 MHz)
- **Data Lines** (Quad SPI):
  - MOSI: GPIO23
  - MISO: GPIO19
  - WP: GPIO22
  - HD: GPIO21

### Operating Mode

- **Speed**: 40 MHz
- **Mode**: QSPI (Quad SPI) - 4 data lines
- **DMA**: Enabled for efficient transfers

## Protocol Specification

### Command Format

Each transaction consists of:

1. **Command Header** (48 bits / 6 bytes)
2. **Data Payload** (0-1023 bytes)

### Command Header Structure (48 bits)

```
Bit Layout:
┌─────────┬──────────────┬────────────────────────────────────┐
│ Bit 0   │ Bits 1-10    │ Bits 11-42                         │
│ Command │ Length       │ Address                            │
│ (1 bit) │ (10 bits)    │ (32 bits)                          │
└─────────┴──────────────┴────────────────────────────────────┘

Byte Packing (MSB first):
Byte 0: [Cmd][Len9-Len3]
Byte 1: [Len2-Len0][Addr31-Addr27]
Byte 2: [Addr26-Addr19]
Byte 3: [Addr18-Addr11]
Byte 4: [Addr10-Addr3]
Byte 5: [Addr2-Addr0][00000]
```

### Command Bit

- **0**: Read operation
- **1**: Write operation

### Length Field (10 bits)

- **Range**: 0-1023 bytes
- **Encoding**: Unsigned 10-bit integer
- **Note**: Length of data payload to read/write

### Address Field (32 bits)

- **Range**: 0x00000000 - 0x007FFFFF (8MB)
- **Encoding**: Unsigned 32-bit integer
- **Byte Order**: Big-endian (MSB first)

## Transaction Sequences

### Write Transaction

```
1. MCU sends Command Header (48 bits) in QSPI mode
   ┌──────────────────────────────────────┐
   │ [1][Length][Address]                 │
   └──────────────────────────────────────┘

2. MCU sends Data Payload (Length bytes) in QSPI mode
   ┌──────────────────────────────────────┐
   │ [Data byte 0][Data byte 1]...[Data N]│
   └──────────────────────────────────────┘

3. FPGA writes data to PSRAM at specified address
```

### Read Transaction

```
1. MCU sends Command Header (48 bits) in QSPI mode
   ┌──────────────────────────────────────┐
   │ [0][Length][Address]                 │
   └──────────────────────────────────────┘

2. FPGA reads data from PSRAM at specified address

3. FPGA sends Data Payload (Length bytes) in QSPI mode
   ┌──────────────────────────────────────┐
   │ [Data byte 0][Data byte 1]...[Data N]│
   └──────────────────────────────────────┘
```

## Memory Map

The 8MB PSRAM is divided into regions:

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

#### OSD Framebuffer (0x000000 - 0x00FFFF)

- **Size**: 64 KB
- **Purpose**: Menu/UI overlay
- **Format**: 160×144 pixels, RGB565 (2 bytes/pixel)
- **Actual Usage**: ~45 KB (46,080 bytes)
- **Access**: FPGA reads for display output

#### GB Framebuffer (0x010000 - 0x01FFFF)

- **Size**: 64 KB
- **Purpose**: Game Boy display
- **Format**: 160×144 pixels, RGB565 (2 bytes/pixel)
- **Actual Usage**: ~45 KB (46,080 bytes)
- **Access**: FPGA reads for display output

#### Module Heap (0x020000 - 0x1FFFFF)

- **Size**: 1.875 MB
- **Purpose**: Dynamic module allocations
- **Usage**: Module code, data, buffers
- **Access**: MCU read/write via QSPI

#### Asset Storage (0x200000 - 0x7FFFFF)

- **Size**: 6 MB
- **Purpose**: Game ROMs, graphics, assets
- **Usage**: Large static data
- **Access**: MCU read/write via QSPI

## Example Transactions

### Example 1: Write 256 bytes to address 0x021000

**Command Header:**

```
Command: 1 (write)
Length: 256 (0x100)
Address: 0x00021000

Bit layout:
[1][0000100000][00000000000000100001000000000000]

Bytes (hex):
0x40 0x20 0x00 0x84 0x00 0x00
```

**Data Payload:**

```
256 bytes of data in QSPI mode
```

### Example 2: Read 512 bytes from address 0x200000

**Command Header:**

```
Command: 0 (read)
Length: 512 (0x200)
Address: 0x00200000

Bit layout:
[0][0001000000][00000000001000000000000000000000]

Bytes (hex):
0x20 0x40 0x01 0x00 0x00 0x00
```

**Expected Response:**

```
512 bytes of data from PSRAM in QSPI mode
```

## FPGA Implementation Requirements

### 1. Command Decoder

```verilog
// Pseudo-code for command decoding
always @(posedge clk) begin
    if (header_received) begin
        cmd_write <= header[47];           // Bit 0
        cmd_length <= header[46:37];       // Bits 1-10
        cmd_address <= header[36:5];       // Bits 11-42
    end
end
```

### 2. State Machine

```
States:
1. IDLE: Wait for CS assertion
2. RECEIVE_HEADER: Receive 48-bit command header
3. DECODE: Parse command, length, address
4. WRITE_DATA: Receive data and write to PSRAM (if write command)
5. READ_DATA: Read from PSRAM and send to MCU (if read command)
6. DONE: Complete transaction, return to IDLE
```

### 3. PSRAM Controller Interface

The FPGA must:

- Translate QSPI commands to PSRAM read/write operations
- Handle PSRAM timing requirements
- Buffer data if needed for PSRAM burst operations
- Manage PSRAM refresh cycles

### 4. Performance Considerations

**Bandwidth:**

- QSPI @ 40 MHz = 160 Mbps (4 bits × 40 MHz)
- Theoretical max: 20 MB/s
- Practical with overhead: ~15 MB/s

**Latency:**

- Command header: 48 bits / 160 Mbps = 0.3 μs
- 1KB transfer: ~50 μs
- PSRAM access: Add PSRAM latency

## Testing Protocol

### Test Sequence 1: Basic Read/Write

```
1. Write pattern (0x00-0xFF) to address 0x021000
2. Read back 256 bytes from 0x021000
3. Verify data matches
```

### Test Sequence 2: Burst Transfer

```
1. Write 2KB of data to address 0x021000
2. Read back 2KB from 0x021000
3. Verify all data matches
```

### Test Sequence 3: Address Boundaries

```
1. Write to start of each region (0x000000, 0x010000, 0x020000, 0x200000)
2. Write to end of each region
3. Verify no address wrapping or corruption
```

## Error Handling

### FPGA Should Handle:

1. **Invalid addresses** (>= 0x800000): Ignore or return zeros
2. **Length = 0**: No-op, return to IDLE
3. **CS de-assertion mid-transaction**: Abort, return to IDLE
4. **PSRAM errors**: Retry or flag error (implementation-specific)

## Debug Signals (Optional)

Recommended debug outputs from FPGA:

- `debug_state[2:0]`: Current state machine state
- `debug_cmd_valid`: Command header decoded
- `debug_cmd_write`: Write operation flag
- `debug_cmd_length[9:0]`: Decoded length
- `debug_cmd_address[31:0]`: Decoded address
- `debug_psram_busy`: PSRAM controller busy

## MCU Test Command

The MCU firmware includes a test command:

```
mcu> psram_test
```

This will:

1. Initialize QSPI interface
2. Write 256-byte test pattern to 0x021000
3. Read back and verify
4. Test 2KB burst transfer
5. Report results

## Integration Checklist

- [ ] QSPI pins configured correctly
- [ ] Command header decoder implemented
- [ ] State machine handles read/write operations
- [ ] PSRAM controller integrated
- [ ] Address decoding correct (8MB range)
- [ ] Data buffering if needed
- [ ] Timing meets 40 MHz QSPI requirements
- [ ] Test with MCU `psram_test` command
- [ ] Verify framebuffer regions work
- [ ] Benchmark transfer speeds

## Reference Implementation

See MCU side implementation:

- `main/fpga_psram.c`: QSPI driver
- `main/fpga_psram.h`: API and memory map
- `main/cmd_psram_test.c`: Test command

## Questions?

Contact the MCU team for:

- Protocol clarifications
- Timing requirements
- Test coordination
- Debug support
