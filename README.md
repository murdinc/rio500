# Rio 500 CLI Tool

A modern command-line interface for the Diamond Rio 500 MP3 player (1999), written in Go.

## About the Rio 500

The Diamond Rio 500 was an early portable MP3 player released in 1999. It features:

- **CPU**: Actel A42MX09-F (Antifuse FPGA - one-time programmable)
- **USB Controller**: ScanLogic SL11R-100
- **Config EEPROM**: 24LC16B (2KB I2C)
- **SRAM**: ISSI IS61LV256 (32KB)
- **Storage**: 4x Samsung KM29U128T NAND Flash (64MB total)
- **USB**: Pre-standard USB 1.1 (data lines reversed from modern USB)

## Installation

```bash
go install github.com/murdinc/rio500@latest
```

Or build from source:

```bash
git clone https://github.com/murdinc/rio500.git
cd rio500
go build -o rio500 .
```

## Usage

### Basic Commands

```bash
# Show device information
rio500 info

# List files on device
rio500 list

# Upload an MP3 file
rio500 upload song.mp3

# Download a file from device
rio500 download "Song Name" output.mp3

# Delete a file
rio500 delete "Song Name"

# Create a folder
rio500 folder create "My Folder"

# Format device storage
rio500 format

# Show firmware version
rio500 firmware-version
```

### Firmware Commands

```bash
# Upgrade firmware
rio500 firmware-upgrade firmware/firmware_2.16.bin

# Force upgrade (for recovery, skips version check)
rio500 firmware-upgrade --force firmware/firmware_2.16.bin
```

### Recovery Commands

These commands were created for attempting to recover bricked devices:

```bash
# Explore USB endpoints and commands
rio500 usb-explore

# Aggressive recovery attempts
rio500 brute-recovery firmware/firmware_2.16.bin

# Control-transfer-only recovery
rio500 control-recovery firmware/firmware_2.16.bin

# Quick flash attempt
rio500 quick-flash firmware/firmware_2.16.bin

# Prepare SmartMedia card for recovery boot
rio500 recovery-card --device /dev/diskX --firmware firmware/firmware_2.16.bin
```

### Bus Pirate EEPROM Access

For hardware-level recovery using a Bus Pirate:

```bash
# Scan I2C bus for devices
rio500 buspirate-eeprom --port /dev/tty.usbmodem000000011

# Dump EEPROM contents
rio500 buspirate-eeprom --port /dev/tty.usbmodem000000011 --dump

# Write to EEPROM
rio500 buspirate-eeprom --port /dev/tty.usbmodem000000011 --write eeprom.bin

# Erase EEPROM (fill with 0xFF)
rio500 buspirate-eeprom --port /dev/tty.usbmodem000000011 --erase

# Monitor UART output
rio500 buspirate-eeprom --port /dev/tty.usbmodem000000011 --uart --baud 9600
```

## Hardware Notes

### USB Connection

The Rio 500 uses pre-standard USB with reversed data lines. Original cables have:
- +5V line cut (device is battery powered)
- D+ and D- swapped compared to standard USB

### Debug Header (6-pin)

The Rio 500 PCB has a 6-pin debug header (JP20A/JP20B) with:
- +V (3.3V)
- GND
- SCL (I2C clock to 24LC16B)
- SDA (I2C data to 24LC16B)
- UART_TXD (to SL11R-100)
- UART_RXD (to SL11R-100)

### Bus Pirate Wiring

**For I2C (EEPROM access):**
| Bus Pirate | Debug Header |
|------------|--------------|
| GND | GND |
| CLK | SCL |
| MOSI | SDA |
| VPU | +V (pull-up reference) |

**For UART (serial monitor):**
| Bus Pirate | Debug Header |
|------------|--------------|
| GND | GND |
| MISO | UART_TXD |
| MOSI | UART_RXD |

## Project Structure

```
rio500/
├── cmd/                    # CLI commands
│   ├── root.go            # Main command setup
│   ├── info.go            # Device info
│   ├── list.go            # List files
│   ├── upload.go          # Upload files
│   ├── download.go        # Download files
│   ├── delete.go          # Delete files
│   ├── folder.go          # Folder management
│   ├── format.go          # Format storage
│   ├── firmware_upgrade.go # Firmware upgrade
│   ├── firmware_version.go # Show firmware version
│   ├── buspirate_eeprom.go # Bus Pirate interface
│   ├── brute_recovery.go  # Recovery tools
│   ├── control_recovery.go
│   ├── quick_flash.go
│   ├── recovery_card.go   # SmartMedia recovery
│   └── usb_explore.go     # USB exploration
├── pkg/rio500/            # Core library
│   ├── rio500.go          # Main device interface
│   ├── usb.go             # USB communication
│   └── font.go            # LCD font data
├── firmware/              # Firmware files
│   ├── firmware_2.15.bin
│   └── firmware_2.16.bin
├── tools/                 # External tools
│   ├── pirate-loader      # Bus Pirate firmware flasher
│   └── bp4-firmware-v8.hex # Bus Pirate v4 firmware with UART
└── reference/             # Reverse engineering reference
    ├── rio500-0.7/        # Original Linux driver source
    ├── Rio500Flash_216.exe # Original Windows flasher
    └── Rio500Flash_216.asm # Disassembly of flasher
```

## Dependencies

- [gousb](https://github.com/google/gousb) - USB access
- [cobra](https://github.com/spf13/cobra) - CLI framework
- [go.bug.st/serial](https://github.com/bugst/go-serial) - Serial port access

## References

- [rio500 Linux driver](http://rio500.sourceforge.net/) - Original reverse engineering
- [Bus Pirate](http://dangerousprototypes.com/docs/Bus_Pirate) - Hardware hacking tool
- [24LC16B Datasheet](https://www.microchip.com/wwwproducts/en/24LC16B) - I2C EEPROM

## License

MIT
