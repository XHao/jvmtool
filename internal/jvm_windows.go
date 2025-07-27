//go:build windows

package internal

import (
	"fmt"
	"os"
	"time"
	"unsafe"

	"golang.org/x/sys/windows"
)

var (
	kernel32                = windows.NewLazySystemDLL("kernel32.dll")
	procCreateEvent         = kernel32.NewProc("CreateEventW")
	procSetEvent            = kernel32.NewProc("SetEvent")
	procOpenEvent           = kernel32.NewProc("OpenEventW")
	procCreateFileMapping   = kernel32.NewProc("CreateFileMappingW")
	procOpenFileMapping     = kernel32.NewProc("OpenFileMappingW")
	procMapViewOfFile       = kernel32.NewProc("MapViewOfFile")
	procUnmapViewOfFile     = kernel32.NewProc("UnmapViewOfFile")
	procWaitForSingleObject = kernel32.NewProc("WaitForSingleObject")
	procGetCurrentProcessId = kernel32.NewProc("GetCurrentProcessId")
)

const (
	EVENT_ALL_ACCESS    = 0x1F0003
	FILE_MAP_ALL_ACCESS = 0x000F001F
	WAIT_TIMEOUT        = 0x00000102
)

// Windows-specific attach implementation
// Based on HotSpot VM's Windows attach mechanism
func (jp *JvmProcess) checkSocket() error {
	// On Windows, Java attach uses named events and shared memory instead of Unix sockets
	attachEventName := fmt.Sprintf("attach_pid_%d", jp.Pid)

	// Try to create or open the attach event
	attachEvent, err := createOrOpenEvent(attachEventName)
	if err != nil {
		// If event doesn't exist, try to trigger creation by signaling the process
		err = jp.triggerAttachEvent()
		if err != nil {
			return fmt.Errorf("failed to trigger attach event: %v", err)
		}

		timeSpent := 0
		for timeSpent < ATTACH_TIMEOUT {
			attachEvent, err = createOrOpenEvent(attachEventName)
			if err == nil {
				windows.CloseHandle(attachEvent)
				return nil
			}
			time.Sleep(100 * time.Millisecond)
			timeSpent += 100
		}
		return fmt.Errorf("attach event %s not created within %dms", attachEventName, ATTACH_TIMEOUT)
	}

	windows.CloseHandle(attachEvent)
	return nil
}

func (jp *JvmProcess) loadAgent(agentPath string, params string) error {
	// Build common request
	request := buildAgentRequest(agentPath, params)

	// Create shared memory for communication
	memoryName := fmt.Sprintf("attach_shm_%d_%d", jp.Pid, getCurrentProcessId())

	// Create shared memory
	sharedMem, err := createSharedMemory(memoryName, len(request.Command)+1024)
	if err != nil {
		return fmt.Errorf("failed to create shared memory: %v", err)
	}
	defer windows.CloseHandle(sharedMem)

	// Map view of shared memory
	memView, err := mapSharedMemory(sharedMem, len(request.Command)+1024)
	if err != nil {
		return fmt.Errorf("failed to map shared memory: %v", err)
	}
	defer unmapSharedMemory(memView)

	// Write command to shared memory
	copy((*[1024]byte)(memView)[:], []byte(request.Command))

	// Create events for synchronization
	requestEventName := fmt.Sprintf("attach_req_%d_%d", jp.Pid, getCurrentProcessId())
	responseEventName := fmt.Sprintf("attach_rsp_%d_%d", jp.Pid, getCurrentProcessId())

	requestEvent, err := createEvent(requestEventName)
	if err != nil {
		return fmt.Errorf("failed to create request event: %v", err)
	}
	defer windows.CloseHandle(requestEvent)

	responseEvent, err := createEvent(responseEventName)
	if err != nil {
		return fmt.Errorf("failed to create response event: %v", err)
	}
	defer windows.CloseHandle(responseEvent)

	// Signal request event
	err = setEvent(requestEvent)
	if err != nil {
		return fmt.Errorf("failed to signal request event: %v", err)
	}

	log("waiting for attach to complete...")

	// Wait for response
	result, err := waitForEvent(responseEvent, ATTACH_TIMEOUT)
	if err != nil {
		return fmt.Errorf("failed to wait for response: %v", err)
	}

	if result == WAIT_TIMEOUT {
		return fmt.Errorf("attach operation timed out")
	}

	log("attach operation completed")

	// Read response from shared memory
	responseData := make([]byte, 1024)
	copy(responseData, (*[1024]byte)(memView)[:])

	// Find the actual response length (null terminated)
	responseLen := 0
	for i, b := range responseData {
		if b == 0 {
			responseLen = i
			break
		}
	}

	if responseLen == 0 {
		return fmt.Errorf("target VM did not respond")
	}

	response := string(responseData[:responseLen])

	// Parse common response
	_, err = parseAgentResponse(response, request.IsNative)
	return err
}

// Helper functions for Windows APIs

func createOrOpenEvent(name string) (windows.Handle, error) {
	namePtr, _ := windows.UTF16PtrFromString(name)
	handle, _, err := procOpenEvent.Call(
		uintptr(EVENT_ALL_ACCESS),
		0, // bInheritHandle
		uintptr(unsafe.Pointer(namePtr)),
	)
	if handle == 0 {
		return 0, err
	}
	return windows.Handle(handle), nil
}

func createEvent(name string) (windows.Handle, error) {
	namePtr, _ := windows.UTF16PtrFromString(name)
	handle, _, err := procCreateEvent.Call(
		0, // lpEventAttributes
		0, // bManualReset
		0, // bInitialState
		uintptr(unsafe.Pointer(namePtr)),
	)
	if handle == 0 {
		return 0, err
	}
	return windows.Handle(handle), nil
}

func setEvent(event windows.Handle) error {
	ret, _, err := procSetEvent.Call(uintptr(event))
	if ret == 0 {
		return err
	}
	return nil
}

func waitForEvent(event windows.Handle, timeout uint32) (uint32, error) {
	ret, _, err := procWaitForSingleObject.Call(
		uintptr(event),
		uintptr(timeout),
	)
	if ret == 0xFFFFFFFF {
		return 0, err
	}
	return uint32(ret), nil
}

func createSharedMemory(name string, size int) (windows.Handle, error) {
	namePtr, _ := windows.UTF16PtrFromString(name)
	handle, _, err := procCreateFileMapping.Call(
		uintptr(windows.InvalidHandle),
		0, // lpFileMappingAttributes
		uintptr(windows.PAGE_READWRITE),
		0,             // dwMaximumSizeHigh
		uintptr(size), // dwMaximumSizeLow
		uintptr(unsafe.Pointer(namePtr)),
	)
	if handle == 0 {
		return 0, err
	}
	return windows.Handle(handle), nil
}

func mapSharedMemory(handle windows.Handle, size int) (unsafe.Pointer, error) {
	ptr, _, err := procMapViewOfFile.Call(
		uintptr(handle),
		uintptr(FILE_MAP_ALL_ACCESS),
		0,             // dwFileOffsetHigh
		0,             // dwFileOffsetLow
		uintptr(size), // dwNumberOfBytesToMap
	)
	if ptr == 0 {
		return nil, err
	}
	return unsafe.Pointer(ptr), nil
}

func unmapSharedMemory(ptr unsafe.Pointer) error {
	ret, _, err := procUnmapViewOfFile.Call(uintptr(ptr))
	if ret == 0 {
		return err
	}
	return nil
}

func getCurrentProcessId() uint32 {
	ret, _, _ := procGetCurrentProcessId.Call()
	return uint32(ret)
}

func (jp *JvmProcess) triggerAttachEvent() error {
	// On Windows, we need to signal the Java process to create the attach event
	// This is typically done by sending a special signal or using process communication

	// Try to find the process and send a signal
	process, err := os.FindProcess(int(jp.Pid))
	if err != nil {
		return fmt.Errorf("Java process does not exist: %v", jp.Pid)
	}

	// On Windows, we can try sending SIGINT to trigger the attach mechanism
	// Note: This is a simplified approach. In a full implementation,
	// you might need to use more sophisticated process communication
	err = process.Signal(os.Interrupt)
	if err != nil {
		return fmt.Errorf("cannot signal Java process: %v", err)
	}

	return nil
}
