package cmd

import (
	"bufio"
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/spf13/cobra"
	"go.bug.st/serial"
)

var (
	bpPort     string
	bpDump     bool
	bpWrite    string
	bpErase    bool
	bpUart     bool
	bpBaud     int
)

var busPirateCmd = &cobra.Command{
	Use:   "buspirate-eeprom",
	Short: "Read/write 24LC16B EEPROM or monitor UART via Bus Pirate",
	Long: `Interface with the Rio 500 debug header using a Bus Pirate.

The 6-pin debug header provides:
  - I2C (SCL/SDA) → 24LC16B EEPROM
  - UART (RXD/TXD) → SL11R-100 USB controller

Bus Pirate connections for I2C:
  - GND (black) → GND
  - MOSI (grey) → SDA
  - CLK (purple) → SCL
  - 3.3V (red) → +V (if device unpowered)

Bus Pirate connections for UART:
  - GND (black) → GND
  - MISO (brown) → UART_TXD (Rio transmits)
  - MOSI (grey) → UART_RXD (Rio receives)
  - 3.3V (red) → +V (if device unpowered)

Examples:
  rio500 buspirate-eeprom --port /dev/tty.usbserial-A1234 --dump
  rio500 buspirate-eeprom --port /dev/tty.usbserial-A1234 --write eeprom_backup.bin
  rio500 buspirate-eeprom --port /dev/tty.usbserial-A1234 --uart --baud 9600`,
	RunE: func(cmd *cobra.Command, args []string) error {
		if bpPort == "" {
			// Try to find Bus Pirate
			ports, _ := serial.GetPortsList()
			for _, p := range ports {
				if strings.Contains(p, "usbserial") || strings.Contains(p, "USB") {
					fmt.Printf("Found potential port: %s\n", p)
				}
			}
			return fmt.Errorf("please specify --port /dev/tty.usbserial-XXXX")
		}

		// Open serial port
		mode := &serial.Mode{
			BaudRate: 115200,
			DataBits: 8,
			Parity:   serial.NoParity,
			StopBits: serial.OneStopBit,
		}

		port, err := serial.Open(bpPort, mode)
		if err != nil {
			return fmt.Errorf("failed to open port: %w", err)
		}
		defer port.Close()

		bp := &BusPirate{port: port}

		fmt.Println("Connecting to Bus Pirate...")

		// UART mode - handle separately before I2C (no reset needed)
		if bpUart {
			return bp.MonitorUART(bpBaud)
		}

		// For I2C modes, reset to known state
		bp.SendCmd("\n\n\n")
		time.Sleep(100 * time.Millisecond)

		// Enter binary mode for faster I2C
		fmt.Println("Entering binary I2C mode...")
		if err := bp.EnterBinaryMode(); err != nil {
			// Fall back to text mode
			fmt.Println("Binary mode failed, using text mode...")
			return bp.TextModeI2C(bpDump, bpWrite, bpErase)
		}

		if bpDump {
			return bp.DumpEEPROM()
		} else if bpWrite != "" {
			return bp.WriteEEPROM(bpWrite)
		} else if bpErase {
			return bp.EraseEEPROM()
		}

		// Default: just scan for devices
		return bp.ScanI2C()
	},
}

type BusPirate struct {
	port serial.Port
}

func (bp *BusPirate) SendCmd(cmd string) (string, error) {
	_, err := bp.port.Write([]byte(cmd + "\n"))
	if err != nil {
		return "", err
	}
	time.Sleep(50 * time.Millisecond)

	buf := make([]byte, 1024)
	n, _ := bp.port.Read(buf)
	return string(buf[:n]), nil
}

func (bp *BusPirate) EnterBinaryMode() error {
	// Send 20x 0x00 to enter binary mode
	for i := 0; i < 20; i++ {
		bp.port.Write([]byte{0x00})
		time.Sleep(5 * time.Millisecond)
	}

	buf := make([]byte, 5)
	bp.port.Read(buf)

	if string(buf) == "BBIO1" {
		// Enter I2C mode (0x02)
		bp.port.Write([]byte{0x02})
		time.Sleep(10 * time.Millisecond)
		bp.port.Read(buf[:4])
		if string(buf[:4]) == "I2C1" {
			fmt.Println("Binary I2C mode active")

			// Configure: 100kHz, 3.3V
			bp.port.Write([]byte{0x62}) // Speed 100kHz
			time.Sleep(10 * time.Millisecond)

			// Enable power
			bp.port.Write([]byte{0x4C}) // Power on, pullups on
			time.Sleep(10 * time.Millisecond)

			return nil
		}
	}
	return fmt.Errorf("failed to enter binary mode")
}

func (bp *BusPirate) ScanI2C() error {
	fmt.Println("\nScanning I2C bus for devices...")

	found := []byte{}
	for addr := byte(0x00); addr < 0x80; addr++ {
		// Start, write address, stop
		bp.port.Write([]byte{0x02})                  // Start
		bp.port.Write([]byte{0x10, addr << 1})       // Write 1 byte
		time.Sleep(5 * time.Millisecond)

		buf := make([]byte, 2)
		bp.port.Read(buf)

		if buf[1] == 0x00 { // ACK received
			found = append(found, addr)
		}

		bp.port.Write([]byte{0x03}) // Stop
		time.Sleep(2 * time.Millisecond)
	}

	if len(found) > 0 {
		fmt.Printf("Found devices at addresses: ")
		for _, addr := range found {
			fmt.Printf("0x%02X ", addr)
		}
		fmt.Println()

		// 24LC16B responds at 0x50-0x57 (or 0xA0-0xAF in 8-bit notation)
		for _, addr := range found {
			if addr >= 0x50 && addr <= 0x57 {
				fmt.Printf("\n24LC16B detected at 0x%02X!\n", addr)
			}
		}
	} else {
		fmt.Println("No I2C devices found.")
		fmt.Println("\nTroubleshooting:")
		fmt.Println("  - Check wiring (GND, SDA, SCL)")
		fmt.Println("  - Ensure device has power")
		fmt.Println("  - Try enabling Bus Pirate power output (W command)")
	}

	return nil
}

func (bp *BusPirate) DumpEEPROM() error {
	fmt.Println("\nDumping 24LC16B EEPROM (2048 bytes)...")

	data := make([]byte, 2048)

	// 24LC16B has 8 blocks of 256 bytes, addressed at 0x50-0x57
	for block := 0; block < 8; block++ {
		addr := byte(0x50 + block)
		fmt.Printf("  Reading block %d (address 0x%02X)...\n", block, addr)

		// Set address pointer to 0
		bp.port.Write([]byte{0x02})                    // Start
		bp.port.Write([]byte{0x11, addr << 1, 0x00})   // Write: addr + offset 0
		time.Sleep(5 * time.Millisecond)

		// Read 256 bytes
		bp.port.Write([]byte{0x02})                    // Repeated start
		bp.port.Write([]byte{0x10, (addr << 1) | 1})   // Read address
		time.Sleep(5 * time.Millisecond)

		// Read bytes
		for i := 0; i < 256; i++ {
			if i < 255 {
				bp.port.Write([]byte{0x06}) // Read with ACK
			} else {
				bp.port.Write([]byte{0x07}) // Read with NACK (last byte)
			}
			time.Sleep(2 * time.Millisecond)

			buf := make([]byte, 2)
			bp.port.Read(buf)
			data[block*256+i] = buf[1]
		}

		bp.port.Write([]byte{0x03}) // Stop
		time.Sleep(10 * time.Millisecond)
	}

	// Save to file
	filename := "eeprom_dump.bin"
	if err := os.WriteFile(filename, data, 0644); err != nil {
		return fmt.Errorf("failed to save dump: %w", err)
	}

	fmt.Printf("\nSaved to %s\n", filename)

	// Print hex dump
	fmt.Println("\nHex dump (first 256 bytes):")
	for i := 0; i < 256; i += 16 {
		fmt.Printf("%04X: ", i)
		for j := 0; j < 16; j++ {
			fmt.Printf("%02X ", data[i+j])
		}
		fmt.Print(" |")
		for j := 0; j < 16; j++ {
			c := data[i+j]
			if c >= 32 && c < 127 {
				fmt.Printf("%c", c)
			} else {
				fmt.Print(".")
			}
		}
		fmt.Println("|")
	}

	return nil
}

func (bp *BusPirate) WriteEEPROM(filename string) error {
	data, err := os.ReadFile(filename)
	if err != nil {
		return fmt.Errorf("failed to read file: %w", err)
	}

	if len(data) > 2048 {
		return fmt.Errorf("file too large (max 2048 bytes, got %d)", len(data))
	}

	fmt.Printf("Writing %d bytes to EEPROM...\n", len(data))
	fmt.Println("WARNING: This will overwrite the EEPROM contents!")
	fmt.Print("Type 'yes' to continue: ")

	reader := bufio.NewReader(os.Stdin)
	response, _ := reader.ReadString('\n')
	if strings.TrimSpace(response) != "yes" {
		fmt.Println("Cancelled.")
		return nil
	}

	// Write page by page (24LC16B has 16-byte pages)
	pageSize := 16
	for offset := 0; offset < len(data); offset += pageSize {
		block := offset / 256
		addr := byte(0x50 + block)
		pageAddr := byte(offset % 256)

		end := offset + pageSize
		if end > len(data) {
			end = len(data)
		}
		page := data[offset:end]

		// Write page
		bp.port.Write([]byte{0x02})                        // Start
		bp.port.Write([]byte{byte(len(page) + 2), addr << 1, pageAddr})
		for _, b := range page {
			bp.port.Write([]byte{b})
		}
		time.Sleep(5 * time.Millisecond)
		bp.port.Write([]byte{0x03}) // Stop

		// Wait for write cycle (5ms max for 24LC16B)
		time.Sleep(10 * time.Millisecond)

		if (offset/pageSize)%16 == 0 {
			fmt.Printf("  Written %d/%d bytes\n", offset+len(page), len(data))
		}
	}

	fmt.Println("Write complete!")
	return nil
}

func (bp *BusPirate) EraseEEPROM() error {
	fmt.Println("Erasing EEPROM (filling with 0xFF)...")
	fmt.Print("Type 'yes' to continue: ")

	reader := bufio.NewReader(os.Stdin)
	response, _ := reader.ReadString('\n')
	if strings.TrimSpace(response) != "yes" {
		fmt.Println("Cancelled.")
		return nil
	}

	// Create 2KB of 0xFF
	data := make([]byte, 2048)
	for i := range data {
		data[i] = 0xFF
	}

	// Write using same method
	tmpFile := "/tmp/eeprom_erase.bin"
	os.WriteFile(tmpFile, data, 0644)
	return bp.WriteEEPROM(tmpFile)
}

func (bp *BusPirate) TextModeI2C(dump bool, write string, erase bool) error {
	fmt.Println("\nUsing text mode I2C (slower but more compatible)")

	// Set I2C mode
	bp.SendCmd("m")
	time.Sleep(100 * time.Millisecond)
	bp.SendCmd("4") // I2C
	time.Sleep(100 * time.Millisecond)
	bp.SendCmd("3") // 100kHz
	time.Sleep(100 * time.Millisecond)

	// Enable power
	bp.SendCmd("W")
	time.Sleep(100 * time.Millisecond)

	// Scan
	fmt.Println("Scanning I2C bus (macro 1)...")
	resp, _ := bp.SendCmd("(1)")
	fmt.Println(resp)

	if dump {
		fmt.Println("\nReading EEPROM...")
		// Read from 24LC16B at 0xA0
		bp.SendCmd("[0xA0 0x00]") // Set address to 0
		time.Sleep(50 * time.Millisecond)

		resp, _ := bp.SendCmd("[0xA1 r:256]") // Read 256 bytes
		fmt.Println(resp)
	}

	return nil
}

func (bp *BusPirate) MonitorUART(baud int) error {
	fmt.Printf("\nSetting up UART monitoring at %d baud...\n", baud)
	fmt.Println("Connect: Bus Pirate MISO → Rio UART_TXD, MOSI → Rio UART_RXD")
	fmt.Println("Press Ctrl+C to exit\n")

	drainBuf := make([]byte, 1024)
	bp.port.SetReadTimeout(200 * time.Millisecond)

	// Exit any current mode first - send 'x' to exit to HiZ
	fmt.Println("Exiting any current mode...")
	bp.port.Write([]byte("x\n"))
	time.Sleep(200 * time.Millisecond)
	bp.port.Read(drainBuf)
	bp.port.Write([]byte("\n"))
	time.Sleep(100 * time.Millisecond)
	bp.port.Read(drainBuf)

	// Now enter mode selection
	fmt.Println("Entering mode selection...")
	bp.port.Write([]byte("m\n"))
	time.Sleep(300 * time.Millisecond)
	n, _ := bp.port.Read(drainBuf)
	if n > 0 {
		fmt.Printf("  Menu: %s\n", strings.TrimSpace(string(drainBuf[:n])))
	}

	// Select UART mode (option 3 on most Bus Pirates)
	fmt.Println("Selecting UART mode (3)...")
	bp.port.Write([]byte("3\n"))
	time.Sleep(300 * time.Millisecond)
	n, _ = bp.port.Read(drainBuf)
	if n > 0 {
		fmt.Printf("  Response: %s\n", strings.TrimSpace(string(drainBuf[:n])))
	}

	// Set baud rate
	var baudOption string
	switch baud {
	case 300:
		baudOption = "1"
	case 1200:
		baudOption = "2"
	case 2400:
		baudOption = "3"
	case 4800:
		baudOption = "4"
	case 9600:
		baudOption = "5"
	case 19200:
		baudOption = "6"
	case 38400:
		baudOption = "7"
	case 57600:
		baudOption = "8"
	case 115200:
		baudOption = "9"
	default:
		baudOption = "5" // Default 9600
	}
	fmt.Printf("Setting baud rate %d...\n", baud)
	bp.port.Write([]byte(baudOption + "\n"))
	time.Sleep(300 * time.Millisecond)
	bp.port.Read(drainBuf)

	// 8N1 settings - answer each prompt
	fmt.Println("Configuring 8N1...")
	settings := []string{"1", "1", "1", "1", "2"} // 8bits, no parity, 1 stop, idle 1, open drain
	for _, s := range settings {
		bp.port.Write([]byte(s + "\n"))
		time.Sleep(200 * time.Millisecond)
		bp.port.Read(drainBuf)
	}

	// Enable power supply
	fmt.Println("Enabling power (W)...")
	bp.port.Write([]byte("W\n"))
	time.Sleep(200 * time.Millisecond)
	bp.port.Read(drainBuf)

	// Enter UART bridge/transparent mode
	fmt.Println("Entering UART bridge mode...")
	bp.port.Write([]byte("(1)\n"))
	time.Sleep(500 * time.Millisecond)
	bp.port.Read(drainBuf) // read the "Are you sure?" prompt

	// Confirm with 'y'
	bp.port.Write([]byte("y\n"))
	time.Sleep(300 * time.Millisecond)

	// Set longer read timeout for monitoring
	bp.port.SetReadTimeout(100 * time.Millisecond)

	fmt.Println("\n========== UART OUTPUT ==========")
	fmt.Println("(Power cycle the Rio 500 now...)")
	fmt.Println("==================================\n")

	// Read and display UART data continuously
	buf := make([]byte, 256)
	for {
		n, _ := bp.port.Read(buf)
		if n > 0 {
			// Print readable characters, show hex for non-printable
			for i := 0; i < n; i++ {
				c := buf[i]
				if c >= 32 && c < 127 {
					fmt.Printf("%c", c)
				} else if c == '\n' || c == '\r' {
					fmt.Printf("%c", c)
				} else if c != 0 {
					fmt.Printf("[%02X]", c)
				}
			}
			// Flush stdout
			os.Stdout.Sync()
		}
	}
}

func init() {
	rootCmd.AddCommand(busPirateCmd)
	busPirateCmd.Flags().StringVar(&bpPort, "port", "", "Bus Pirate serial port (e.g., /dev/tty.usbserial-A1234)")
	busPirateCmd.Flags().BoolVar(&bpDump, "dump", false, "Dump EEPROM contents to file")
	busPirateCmd.Flags().StringVar(&bpWrite, "write", "", "Write file to EEPROM")
	busPirateCmd.Flags().BoolVar(&bpErase, "erase", false, "Erase EEPROM (fill with 0xFF)")
	busPirateCmd.Flags().BoolVar(&bpUart, "uart", false, "Monitor UART debug output")
	busPirateCmd.Flags().IntVar(&bpBaud, "baud", 9600, "UART baud rate (9600, 19200, 38400, 57600, 115200)")
}
