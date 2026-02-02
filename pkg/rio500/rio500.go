package rio500

import (
	"encoding/binary"
	"fmt"
	"os"
	"time"
)

// USB identifiers for Rio 500
const (
	VendorID  = 0x0841 // Diamond Multimedia
	ProductID = 0x0001 // Rio 500

	// Block sizes
	FolderBlockSize = 0x4000 // 16KB folder block

	// USB Commands
	CmdReadFromUSB          = 0x45
	CmdWriteToUSB           = 0x46
	CmdStartUSBComm         = 0x47
	CmdEndUSBComm           = 0x48
	CmdFormatDevice         = 0x4D
	CmdQueryFreeMem         = 0x50
	CmdQueryOffsetLastWrite = 0x43
	CmdSendFolderLocation   = 0x56
	CmdEndFolderTransfers   = 0x58
	CmdSetCard              = 0x51
	CmdGetMemStatus         = 0x57
	CmdGetNumFolderBlocks   = 0x59
	CmdSetReadAddress       = 0x4E
	CmdSetWriteAddress      = 0x4C
	CmdWait                 = 0x42
	CmdFirmwareWrite        = 0x3F // Firmware write command (size in val/idx)

	// Storage types
	StorageInternal = 0
	StorageExternal = 1
)

// MP3 bitrate lookup tables (index 1-14, index 0 and 15 are invalid)
// MPEG-1 Layer 3 (most common)
var bitrateMPEG1L3 = []uint16{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}

// MPEG-1 Layer 2
var bitrateMPEG1L2 = []uint16{0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0}

// MPEG-1 Layer 1
var bitrateMPEG1L1 = []uint16{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0}

// MPEG-2/2.5 Layer 2/3
var bitrateMPEG2L23 = []uint16{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}

// MPEG-2/2.5 Layer 1
var bitrateMPEG2L1 = []uint16{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0}

// Device represents a Rio 500 device
type Device struct {
	usb         *USBDevice
	card        int  // 0 = internal, 1 = external (SmartMedia)
	initialized bool
}

// MemStatus represents memory status from the device
type MemStatus struct {
	Unknown1       uint16
	BlockSize      uint16
	NumBlocks      uint16
	FirstFreeBlock uint16
	NumUnusedBlocks uint16
	Unknown2       uint32
	Unknown3       uint32
}

// RioBitmapData represents the bitmap display data for a song/folder name
type RioBitmapData struct {
	NumBlocks uint16
	Bitmap    [1536]byte
}

// SongEntry represents a song entry in the device (2048 bytes)
type SongEntry struct {
	Offset    uint16        // Block offset
	Unknown1  uint16
	Length    uint32        // File size in bytes
	Unknown2  uint16
	Unknown3  uint16
	MP3Sig    uint32        // MP3 signature
	Time      uint32        // Unix timestamp
	Bitmap    RioBitmapData // Display bitmap
	Name1     [362]byte     // Primary name
	Name2     [128]byte     // Secondary name
}

// FolderEntry represents a folder entry in the device (2048 bytes)
type FolderEntry struct {
	Offset           uint16        // Block offset to song list
	Unknown1         uint16
	FirstFreeEntryOff uint16       // Offset to first free entry
	Unknown2         uint16
	Unknown3         uint32
	Unknown4         uint32
	Time             uint32        // Unix timestamp
	Bitmap           RioBitmapData // Display bitmap
	Name1            [362]byte     // Primary name
	Name2            [128]byte     // Secondary name
}

// FolderLocation is sent to tell the device where folders are
type FolderLocation struct {
	Offset    uint16
	Bytes     uint16
	FolderNum uint16
}

// FolderInfo provides folder information for display
type FolderInfo struct {
	Name      string
	FolderNum int
	SongCount int
}

// SongInfo provides song information for display
type SongInfo struct {
	Name      string
	Size      uint32
	SongNum   int
	FolderNum int
	Timestamp time.Time
}

// New creates a new Rio 500 device instance
func New(usb *USBDevice) *Device {
	return &Device{
		usb:  usb,
		card: StorageInternal,
	}
}

// Close closes the device connection
func (d *Device) Close() error {
	// Finalize communication with the device first
	if err := d.Finalize(); err != nil {
		// Log the error but continue to close USB, as Finalize might fail
		// if communication is already broken, but we still need to release USB
		fmt.Fprintf(os.Stderr, "Error finalizing device communication: %v\n", err)
	}

	if d.usb != nil {
		return d.usb.Close()
	}
	return nil
}

// SetStorage sets whether to use internal or external storage
func (d *Device) SetStorage(card int) {
	d.card = card
}

// GetStorage returns current storage type
func (d *Device) GetStorage() int {
	return d.card
}

// Initialize starts communication with the device
func (d *Device) Initialize() error {
	if d.initialized {
		return nil
	}

	_, err := d.sendCommand(CmdStartUSBComm, 0x00, 0x00)
	if err != nil {
		return fmt.Errorf("failed to start communication: %w", err)
	}

	d.initialized = true
	return nil
}

// Finalize ends communication with the device
func (d *Device) Finalize() error {
	if !d.initialized {
		return nil
	}

	d.sendCommand(CmdEndUSBComm, 0x00, 0x00)
	d.sendCommand(CmdWait, 0x00, 0x00)

	d.initialized = false
	return nil
}

// Format formats the device storage
func (d *Device) Format() error {
	if err := d.Initialize(); err != nil {
		return err
	}
	defer d.Finalize()

	_, err := d.sendCommand(CmdFormatDevice, 0x2185, d.card)
	if err != nil {
		return fmt.Errorf("format failed: %w", err)
	}

	// Wait for flash to update
	time.Sleep(1 * time.Second)

	return nil
}

// EnterFirmwareUpgradeMode sends the magic command to enable firmware writing
func (d *Device) EnterFirmwareUpgradeMode() error {
	// Must initialize communication first
	if err := d.Initialize(); err != nil {
		return fmt.Errorf("failed to initialize before firmware mode: %w", err)
	}

	result, err := d.sendCommand(CmdFormatDevice, 0x2185, d.card)
	if err != nil {
		return fmt.Errorf("failed to send firmware upgrade command: %w", err)
	}

	// Check return value - in Rio500Flash, 0 means failure
	if result == 0 {
		return fmt.Errorf("device rejected firmware upgrade command (returned 0)")
	}

	// Wait for flash memory to prepare
	time.Sleep(1 * time.Second)
	return nil
}

// WriteFirmware writes the provided firmware data to the device
// Based on Rio500Flash_216.exe analysis:
// 1. Send command 0x3F with size (val=low 16 bits, idx=high 16 bits)
// 2. Write data in 64KB (0x10000) chunks via bulk transfer
func (d *Device) WriteFirmware(data []byte, progress func(percent int)) error {
	return d.WriteFirmwareWithRecovery(data, progress, false)
}

// WriteFirmwareWithRecovery writes firmware with optional recovery mode for bricked devices
func (d *Device) WriteFirmwareWithRecovery(data []byte, progress func(percent int), recoveryMode bool) error {
	size := len(data)
	if size == 0 {
		return fmt.Errorf("firmware data is empty")
	}

	// Try to initialize communication (may fail on bricked device)
	initErr := d.Initialize()
	if initErr != nil {
		if !recoveryMode {
			return fmt.Errorf("failed to initialize for firmware write: %w", initErr)
		}
		fmt.Fprintf(os.Stderr, "Warning: Initialize failed (%v), continuing in recovery mode...\n", initErr)
	}

	// Wait for device to be ready (Rio500Flash does this after 0x47)
	// Send 0x42 and check if bit 31 is set
	for i := 0; i < 50; i++ {
		result, err := d.sendCommand(CmdWait, 0, 0)
		if err != nil {
			if !recoveryMode {
				return fmt.Errorf("wait command failed: %w", err)
			}
			// In recovery mode, continue anyway
			break
		}
		// Check bit 31
		if (result & 0x80000000) != 0 {
			break
		}
		time.Sleep(1000 * time.Millisecond)
	}

	// Send firmware write command 0x3F with size
	// Parameters are SWAPPED: val=sizeHigh (size >> 16), idx=sizeLow (size & 0xFFFF)
	// This was discovered through testing - device returns the full size on success
	sizeLow := size & 0xFFFF
	sizeHigh := (size >> 16) & 0xFFFF

	result, err := d.sendCommand(CmdFirmwareWrite, sizeHigh, sizeLow)
	if err != nil {
		if !recoveryMode {
			return fmt.Errorf("failed to send firmware write command: %w", err)
		}
		fmt.Fprintf(os.Stderr, "Warning: Firmware write command failed (%v), attempting bulk write anyway...\n", err)
	}

	// Device returns the full size on success
	if result == 0 && !recoveryMode {
		return fmt.Errorf("firmware write command rejected by device")
	}

	// Write data in 64KB (0x10000) chunks as per Rio500Flash
	const firmwareChunkSize = 0x10000 // 64KB chunks
	totalWritten := 0

	for totalWritten < size {
		chunkSize := firmwareChunkSize
		if totalWritten+chunkSize > size {
			chunkSize = size - totalWritten
		}

		chunk := data[totalWritten : totalWritten+chunkSize]
		err := d.usb.BulkWrite(chunk)
		if err != nil {
			return fmt.Errorf("firmware bulk write failed after %d bytes: %w", totalWritten, err)
		}

		totalWritten += chunkSize
		if progress != nil {
			progress(totalWritten * 100 / size)
		}
	}

	// Send wait commands to let device process
	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	// Send commit/format command (0x4D with 0x2185) to finalize firmware write
	// Based on Rio500Flash ASM, this is called AFTER data write
	d.sendCommand(CmdFormatDevice, 0x2185, d.card)

	// More wait commands
	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	return nil
}


// GetMemoryStatus returns memory status information
func (d *Device) GetMemoryStatus() (*MemStatus, error) {
	if err := d.Initialize(); err != nil {
		return nil, err
	}

	// Set card
	_, err := d.sendCommand(CmdSetCard, 1, d.card)
	if err != nil {
		return nil, err
	}

	// Get status
	data := make([]byte, 18) // sizeof(MemStatus)
	err = d.usb.ControlIn(CmdGetMemStatus, 0, 0, data)
	if err != nil {
		return nil, fmt.Errorf("failed to get memory status: %w", err)
	}

	status := &MemStatus{
		Unknown1:        binary.LittleEndian.Uint16(data[0:2]),
		BlockSize:       binary.LittleEndian.Uint16(data[2:4]),
		NumBlocks:       binary.LittleEndian.Uint16(data[4:6]),
		FirstFreeBlock:  binary.LittleEndian.Uint16(data[6:8]),
		NumUnusedBlocks: binary.LittleEndian.Uint16(data[8:10]),
		Unknown2:        binary.LittleEndian.Uint32(data[10:14]),
		Unknown3:        binary.LittleEndian.Uint32(data[14:18]),
	}

	return status, nil
}

// GetFreeMemory returns free memory in bytes
func (d *Device) GetFreeMemory() (uint32, error) {
	if err := d.Initialize(); err != nil {
		return 0, err
	}

	d.sendCommand(CmdWait, 0, 0)
	result, err := d.sendCommand(CmdQueryFreeMem, 0, d.card)
	if err != nil {
		return 0, err
	}
	d.sendCommand(CmdWait, 0, 0)

	return result, nil
}

// GetTotalMemory returns total memory in bytes
func (d *Device) GetTotalMemory() (uint32, error) {
	status, err := d.GetMemoryStatus()
	if err != nil {
		return 0, err
	}

	return uint32(status.NumBlocks) * uint32(status.BlockSize), nil
}

// GetFirmwareVersion returns the firmware revision
func (d *Device) GetFirmwareVersion() (uint16, error) {
	if err := d.Initialize(); err != nil {
		return 0, err
	}

	result, err := d.sendCommand(0x40, 0, 0)
	if err != nil {
		return 0, err
	}

	return uint16(result & 0xFFFF), nil
}

// ListFolders returns all folders on the device
func (d *Device) ListFolders() ([]FolderInfo, error) {
	if err := d.Initialize(); err != nil {
		return nil, err
	}

	entries, err := d.readFolderEntries()
	if err != nil {
		return nil, err
	}

	folders := make([]FolderInfo, len(entries))
	for i, entry := range entries {
		folders[i] = FolderInfo{
			Name:      nullTermString(entry.Name1[:]),
			FolderNum: i,
			SongCount: int(entry.FirstFreeEntryOff / 0x800), // Rough estimate
		}
	}

	return folders, nil
}

// ListSongs returns all songs in a folder
func (d *Device) ListSongs(folderNum int) ([]SongInfo, error) {
	if err := d.Initialize(); err != nil {
		return nil, err
	}

	folderEntries, err := d.readFolderEntries()
	if err != nil {
		return nil, err
	}

	if folderNum >= len(folderEntries) {
		return nil, fmt.Errorf("folder %d does not exist", folderNum)
	}

	songEntries, err := d.readSongEntries(folderEntries, folderNum)
	if err != nil {
		return nil, err
	}

	songs := make([]SongInfo, len(songEntries))
	for i, entry := range songEntries {
		songs[i] = SongInfo{
			Name:      nullTermString(entry.Name1[:]),
			Size:      entry.Length,
			SongNum:   i,
			FolderNum: folderNum,
			Timestamp: time.Unix(int64(entry.Time), 0),
		}
	}

	return songs, nil
}

// AddFolder creates a new folder on the device
func (d *Device) AddFolder(name string) error {
	if err := d.Initialize(); err != nil {
		return err
	}
	defer d.Finalize()

	// Get number of folder blocks to determine if folders exist
	d.sendCommand(CmdWait, 0, 0)
	blockCount, err := d.sendCommand(CmdGetNumFolderBlocks, 0xFF00, d.card)
	if err != nil {
		return err
	}

	var folders []FolderEntry

	if blockCount == 0 {
		// First folder - no existing folders to read
		folders = nil
	} else {
		// Read existing folder entries directly (don't call readFolderEntries which queries again)
		totalSize := int(blockCount) * FolderBlockSize
		block := make([]byte, totalSize)

		err = d.sendReadCommand(0xFF00, int(blockCount))
		if err != nil {
			return err
		}

		n, err := d.usb.BulkRead(block)
		if err != nil {
			return err
		}
		if n != totalSize {
			return fmt.Errorf("short read: got %d, expected %d", n, totalSize)
		}

		// Parse folder entries
		maxEntries := int(blockCount) * 8
		for i := 0; i < maxEntries; i++ {
			offset := i * 2048
			if offset+2 > len(block) {
				break
			}

			entryOffset := binary.LittleEndian.Uint16(block[offset : offset+2])
			if entryOffset == 0xFFFF {
				break
			}

			entry := parseFolderEntry(block[offset : offset+2048])
			folders = append(folders, entry)
		}

		if len(folders) == 0 {
			return fmt.Errorf("failed to read existing folder list, please try again")
		}
	}

	lastFolder := len(folders)

	if lastFolder >= 256 {
		return fmt.Errorf("maximum folders reached (256)")
	}

	// Create new folder entry
	newEntry := d.newFolderEntry(name)

	// Write empty song block for the new folder
	err = d.writeSongEntries(lastFolder, nil)
	if err != nil {
		return err
	}

	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	// Get location of the new song block
	songBlockLoc, err := d.sendCommand(CmdQueryOffsetLastWrite, 0, 0)
	if err != nil {
		return err
	}
	newEntry.Offset = uint16(songBlockLoc)

	folders = append(folders, newEntry)

	// Write folder list
	err = d.writeFolderEntries(folders)
	if err != nil {
		return err
	}

	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	// Tell device where the folder block is
	folderBlockLoc, err := d.sendCommand(CmdQueryOffsetLastWrite, 0, 0)
	if err != nil {
		return err
	}
	// Use 0 for folder list updates (same as DeleteFolder does)
	err = d.sendFolderLocation(int(folderBlockLoc), 0)
	if err != nil {
		return err
	}

	d.sendCommand(CmdEndFolderTransfers, 0, d.card)

	return nil
}

// DeleteFolder deletes a folder and all its songs
func (d *Device) DeleteFolder(folderNum int) error {
	if err := d.Initialize(); err != nil {
		return err
	}
	defer d.Finalize()

	folders, err := d.readFolderEntries()
	if err != nil {
		return err
	}

	if folderNum >= len(folders) {
		return fmt.Errorf("folder %d does not exist", folderNum)
	}

	// Delete all songs in folder first
	songs, err := d.readSongEntries(folders, folderNum)
	if err != nil {
		return err
	}

	for songNum := len(songs) - 1; songNum >= 0; songNum-- {
		err = d.deleteSongInternal(songNum, folderNum, folders)
		if err != nil {
			return err
		}
		// Re-read folders after each delete
		folders, _ = d.readFolderEntries()
	}

	// Remove folder from list
	folders = append(folders[:folderNum], folders[folderNum+1:]...)

	d.sendCommand(CmdSetWriteAddress, (folderNum<<8)|0xFF, d.card)

	err = d.writeFolderEntries(folders)
	if err != nil {
		return err
	}

	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	folderBlockOffset, _ := d.sendCommand(CmdQueryOffsetLastWrite, 0, 0)
	d.sendFolderLocation(int(folderBlockOffset), 0)
	d.sendCommand(CmdEndFolderTransfers, 0, d.card)

	return nil
}

// DeleteSong deletes a song from a folder
func (d *Device) DeleteSong(folderNum, songNum int) error {
	if err := d.Initialize(); err != nil {
		return err
	}
	defer d.Finalize()

	folders, err := d.readFolderEntries()
	if err != nil {
		return err
	}

	return d.deleteSongInternal(songNum, folderNum, folders)
}

// UploadSong uploads a song to a folder
func (d *Device) UploadSong(folderNum int, filename string, data []byte, progress func(percent int)) error {
	if err := d.Initialize(); err != nil {
		return err
	}
	defer d.Finalize()

	// Check free memory
	freeMemory, err := d.GetFreeMemory()
	if err != nil {
		return err
	}

	if uint32(len(data)) > freeMemory {
		return fmt.Errorf("not enough memory: need %d bytes, have %d", len(data), freeMemory)
	}

	// Read current folder and song structure
	folders, err := d.readFolderEntries()
	if err != nil {
		return err
	}

	if folderNum >= len(folders) {
		folderNum = 0
	}

	songs, err := d.readSongEntries(folders, folderNum)
	if err != nil {
		return err
	}

	// Write the song data
	songLocation, err := d.writeSongData(data, progress)
	if err != nil {
		return err
	}

	// Create new song entry
	newSong := d.newSongEntry(filename, uint32(len(data)), uint16(songLocation))
	songs = append(songs, newSong)

	// Write song entries
	err = d.writeSongEntries(folderNum, songs)
	if err != nil {
		return err
	}

	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	songBlockOffset, _ := d.sendCommand(CmdQueryOffsetLastWrite, 0, 0)

	// Update folder entry
	folders[folderNum].Offset = uint16(songBlockOffset)
	folders[folderNum].FirstFreeEntryOff += 0x800

	err = d.writeFolderEntries(folders)
	if err != nil {
		return err
	}

	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	folderBlockOffset, _ := d.sendCommand(CmdQueryOffsetLastWrite, 0, 0)
	d.sendFolderLocation(int(folderBlockOffset), folderNum)
	d.sendCommand(CmdEndFolderTransfers, 0, d.card)

	return nil
}

// DownloadSong downloads a song from a folder
func (d *Device) DownloadSong(folderNum, songNum int, progress func(percent int)) ([]byte, error) {
	if err := d.Initialize(); err != nil {
		return nil, err
	}
	defer d.Finalize()

	// Read folder and song structure
	folders, err := d.readFolderEntries()
	if err != nil {
		return nil, err
	}

	if folderNum >= len(folders) {
		return nil, fmt.Errorf("folder %d does not exist", folderNum)
	}

	songs, err := d.readSongEntries(folders, folderNum)
	if err != nil {
		return nil, err
	}

	if songNum >= len(songs) {
		return nil, fmt.Errorf("song %d does not exist in folder %d", songNum, folderNum)
	}

	song := songs[songNum]
	size := song.Length

	// Trick the Rio into reading song data by temporarily changing folder offset
	// to point to the song's data location
	oldOffset := folders[0].Offset
	folders[0].Offset = song.Offset

	// Write modified folder entries
	err = d.writeFolderEntries(folders)
	if err != nil {
		return nil, err
	}

	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	folderBlockOffset, _ := d.sendCommand(CmdQueryOffsetLastWrite, 0, 0)
	d.sendFolderLocation(int(folderBlockOffset), folderNum)
	d.sendCommand(CmdEndFolderTransfers, 0, d.card)

	// Now read the song data
	data, err := d.readSongData(size, progress)
	if err != nil {
		return nil, err
	}

	// Restore original folder offset
	folders[0].Offset = oldOffset
	err = d.writeFolderEntries(folders)
	if err != nil {
		return nil, err
	}

	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	folderBlockOffset, _ = d.sendCommand(CmdQueryOffsetLastWrite, 0, 0)
	d.sendFolderLocation(int(folderBlockOffset), folderNum)
	d.sendCommand(CmdEndFolderTransfers, 0, d.card)

	return data, nil
}

// readSongData reads song data from the device
func (d *Device) readSongData(size uint32, progress func(percent int)) ([]byte, error) {
	data := make([]byte, size)
	total := 0

	// Read first 0x4000 bytes
	firstRead := int(size)
	if firstRead > 0x4000 {
		firstRead = 0x4000
	}

	d.sendCommand(CmdSetReadAddress, 0xFF, d.card)
	d.sendCommand(CmdReadFromUSB, 0, firstRead)

	n, err := d.usb.BulkRead(data[:firstRead])
	if err != nil {
		return nil, fmt.Errorf("failed to read first block: %w", err)
	}
	total += n

	if progress != nil {
		progress(total * 100 / int(size))
	}

	remaining := int(size) - firstRead
	if remaining <= 0 {
		return data[:total], nil
	}

	numBigBlocks := remaining / 0x10000
	remainder := remaining % 0x10000

	// Read in 64KB chunks
	numChunks := 0x10
	blocksLeft := numBigBlocks

	for blocksLeft > numChunks {
		d.sendCommand(CmdReadFromUSB, numChunks, 0)

		for j := 0; j < numChunks/2; j++ {
			n, err := d.usb.BulkRead(data[total : total+0x20000])
			if err != nil {
				return nil, fmt.Errorf("failed to read block: %w", err)
			}
			total += n

			if progress != nil {
				progress(total * 100 / int(size))
			}
		}

		blocksLeft -= numChunks
		d.sendCommand(CmdWait, 0, 0)
		d.sendCommand(CmdWait, 0, 0)
	}

	// Read remaining big blocks
	if blocksLeft > 0 {
		d.sendCommand(CmdReadFromUSB, blocksLeft, 0)

		for blocksLeft > 0 {
			readSize := 0x10000
			if total+readSize > int(size) {
				readSize = int(size) - total
			}

			n, err := d.usb.BulkRead(data[total : total+readSize])
			if err != nil {
				return nil, fmt.Errorf("failed to read block: %w", err)
			}
			total += n
			blocksLeft--

			if progress != nil {
				progress(total * 100 / int(size))
			}

			d.sendCommand(CmdWait, 0, 0)
			d.sendCommand(CmdWait, 0, 0)
		}
	}

	// Read remainder
	for remainder > 0 {
		thisRead := remainder
		if thisRead > 0x4000 {
			thisRead = 0x4000
		}

		d.sendCommand(CmdReadFromUSB, 0, thisRead)

		n, err := d.usb.BulkRead(data[total : total+thisRead])
		if err != nil {
			return nil, fmt.Errorf("failed to read remainder: %w", err)
		}
		total += n
		remainder -= thisRead

		if progress != nil {
			progress(total * 100 / int(size))
		}
	}

	return data[:total], nil
}

// Helper functions

func (d *Device) sendCommand(req int, val int, idx int) (uint32, error) {
	data := make([]byte, 4)
	err := d.usb.ControlIn(byte(req), uint16(val), uint16(idx), data)
	if err != nil {
		return 0, err
	}
	result := binary.LittleEndian.Uint32(data)
	return result, nil
}

func (d *Device) sendCommandOut(req int, val int, idx int) error {
	// Control OUT with no data (just setup packet)
	err := d.usb.ControlOut(byte(req), uint16(val), uint16(idx), nil)
	if err != nil {
		return err
	}
	return nil
}


func (d *Device) readFolderEntries() ([]FolderEntry, error) {
	// Get number of folder blocks
	blockCount, err := d.sendCommand(CmdGetNumFolderBlocks, 0xFF00, d.card)
	if err != nil {
		return nil, err
	}

	if blockCount == 0 {
		return nil, nil
	}

	totalSize := int(blockCount) * FolderBlockSize
	block := make([]byte, totalSize)

	// Read folder blocks
	err = d.sendReadCommand(0xFF00, int(blockCount))
	if err != nil {
		return nil, err
	}

	n, err := d.usb.BulkRead(block)
	if err != nil {
		return nil, err
	}
	if n != totalSize {
		return nil, fmt.Errorf("short read: got %d, expected %d", n, totalSize)
	}

	// Parse folder entries (2048 bytes each, 8 per block)
	var entries []FolderEntry
	maxEntries := int(blockCount) * 8

	for i := 0; i < maxEntries; i++ {
		offset := i * 2048
		if offset+2 > len(block) {
			break
		}

		entryOffset := binary.LittleEndian.Uint16(block[offset : offset+2])
		if entryOffset == 0xFFFF {
			break
		}

		entry := parseFolderEntry(block[offset : offset+2048])
		entries = append(entries, entry)
	}

	return entries, nil
}

func (d *Device) readSongEntries(folders []FolderEntry, folderNum int) ([]SongEntry, error) {
	if folderNum >= len(folders) {
		return nil, fmt.Errorf("invalid folder number")
	}

	folder := folders[folderNum]

	// Calculate number of blocks needed
	numBlocks := int(folder.FirstFreeEntryOff) / FolderBlockSize
	if folder.FirstFreeEntryOff%FolderBlockSize > 0 {
		numBlocks++
	}

	if numBlocks == 0 {
		return nil, nil
	}

	// Calculate address
	address := (folderNum << 8) | 0xFF
	address &= 0x0FFF

	size := numBlocks * FolderBlockSize
	block := make([]byte, size)

	err := d.sendReadCommand(address, numBlocks)
	if err != nil {
		return nil, err
	}

	n, err := d.usb.BulkRead(block)
	if err != nil {
		return nil, err
	}
	if n != size {
		return nil, fmt.Errorf("short read")
	}

	// Parse song entries
	var entries []SongEntry
	count := int(folder.FirstFreeEntryOff) / 0x800

	for i := 0; i < count; i++ {
		offset := i * 2048
		if offset+2 > len(block) {
			break
		}

		entryOffset := binary.LittleEndian.Uint16(block[offset : offset+2])
		if entryOffset == 0xFFFF {
			break
		}

		entry := parseSongEntry(block[offset : offset+2048])
		entries = append(entries, entry)
	}

	return entries, nil
}

func (d *Device) writeFolderEntries(folders []FolderEntry) error {
	if len(folders) == 0 {
		// Write empty block
		block := newEmptyBlock()
		err := d.sendWriteCommand(0xFF00, 1)
		if err != nil {
			return err
		}
		return d.usb.BulkWrite(block)
	}

	numBlocks := (len(folders) + 7) / 8
	err := d.sendWriteCommand(0xFF00, numBlocks)
	if err != nil {
		return err
	}

	block := newEmptyBlock()
	count := 0

	for _, folder := range folders {
		copy(block[count*2048:], serializeFolderEntry(folder))
		count++

		if count == 8 {
			err = d.usb.BulkWrite(block)
			if err != nil {
				return err
			}
			block = newEmptyBlock()
			count = 0
		}
	}

	if count > 0 {
		err = d.usb.BulkWrite(block)
		if err != nil {
			return err
		}
	}

	return nil
}

func (d *Device) writeSongEntries(folderNum int, songs []SongEntry) error {
	address := (folderNum << 8) | 0xFF
	address &= 0xFFFF

	if len(songs) == 0 {
		block := newEmptyBlock()
		err := d.sendWriteCommand(address, 1)
		if err != nil {
			return err
		}
		return d.usb.BulkWrite(block)
	}

	numBlocks := (len(songs) + 7) / 8
	err := d.sendWriteCommand(address, numBlocks)
	if err != nil {
		return err
	}

	block := newEmptyBlock()
	count := 0

	for _, song := range songs {
		copy(block[count*2048:], serializeSongEntry(song))
		count++

		if count == 8 {
			err = d.usb.BulkWrite(block)
			if err != nil {
				return err
			}
			block = newEmptyBlock()
			count = 0
		}
	}

	if count > 0 {
		err = d.usb.BulkWrite(block)
		if err != nil {
			return err
		}
	}

	return nil
}

func (d *Device) writeSongData(data []byte, progress func(percent int)) (int, error) {
	size := len(data)
	const chunkSize = 0x4000 // 16KB, consistent with remainder handling in C code

	d.sendCommand(0x4F, 0xFFFF, d.card) // This command is used to set a flag or state before writing

	totalWritten := 0

	for totalWritten < size {
		currentChunkSize := chunkSize
		if totalWritten+currentChunkSize > size {
			currentChunkSize = size - totalWritten
		}

		chunk := data[totalWritten : totalWritten+currentChunkSize]

		// Command to write a chunk (val=0 for bytes, idx=currentChunkSize for exact size)
		// This matches the C code's `send_command (rio_dev, 0x46, 00, j)` for remainders
		d.sendCommand(CmdWriteToUSB, 0, currentChunkSize)

		err := d.usb.BulkWrite(chunk)
		if err != nil {
			return 0, fmt.Errorf("bulk write failed at %d bytes (chunk size %d): %w", totalWritten, currentChunkSize, err)
		}
		totalWritten += currentChunkSize

		if progress != nil {
			progress(totalWritten * 100 / size)
		}

		// Add waits after each chunk, similar to firmware write, to give device time to process
		d.sendCommand(CmdWait, 0, 0)
		d.sendCommand(CmdWait, 0, 0)
	}

	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	location, err := d.sendCommand(CmdQueryOffsetLastWrite, 0, 0)
	if err != nil {
		return 0, err
	}

	return int(location), nil
}

func (d *Device) sendReadCommand(address int, numBlocks int) error {
	length := numBlocks * FolderBlockSize
	numBigReads := length / 0x10000
	numSmallReads := length % 0x10000

	_, err := d.sendCommand(CmdSetReadAddress, address, d.card)
	if err != nil {
		return err
	}

	_, err = d.sendCommand(CmdReadFromUSB, numBigReads, numSmallReads)
	return err
}

func (d *Device) sendWriteCommand(address int, numBlocks int) error {
	length := numBlocks * FolderBlockSize
	numBigWrites := length / 0x10000
	numSmallWrites := length % 0x10000

	d.sendCommand(CmdSetWriteAddress, address, d.card)
	d.sendCommand(0x4F, 0xFFFF, d.card)
	_, err := d.sendCommand(CmdWriteToUSB, numBigWrites, numSmallWrites)
	return err
}

func (d *Device) sendFolderLocation(offset int, folderNum int) error {
	loc := FolderLocation{
		Offset:    uint16(offset),
		Bytes:     0x4000,
		FolderNum: uint16(folderNum),
	}

	data := make([]byte, 6)
	binary.LittleEndian.PutUint16(data[0:2], loc.Offset)
	binary.LittleEndian.PutUint16(data[2:4], loc.Bytes)
	binary.LittleEndian.PutUint16(data[4:6], loc.FolderNum)

	return d.usb.ControlOut(CmdSendFolderLocation, 0, 0, data)
}

func (d *Device) deleteSongInternal(songNum, folderNum int, folders []FolderEntry) error {
	songs, err := d.readSongEntries(folders, folderNum)
	if err != nil {
		return err
	}

	if songNum >= len(songs) {
		return fmt.Errorf("song %d does not exist", songNum)
	}

	// Remove song from list
	songs = append(songs[:songNum], songs[songNum+1:]...)

	// Send remove command
	d.sendCommand(CmdSetWriteAddress, (folderNum<<8)|songNum, d.card)

	// Write updated song list
	err = d.writeSongEntries(folderNum, songs)
	if err != nil {
		return err
	}

	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	songBlockOffset, _ := d.sendCommand(CmdQueryOffsetLastWrite, 0, 0)

	// Update folder entry
	folders[folderNum].Offset = uint16(songBlockOffset)
	folders[folderNum].FirstFreeEntryOff -= 0x800

	err = d.writeFolderEntries(folders)
	if err != nil {
		return err
	}

	d.sendCommand(CmdWait, 0, 0)
	d.sendCommand(CmdWait, 0, 0)

	folderBlockOffset, _ := d.sendCommand(CmdQueryOffsetLastWrite, 0, 0)
	d.sendFolderLocation(int(folderBlockOffset), folderNum)
	d.sendCommand(CmdEndFolderTransfers, 0, d.card)

	return nil
}

func (d *Device) newFolderEntry(name string) FolderEntry {
	entry := FolderEntry{
		Unknown3: 0x002100FF,
		Time:     uint32(time.Now().Unix()),
	}

	copy(entry.Name1[:], name)
	copy(entry.Name2[:], name)

	// Render name to bitmap for display
	bitmapData, numBlocks := RenderTextToBitmap(name)
	entry.Bitmap.NumBlocks = uint16(numBlocks)
	copy(entry.Bitmap.Bitmap[:], bitmapData)

	return entry
}

func (d *Device) newSongEntry(name string, size uint32, offset uint16) SongEntry {
	entry := SongEntry{
		Offset:   offset,
		Length:   size,
		Unknown3: 0x0020,
		MP3Sig:   0x0092FBFF,
		Time:     uint32(time.Now().Unix()),
	}

	copy(entry.Name1[:], name)
	copy(entry.Name2[:], name)

	// Render name to bitmap for display
	bitmapData, numBlocks := RenderTextToBitmap(name)
	entry.Bitmap.NumBlocks = uint16(numBlocks)
	copy(entry.Bitmap.Bitmap[:], bitmapData)

	return entry
}

// Utility functions

func newEmptyBlock() []byte {
	block := make([]byte, FolderBlockSize)
	// Mark entries as empty (0xFFFF at start of each entry)
	for i := 0; i < FolderBlockSize; i += 0x800 {
		block[i] = 0xFF
		block[i+1] = 0xFF
	}
	return block
}

func parseFolderEntry(data []byte) FolderEntry {
	entry := FolderEntry{
		Offset:            binary.LittleEndian.Uint16(data[0:2]),
		Unknown1:          binary.LittleEndian.Uint16(data[2:4]),
		FirstFreeEntryOff: binary.LittleEndian.Uint16(data[4:6]),
		Unknown2:          binary.LittleEndian.Uint16(data[6:8]),
		Unknown3:          binary.LittleEndian.Uint32(data[8:12]),
		Unknown4:          binary.LittleEndian.Uint32(data[12:16]),
		Time:              binary.LittleEndian.Uint32(data[16:20]),
	}

	// Bitmap starts at offset 20
	entry.Bitmap.NumBlocks = binary.LittleEndian.Uint16(data[20:22])
	copy(entry.Bitmap.Bitmap[:], data[22:22+1536])

	// Names
	copy(entry.Name1[:], data[22+1536:22+1536+362])
	copy(entry.Name2[:], data[22+1536+362:22+1536+362+128])

	return entry
}

func parseSongEntry(data []byte) SongEntry {
	entry := SongEntry{
		Offset:   binary.LittleEndian.Uint16(data[0:2]),
		Unknown1: binary.LittleEndian.Uint16(data[2:4]),
		Length:   binary.LittleEndian.Uint32(data[4:8]),
		Unknown2: binary.LittleEndian.Uint16(data[8:10]),
		Unknown3: binary.LittleEndian.Uint16(data[10:12]),
		MP3Sig:   binary.LittleEndian.Uint32(data[12:16]),
		Time:     binary.LittleEndian.Uint32(data[16:20]),
	}

	entry.Bitmap.NumBlocks = binary.LittleEndian.Uint16(data[20:22])
	copy(entry.Bitmap.Bitmap[:], data[22:22+1536])

	copy(entry.Name1[:], data[22+1536:22+1536+362])
	copy(entry.Name2[:], data[22+1536+362:22+1536+362+128])

	return entry
}

func serializeFolderEntry(entry FolderEntry) []byte {
	data := make([]byte, 2048)

	binary.LittleEndian.PutUint16(data[0:2], entry.Offset)
	binary.LittleEndian.PutUint16(data[2:4], entry.Unknown1)
	binary.LittleEndian.PutUint16(data[4:6], entry.FirstFreeEntryOff)
	binary.LittleEndian.PutUint16(data[6:8], entry.Unknown2)
	binary.LittleEndian.PutUint32(data[8:12], entry.Unknown3)
	binary.LittleEndian.PutUint32(data[12:16], entry.Unknown4)
	binary.LittleEndian.PutUint32(data[16:20], entry.Time)

	binary.LittleEndian.PutUint16(data[20:22], entry.Bitmap.NumBlocks)
	copy(data[22:22+1536], entry.Bitmap.Bitmap[:])

	copy(data[22+1536:], entry.Name1[:])
	copy(data[22+1536+362:], entry.Name2[:])

	return data
}

func serializeSongEntry(entry SongEntry) []byte {
	data := make([]byte, 2048)

	binary.LittleEndian.PutUint16(data[0:2], entry.Offset)
	binary.LittleEndian.PutUint16(data[2:4], entry.Unknown1)
	binary.LittleEndian.PutUint32(data[4:8], entry.Length)
	binary.LittleEndian.PutUint16(data[8:10], entry.Unknown2)
	binary.LittleEndian.PutUint16(data[10:12], entry.Unknown3)
	binary.LittleEndian.PutUint32(data[12:16], entry.MP3Sig)
	binary.LittleEndian.PutUint32(data[16:20], entry.Time)

	binary.LittleEndian.PutUint16(data[20:22], entry.Bitmap.NumBlocks)
	copy(data[22:22+1536], entry.Bitmap.Bitmap[:])

	copy(data[22+1536:], entry.Name1[:])
	copy(data[22+1536+362:], entry.Name2[:])

	return data
}

func nullTermString(b []byte) string {
	for i, v := range b {
		if v == 0 {
			return string(b[:i])
		}
	}
	return string(b)
}
