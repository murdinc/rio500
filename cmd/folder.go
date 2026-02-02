package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
)

var folderCmd = &cobra.Command{
	Use:   "folder",
	Short: "Folder operations",
	Long:  `Commands for managing folders on the Rio 500.`,
}

var folderAddCmd = &cobra.Command{
	Use:   "add <name>",
	Short: "Add a new folder",
	Long:  `Create a new folder on the Rio 500.`,
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		dev, err := getDevice()
		if err != nil {
			return err
		}
		defer dev.Close()

		name := args[0]
		fmt.Printf("Creating folder '%s'...\n", name)

		if err := dev.AddFolder(name); err != nil {
			return fmt.Errorf("failed to create folder: %w", err)
		}

		fmt.Println("Done!")
		return nil
	},
}

var folderDeleteCmd = &cobra.Command{
	Use:   "delete <folder>",
	Short: "Delete a folder",
	Long:  `Delete a folder and all its songs from the Rio 500.`,
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		dev, err := getDevice()
		if err != nil {
			return err
		}
		defer dev.Close()

		var folderNum int
		if _, err := fmt.Sscanf(args[0], "%d", &folderNum); err != nil {
			return fmt.Errorf("invalid folder number: %s", args[0])
		}

		fmt.Printf("Deleting folder %d and all its songs...\n", folderNum)

		if err := dev.DeleteFolder(folderNum); err != nil {
			return fmt.Errorf("failed to delete folder: %w", err)
		}

		fmt.Println("Done!")
		return nil
	},
}

func init() {
	rootCmd.AddCommand(folderCmd)
	folderCmd.AddCommand(folderAddCmd)
	folderCmd.AddCommand(folderDeleteCmd)
}
