package cmd

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"

	"github.com/spf13/cobra"
)

var downloadCmd = &cobra.Command{
	Use:   "download <song_num> [folder_num] [output_file]",
	Short: "Download a song from the Rio 500",
	Long: `Download a song from the Rio 500 to your computer.

Examples:
  rio500 download 0           # Download song 0 from folder 0
  rio500 download 2 1         # Download song 2 from folder 1
  rio500 download 0 0 my.mp3  # Download song 0 from folder 0 to my.mp3`,
	Args: cobra.RangeArgs(1, 3),
	RunE: func(cmd *cobra.Command, args []string) error {
		songNum, err := strconv.Atoi(args[0])
		if err != nil {
			return fmt.Errorf("invalid song number: %s", args[0])
		}

		folderNum := 0
		if len(args) >= 2 {
			folderNum, err = strconv.Atoi(args[1])
			if err != nil {
				return fmt.Errorf("invalid folder number: %s", args[1])
			}
		}

		outputFile := ""
		if len(args) >= 3 {
			outputFile = args[2]
		}

		dev, err := getDevice()
		if err != nil {
			return err
		}
		defer dev.Close()

		// Get song info first
		songs, err := dev.ListSongs(folderNum)
		if err != nil {
			return fmt.Errorf("failed to list songs: %w", err)
		}

		if songNum >= len(songs) {
			return fmt.Errorf("song %d does not exist in folder %d (only %d songs)", songNum, folderNum, len(songs))
		}

		song := songs[songNum]

		// Determine output filename
		if outputFile == "" {
			outputFile = song.Name
			// Clean up filename
			outputFile = filepath.Base(outputFile)
			if outputFile == "" || outputFile == "." {
				outputFile = fmt.Sprintf("song_%d_%d.mp3", folderNum, songNum)
			}
		}

		fmt.Printf("Downloading: %s (%d bytes) -> %s\n", song.Name, song.Size, outputFile)

		// Download the song
		data, err := dev.DownloadSong(folderNum, songNum, func(percent int) {
			fmt.Printf("\rProgress: %d%%", percent)
		})
		if err != nil {
			return fmt.Errorf("failed to download song: %w", err)
		}
		fmt.Println()

		// Write to file
		err = os.WriteFile(outputFile, data, 0644)
		if err != nil {
			return fmt.Errorf("failed to write file: %w", err)
		}

		fmt.Printf("Successfully downloaded %d bytes to %s\n", len(data), outputFile)
		return nil
	},
}

func init() {
	rootCmd.AddCommand(downloadCmd)
}
