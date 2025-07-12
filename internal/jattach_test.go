package internal

import (
	"os"
	"os/user"
	"strconv"
	"testing"
)

// TestParseJattachFlags tests the ParseJattachFlags function.
func TestParseJattachFlags(t *testing.T) {
	args := []string{
		"-user", "testuser",
		"-pid", "12345",
		"-agentpath", "/tmp/agent.jar",
		"-agentparams", "foo=bar",
	}
	opt, err := ParseJattachFlags(args)
	if err != nil {
		t.Fatalf("ParseJattachFlags failed: %v", err)
	}
	if opt.User != "testuser" {
		t.Errorf("expected user 'testuser', got '%s'", opt.User)
	}
	if opt.Pid != "12345" {
		t.Errorf("expected pid '12345', got '%s'", opt.Pid)
	}
	if opt.AgentPath != "/tmp/agent.jar" {
		t.Errorf("expected agentpath '/tmp/agent.jar', got '%s'", opt.AgentPath)
	}
	if opt.AgentParams != "foo=bar" {
		t.Errorf("expected agentparams 'foo=bar', got '%s'", opt.AgentParams)
	}
}

// TestJattachValidate tests the JattachValidate method of JattachOption.
func TestJattachValidate(t *testing.T) {
	u, _ := user.Current()
	pid := os.Getpid()

	tests := []struct {
		name     string
		option   JattachOption
		expected string
	}{
		{
			name: "pid err",
			option: JattachOption{
				User:      u.Username,
				Pid:       strconv.Itoa(pid),
				AgentPath: "/tmp/agent.jar",
			},
			expected: "pid does not belong to the specified user",
		},
		{
			name: "missing pid",
			option: JattachOption{
				User:      u.Username,
				Pid:       "",
				AgentPath: "/tmp/agent.jar",
			},
			expected: "pid is required",
		},
		{
			name: "missing agentpath",
			option: JattachOption{
				User:      u.Username,
				Pid:       "12345",
				AgentPath: "",
			},
			expected: "agentpath is required",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := tt.option.JattachValidate()
			if tt.expected == "" && err != nil {
				t.Errorf("expected no error, got: %v", err)
			} else if tt.expected != "" && (err == nil || err.Error() != tt.expected) {
				t.Errorf("expected error '%s', got: %v", tt.expected, err)
			}
		})
	}
}
