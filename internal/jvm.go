package internal

import (
	"fmt"
	"os"
	"os/user"
	"strings"
	"syscall"
	"time"

	"golang.org/x/sys/unix"
)

type JvmProcess struct {
	Pid int32
	Cmd string
	user.User
}

// jdk/src/jdk.attach/share/classes/sun/tools/attach/HotSpotVirtualMachine.java
func (jp *JvmProcess) checkSocket() error {
	socketPath := fmt.Sprintf("%s/.java_pid%d", os.TempDir(), jp.Pid)
	attachFile := fmt.Sprintf("%s/.attach_pid%d", os.TempDir(), jp.Pid)
	var created bool
	timeout := 10_000
	timeSpend := 0
	for {
		_, err := os.Stat(socketPath)
		if err == nil {
			return nil
		}
		if timeSpend > timeout {
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
			f.Close()
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

	request := make([]byte, 0)
	// Protocol version
	request = append(request, byte('1'))
	request = append(request, byte(0))
	// Command: "load"
	request = append(request, []byte("load")...)
	request = append(request, byte(0))
	// Argument 1: "instrument"
	request = append(request, []byte("instrument")...)
	request = append(request, byte(0))
	// Argument 2: "false"
	request = append(request, []byte("false")...)
	request = append(request, byte(0))
	// Argument 3: agent JAR path (with optional params)
	request = append(request, []byte(agentPath)...)
	if params != "" {
		request = append(request, []byte("="+params)...)
	}
	request = append(request, byte(0))

	if _, err = unix.Write(fd, request); err != nil {
		return fmt.Errorf("failed to write attach request to process %v: %v", jp.Pid, err.Error())
	}

	log("waiting for attach to complete...")
	ret, err := readAttachResponse(fd, jp.Pid)
	if err != nil {
		return err
	}
	log("attach operation completed")

	// ret[0]: attach result code, "0" means success
	if len(ret) == 0 {
		return fmt.Errorf("empty response from target process")
	}
	if ret[0] != "0" {
		return fmt.Errorf("attach to %v failed, error code: %v", jp.Pid, ret[0])
	}

	// ret[1]: load command result code, "0" means success
	var code string
	if len(ret) == 1 {
		code = ret[0]
	} else {
		if strings.Contains(ret[1], "return code: ") {
			code = ret[1][13:]
		} else {
			code = ret[1]
		}
	}
	switch code {
	case "0":
		return nil
	case "100":
		return fmt.Errorf("agent load failed, code 100: Agent JAR not found or no Agent-Class attribute")
	case "101":
		return fmt.Errorf("agent load failed, code 101: Unable to add JAR file to system class path")
	case "102":
		return fmt.Errorf("agent load failed, code 102: No agentmain method or agentmain failed")
	}
	return nil
}

func readAttachResponse(fd int, pid int32) ([]string, error) {
	var lines []string
	buf := make([]byte, 4096)
	n, err := unix.Read(fd, buf)
	if err != nil {
		return nil, fmt.Errorf("failed to read attach response from process %v: %v", pid, err.Error())
	}
	start := 0
	for i := 0; i < n; i++ {
		if buf[i] == 0 {
			lines = append(lines, string(buf[start:i]))
			start = i + 1
		}
	}
	if start < n {
		lines = append(lines, string(buf[start:n]))
	}
	return lines, nil
}
