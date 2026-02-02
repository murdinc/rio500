package cmd

import (
	"fmt"
	"os"
	"time"

	"github.com/google/gousb"
	"github.com/spf13/cobra"
)

var controlRecoveryCmd = &cobra.Command{
	Use:   "control-recovery <firmware.bin>",
	Short: "Firmware recovery using only control transfers",
	Long: `Attempts firmware recovery using USB control transfers only.

Since bulk transfers fail on bricked devices, this attempts to send
firmware data through control transfers which seem to still work.`,
	Args: cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		firmwarePath := args[0]

		firmwareData, err := os.ReadFile(firmwarePath)
		if err != nil {
			return fmt.Errorf("failed to read firmware file: %w", err)
		}
		fmt.Printf("Firmware: %s (%d bytes)\n", firmwarePath, len(firmwareData))

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

		fmt.Println("\n=== Control Transfer Recovery ===\n")

		// Step 1: Send start communication
		fmt.Print("Sending 0x47 (start comm)... ")
		_, err = dev.Control(0x40, 0x47, 0x00, 0x00, nil)
		if err != nil {
			fmt.Printf("failed: %v\n", err)
		} else {
			fmt.Println("OK")
		}
		time.Sleep(100 * time.Millisecond)

		// Step 2: Send firmware mode command
		fmt.Print("Sending 0x4D (firmware mode)... ")
		_, err = dev.Control(0x40, 0x4D, 0x2185, 0x00, nil)
		if err != nil {
			fmt.Printf("failed: %v\n", err)
		} else {
			fmt.Println("OK")
		}
		time.Sleep(500 * time.Millisecond)

		// Step 3: Try various approaches to send firmware data

		// Approach A: Send firmware in small chunks via control OUT
		fmt.Println("\n--- Approach A: Control OUT with data ---")
		chunkSize := 64 // Max control transfer size is typically 64 bytes
		sent := 0
		errors := 0

		for sent < len(firmwareData) && errors < 10 {
			end := sent + chunkSize
			if end > len(firmwareData) {
				end = len(firmwareData)
			}

			chunk := firmwareData[sent:end]

			// Try sending chunk as control OUT data
			// Request 0x46 with the chunk as data payload
			_, err = dev.Control(0x40, 0x46, uint16(sent>>16), uint16(sent&0xFFFF), chunk)
			if err != nil {
				errors++
				if errors <= 3 {
					fmt.Printf("  Chunk at %d failed: %v\n", sent, err)
				}
			} else {
				sent += len(chunk)
				if sent%(chunkSize*100) == 0 || sent == len(firmwareData) {
					fmt.Printf("  Sent: %d/%d bytes\n", sent, len(firmwareData))
				}
			}
			time.Sleep(1 * time.Millisecond)
		}

		if sent == len(firmwareData) {
			fmt.Printf("Successfully sent all %d bytes via control transfers!\n", sent)
		} else {
			fmt.Printf("Control OUT approach sent %d/%d bytes before failing\n", sent, len(firmwareData))
		}

		// Approach B: Try using request 0x3F with different parameters
		fmt.Println("\n--- Approach B: Different 0x3F parameters ---")

		// Maybe 0x3F needs different value/index
		testParams := []struct {
			val uint16
			idx uint16
			desc string
		}{
			{0x0000, 0x0000, "val=0, idx=0"},
			{0x0001, 0x4000, "val=1, idx=0x4000 (size low)"},
			{0x4000, 0x0001, "val=0x4000, idx=1"},
			{uint16(len(firmwareData) >> 16), uint16(len(firmwareData) & 0xFFFF), "val=sizeHi, idx=sizeLo"},
			{uint16(len(firmwareData) & 0xFFFF), uint16(len(firmwareData) >> 16), "val=sizeLo, idx=sizeHi"},
		}

		for _, p := range testParams {
			fmt.Printf("  0x3F with %s... ", p.desc)
			_, err = dev.Control(0x40, 0x3F, p.val, p.idx, nil)
			if err != nil {
				fmt.Printf("failed: %v\n", err)
			} else {
				fmt.Println("OK!")
			}
			time.Sleep(100 * time.Millisecond)
		}

		// Approach C: Try reading status/response
		fmt.Println("\n--- Approach C: Reading device status ---")

		statusCmds := []byte{0x40, 0x42, 0x43, 0x47, 0x50, 0x57, 0x59}
		for _, cmd := range statusCmds {
			buf := make([]byte, 64)
			n, err := dev.Control(0xC0, cmd, 0x00, 0x00, buf)
			if err != nil {
				fmt.Printf("  0x%02X: failed (%v)\n", cmd, err)
			} else {
				// Check if response is all zeros
				nonZero := false
				for i := 0; i < n; i++ {
					if buf[i] != 0 {
						nonZero = true
						break
					}
				}
				if nonZero {
					fmt.Printf("  0x%02X: %d bytes: %X\n", cmd, n, buf[:n])
				} else {
					fmt.Printf("  0x%02X: %d bytes (all zeros)\n", cmd, n)
				}
			}
			time.Sleep(50 * time.Millisecond)
		}

		// Approach D: Try the exact sequence from working firmware flash
		fmt.Println("\n--- Approach D: Exact firmware sequence ---")

		// Initialize
		fmt.Print("  0x47 init... ")
		dev.Control(0x40, 0x47, 0x00, 0x00, nil)
		fmt.Println("sent")
		time.Sleep(100 * time.Millisecond)

		// Wait loop
		for i := 0; i < 5; i++ {
			buf := make([]byte, 4)
			dev.Control(0xC0, 0x42, 0x00, 0x00, buf)
			fmt.Printf("  0x42 wait [%d]: %X\n", i, buf)
			time.Sleep(200 * time.Millisecond)
		}

		// Send firmware write setup
		sizeLo := uint16(len(firmwareData) & 0xFFFF)
		sizeHi := uint16((len(firmwareData) >> 16) & 0xFFFF)

		fmt.Printf("  0x3F firmware setup (sizeHi=%04X, sizeLo=%04X)... ", sizeHi, sizeLo)
		_, err = dev.Control(0x40, 0x3F, sizeHi, sizeLo, nil)
		if err != nil {
			fmt.Printf("failed: %v\n", err)

			// Try alternate: send firmware data WITH the 0x3F command
			fmt.Println("  Trying 0x3F with firmware data attached...")
			_, err = dev.Control(0x40, 0x3F, sizeHi, sizeLo, firmwareData[:64])
			if err != nil {
				fmt.Printf("  Also failed: %v\n", err)
			} else {
				fmt.Println("  Interesting - that worked!")
			}
		} else {
			fmt.Println("OK")
		}

		// Commit
		fmt.Print("  0x4D commit... ")
		dev.Control(0x40, 0x4D, 0x2185, 0x00, nil)
		fmt.Println("sent")

		// Final wait
		for i := 0; i < 3; i++ {
			dev.Control(0x40, 0x42, 0x00, 0x00, nil)
			time.Sleep(100 * time.Millisecond)
		}

		fmt.Println("\n=== Recovery Attempts Complete ===")
		fmt.Println("Power cycle the device and check if anything changed.")

		return nil
	},
}

func init() {
	rootCmd.AddCommand(controlRecoveryCmd)
}
