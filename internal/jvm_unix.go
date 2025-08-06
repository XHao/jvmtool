//go:build unix

package internal

import (
	"fmt"
	"io"
	"os"
	"syscall"
	"time"

	"golang.org/x/sys/unix"
)

// jdk/src/jdk.attach/share/classes/sun/tools/attach/HotSpotVirtualMachine.java
func (jp *JvmProcess) checkSocket() error {
	socketPath := fmt.Sprintf("%s/.java_pid%d", os.TempDir(), jp.Pid)
	attachFile := fmt.Sprintf("%s/.attach_pid%d", os.TempDir(), jp.Pid)
	var created bool
	timeSpend := 0
	for {
		_, err := os.Stat(socketPath)
		if err == nil {
			return nil
		}
		if timeSpend > ATTACH_TIMEOUT {
			break
		}
		if created {
			time.Sleep(1000 * time.Millisecond)
			timeSpend += 1000
			continue
		}
		created = true
		f, err := os.Create(attachFile)
		if f != nil {
			defer f.Close()
		}
		defer os.Remove(attachFile)
		if err != nil {
			return fmt.Errorf("attach failed, cannot create file, %v", err.Error())
		} else {
			p, err := os.FindProcess(int(jp.Pid))
			if err != nil {
				return fmt.Errorf("java process does not exist, %v", jp.Pid)
			}
			err = p.Signal(syscall.SIGQUIT)
			if err != nil {
				return fmt.Errorf("cannot send signal %v to Java process", syscall.SIGQUIT)
			}
		}
		time.Sleep(1000 * time.Millisecond)
		timeSpend += 1000
	}
	return fmt.Errorf("unable to open socket file %s: target process %d doesn't respond within %dms or HotSpot VM not loaded", socketPath, jp.Pid, timeSpend)
}

func (jp *JvmProcess) loadAgent(agentPath string, params string) error {
	socketPath := fmt.Sprintf("%s/.java_pid%d", os.TempDir(), jp.Pid)

	// Build common request
	request := buildAgentRequest(agentPath, params)

	// Create and connect Unix socket
	fd, err := unix.Socket(unix.AF_UNIX, unix.SOCK_STREAM, 0)
	if err != nil {
		return fmt.Errorf("failed to create unix socket: %v", err.Error())
	}
	addr := unix.SockaddrUnix{
		Name: socketPath,
	}
	err = unix.Connect(fd, &addr)
	if err != nil {
		return fmt.Errorf("failed to connect to target process %v: %v %v", jp.Pid, socketPath, err.Error())
	}
	defer unix.Close(fd)

	// Send request
	if _, err = unix.Write(fd, request.RequestData); err != nil {
		return fmt.Errorf("failed to write attach request to process %v: %v", jp.Pid, err.Error())
	}

	log("waiting for attach to complete...")

	// Read response
	resp, err := readAttachResponse(fd, jp.Pid)
	if err != nil {
		return err
	}

	log("attach operation completed")

	// Parse common response
	_, err = parseAgentResponse(resp, request.IsNative)
	return err
}

func readAttachResponse(fd int, pid int32) (resp string, err error) {
	buf := make([]byte, 4096)
	var data []byte
	n := 0
	for {
		n, err = unix.Read(fd, buf)
		if n > 0 {
			data = append(data, buf[:n]...)
		}
		if err != nil {
			if err == io.EOF {
				break
			}
			return "", fmt.Errorf("failed to read attach response from process %v: %v", pid, err.Error())
		}
		if n == 0 {
			break
		}
	}
	resp = string(data)
	return
}
