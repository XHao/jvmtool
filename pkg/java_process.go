package pkg

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"

	"github.com/shirou/gopsutil/process"
)

// JavaProcessValidator validates Java process related parameters
type JavaProcessValidator struct {
	User string
	Pid  string
}

// ValidateJavaProcess validates that a Java process exists and belongs to the specified user
func (v *JavaProcessValidator) ValidateJavaProcess() error {
	if v.Pid == "" {
		return fmt.Errorf("pid is required")
	}

	// Check if process exists
	pid, err := strconv.Atoi(v.Pid)
	if err != nil {
		return fmt.Errorf("invalid pid: %s", v.Pid)
	}

	_, err = process.NewProcess(int32(pid))
	if err != nil {
		return fmt.Errorf("process not found")
	}

	// Check if process belongs to the specified user via hsperfdata
	pidFile := GetHsperfdataPath(v.User, v.Pid)
	if !PathExists(pidFile) {
		return fmt.Errorf("pid does not belong to the specified user")
	}

	return nil
}

// GetHsperfdataPath returns the hsperfdata file path for a user and PID
func GetHsperfdataPath(username, pid string) string {
	return filepath.Join(os.TempDir(), "hsperfdata_"+username, pid)
}

// GetHsperfdataDir returns the hsperfdata directory for a user
func GetHsperfdataDir(username string) string {
	return filepath.Join(os.TempDir(), "hsperfdata_"+username)
}

// DiscoverJavaProcesses discovers Java process PIDs from hsperfdata files
func DiscoverJavaProcesses(username string) ([]int32, error) {
	hsperfdataDir := GetHsperfdataDir(username)
	fileNamePattern := filepath.Join(hsperfdataDir, "*")

	files, err := filepath.Glob(fileNamePattern)
	if err != nil {
		return nil, fmt.Errorf("failed to search hsperfdata files: %w", err)
	}

	var pids []int32
	for _, file := range files {
		filename := filepath.Base(file)
		if pid, err := strconv.Atoi(filename); err == nil {
			pids = append(pids, int32(pid))
		}
	}

	return pids, nil
}

// Pid safely converts a string to int32
func Pid(s string) int32 {
	i, err := strconv.Atoi(s)
	if err != nil {
		return -1
	}
	return int32(i)
}
