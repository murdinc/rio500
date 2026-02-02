package cmd

import (
	"fmt"
	"os"
	"time"

	"github.com/google/gousb"
	"github.com/spf13/cobra"
)

var usbExploreCmd = &cobra.Command{
	Use:   "usb-explore [firmware.bin]",
	Short: "Deep USB exploration for bricked devices",
	Long:  `Exhaustively explores USB capabilities of a bricked device.`,
	Args:  cobra.MaximumNArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		var firmwareData []byte
		if len(args) > 0 {
			var err error
			firmwareData, err = os.ReadFile(args[0])
			if err != nil {
				return fmt.Errorf("failed to read firmware: %w", err)
			}
			fmt.Printf("Firmware loaded: %d bytes\n", len(firmwareData))
		}

		ctx := gousb.NewContext()
		defer ctx.Close()

		dev, err := ctx.OpenDeviceWithVIDPID(0x0841, 0x0001)
		if err != nil {
			return fmt.Errorf("failed to open device: %w", err)
		}
		if dev == nil {
			return fmt.Errorf("Rio 500 not found")
		}
		defer dev.Close()
		dev.SetAutoDetach(true)

		fmt.Println("\n=== Deep USB Exploration ===\n")

		// 1. Enumerate all descriptors
		fmt.Println("--- Device Descriptors ---")
		desc := dev.Desc
		fmt.Printf("Vendor: %04X, Product: %04X\n", desc.Vendor, desc.Product)
		fmt.Printf("Class: %d, SubClass: %d, Protocol: %d\n", desc.Class, desc.SubClass, desc.Protocol)
		fmt.Printf("MaxControlPacket: %d\n", desc.MaxControlPacketSize)

		for cfgNum, cfg := range desc.Configs {
			fmt.Printf("\nConfig %d: MaxPower=%dmA\n", cfgNum, cfg.MaxPower*2)
			for _, iface := range cfg.Interfaces {
				for _, alt := range iface.AltSettings {
					fmt.Printf("  Interface %d Alt %d: Class=%d SubClass=%d Protocol=%d\n",
						alt.Number, alt.Alternate, alt.Class, alt.SubClass, alt.Protocol)
					for _, ep := range alt.Endpoints {
						fmt.Printf("    Endpoint 0x%02X: %s, %s, MaxPacket=%d\n",
							ep.Address, ep.Direction, ep.TransferType, ep.MaxPacketSize)
					}
				}
			}
		}

		// 2. Try to get string descriptors
		fmt.Println("\n--- String Descriptors ---")
		for i := 0; i < 10; i++ {
			str, err := dev.GetStringDescriptor(i)
			if err == nil && str != "" {
				fmt.Printf("  String %d: %s\n", i, str)
			}
		}

		// 3. Try all possible control requests (read)
		fmt.Println("\n--- Scanning Control IN requests ---")
		workingCmds := []byte{}
		for req := 0; req < 256; req++ {
			buf := make([]byte, 64)
			n, err := dev.Control(0xC0, byte(req), 0x00, 0x00, buf)
			if err == nil && n > 0 {
				nonZero := false
				for i := 0; i < n; i++ {
					if buf[i] != 0 {
						nonZero = true
						break
					}
				}
				if nonZero {
					fmt.Printf("  0x%02X: %d bytes: %X\n", req, n, buf[:n])
				} else {
					fmt.Printf("  0x%02X: %d bytes (zeros)\n", req, n)
				}
				workingCmds = append(workingCmds, byte(req))
			}
		}
		fmt.Printf("Working IN commands: %X\n", workingCmds)

		// 4. Try control OUT with no data (just to see which are accepted)
		fmt.Println("\n--- Scanning Control OUT requests (no data) ---")
		workingOutCmds := []byte{}
		for req := 0; req < 256; req++ {
			_, err := dev.Control(0x40, byte(req), 0x00, 0x00, nil)
			if err == nil {
				workingOutCmds = append(workingOutCmds, byte(req))
			}
		}
		fmt.Printf("Working OUT commands (no data): %X\n", workingOutCmds)

		// 5. Try standard USB requests
		fmt.Println("\n--- Standard USB Requests ---")

		// GET_STATUS
		buf := make([]byte, 2)
		n, err := dev.Control(0x80, 0x00, 0x00, 0x00, buf)
		if err == nil {
			fmt.Printf("  GET_STATUS (device): %X\n", buf[:n])
		}

		// GET_CONFIGURATION
		buf = make([]byte, 1)
		n, err = dev.Control(0x80, 0x08, 0x00, 0x00, buf)
		if err == nil {
			fmt.Printf("  GET_CONFIGURATION: %X\n", buf[:n])
		}

		// GET_INTERFACE
		buf = make([]byte, 1)
		n, err = dev.Control(0x81, 0x0A, 0x00, 0x00, buf)
		if err == nil {
			fmt.Printf("  GET_INTERFACE: %X\n", buf[:n])
		}

		// 6. Try different value/index combinations for working commands
		fmt.Println("\n--- Exploring parameters for key commands ---")

		keyCmds := []byte{0x3F, 0x45, 0x46, 0x47, 0x4D}
		for _, cmd := range keyCmds {
			fmt.Printf("\nCommand 0x%02X with different params:\n", cmd)
			interesting := 0
			for val := uint16(0); val <= 0xFF && interesting < 5; val++ {
				for idx := uint16(0); idx <= 0xFF && interesting < 5; idx++ {
					buf := make([]byte, 64)
					n, err := dev.Control(0xC0, cmd, val, idx, buf)
					if err == nil && n > 0 {
						nonZero := false
						for i := 0; i < n; i++ {
							if buf[i] != 0 {
								nonZero = true
								break
							}
						}
						if nonZero {
							fmt.Printf("  val=%04X idx=%04X: %X\n", val, idx, buf[:n])
							interesting++
						}
					}
				}
			}
		}

		// 7. Try sending small data chunks via control
		if len(firmwareData) > 0 {
			fmt.Println("\n--- Trying small control writes ---")

			chunkSizes := []int{1, 2, 4, 8, 16, 32, 64}
			for _, size := range chunkSizes {
				// Reconnect device (it may have crashed)
				dev.Close()
				time.Sleep(500 * time.Millisecond)

				dev, err = ctx.OpenDeviceWithVIDPID(0x0841, 0x0001)
				if err != nil || dev == nil {
					fmt.Printf("  Device disconnected, waiting...\n")
					time.Sleep(2 * time.Second)
					dev, _ = ctx.OpenDeviceWithVIDPID(0x0841, 0x0001)
					if dev == nil {
						continue
					}
				}
				dev.SetAutoDetach(true)

				// Send init sequence
				dev.Control(0x40, 0x47, 0x00, 0x00, nil)
				time.Sleep(50 * time.Millisecond)

				// Try sending chunk
				chunk := firmwareData[:size]
				_, err = dev.Control(0x40, 0x46, 0x00, 0x00, chunk)
				if err != nil {
					fmt.Printf("  %d bytes: failed (%v)\n", size, err)
				} else {
					fmt.Printf("  %d bytes: OK!\n", size)
				}
			}
		}

		// 8. Try reading memory at different addresses
		fmt.Println("\n--- Trying to read memory/data ---")

		// Reconnect
		dev.Close()
		time.Sleep(500 * time.Millisecond)
		dev, _ = ctx.OpenDeviceWithVIDPID(0x0841, 0x0001)
		if dev != nil {
			dev.SetAutoDetach(true)

			// Command 0x45 is "Transfer data to computer"
			// Try with different addresses
			addresses := []uint16{0x0000, 0x0017, 0x00FF, 0xFF00, 0xFFFF}
			for _, addr := range addresses {
				dev.Control(0x40, 0x4E, addr, 0x00, nil)  // Set read address
				time.Sleep(50 * time.Millisecond)

				buf := make([]byte, 64)
				n, err := dev.Control(0xC0, 0x45, 0x00, 0x40, buf)
				if err == nil && n > 0 {
					nonZero := false
					for i := 0; i < n; i++ {
						if buf[i] != 0 {
							nonZero = true
							break
						}
					}
					if nonZero {
						fmt.Printf("  Address 0x%04X: %X\n", addr, buf[:n])
					} else {
						fmt.Printf("  Address 0x%04X: %d bytes (zeros)\n", addr, n)
					}
				}
			}
		}

		// 9. Check for DFU interface
		fmt.Println("\n--- Checking for DFU ---")
		// DFU uses class 0xFE, subclass 0x01
		// Try DFU_GETSTATUS (request 0x03)
		buf = make([]byte, 6)
		_, err = dev.Control(0xA1, 0x03, 0x00, 0x00, buf)
		if err == nil {
			fmt.Printf("  DFU_GETSTATUS: %X (DFU might be supported!)\n", buf)
		} else {
			fmt.Println("  No DFU interface found")
		}

		fmt.Println("\n=== Exploration Complete ===")
		return nil
	},
}

func init() {
	rootCmd.AddCommand(usbExploreCmd)
}
