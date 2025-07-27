package internal

import (
	"bufio"
	"fmt"
	"os"
	"regexp"
	"strconv"
	"strings"
	"time"
)

// MessageType represents the type of message from agent
type MessageType int

const (
	StatusMessage MessageType = iota
	ErrorMessage
	DataMessage
	ProgressMessage
	ResultMessage
)

// StatusCode represents the status of agent operations
type StatusCode int

const (
	Success StatusCode = iota
	Error
	Running
	Completed
)

// AgentMessage represents a parsed message from agent
type AgentMessage struct {
	Type      MessageType
	Status    StatusCode
	Content   string
	Timestamp string
	Metadata  map[string]string
}

// JTProtocolReader reads and parses .jt protocol files
type JTProtocolReader struct {
	protocolFile string
}

// NewJTProtocolReader creates a new .jt protocol reader with specified path
func NewJTProtocolReader(basePath string) *JTProtocolReader {
	return &JTProtocolReader{
		protocolFile: basePath + ".jt",
	}
}

// ReadAllMessages reads all messages from the .jt file
func (r *JTProtocolReader) ReadAllMessages() ([]*AgentMessage, error) {
	file, err := os.Open(r.protocolFile)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	var messages []*AgentMessage
	scanner := bufio.NewScanner(file)

	for scanner.Scan() {
		line := scanner.Text()
		if msg := r.parseMessage(line); msg != nil {
			messages = append(messages, msg)
		}
	}

	return messages, scanner.Err()
}

// ReadLatestStatus reads the latest status message
func (r *JTProtocolReader) ReadLatestStatus() (*AgentMessage, error) {
	messages, err := r.ReadAllMessages()
	if err != nil {
		return nil, err
	}

	// Find the latest status message
	for i := len(messages) - 1; i >= 0; i-- {
		if messages[i].Type == StatusMessage {
			return messages[i], nil
		}
	}

	return nil, nil
}

// ReadErrors reads all error messages
func (r *JTProtocolReader) ReadErrors() ([]*AgentMessage, error) {
	messages, err := r.ReadAllMessages()
	if err != nil {
		return nil, err
	}

	var errors []*AgentMessage
	for _, msg := range messages {
		if msg.Type == ErrorMessage {
			errors = append(errors, msg)
		}
	}

	return errors, nil
}

// ReadData reads all data messages
func (r *JTProtocolReader) ReadData() ([]*AgentMessage, error) {
	messages, err := r.ReadAllMessages()
	if err != nil {
		return nil, err
	}

	var dataMessages []*AgentMessage
	for _, msg := range messages {
		if msg.Type == DataMessage {
			dataMessages = append(dataMessages, msg)
		}
	}

	return dataMessages, nil
}

// ReadLatestProgress reads the latest progress message
func (r *JTProtocolReader) ReadLatestProgress() (*AgentMessage, error) {
	messages, err := r.ReadAllMessages()
	if err != nil {
		return nil, err
	}

	// Find the latest progress message
	for i := len(messages) - 1; i >= 0; i-- {
		if messages[i].Type == ProgressMessage {
			return messages[i], nil
		}
	}

	return nil, nil
}

// ReadResults reads all result messages
func (r *JTProtocolReader) ReadResults() ([]*AgentMessage, error) {
	messages, err := r.ReadAllMessages()
	if err != nil {
		return nil, err
	}

	var results []*AgentMessage
	for _, msg := range messages {
		if msg.Type == ResultMessage {
			results = append(results, msg)
		}
	}

	return results, nil
}

// WaitForStatus waits for a specific status or timeout
func (r *JTProtocolReader) WaitForStatus(targetStatus StatusCode, timeout time.Duration) (*AgentMessage, error) {
	deadline := time.Now().Add(timeout)

	for time.Now().Before(deadline) {
		if msg, err := r.ReadLatestStatus(); err == nil && msg != nil {
			if msg.Status == targetStatus {
				return msg, nil
			}
		}

		// Check for errors
		if errors, err := r.ReadErrors(); err == nil && len(errors) > 0 {
			return errors[len(errors)-1], fmt.Errorf("agent error: %s", errors[len(errors)-1].Content)
		}

		time.Sleep(100 * time.Millisecond)
	}

	return nil, fmt.Errorf("timeout waiting for status %d", targetStatus)
}

// Cleanup removes the protocol file
func (r *JTProtocolReader) Cleanup() {
	os.Remove(r.protocolFile)
}

// GetProtocolFile returns the path to the protocol file
func (r *JTProtocolReader) GetProtocolFile() string {
	return r.protocolFile
}

// parseMessage parses a single message line from .jt format
func (r *JTProtocolReader) parseMessage(line string) *AgentMessage {
	// Expected format: [timestamp] TYPE status key=value | content
	re := regexp.MustCompile(`^\[([^\]]+)\]\s+(\w+)\s+(\d+)(.*)`)
	matches := re.FindStringSubmatch(line)

	if len(matches) < 4 {
		return nil
	}

	timestamp := matches[1]
	typeStr := matches[2]
	statusStr := matches[3]
	remainder := matches[4]

	// Parse message type
	var msgType MessageType
	switch typeStr {
	case "STATUS":
		msgType = StatusMessage
	case "ERROR":
		msgType = ErrorMessage
	case "DATA":
		msgType = DataMessage
	case "PROGRESS":
		msgType = ProgressMessage
	case "RESULT":
		msgType = ResultMessage
	default:
		return nil
	}

	// Parse status code
	status, err := strconv.Atoi(statusStr)
	if err != nil {
		return nil
	}

	// Parse metadata and content
	metadata := make(map[string]string)
	content := ""

	if strings.Contains(remainder, " | ") {
		parts := strings.SplitN(remainder, " | ", 2)
		content = parts[1]
		remainder = parts[0]
	}

	// Parse metadata (key=value pairs)
	metaParts := strings.Fields(remainder)
	for _, part := range metaParts {
		if strings.Contains(part, "=") {
			kv := strings.SplitN(part, "=", 2)
			if len(kv) == 2 {
				metadata[kv[0]] = kv[1]
			}
		}
	}

	return &AgentMessage{
		Type:      msgType,
		Status:    StatusCode(status),
		Content:   content,
		Timestamp: timestamp,
		Metadata:  metadata,
	}
}
