package internal

import (
	"errors"
	"fmt"
	"os/user"
	"strings"
)

const (
	ATTACH_TIMEOUT = 9000
)

type JvmProcess struct {
	Pid int32
	Cmd string
	user.User

	mainClassOrJar string
	vmArgs         string
	mainArgs       string
}

// AgentRequest represents a common agent loading request
type AgentRequest struct {
	AgentPath   string
	Params      string
	IsNative    bool
	Command     string
	RequestData []byte
}

// AgentResponse represents the response from agent loading
type AgentResponse struct {
	Success    bool
	ReturnCode string
	Message    string
	ErrorCode  string
}

// Common agent type detection logic
func detectAgentType(agentPath string) bool {
	return strings.HasSuffix(agentPath, ".so") ||
		strings.HasSuffix(agentPath, ".dylib")
}

// Common request building logic
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

// Common response parsing logic
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
