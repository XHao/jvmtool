package pkg

import (
	"os/user"
)

const (
	ATTACH_TIMEOUT = 9000
)

type JvmProcess struct {
	Pid int32
	Cmd string
	user.User

	MainClassOrJar string
	VmArgs         string
	MainArgs       string
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
