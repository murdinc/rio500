package cmd

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/spf13/cobra"
)

var (
	rawWrite bool
)

var recoveryCardCmd = &cobra.Command{
	Use:   "recovery-card <device> <firmware.bin>",
	Short: "Prepare a SmartMedia card for firmware recovery",
	Long: `Prepares a SmartMedia card with firmware for potential device recovery.

This command will:
1. Format the SmartMedia card as FAT16 (standard for 1999-era devices)
2. Copy the firmware file with multiple naming conventions
3. Optionally write the firmware raw to the beginning of the card

Examples:
  rio500 recovery-card /dev/disk4 ./firmware/firmware_2.16.bin
  rio500 recovery-card /dev/disk4 ./firmware/firmware_2.16.bin --raw

WARNING: This will ERASE ALL DATA on the specified device!
Make sure you specify the correct device - double check with 'diskutil list' first.`,
	Args: cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		device := args[0]
		firmwarePath := args[1]

		// Validate firmware file exists
		firmwareData, err := os.ReadFile(firmwarePath)
		if err != nil {
			return fmt.Errorf("failed to read firmware file: %w", err)
		}
		fmt.Printf("Firmware file: %s (%d bytes)\n", firmwarePath, len(firmwareData))

		// Safety check - confirm device
		fmt.Printf("\nWARNING: This will ERASE ALL DATA on %s!\n", device)
		fmt.Println("Make sure this is your SmartMedia card reader, not your main drive!")
		fmt.Printf("\nTo verify, run: diskutil list\n")
		fmt.Print("\nType 'yes' to continue: ")

		reader := bufio.NewReader(os.Stdin)
		response, _ := reader.ReadString('\n')
		if strings.TrimSpace(strings.ToLower(response)) != "yes" {
			fmt.Println("Cancelled.")
			return nil
		}

		if runtime.GOOS == "darwin" {
			return prepareMacOS(device, firmwarePath, firmwareData, rawWrite)
		} else if runtime.GOOS == "linux" {
			return prepareLinux(device, firmwarePath, firmwareData, rawWrite)
		} else {
			return fmt.Errorf("unsupported operating system: %s", runtime.GOOS)
		}
	},
}

func init() {
	rootCmd.AddCommand(recoveryCardCmd)
	recoveryCardCmd.Flags().BoolVar(&rawWrite, "raw", false, "Also write firmware raw to the beginning of the card (after FAT16 format)")
}

func prepareMacOS(device, firmwarePath string, firmwareData []byte, raw bool) error {
	// On macOS, we need to unmount but not eject
	fmt.Println("\n[1/5] Unmounting device...")

	// Get the raw device (disk4 -> rdisk4 for raw access)
	rawDevice := device
	if strings.Contains(device, "/dev/disk") {
		rawDevice = strings.Replace(device, "/dev/disk", "/dev/rdisk", 1)
	}

	// Unmount all volumes on the disk
	unmountCmd := exec.Command("diskutil", "unmountDisk", device)
	unmountCmd.Stdout = os.Stdout
	unmountCmd.Stderr = os.Stderr
	if err := unmountCmd.Run(); err != nil {
		fmt.Printf("Warning: unmount returned error (may be okay): %v\n", err)
	}

	// Try diskutil first, but fall back to newfs_msdos if it fails
	fmt.Println("\n[2/5] Formatting as FAT16 (MS-DOS)...")
	formatCmd := exec.Command("diskutil", "eraseDisk", "MS-DOS", "RIO500", "MBRFormat", device)
	formatCmd.Stdout = os.Stdout
	formatCmd.Stderr = os.Stderr
	if err := formatCmd.Run(); err != nil {
		fmt.Printf("diskutil failed, trying alternative method...\n")

		// Unmount again
		exec.Command("diskutil", "unmountDisk", device).Run()

		// Zero out the first 1MB to clear any existing partition table
		fmt.Println("Clearing existing data...")
		ddCmd := exec.Command("dd", "if=/dev/zero", "of="+rawDevice, "bs=1m", "count=1")
		ddCmd.Stdout = os.Stdout
		ddCmd.Stderr = os.Stderr
		ddCmd.Run()

		// Create MBR partition table with fdisk
		fmt.Println("Creating MBR partition table...")
		// Use fdisk to create a FAT16 partition
		fdiskInput := "y\nedit 1\n06\n\n\n\n\nwrite\nquit\n"
		fdiskCmd := exec.Command("fdisk", "-e", rawDevice)
		fdiskCmd.Stdin = strings.NewReader(fdiskInput)
		fdiskCmd.Stdout = os.Stdout
		fdiskCmd.Stderr = os.Stderr
		fdiskCmd.Run()

		// Try newfs_msdos directly on the raw device
		fmt.Println("Formatting with newfs_msdos...")
		newfsCmd := exec.Command("newfs_msdos", "-F", "16", "-v", "RIO500", rawDevice)
		newfsCmd.Stdout = os.Stdout
		newfsCmd.Stderr = os.Stderr
		if err := newfsCmd.Run(); err != nil {
			// Last resort: just do raw write without filesystem
			fmt.Printf("Warning: Could not create FAT16 filesystem: %v\n", err)
			fmt.Println("Proceeding with raw write only...")
			return writeRawOnly(rawDevice, firmwareData)
		}
	}

	// Find the mount point
	fmt.Println("\n[3/5] Copying firmware files...")

	// Wait a moment for mount
	mountPoint := "/Volumes/RIO500"

	// Check if mounted
	if _, err := os.Stat(mountPoint); os.IsNotExist(err) {
		// Try to mount
		mountCmd := exec.Command("diskutil", "mount", device+"s1")
		mountCmd.Run()
	}

	// Copy firmware with multiple names
	firmwareNames := []string{
		"RIO500.BIN",
		"FIRMWARE.BIN",
		"UPDATE.BIN",
		"FLASH.BIN",
		"SYSTEM.BIN",
		"RIO.BIN",
		filepath.Base(firmwarePath),
	}

	for _, name := range firmwareNames {
		destPath := filepath.Join(mountPoint, name)
		if err := os.WriteFile(destPath, firmwareData, 0644); err != nil {
			fmt.Printf("  Warning: could not write %s: %v\n", name, err)
		} else {
			fmt.Printf("  Copied: %s\n", name)
		}
	}

	// Also copy to root with uppercase
	upperName := strings.ToUpper(filepath.Base(firmwarePath))
	if upperName != filepath.Base(firmwarePath) {
		destPath := filepath.Join(mountPoint, upperName)
		os.WriteFile(destPath, firmwareData, 0644)
		fmt.Printf("  Copied: %s\n", upperName)
	}

	// Sync filesystem
	exec.Command("sync").Run()

	// Raw write option
	if raw {
		fmt.Println("\n[4/5] Writing raw firmware to beginning of card...")

		// Unmount first
		exec.Command("diskutil", "unmountDisk", device).Run()

		// Open raw device and write
		f, err := os.OpenFile(rawDevice, os.O_WRONLY, 0)
		if err != nil {
			return fmt.Errorf("failed to open raw device: %w (try with sudo)", err)
		}

		// Skip the first 512 bytes (MBR) and write at sector 1
		// Or try writing at various offsets that might be checked by bootloader

		// Write at offset 0 (overwrites MBR - risky but might be what bootloader needs)
		// Actually, let's write at a few strategic locations

		offsets := []int64{
			0,           // Very beginning
			512,         // After MBR
			0x4000,      // 16KB offset (block size mentioned in protocol)
			0x10000,     // 64KB offset
		}

		for _, offset := range offsets {
			_, err = f.Seek(offset, io.SeekStart)
			if err != nil {
				fmt.Printf("  Warning: seek to 0x%X failed: %v\n", offset, err)
				continue
			}
			n, err := f.Write(firmwareData)
			if err != nil {
				fmt.Printf("  Warning: write at 0x%X failed: %v\n", offset, err)
			} else {
				fmt.Printf("  Wrote %d bytes at offset 0x%X\n", n, offset)
			}
		}

		f.Sync()
		f.Close()
	} else {
		fmt.Println("\n[4/5] Skipping raw write (use --raw to enable)")
	}

	// Eject
	fmt.Println("\n[5/5] Ejecting card...")
	exec.Command("diskutil", "eject", device).Run()

	fmt.Println("\n========================================")
	fmt.Println("Recovery card prepared!")
	fmt.Println("========================================")
	fmt.Println("\nTo attempt recovery:")
	fmt.Println("1. Insert the SmartMedia card into your Rio 500")
	fmt.Println("2. Remove batteries from the Rio 500")
	fmt.Println("3. Try these button combinations while inserting batteries:")
	fmt.Println("   - Hold PLAY")
	fmt.Println("   - Hold STOP")
	fmt.Println("   - Hold MENU")
	fmt.Println("   - Hold PLAY + STOP")
	fmt.Println("   - No buttons (just insert batteries)")
	fmt.Println("4. Watch for any change on the screen")
	fmt.Println("\nIf one method shows different behavior, that may be the recovery mode!")

	return nil
}

func prepareLinux(device, firmwarePath string, firmwareData []byte, raw bool) error {
	// On Linux, use mkfs.vfat and mount
	fmt.Println("\n[1/4] Unmounting device...")

	// Unmount any mounted partitions
	exec.Command("umount", device+"1").Run()
	exec.Command("umount", device).Run()

	// Create partition table and FAT16 filesystem
	fmt.Println("\n[2/4] Creating partition table and FAT16 filesystem...")

	// Use fdisk to create MBR partition table
	fdiskCmd := exec.Command("sh", "-c", fmt.Sprintf("echo -e 'o\\nn\\np\\n1\\n\\n\\nt\\n6\\nw' | fdisk %s", device))
	fdiskCmd.Run()

	// Format as FAT16
	mkfsCmd := exec.Command("mkfs.vfat", "-F", "16", "-n", "RIO500", device+"1")
	mkfsCmd.Stdout = os.Stdout
	mkfsCmd.Stderr = os.Stderr
	if err := mkfsCmd.Run(); err != nil {
		return fmt.Errorf("mkfs.vfat failed: %w", err)
	}

	// Mount
	fmt.Println("\n[3/4] Mounting and copying firmware files...")
	mountPoint := "/mnt/rio500_recovery"
	os.MkdirAll(mountPoint, 0755)

	mountCmd := exec.Command("mount", device+"1", mountPoint)
	if err := mountCmd.Run(); err != nil {
		return fmt.Errorf("mount failed: %w", err)
	}
	defer exec.Command("umount", mountPoint).Run()

	// Copy firmware with multiple names
	firmwareNames := []string{
		"RIO500.BIN",
		"FIRMWARE.BIN",
		"UPDATE.BIN",
		"FLASH.BIN",
		"SYSTEM.BIN",
		"RIO.BIN",
		filepath.Base(firmwarePath),
	}

	for _, name := range firmwareNames {
		destPath := filepath.Join(mountPoint, name)
		if err := os.WriteFile(destPath, firmwareData, 0644); err != nil {
			fmt.Printf("  Warning: could not write %s: %v\n", name, err)
		} else {
			fmt.Printf("  Copied: %s\n", name)
		}
	}

	exec.Command("sync").Run()

	// Raw write option
	if raw {
		fmt.Println("\n[4/4] Writing raw firmware to card...")

		// Unmount first
		exec.Command("umount", mountPoint).Run()

		f, err := os.OpenFile(device, os.O_WRONLY, 0)
		if err != nil {
			return fmt.Errorf("failed to open device: %w", err)
		}

		offsets := []int64{0, 512, 0x4000, 0x10000}
		for _, offset := range offsets {
			f.Seek(offset, io.SeekStart)
			n, err := f.Write(firmwareData)
			if err != nil {
				fmt.Printf("  Warning: write at 0x%X failed: %v\n", offset, err)
			} else {
				fmt.Printf("  Wrote %d bytes at offset 0x%X\n", n, offset)
			}
		}

		f.Sync()
		f.Close()
	} else {
		fmt.Println("\n[4/4] Skipping raw write (use --raw to enable)")
	}

	fmt.Println("\n========================================")
	fmt.Println("Recovery card prepared!")
	fmt.Println("========================================")
	fmt.Println("\nTo attempt recovery:")
	fmt.Println("1. Insert the SmartMedia card into your Rio 500")
	fmt.Println("2. Remove batteries from the Rio 500")
	fmt.Println("3. Try these button combinations while inserting batteries:")
	fmt.Println("   - Hold PLAY")
	fmt.Println("   - Hold STOP")
	fmt.Println("   - Hold MENU")
	fmt.Println("   - Hold PLAY + STOP")
	fmt.Println("   - No buttons (just insert batteries)")
	fmt.Println("4. Watch for any change on the screen")

	return nil
}

// writeRawOnly writes the firmware directly to the device without a filesystem
// This is a fallback when FAT16 formatting fails
func writeRawOnly(rawDevice string, firmwareData []byte) error {
	fmt.Println("\nWriting raw firmware data (no filesystem)...")

	f, err := os.OpenFile(rawDevice, os.O_RDWR, 0)
	if err != nil {
		return fmt.Errorf("failed to open raw device: %w (try with sudo)", err)
	}
	defer f.Close()

	// Write firmware at multiple offsets
	offsets := []int64{
		0,           // Very beginning
		512,         // After potential MBR
		0x4000,      // 16KB offset (Rio block size)
		0x10000,     // 64KB offset
		0x17 * 0x4000, // Offset 0x17 blocks (where Rio starts storing data per protocol doc)
	}

	for _, offset := range offsets {
		_, err = f.Seek(offset, io.SeekStart)
		if err != nil {
			fmt.Printf("  Warning: seek to 0x%X failed: %v\n", offset, err)
			continue
		}
		n, err := f.Write(firmwareData)
		if err != nil {
			fmt.Printf("  Warning: write at 0x%X failed: %v\n", offset, err)
		} else {
			fmt.Printf("  Wrote %d bytes at offset 0x%X\n", n, offset)
		}
	}

	f.Sync()

	fmt.Println("\n========================================")
	fmt.Println("Raw firmware written to card!")
	fmt.Println("========================================")
	fmt.Println("\nNote: No filesystem was created (formatting failed).")
	fmt.Println("The firmware was written directly to the card at multiple offsets.")
	fmt.Println("\nTo attempt recovery:")
	fmt.Println("1. Insert the SmartMedia card into your Rio 500")
	fmt.Println("2. Remove batteries from the Rio 500")
	fmt.Println("3. Try button combinations while inserting batteries")
	fmt.Println("4. Watch for any change on the screen")

	return nil
}
