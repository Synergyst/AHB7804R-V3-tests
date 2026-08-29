# AHB7804R-V3 FW Modification Toolkit

This toolkit provides a non-destructive way to analyze the differences between a stock AHB7804R-V3 firmware dump and a modified live dump, and to apply those differences to create new, flashable binary images.

## Overview

The toolkit is designed for reverse engineers and firmware modders who need to move modifications from a live system (captured via NFS/mtd) back into a flashable binary for use with SPI NOR programmers (such as XGecu).

### Key Features
- **Non-Destructive**: All analysis is performed in a read-only fashion.
- **Binary Precision**: Uses a JSON-based patch system to record exact offsets and byte sequences.
- **Safety Checks**: Prevents accidental overwriting of existing firmware dumps.
- **No Dependencies**: Built using standard Bash and Node.js (standard library) to avoid dependency hell.

## Usage

The main entry point is `toolkit.sh`.

### 1. Analyze and Create a Patch
Use the `compare` command to compare a stock binary against a modded (live) binary. This generates a `mods.json` file containing the binary delta.

```bash
./toolkit.sh compare <original_stock.bin> <live_dump.bin> <output_patch.json>
```

**Example:**
```bash
./toolkit.sh compare stock_fw.bin live_dump.bin my_mods.json
```

### 2. Generate a Flashable Binary
Use the `patch` command to apply a previously generated patch file to a stock binary. This creates a new binary that can be programmed directly to the SPI NOR chip.

```bash
./toolkit.sh patch <stock_bin> <patch_json> <output_bin>
```

**Example:**
```bash
./toolkit.sh patch stock_fw.bin my_mods.json flash_me.bin
```

## Technical Details

### Patch Format
The toolkit generates a JSON file consisting of "patch segments". Each segment contains:
- `offset`: The byte position in the binary where the change starts.
- `length`: The number of bytes being modified.
- `old`: The original hex bytes (for verification).
- `new`: The replacement hex bytes.

### Requirements
- **Bash**
- **Node.js** (Version 12+)

## Disclaimer
Modifying firmware can brick your device. Always ensure you have a verified backup of your original SPI NOR dump before flashing any modified binaries.
