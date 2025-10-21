# Double Buffering Specification for OSD

## Overview

The MCU now implements double buffering to eliminate tearing artifacts in the OSD display. This requires FPGA support for:
1. Two framebuffer regions in PSRAM
2. Buffer swap command
3. VSYNC signal (optional but recommended)

## Memory Layout

```
┌─────────────────────────────────────────────────────────┐
│ Address Range          │ Size      │ Purpose            │
├─────────────────────────────────────────────────────────┤
│ 0x000000 - 0x00B3FF    │ 46 KB     │ OSD Buffer A       │
│ 0x00B500 - 0x016900    │ 46 KB     │ OSD Buffer B       │
│ 0x010000 - 0x01FFFF    │ 64 KB     │ GB Framebuffer     │
│ 0x020000 - 0x1FFFFF    │ 1.875 MB  │ Module Heap        │
│ 0x200000 - 0x7FFFFF    │ 6 MB      │ Asset Storage      │
└─────────────────────────────────────────────────────────┘
```

**Note:** OSD buffers are 46,080 bytes each (160×144×2 bytes RGB565)

## How It Works

### MCU Side (Implemented)

1. **Rendering Phase:**
   - LVGL draws to internal buffer (`buffy`)
   - MCU writes complete frame to **back buffer** in PSRAM
   - Back buffer alternates between 0x000000 (A) and 0x00B500 (B)

2. **Swap Phase:**
   - MCU waits for all SPI transactions to complete
   - (Optional) MCU waits for VSYNC signal from FPGA
   - MCU sends buffer swap command to FPGA
   - MCU swaps back buffer pointer for next frame

3. **Buffer Swap Command:**
   ```
   Address: 0x100000 (special command address, outside PSRAM range)
   Data: 16-bit value
     - 0 = Display buffer A (0x000000)
     - 1 = Display buffer B (0x00B500)
   ```

### FPGA Side (To Implement)

1. **Dual Buffer Support:**
   - Maintain a "display buffer" pointer (which buffer to read for display)
   - Default to buffer A (0x000000) on startup
   - When reading OSD pixels for display, use current display buffer address

2. **Buffer Swap Command Handler:**
   - Detect writes to address 0x100000
   - Read 16-bit data value:
     - If 0: Set display buffer to 0x000000 (buffer A)
     - If 1: Set display buffer to 0x00B500 (buffer B)
   - Swap should be atomic (single register update)

3. **VSYNC Signal (Optional but Recommended):**
   - Output GPIO signal to MCU on GPIO13
   - Pulse high at start of vertical blank period
   - Pulse duration: 1-10 microseconds
   - This allows MCU to swap buffers during vblank for tear-free display

## Implementation Example (Verilog Pseudocode)

```verilog
// Buffer control
reg [31:0] osd_display_buffer = 32'h00000000;  // Start with buffer A
wire [31:0] osd_read_addr;

// When reading OSD pixels for display
assign osd_read_addr = osd_display_buffer + pixel_offset;

// Command decoder
always @(posedge clk) begin
    if (write_cmd && write_addr == 32'h00100000) begin
        // Buffer swap command received
        if (write_data[0] == 1'b0)
            osd_display_buffer <= 32'h00000000;  // Buffer A
        else
            osd_display_buffer <= 32'h000B500;   // Buffer B
    end
end

// VSYNC output (optional)
always @(posedge clk) begin
    if (vsync_start) begin
        vsync_out <= 1'b1;
        vsync_counter <= 10;  // Pulse for 10 clocks
    end else if (vsync_counter > 0) begin
        vsync_counter <= vsync_counter - 1;
        if (vsync_counter == 1)
            vsync_out <= 1'b0;
    end
end
```

## Testing Without VSYNC

The MCU firmware currently has VSYNC **disabled** by default:
```c
vsync_enabled = false;  // In main.c
```

This allows testing double buffering without VSYNC:
- MCU writes to back buffer
- MCU sends swap command immediately after write completes
- FPGA switches display buffer
- Some tearing may still occur, but should be much better than single buffer

## Enabling VSYNC

Once FPGA implements VSYNC signal on GPIO13:

1. **FPGA:** Output pulse on GPIO13 at start of vertical blank
2. **MCU:** Set `vsync_enabled = true` in main.c and rebuild
3. **Result:** MCU will wait for VSYNC before sending swap command

## Benefits

- **Eliminates tearing:** Display buffer never changes mid-frame
- **Smooth animation:** Clean buffer swaps during vblank
- **Better performance:** MCU can render next frame while FPGA displays current

## Current Status

✅ **MCU Implementation:** Complete
- Double buffering logic implemented
- Buffer swap command sending
- VSYNC ISR ready (disabled until FPGA support)

⏳ **FPGA Implementation:** Pending
- Need dual buffer support
- Need buffer swap command handler (address 0x100000)
- Need VSYNC signal output (GPIO13) - optional

## Testing Commands

```bash
# Flash updated MCU firmware
idf.py flash

# Monitor for debug output
idf.py monitor

# Look for these log messages:
# - "VSYNC disabled - double buffering without sync"
# - "Frame complete, next back buffer: 0x00XXXX"
```

## Questions?

Contact MCU team for:
- Protocol clarifications
- Timing requirements
- Debug support
- VSYNC signal specifications
