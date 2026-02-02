package cmd

import (
	"fmt"
	"os"

	"github.com/murdinc/rio500/pkg/rio500"
	"github.com/spf13/cobra"
)

var (
	externalFlag bool
)

var rootCmd = &cobra.Command{
	Use:   "rio500",
	Short: "CLI tool for Diamond Rio 500 MP3 player",
	Long: `rio500 is a command-line interface for interacting with the
Diamond Rio 500 MP3 player via USB.

The Rio 500 was released in 1999 as a successor to the PMP300.
This tool allows you to manage files, view device information, and more.`,
}

func Execute() {
	if err := rootCmd.Execute(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func init() {
	rootCmd.PersistentFlags().BoolVar(&externalFlag, "external", false, "Use external storage (SmartMedia card)")
}

// getDevice opens the Rio 500 and returns an initialized device
func getDevice() (*rio500.Device, error) {
	fmt.Println("Connecting to Rio 500...")

	usb, err := rio500.OpenUSB()
	if err != nil {
		return nil, fmt.Errorf("failed to open USB device: %w", err)
	}

	dev := rio500.New(usb)

	if externalFlag {
		fmt.Println("Using external storage (SmartMedia)...")
		dev.SetStorage(rio500.StorageExternal)
	} else {
		fmt.Println("Using internal storage...")
		dev.SetStorage(rio500.StorageInternal)
	}

	return dev, nil
}
