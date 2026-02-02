package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
)

var deleteCmd = &cobra.Command{
	Use:   "delete <folder> <song>",
	Short: "Delete a song from the device",
	Long:  `Delete a song from a specific folder on the Rio 500.`,
	Args:  cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		dev, err := getDevice()
		if err != nil {
			return err
		}
		defer dev.Close()

		var folderNum, songNum int
		if _, err := fmt.Sscanf(args[0], "%d", &folderNum); err != nil {
			return fmt.Errorf("invalid folder number: %s", args[0])
		}
		if _, err := fmt.Sscanf(args[1], "%d", &songNum); err != nil {
			return fmt.Errorf("invalid song number: %s", args[1])
		}

		fmt.Printf("Deleting song %d from folder %d...\n", songNum, folderNum)

		if err := dev.DeleteSong(folderNum, songNum); err != nil {
			return fmt.Errorf("delete failed: %w", err)
		}

		fmt.Println("Done!")
		return nil
	},
}

func init() {
	rootCmd.AddCommand(deleteCmd)
}
