package cmd

import (
	"fmt"
	"os"
	"time"

	"github.com/google/gousb"
	"github.com/spf13/cobra"
)

var quickFlashCmd = &cobra.Command{
	Use:   "quick-flash <firmware.bin>",
	Short: "Fast firmware flash attempt",
	Long: `Immediately attempts to flash firmware without scanning.

Sends commands as fast as possible after USB connection,
since the device may only be responsive briefly.`,
	Args: cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		firmwareData, err := os.ReadFile(args[0])
		if err != nil {
			return fmt.Errorf("failed to read firmware: %w", err)
		}
		fmt.Printf("Firmware: %d bytes\n\n", len(firmwareData))

		// Try multiple times with fresh USB connections
		for attempt := 1; attempt <= 5; attempt++ {
			fmt.Printf("=== Attempt %d ===\n", attempt)

			ctx := gousb.NewContext()
			dev, err := ctx.OpenDeviceWithVIDPID(0x0841, 0x0001)
			if err != nil || dev == nil {
				fmt.Printf("Device not found, waiting...\n")
				ctx.Close()
				time.Sleep(2 * time.Second)
				continue
			}
			dev.SetAutoDetach(true)

			// Immediately send commands - no delays
			fmt.Print("0x47... ")
			_, e1 := dev.Control(0x40, 0x47, 0x00, 0x00, nil)
			if e1 != nil {
				fmt.Printf("fail ")
			} else {
				fmt.Printf("OK ")
			}

			fmt.Print("0x4D... ")
			_, e2 := dev.Control(0x40, 0x4D, 0x2185, 0x00, nil)
			if e2 != nil {
				fmt.Printf("fail ")
			} else {
				fmt.Printf("OK ")
			}

			fmt.Print("0x4F... ")
			_, e3 := dev.Control(0x40, 0x4F, 0xFFFF, 0x00, nil)
			if e3 != nil {
				fmt.Printf("fail ")
			} else {
				fmt.Printf("OK ")
			}

			// Try to send firmware immediately
			fmt.Print("\nSending firmware chunks: ")

			sent := 0
			consecutiveErrors := 0
			chunkSize := 64

			for sent < len(firmwareData) && consecutiveErrors < 3 {
				end := sent + chunkSize
				if end > len(firmwareData) {
					end = len(firmwareData)
				}
				chunk := firmwareData[sent:end]

				// Try control transfer with data
				_, err := dev.Control(0x40, 0x46, uint16(sent>>16), uint16(sent&0xFFFF), chunk)
				if err != nil {
					consecutiveErrors++
					fmt.Print("x")
				} else {
					sent += len(chunk)
					consecutiveErrors = 0
					if sent%(chunkSize*16) == 0 {
						fmt.Printf("[%d]", sent)
					} else {
						fmt.Print(".")
					}
				}
			}

			fmt.Printf("\nSent: %d/%d bytes\n", sent, len(firmwareData))

			// Try commit
			fmt.Print("0x4D commit... ")
			dev.Control(0x40, 0x4D, 0x2185, 0x00, nil)
			fmt.Println("sent")

			// Try bulk write as well
			fmt.Print("Trying bulk endpoint... ")
			cfg, err := dev.Config(1)
			if err == nil {
				intf, err := cfg.Interface(0, 0)
				if err == nil {
					outEp, err := intf.OutEndpoint(0x02)
					if err == nil {
						// Try writing remaining firmware via bulk
						n, err := outEp.Write(firmwareData[sent:])
						if err != nil {
							fmt.Printf("failed: %v\n", err)
						} else {
							fmt.Printf("wrote %d bytes!\n", n)
						}
					} else {
						fmt.Printf("no endpoint: %v\n", err)
					}
					intf.Close()
				}
				cfg.Close()
			}

			dev.Close()
			ctx.Close()

			if sent > 0 {
				fmt.Println("\nSome data was sent! Power cycle and check device.")
			}

			fmt.Println()
			time.Sleep(1 * time.Second)
		}

		return nil
	},
}

func init() {
	rootCmd.AddCommand(quickFlashCmd)
}
