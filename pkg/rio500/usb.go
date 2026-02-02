package rio500

import (
	"fmt"
	"time"

	"github.com/google/gousb"
)

// USB direction constants
const (
	DirIn  = 0x80
	DirOut = 0x00

	// Bulk endpoints
	EndpointIn  = 0x81
	EndpointOut = 0x02

	// Timeouts
	ControlTimeout = 5000  // ms
	BulkTimeout    = 30000 // ms
)

// USBDevice represents a connection to the Rio 500 via USB
type USBDevice struct {
	ctx    *gousb.Context
	dev    *gousb.Device
	intf   *gousb.Interface
	config *gousb.Config
	inEp   *gousb.InEndpoint
	outEp  *gousb.OutEndpoint
}

// OpenUSB finds and opens the Rio 500 device
func OpenUSB() (*USBDevice, error) {
	ctx := gousb.NewContext()

	// Debug level 0 = off, increase for troubleshooting
	ctx.Debug(0)

	// Find Rio 500
	dev, err := ctx.OpenDeviceWithVIDPID(VendorID, ProductID)
	if err != nil {
		ctx.Close()
		return nil, fmt.Errorf("failed to open device: %w", err)
	}

	if dev == nil {
		ctx.Close()
		return nil, fmt.Errorf("Rio 500 not found (VID=%04X, PID=%04X)", VendorID, ProductID)
	}

	// Set auto-detach kernel driver
	if err := dev.SetAutoDetach(true); err != nil {
		dev.Close()
		ctx.Close()
		return nil, fmt.Errorf("failed to set auto detach: %w", err)
	}

	// Small delay after opening device
	time.Sleep(100 * time.Millisecond)

	// Get configuration
	config, err := dev.Config(1)
	if err != nil {
		dev.Close()
		ctx.Close()
		return nil, fmt.Errorf("failed to get config: %w", err)
	}

	// Claim interface 0 explicitly
	intf, err := config.Interface(0, 0)
	if err != nil {
		config.Close()
		dev.Close()
		ctx.Close()
		return nil, fmt.Errorf("failed to claim interface: %w", err)
	}

	// Get endpoints
	inEp, err := intf.InEndpoint(EndpointIn)
	if err != nil {
		intf.Close()
		config.Close()
		dev.Close()
		ctx.Close()
		return nil, fmt.Errorf("failed to get IN endpoint: %w", err)
	}

	outEp, err := intf.OutEndpoint(EndpointOut)
	if err != nil {
		intf.Close()
		config.Close()
		dev.Close()
		ctx.Close()
		return nil, fmt.Errorf("failed to get OUT endpoint: %w", err)
	}

	return &USBDevice{
		ctx:    ctx,
		dev:    dev,
		intf:   intf,
		config: config,
		inEp:   inEp,
		outEp:  outEp,
	}, nil
}

// Close closes the USB connection
func (u *USBDevice) Close() error {
	if u.intf != nil {
		u.intf.Close()
	}
	if u.config != nil {
		u.config.Close()
	}
	if u.dev != nil {
		u.dev.Close()
	}
	if u.ctx != nil {
		u.ctx.Close()
	}
	return nil
}

// ControlIn sends a control request and reads data back
func (u *USBDevice) ControlIn(req byte, val uint16, idx uint16, data []byte) error {
	// USB control transfer: vendor, device-to-host
	reqType := uint8(gousb.ControlIn | gousb.ControlVendor | gousb.ControlDevice)

	n, err := u.dev.Control(reqType, req, val, idx, data)
	if err != nil {
		return fmt.Errorf("control IN failed (req=0x%02x val=0x%04x idx=0x%04x): %w", req, val, idx, err)
	}

	if n != len(data) {
		return fmt.Errorf("control IN short read: got %d, expected %d", n, len(data))
	}

	// Delay after control transfer (matches original rio500 driver)
	time.Sleep(400 * time.Microsecond)

	return nil
}

// ControlOut sends a control request with data
func (u *USBDevice) ControlOut(req byte, val uint16, idx uint16, data []byte) error {
	// USB control transfer: vendor, host-to-device
	reqType := uint8(gousb.ControlOut | gousb.ControlVendor | gousb.ControlDevice)

	n, err := u.dev.Control(reqType, req, val, idx, data)
	if err != nil {
		return fmt.Errorf("control OUT failed (req=0x%02x val=0x%04x idx=0x%04x): %w", req, val, idx, err)
	}

	if n != len(data) {
		return fmt.Errorf("control OUT short write: sent %d, expected %d", n, len(data))
	}

	// Delay after control transfer (matches original rio500 driver)
	time.Sleep(400 * time.Microsecond)

	return nil
}

// BulkRead reads data from the bulk IN endpoint
func (u *USBDevice) BulkRead(data []byte) (int, error) {
	total := 0
	remaining := len(data)

	for remaining > 0 {
		n, err := u.inEp.Read(data[total:])
		if err != nil {
			return total, fmt.Errorf("bulk read failed: %w", err)
		}
		total += n
		remaining -= n
	}

	return total, nil
}

// BulkWrite writes data to the bulk OUT endpoint
func (u *USBDevice) BulkWrite(data []byte) error {
	total := 0
	remaining := len(data)

	for remaining > 0 {
		n, err := u.outEp.Write(data[total:])
		if err != nil {
			return fmt.Errorf("bulk write failed: %w", err)
		}
		total += n
		remaining -= n
	}

	return nil
}

// GetDeviceInfo returns device descriptor information
func (u *USBDevice) GetDeviceInfo() (string, error) {
	manufacturer, _ := u.dev.Manufacturer()
	product, _ := u.dev.Product()
	serial, _ := u.dev.SerialNumber()

	return fmt.Sprintf("Manufacturer: %s, Product: %s, Serial: %s",
		manufacturer, product, serial), nil
}
