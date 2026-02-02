package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
)

var firmwareVersionCmd = &cobra.Command{
	Use:   "firmware-version",
	Short: "Display the Rio 500 device firmware version",
	Long:  `Displays the firmware version of the connected Diamond Rio 500 MP3 player.`,
	RunE: func(cmd *cobra.Command, args []string) error {
		dev, err := getDevice()
		if err != nil {
			return err
		}
		defer dev.Close()

		firmwareVersion, err := dev.GetFirmwareVersion()
		if err != nil {
			return fmt.Errorf("failed to get firmware version: %w", err)
		}

		major := firmwareVersion >> 8
		minor := firmwareVersion & 0xFF

		fmt.Printf("Rio 500 Firmware Version: %d.%02d\n", major, minor)
		return nil
	},
}

func init() {
	rootCmd.AddCommand(firmwareVersionCmd)
}
