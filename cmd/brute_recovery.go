package cmd

import (
	"fmt"
	"os"
	"time"

	"github.com/google/gousb"
	"github.com/spf13/cobra"
)

var bruteRecoveryCmd = &cobra.Command{
	Use:   "brute-recovery <firmware.bin>",
	Short: "Aggressive firmware recovery attempt for bricked devices",
	Long: `Attempts aggressive firmware recovery by bypassing normal protocol.

This command tries multiple approaches:
1. Direct bulk writes without control commands
2. Writing to different endpoints
3. Various USB reset sequences
4. Raw data transfers

Use this when the device shows on USB but doesn't respond to normal commands.`,
	Args: cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		firmwarePath := args[0]

		// Read firmware file
		firmwareData, err := os.ReadFile(firmwarePath)
		if err != nil {
			return fmt.Errorf("failed to read firmware file: %w", err)
		}
		fmt.Printf("Firmware: %s (%d bytes)\n", firmwarePath, len(firmwareData))

		// Create USB context with debug
		ctx := gousb.NewContext()
		defer ctx.Close()
		ctx.Debug(2) // Enable debug output

		fmt.Println("\n=== Attempting Brute Force Recovery ===\n")

		// Find the device
		dev, err := ctx.OpenDeviceWithVIDPID(0x0841, 0x0001)
		if err != nil {
			return fmt.Errorf("failed to open device: %w", err)
		}
		if dev == nil {
			return fmt.Errorf("Rio 500 not found")
		}
		defer dev.Close()

		fmt.Println("Device found!")

		// Try to set auto detach
		dev.SetAutoDetach(true)

		// Get device descriptor info
		desc := dev.Desc
		fmt.Printf("Device: VID=%04x PID=%04x\n", desc.Vendor, desc.Product)
		fmt.Printf("USB Version: %s\n", desc.Spec.String())
		fmt.Printf("Device Version: %s\n", desc.Device.String())
		fmt.Printf("Configs: %d\n", len(desc.Configs))

		// Try multiple recovery approaches
		approaches := []func(*gousb.Device, []byte) error{
			tryDirectBulkWrite,
			tryControlOutFirmware,
			tryResetAndWrite,
			tryAllEndpoints,
		}

		for i, approach := range approaches {
			fmt.Printf("\n--- Approach %d ---\n", i+1)
			err := approach(dev, firmwareData)
			if err != nil {
				fmt.Printf("Approach %d failed: %v\n", i+1, err)
			} else {
				fmt.Printf("Approach %d completed without error\n", i+1)
			}
			time.Sleep(500 * time.Millisecond)
		}

		fmt.Println("\n=== Recovery Attempts Complete ===")
		fmt.Println("Remove USB, power cycle device, and check if screen changed.")

		return nil
	},
}

func init() {
	rootCmd.AddCommand(bruteRecoveryCmd)
}

// tryDirectBulkWrite attempts to write firmware directly to bulk endpoint
func tryDirectBulkWrite(dev *gousb.Device, firmware []byte) error {
	fmt.Println("Trying direct bulk write to endpoint 0x02...")

	cfg, err := dev.Config(1)
	if err != nil {
		return fmt.Errorf("failed to get config: %w", err)
	}
	defer cfg.Close()

	intf, err := cfg.Interface(0, 0)
	if err != nil {
		return fmt.Errorf("failed to claim interface: %w", err)
	}
	defer intf.Close()

	// Try to get OUT endpoint
	outEp, err := intf.OutEndpoint(0x02)
	if err != nil {
		fmt.Printf("  Could not get endpoint 0x02: %v\n", err)
		return err
	}

	// Write firmware in chunks
	chunkSize := 64 * 1024 // 64KB
	written := 0

	for written < len(firmware) {
		end := written + chunkSize
		if end > len(firmware) {
			end = len(firmware)
		}

		n, err := outEp.Write(firmware[written:end])
		if err != nil {
			fmt.Printf("  Write failed at %d bytes: %v\n", written, err)
			return err
		}
		written += n
		fmt.Printf("  Written: %d/%d bytes\n", written, len(firmware))
	}

	return nil
}

// tryControlOutFirmware tries sending firmware via control transfers
func tryControlOutFirmware(dev *gousb.Device, firmware []byte) error {
	fmt.Println("Trying control transfer firmware upload...")

	// Try various control transfer approaches
	controlCmds := []struct {
		name string
		req  uint8
		val  uint16
		idx  uint16
	}{
		{"Start comm (0x47)", 0x47, 0x00, 0x00},
		{"Firmware mode (0x4D)", 0x4D, 0x2185, 0x00},
		{"Firmware write (0x3F)", 0x3F, 0x0001, 0x4000},
		{"Unknown (0x4F)", 0x4F, 0xFFFF, 0x00},
		{"Write cmd (0x46)", 0x46, 0x0001, 0x4000},
	}

	for _, cmd := range controlCmds {
		fmt.Printf("  Sending %s...", cmd.name)

		// Try control OUT (host to device, vendor, device)
		_, err := dev.Control(
			0x40, // bmRequestType: host-to-device, vendor, device
			cmd.req,
			cmd.val,
			cmd.idx,
			nil,
		)
		if err != nil {
			fmt.Printf(" failed: %v\n", err)
		} else {
			fmt.Printf(" OK\n")
		}
		time.Sleep(100 * time.Millisecond)
	}

	return nil
}

// tryResetAndWrite tries USB reset then write
func tryResetAndWrite(dev *gousb.Device, firmware []byte) error {
	fmt.Println("Trying USB reset sequence...")

	// Send some zero-length packets or reset sequences
	// This might trigger a different device state

	// Try control transfer with different request types
	requestTypes := []uint8{
		0x00, // Standard, host-to-device, device
		0x01, // Standard, host-to-device, interface
		0x02, // Standard, host-to-device, endpoint
		0x40, // Vendor, host-to-device, device
		0x41, // Vendor, host-to-device, interface
		0x42, // Vendor, host-to-device, endpoint
		0x80, // Standard, device-to-host, device
		0xC0, // Vendor, device-to-host, device
	}

	for _, reqType := range requestTypes {
		buf := make([]byte, 64)
		n, err := dev.Control(reqType, 0x00, 0x00, 0x00, buf)
		if err == nil {
			fmt.Printf("  ReqType 0x%02X responded with %d bytes: %X\n", reqType, n, buf[:n])
		}
	}

	return nil
}

// tryAllEndpoints tries to find and write to any available endpoint
func tryAllEndpoints(dev *gousb.Device, firmware []byte) error {
	fmt.Println("Scanning for available endpoints...")

	cfg, err := dev.Config(1)
	if err != nil {
		return fmt.Errorf("failed to get config: %w", err)
	}
	defer cfg.Close()

	// Iterate through interfaces
	for _, ifaceDesc := range cfg.Desc.Interfaces {
		for _, alt := range ifaceDesc.AltSettings {
			fmt.Printf("  Interface %d, Alt %d:\n", alt.Number, alt.Alternate)

			intf, err := cfg.Interface(alt.Number, alt.Alternate)
			if err != nil {
				fmt.Printf("    Could not claim: %v\n", err)
				continue
			}

			for _, ep := range alt.Endpoints {
				fmt.Printf("    Endpoint 0x%02X: %s, MaxPacket=%d\n",
					ep.Address, ep.Direction.String(), ep.MaxPacketSize)

				// Try writing to OUT endpoints
				if ep.Direction == gousb.EndpointDirectionOut {
					outEp, err := intf.OutEndpoint(int(ep.Address))
					if err != nil {
						fmt.Printf("      Could not open: %v\n", err)
						continue
					}

					// Try writing a small chunk
					testData := firmware[:min(512, len(firmware))]
					n, err := outEp.Write(testData)
					if err != nil {
						fmt.Printf("      Write failed: %v\n", err)
					} else {
						fmt.Printf("      Wrote %d bytes!\n", n)
					}
				}
			}

			intf.Close()
		}
	}

	return nil
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}
