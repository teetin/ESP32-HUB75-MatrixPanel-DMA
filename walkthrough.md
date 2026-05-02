# HUB75 Protocol and GPIO Implementation Walkthrough

This document explains the HUB75 protocol and how the GPIO-based implementation works.

## HUB75 Protocol Overview

HUB75 is a common protocol used to drive RGB LED matrix panels. These panels are "dumb" in the sense that they don't have built-in memory for the image. They must be constantly refreshed by an external controller.

### Signals

A standard HUB75 interface consists of the following signals:

- **R1, G1, B1**: Color data for the upper half of the panel.
- **R2, G2, B2**: Color data for the lower half of the panel.
- **CLK** (Clock): Shifts the RGB data into the panel's shift registers on each transition (usually rising edge).
- **LAT** (Latch) / **STB** (Strobe): Latches the data from the shift registers to the output drivers.
- **OE** (Output Enable): Enables the LED drivers. Usually active low. This is used for PWM/BCM to control brightness and color depth.
- **A, B, C, D, E** (Address lines): Selects which row (or pair of rows) is currently being driven.

### Scanning and Multiplexing

Panels are divided into two halves that are driven in parallel. For example, a 64x32 panel is treated as two 64x16 blocks.
The address lines (A, B, C, D) select one of the 16 rows in each block.

#### Scan Settings
The "scan rate" defines how many rows are updated in one refresh cycle relative to the total number of rows.
- **1/16 Scan**: 4 address lines (A, B, C, D) are used to select 1 of 16 rows in each half. A 32-row panel with 1/16 scan has 2 parallel blocks of 16 rows.
- **1/32 Scan**: 5 address lines (A, B, C, D, E) select 1 of 32 rows. Common for 64x64 panels.
- **1/8 Scan**: 3 address lines (A, B, C) select 1 of 8 rows.
- **1/4 Scan**: Often used in outdoor panels, sometimes requiring special mapping as they might update 4 rows in parallel but use different routing.

### Plane Phases: Binary Code Modulation (BCM)

To achieve multiple colors and brightness levels, we use Binary Code Modulation. Since the LEDs are either ON or OFF, we rapidly toggle them to create the illusion of different intensities.

BCM works by dividing the "frame time" into multiple "planes" (or subframes). For 8-bit color depth, we have 8 planes. Each plane corresponds to a bit in the color value (from LSB to MSB).

#### Timing for 8-bit Color:
- **Plane 0 (LSB)**: Displayed for $2^0 = 1$ unit of time.
- **Plane 1**: Displayed for $2^1 = 2$ units of time.
- **Plane 2**: Displayed for $2^2 = 4$ units of time.
- **Plane 3**: Displayed for $2^3 = 8$ units of time.
- **Plane 4**: Displayed for $2^4 = 16$ units of time.
- **Plane 5**: Displayed for $2^5 = 32$ units of time.
- **Plane 6**: Displayed for $2^6 = 64$ units of time.
- **Plane 7 (MSB)**: Displayed for $2^7 = 128$ units of time.

The total time for one row refresh is the sum of these times ($1+2+4+8+16+32+64+128 = 255$ units). By controlling which planes are ON, we can achieve 256 levels of brightness for each color channel (R, G, B).

## HUB75 Protocol Specialities

### Latch Blanking
When switching between rows, there is a brief moment where the address lines change and the latch is toggled. If the LEDs are ON during this transition, "ghosting" or "bleeding" can occur, where light from one row appears faintly on another.
**Latch Blanking** involves disabling the display (OE High) for a few clock cycles before and after the LAT pulse to ensure a clean transition.

### Clock Phase
Different panels may sample the RGB data on either the **rising** or **falling** edge of the CLK signal. Misconfiguring the clock phase can lead to a 1-pixel horizontal shift or general image corruption.

### Driver ICs
While many panels use simple shift registers (like 74HC595), some high-end panels use specialized driver ICs:
- **ICN2038S / FM6124 / FM6126A**: These require a specific initialization sequence (sending magic numbers to internal registers) to unlock the drivers and set the global current/brightness.
- **SM5266P**: Uses a shift-register based line decoder instead of a traditional 138-style decoder. The row address is "clocked in" rather than set directly via address pins.

## GPIO Implementation Details

The GPIO-based implementation manually toggles the ESP32 pins to replicate the HUB75 protocol. This is necessary for ESP32 variants that lack the dedicated LCD/I2S DMA hardware (like the ESP32-C3).

### Refresh Cycle Architecture

1. **Internal Framebuffer**: A buffer in SRAM stores the RGB data for each pixel.
2. **Refresh Task**: A dedicated FreeRTOS task runs in a continuous loop:
   - For each Row Address (0 to $\text{ScanRate}-1$):
     - For each Bitplane (0 to $\text{Depth}-1$):
       - **Shift out pixels**: Loop through each pixel in the row.
         - Read RGB data from framebuffer.
         - Set R1, G1, B1, R2, G2, B2 pins.
         - Toggle CLK.
       - **Latch & Address**:
         - OE High (Disable display).
         - Set Address pins (A, B, C, D, E).
         - LAT High then Low (Latch data).
         - OE Low (Enable display).
       - **Wait**: Delay for a time period proportional to the bitplane weight ($2^{\text{plane}}$).

### Optimization Techniques
To achieve reasonable refresh rates without DMA:
- **Direct Register Access**: Use `GPIO.out_w1ts` and `GPIO.out_w1tc` to set/clear pins in a single instruction.
- **Task Pinning**: Run the refresh task on Core 1 to avoid interference with WiFi/Bluetooth on Core 0.
- **Interrupt Disabling**: Briefly disable interrupts during the pixel-shifting loop to prevent jitter.
