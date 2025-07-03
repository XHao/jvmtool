package pkg

import (
	"os"
	"syscall"
	"testing"
)

// TestPathExists tests the PathExists function for both existing and non-existing paths.
func TestPathExists(t *testing.T) {
	// Create a temporary file
	tmpFile, err := os.CreateTemp("", "testpathexists")
	if err != nil {
		t.Fatalf("Failed to create temp file: %v", err)
	}
	defer os.Remove(tmpFile.Name())

	// Existing file should return true
	if !PathExists(tmpFile.Name()) {
		t.Errorf("PathExists should return true for existing file")
	}

	// Non-existing file should return false
	nonExistPath := tmpFile.Name() + "_notexist"
	if PathExists(nonExistPath) {
		t.Errorf("PathExists should return false for non-existing file")
	}

	// Existing directory should return true
	tmpDir := os.TempDir()
	if !PathExists(tmpDir) {
		t.Errorf("PathExists should return true for existing directory")
	}
}

// TestPidExists tests the PidExists function for various scenarios.
func TestPidExists(t *testing.T) {
	// Invalid pid (<=0) should return false and error
	if exist, err := PidExists(0); exist || err == nil {
		t.Errorf("PidExists(0) should return false and error")
	}
	if exist, err := PidExists(-1); exist || err == nil {
		t.Errorf("PidExists(-1) should return false and error")
	}

	// Current process should exist
	pid := int32(os.Getpid())
	exist, err := PidExists(pid)
	if err != nil {
		t.Errorf("PidExists(%d) returned error: %v", pid, err)
	}
	if !exist {
		t.Errorf("PidExists(%d) should return true for current process", pid)
	}

	// Try to find a non-existent pid (use a high unlikely pid)
	nonExistPid := int32(999999)
	exist, err = PidExists(nonExistPid)
	if err != nil && err != syscall.ESRCH {
		t.Logf("PidExists(%d) returned error: %v (may be expected on some systems)", nonExistPid, err)
	}
	if exist {
		t.Errorf("PidExists(%d) should return false for non-existent pid", nonExistPid)
	}
}
