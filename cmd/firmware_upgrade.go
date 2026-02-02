package cmd

import (
	"bufio"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/schollz/progressbar/v3"
	"github.com/spf13/cobra"
)

var forceFlash bool

var firmwareUpgradeCmd = &cobra.Command{
	Use:   "firmware-upgrade [path_to_firmware.bin]",
	Short: "Upgrade the firmware of the Rio 500 device",
	Long: `Upgrades the firmware of the connected Diamond Rio 500 MP3 player.

The process is:
1. Checks the current firmware version.
2. Uploads the new firmware file.
3. Prompts you to reboot the device.
4. Re-connects and verifies the new firmware version.

WARNING: This is a potentially dangerous operation. Use with caution.`,
	Args: cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		firmwarePath := args[0]

		// --- 1. Read firmware file ---
		fmt.Printf("Reading firmware file: %s\n", firmwarePath)
		firmwareData, err := ioutil.ReadFile(firmwarePath)
		if err != nil {
			return fmt.Errorf("failed to read firmware file: %w", err)
		}
		fmt.Printf("Firmware size: %d bytes\n", len(firmwareData))

		var initialVersion uint16
		if forceFlash {
			// Skip version check in force mode (for recovery)
			fmt.Println("\nFORCE MODE: Skipping version check (device may be unresponsive)")
		} else {
			// --- 2. Get initial version ---
			dev, err := getDevice()
			if err != nil {
				return err
			}
			initialVersion, err = dev.GetFirmwareVersion()
			if err != nil {
				dev.Close()
				return fmt.Errorf("failed to get initial firmware version: %w\n\nTip: Use --force flag to skip version check for device recovery", err)
			}
			dev.Close()
			initialMajor := initialVersion >> 8
			initialMinor := initialVersion & 0xFF
			fmt.Printf("Current device firmware version: %d.%02x\n", initialMajor, initialMinor)
		}

		// --- 3. Confirmation ---
		fmt.Println("\nWARNING: This will attempt to flash the firmware on your Rio 500.")
		fmt.Println("This is a potentially destructive action.")
		fmt.Print("Are you sure you want to continue? (y/N): ")

		reader := bufio.NewReader(os.Stdin)
		response, _ := reader.ReadString('\n')
		if strings.TrimSpace(strings.ToLower(response)) != "y" {
			fmt.Println("Firmware upgrade cancelled.")
			return nil
		}

		// --- 4. Upgrade firmware - First Pass ---
		fmt.Printf("Starting firmware upgrade (pass 1/2)...\n")
		err = flashFirmware(firmwareData, 1, forceFlash)
		if err != nil {
			return fmt.Errorf("firmware upgrade failed (pass 1/2): %w", err)
		}

		// --- 5. Upgrade firmware - Second Pass ---
		fmt.Printf("Starting firmware upgrade (pass 2/2)...\n")
		err = flashFirmware(firmwareData, 2, forceFlash)
		if err != nil {
			return fmt.Errorf("firmware upgrade failed (pass 2/2): %w", err)
		}

		fmt.Printf("Firmware successfully upgraded after two passes.\n")
		fmt.Printf("Please reboot your Rio500 device now.\n")
		fmt.Printf("After reboot, run 'rio500 firmware-version' to confirm the upgrade.\n")
		return nil
	},
}

func init() {
	rootCmd.AddCommand(firmwareUpgradeCmd)
	firmwareUpgradeCmd.Flags().BoolVar(&forceFlash, "force", false, "Force flash without version check (for device recovery)")
}

// flashFirmware connects to the device, displays a progress bar, and writes the firmware data.
func flashFirmware(firmwareData []byte, passNumber int, recoveryMode bool) error {
	dev, err := getDevice()
	if err != nil {
		return err
	}
	defer dev.Close()

	description := fmt.Sprintf("Uploading (Pass %d/2)", passNumber)
	if recoveryMode {
		description = fmt.Sprintf("Recovery (Pass %d/2)", passNumber)
	}

	bar := progressbar.NewOptions(len(firmwareData),
		progressbar.OptionSetDescription(description),
		progressbar.OptionSetWriter(os.Stderr),
		progressbar.OptionShowBytes(true),
		progressbar.OptionSetWidth(15),
		progressbar.OptionThrottle(65*time.Millisecond),
		progressbar.OptionShowCount(),
		progressbar.OptionSpinnerType(14),
		progressbar.OptionFullWidth(),
		progressbar.OptionSetRenderBlankState(true),
	)

	progressFunc := func(percent int) {
		bar.Set(percent * len(firmwareData) / 100)
	}

	err = dev.WriteFirmwareWithRecovery(firmwareData, progressFunc, recoveryMode)
	if err != nil {
		return fmt.Errorf("firmware write failed (pass %d/2): %w", passNumber, err)
	}

	fmt.Printf("\nFirmware upload complete (Pass %d/2).\n", passNumber)
	return nil
}

// verifyFirmwareVersion checks the final firmware version against expected and initial versions
func verifyFirmwareVersion(firmwarePath string, initialVersion, finalVersion uint16) error {
	finalMajor := finalVersion >> 8
	finalMinor := finalVersion & 0xFF

	// Try to parse version from filename, e.g., "firmware_2.16.bin" -> 2.16
	filename := filepath.Base(firmwarePath)
	parts := strings.Split(strings.TrimSuffix(filename, filepath.Ext(filename)), "_")
	var expectedMajor, expectedMinor uint16
	if len(parts) > 1 {
		versionParts := strings.Split(parts[len(parts)-1], ".")
		if len(versionParts) == 2 {
			maj, _ := strconv.Atoi(versionParts[0])
			min, _ := strconv.Atoi(versionParts[1])
			expectedMajor = uint16(maj)
			// Minor version is stored as hex (BCD-like), so convert decimal to hex representation
			expectedMinor = uint16(min)
		}
	}

	// Check if version matches expected from filename
	if expectedMajor > 0 && finalMajor == expectedMajor && finalMinor == expectedMinor {
		fmt.Printf("\nSUCCESS: Firmware version matches expected version %d.%02x\n", expectedMajor, expectedMinor)
		return nil
	}

	// Check if version changed from initial
	if finalVersion != initialVersion {
		initialMajor := initialVersion >> 8
		initialMinor := initialVersion & 0xFF
		fmt.Printf("\nSUCCESS: Firmware version changed from %d.%02x to %d.%02x\n",
			initialMajor, initialMinor, finalMajor, finalMinor)
		return nil
	}

	fmt.Println("\nWARNING: Firmware version has not changed.")
	fmt.Println("The flash may have failed, or you may have re-flashed the same version.")
	return nil
}
