//go:build unix

package pkg

import (
	"errors"
	"fmt"
	"io"
	"os"
	"strings"
	"syscall"
	"time"

	"golang.org/x/sys/unix"
)

// jdk/src/jdk.attach/share/classes/sun/tools/attach/HotSpotVirtualMachine.java
func (jp *JvmProcess) CheckSocket() error {
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

func (jp *JvmProcess) LoadAgent(agentPath string, params string) error {
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

	Log("waiting for attach to complete...")

	// Read response
	resp, err := readAttachResponse(fd, jp.Pid)
	if err != nil {
		return err
	}

	Log("attach operation completed")

	// Parse common response
	_, err = parseAgentResponse(resp, request.IsNative)
	return err
}

func buildAgentRequest(agentPath, params string) *AgentRequest {
	isNative := detectAgentType(agentPath)

	var command string
	var arg1, arg2, arg3 string

	if isNative {
		// For native agents: agentPath, "true", params
		arg1 = agentPath
		arg2 = "true"
		arg3 = params
		command = fmt.Sprintf("load\x00%s\x00%s\x00%s\x00", arg1, arg2, arg3)
	} else {
		// For Java agents: "instrument", "false", agentPath[=params]
		arg1 = "instrument"
		arg2 = "false"
		arg3 = agentPath
		if params != "" {
			arg3 += "=" + params
		}
		command = fmt.Sprintf("load\x00%s\x00%s\x00%s\x00", arg1, arg2, arg3)
	}

	// Build request data with protocol version
	requestData := make([]byte, 0)
	requestData = append(requestData, byte('1')) // Protocol version
	requestData = append(requestData, byte(0))
	requestData = append(requestData, []byte("load")...)
	requestData = append(requestData, byte(0))
	requestData = append(requestData, []byte(arg1)...)
	requestData = append(requestData, byte(0))
	requestData = append(requestData, []byte(arg2)...)
	requestData = append(requestData, byte(0))
	requestData = append(requestData, []byte(arg3)...)
	requestData = append(requestData, byte(0))

	return &AgentRequest{
		AgentPath:   agentPath,
		Params:      params,
		IsNative:    isNative,
		Command:     command,
		RequestData: requestData,
	}
}

func detectAgentType(agentPath string) bool {
	return strings.HasSuffix(agentPath, ".so") ||
		strings.HasSuffix(agentPath, ".dylib")
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

func parseAgentResponse(responseData string, isNative bool) (*AgentResponse, error) {
	if len(responseData) == 0 {
		return nil, fmt.Errorf("target VM did not respond")
	}

	ret := strings.Split(responseData, "\n")
	if len(ret) < 2 {
		return nil, fmt.Errorf("invalid response format")
	}

	returnCode := ret[0]
	response := &AgentResponse{
		ReturnCode: returnCode,
		Success:    returnCode == "0",
	}

	if returnCode != "0" {
		response.Message = fmt.Sprintf("agent load failed, return code: %s", returnCode)
		return response, errors.New(response.Message)
	}

	// Parse error code from second line
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

	response.ErrorCode = errCode

	// Handle error codes
	switch errCode {
	case "-1":
		response.Message = ret[1]
		return response, errors.New(ret[1])
	case "0":
		response.Success = true
		return response, nil
	case "100":
		if isNative {
			response.Message = "agent load failed, code 100: Native agent library not found or unable to load"
		} else {
			response.Message = "agent load failed, code 100: Agent JAR not found or no Agent-Class attribute"
		}
		return response, errors.New(response.Message)
	case "101":
		response.Message = "agent load failed, code 101: Unable to add JAR file to system class path"
		return response, errors.New(response.Message)
	case "102":
		if isNative {
			response.Message = "agent load failed, code 102: Agent_OnAttach function not found or failed"
		} else {
			response.Message = "agent load failed, code 102: No agentmain method or agentmain failed"
		}
		return response, errors.New(response.Message)
	}

	response.Message = fmt.Sprintf("agent load failed, unknown message: %s", ret[1])
	return response, errors.New(response.Message)
}
