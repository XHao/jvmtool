package pkg

import (
	"fmt"
	"os"
	"syscall"
)

// PathExists checks whether the given file or directory path exists.
// Returns true if the path exists, false otherwise.
func PathExists(path string) bool {
	_, err := os.Stat(path)
	if err == nil {
		return true
	}
	return !os.IsNotExist(err)
}

// PidExists checks whether a process with the given pid exists on the system.
// Returns true if the process exists, false otherwise. If an error occurs during the check, it is returned.
var PidExists = func(pid int32) (bool, error) {
	if pid <= 0 {
		return false, fmt.Errorf("invalid pid %v", pid)
	}
	proc, err := os.FindProcess(int(pid))
	if err != nil {
		return false, err
	}
	// procfs does not exist or is not mounted, check PID existence by signalling the pid
	err = proc.Signal(syscall.Signal(0))
	if err == nil {
		return true, nil
	}
	if err.Error() == "os: process already finished" {
		return false, nil
	}
	errno, ok := err.(syscall.Errno)
	if !ok {
		return false, err
	}
	switch errno {
	case syscall.ESRCH:
		return false, nil
	case syscall.EPERM:
		return true, nil
	}

	return false, err
}
