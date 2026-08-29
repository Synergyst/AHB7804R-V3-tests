# AHB7804R-V3 FW Modification Toolkit

This toolkit provides a comprehensive suite of tools for analyzing, modifying, and deploying firmware on the AHB7804R-V3 NVR. It allows for non-destructive analysis of firmware dumps and the creation of flashable binaries for SPI NOR programmers (such as XGecu).

## Repository Overview

### 🛠️ Binary Modification Tools
These tools allow you to move modifications from a live system back into a flashable binary without needing to unpack the entire filesystem.

- **`toolkit.sh`**: The main CLI wrapper for binary analysis and patching.
- **`bin-diff.js`**: Analyzes differences between two binaries and generates a JSON-based patch blueprint.
- **`bin-patch.js`**: Applies a JSON patch blueprint to a stock binary to create a new flashable image.

**Usage:**
```bash
# Create a patch from a stock dump and a live dump
./toolkit.sh compare <stock.bin> <live_dump.bin> <mods.json>

# Apply that patch to a clean stock image to create a flashable binary
./toolkit.sh patch <stock_bin> <mods.json> <flashable_output.bin>
```

### 📡 Live Capture & Deployment
Tools used to interact with the device in real-time and extract data.

- **`capture.sh`**: A telnet-based automation script that mounts NFS on the NVR and launches the frame streaming pipeline.
- **`transfer-rsh.expect`**: An Expect script used to push binary files (like `rsh`) to the NVR via telnet using `cat > file` to bypass the lack of SCP/FTP.
- **`frame_stream_tcp.cpp`**: A high-performance frame grabber that reads raw video buffers from physical memory (`/dev/mem`) and streams them over TCP.
- **`sofia_wdt_supervisor.cpp`**: A "Watchdog Mitigation Wrapper" that pulses the Sofia process (SIGCONT/SIGSTOP) to prevent Hardware Watchdog Timer (WDT) trips during heavy I/O.

### 🏗️ Build & Maintenance
- **`build-new-flash-image.sh`**: A script to rebuild the SquashFS partition and inject it back into the firmware binary at the correct offset.
- **`rsh.c`**: Source for a minimal remote shell utility.

## Technical Details

### Firmware Layout
The SPI NOR flash is partitioned into several areas. The primary OS resides in a SquashFS partition starting at offset `0x30000` (196608 bytes).

### Requirements
- **Host System**: Linux (with `mount` and `loop` support).
- **Runtime**: Node.js (v12+), Bash, Expect.
- **Hardware**: SPI NOR Programmer (e.g., XGecu) for flashing the generated `.bin` files.

## Disclaimer
Modifying firmware can brick your device. Always ensure you have a verified backup of your original SPI NOR dump before flashing any modified binaries.
