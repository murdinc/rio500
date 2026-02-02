package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
)

var infoCmd = &cobra.Command{
	Use:   "info",
	Short: "Display device information",
	Long:  `Display information about the connected Rio 500 device.`,
	RunE: func(cmd *cobra.Command, args []string) error {
		dev, err := getDevice()
		if err != nil {
			return err
		}
		defer dev.Close()

		fmt.Println("\nRio 500 Device Information")
		fmt.Println("==========================")

		// Firmware version
		firmware, err := dev.GetFirmwareVersion()
		if err != nil {
			fmt.Printf("Firmware: (error: %v)\n", err)
		} else {
			fmt.Printf("Firmware Version: %d\n", firmware)
		}

		// Storage type
		if externalFlag {
			fmt.Println("Storage: External (SmartMedia)")
		} else {
			fmt.Println("Storage: Internal")
		}

		// Memory info
		total, err := dev.GetTotalMemory()
		if err != nil {
			fmt.Printf("Total Memory: (error: %v)\n", err)
		} else {
			fmt.Printf("Total Memory: %.2f MB\n", float64(total)/1024.0/1024.0)
		}

		free, err := dev.GetFreeMemory()
		if err != nil {
			fmt.Printf("Free Memory: (error: %v)\n", err)
		} else {
			fmt.Printf("Free Memory: %.2f MB\n", float64(free)/1024.0/1024.0)
			if total > 0 {
				used := total - free
				fmt.Printf("Used Memory: %.2f MB (%.1f%%)\n",
					float64(used)/1024.0/1024.0,
					float64(used)*100.0/float64(total))
			}
		}

		// Folder count
		folders, err := dev.ListFolders()
		if err != nil {
			fmt.Printf("Folders: (error: %v)\n", err)
		} else {
			fmt.Printf("Folders: %d\n", len(folders))
		}

		return nil
	},
}

func init() {
	rootCmd.AddCommand(infoCmd)
}
