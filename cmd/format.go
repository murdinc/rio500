package cmd

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/spf13/cobra"
)

var (
	forceFormat bool
)

var formatCmd = &cobra.Command{
	Use:   "format",
	Short: "Format the device storage",
	Long: `Format the Rio 500 storage, erasing all data.
This will delete all folders and songs on the device.`,
	RunE: func(cmd *cobra.Command, args []string) error {
		dev, err := getDevice()
		if err != nil {
			return err
		}
		defer dev.Close()

		storage := "internal"
		if externalFlag {
			storage = "external (SmartMedia)"
		}

		if !forceFormat {
			fmt.Printf("\nWARNING: This will erase ALL data on %s storage!\n", storage)
			fmt.Print("Are you sure? (yes/no): ")

			reader := bufio.NewReader(os.Stdin)
			response, _ := reader.ReadString('\n')
			response = strings.TrimSpace(strings.ToLower(response))

			if response != "yes" {
				fmt.Println("Format cancelled.")
				return nil
			}
		}

		fmt.Printf("Formatting %s storage...\n", storage)

		if err := dev.Format(); err != nil {
			return fmt.Errorf("format failed: %w", err)
		}

		fmt.Println("Format complete!")
		return nil
	},
}

func init() {
	rootCmd.AddCommand(formatCmd)
	formatCmd.Flags().BoolVarP(&forceFormat, "force", "f", false, "Skip confirmation prompt")
}
