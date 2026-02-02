package cmd

import (
	"fmt"

	"github.com/murdinc/rio500/pkg/rio500"
	"github.com/spf13/cobra"
)

var listCmd = &cobra.Command{
	Use:   "list [folder]",
	Short: "List folders and songs on the device",
	Long: `List folders and songs stored on the Rio 500.
Without arguments, lists all folders. With a folder number, lists songs in that folder.`,
	Args: cobra.MaximumNArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		dev, err := getDevice()
		if err != nil {
			return err
		}
		defer dev.Close()

		if len(args) == 0 {
			return listFolders(dev)
		}

		var folderNum int
		if _, err := fmt.Sscanf(args[0], "%d", &folderNum); err != nil {
			return fmt.Errorf("invalid folder number: %s", args[0])
		}

		return listSongs(dev, folderNum)
	},
}

func init() {
	rootCmd.AddCommand(listCmd)
}

func listFolders(dev *rio500.Device) error {
	folders, err := dev.ListFolders()
	if err != nil {
		return fmt.Errorf("failed to list folders: %w", err)
	}

	if len(folders) == 0 {
		fmt.Println("No folders found on device.")
		fmt.Println("Use 'rio500 folder add <name>' to create a folder.")
		return nil
	}

	fmt.Printf("\nFolders on device:\n")
	fmt.Printf("%-4s  %-40s  %s\n", "#", "Name", "Songs")
	fmt.Printf("%-4s  %-40s  %s\n", "---", "----", "-----")

	for _, folder := range folders {
		fmt.Printf("%-4d  %-40s  ~%d\n", folder.FolderNum, folder.Name, folder.SongCount)
	}

	fmt.Printf("\nTotal: %d folder(s)\n", len(folders))
	return nil
}

func listSongs(dev *rio500.Device, folderNum int) error {
	songs, err := dev.ListSongs(folderNum)
	if err != nil {
		return fmt.Errorf("failed to list songs: %w", err)
	}

	if len(songs) == 0 {
		fmt.Printf("No songs in folder %d.\n", folderNum)
		return nil
	}

	fmt.Printf("\nSongs in folder %d:\n", folderNum)
	fmt.Printf("%-4s  %-50s  %s\n", "#", "Name", "Size")
	fmt.Printf("%-4s  %-50s  %s\n", "---", "----", "----")

	var totalSize uint32
	for _, song := range songs {
		sizeMB := float64(song.Size) / 1024.0 / 1024.0
		fmt.Printf("%-4d  %-50s  %.2f MB\n", song.SongNum, song.Name, sizeMB)
		totalSize += song.Size
	}

	fmt.Printf("\nTotal: %d song(s), %.2f MB\n", len(songs), float64(totalSize)/1024.0/1024.0)
	return nil
}
