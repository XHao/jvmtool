package internal

import (
	"errors"
	"fmt"
	"io"
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
	timeout := 9_000
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
	resp, err := readAttachResponse(fd, jp.Pid)
	if err != nil {
		return err
	}
	log("attach operation completed")

	if len(resp) == 0 {
		return fmt.Errorf("target VM did not respond")
	}
	ret := strings.Split(resp, "\n")
	returnCode := ret[0]
	if returnCode != "0" {
		return fmt.Errorf("agent load failed, return code: %s", returnCode)

	}
	var errCode string
	if strings.HasPrefix(ret[1], "return code: ") {
		errCode = ret[1][13:]
	} else {
		b := ret[1][0]
		if b == '-' || (b >= '0' && b <= '9') {
			errCode = ret[1]
		} else {
			errCode = "-1"
		}
	}

	switch errCode {
	case "-1":
		return errors.New(ret[1])
	case "0":
		return nil
	case "100":
		return fmt.Errorf("agent load failed, code 100: Agent JAR not found or no Agent-Class attribute")
	case "101":
		return fmt.Errorf("agent load failed, code 101: Unable to add JAR file to system class path")
	case "102":
		return fmt.Errorf("agent load failed, code 102: No agentmain method or agentmain failed")
	}
	return fmt.Errorf("agent load failed, unknown message: %s", ret[1])
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
