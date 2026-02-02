package cmd

import (
	"fmt"
	"os"
	"path/filepath"

	"github.com/spf13/cobra"
)



var uploadCmd = &cobra.Command{
	Use:   "upload <folder> <file> [file...]",
	Short: "Upload MP3 files to the device",
	Long:  `Upload one or more MP3 files to a specific folder on the Rio 500.`, 
	Args:  cobra.MinimumNArgs(2),
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

		files := args[1:]

		for i, file := range files {
			fmt.Printf("\n[%d/%d] Uploading %s...\n", i+1, len(files), filepath.Base(file))

			data, err := os.ReadFile(file)
			if err != nil {
				fmt.Printf("  Error reading file: %v\n", err)
				continue
			}

			sizeMB := float64(len(data)) / 1024.0 / 1024.0
			fmt.Printf("  Size: %.2f MB\n", sizeMB)

			progress := func(percent int) {
				fmt.Printf("\r  Progress: %d%%", percent)
			}

			err = dev.UploadSong(folderNum, filepath.Base(file), data, progress)
			if err != nil {
				fmt.Printf("\n  Error: %v\n", err)
				continue
			}

			fmt.Printf("\n  Done!\n")
		}

		return nil
	},
}

func init() {
	rootCmd.AddCommand(uploadCmd)

}
