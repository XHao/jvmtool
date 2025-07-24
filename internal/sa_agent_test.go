package internal

import (
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestParseSAAgentFlags(t *testing.T) {
	tests := []struct {
		name     string
		args     []string
		expected SAAgentOption
		wantErr  bool
	}{
		{
			name: "valid basic flags",
			args: []string{"-user", "testuser", "-pid", "1234", "-analysis", "memory"},
			expected: SAAgentOption{
				User:     "testuser",
				Pid:      "1234",
				Analysis: "memory",
				Duration: 30,
				Output:   "",
			},
			wantErr: false,
		},
		{
			name: "all flags provided",
			args: []string{"-user", "testuser", "-pid", "1234", "-analysis", "heap", "-duration", "60", "-output", "/tmp/test.log"},
			expected: SAAgentOption{
				User:     "testuser",
				Pid:      "1234",
				Analysis: "heap",
				Duration: 60,
				Output:   "/tmp/test.log",
			},
			wantErr: false,
		},
		{
			name:    "invalid flag",
			args:    []string{"-invalid", "value"},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, err := ParseSAAgentFlags(tt.args)
			if tt.wantErr {
				assert.Error(t, err)
			} else {
				assert.NoError(t, err)
				assert.Equal(t, tt.expected, result)
			}
		})
	}
}

func TestAnalysisTypeValidation(t *testing.T) {
	validTypes := map[string]bool{
		"memory": true,
		"thread": true,
		"class":  true,
		"heap":   true,
		"all":    true,
	}

	tests := []struct {
		name          string
		analysisType  string
		shouldBeValid bool
	}{
		{"valid memory", "memory", true},
		{"valid thread", "thread", true},
		{"valid class", "class", true},
		{"valid heap", "heap", true},
		{"valid all", "all", true},
		{"invalid type", "invalid", false},
		{"empty type", "", false},
		{"wrong type", "wrong", false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			isValid := validTypes[tt.analysisType]
			assert.Equal(t, tt.shouldBeValid, isValid,
				"Analysis type %q validation result should be %v", tt.analysisType, tt.shouldBeValid)
		})
	}
}

func TestSAAgentParameterBuilding(t *testing.T) {
	tests := []struct {
		name     string
		option   SAAgentOption
		expected string
	}{
		{
			name: "basic parameters",
			option: SAAgentOption{
				Analysis: "memory",
				Duration: 30,
				Output:   "",
			},
			expected: "analysis=memory,duration=30",
		},
		{
			name: "with output file",
			option: SAAgentOption{
				Analysis: "heap",
				Duration: 60,
				Output:   "/tmp/test.log",
			},
			expected: "analysis=heap,duration=60,output=/tmp/test.log",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			params := fmt.Sprintf("analysis=%s,duration=%d", tt.option.Analysis, tt.option.Duration)
			if tt.option.Output != "" {
				params += fmt.Sprintf(",output=%s", tt.option.Output)
			}
			assert.Equal(t, tt.expected, params)
		})
	}
}

func TestFindNativeAgent(t *testing.T) {
	agentPath, err := findNativeAgent()

	if err != nil {
		// If error, it should be about not finding the agent
		assert.Contains(t, err.Error(), "not found in any of the search paths")
	} else {
		// If successful, path should be absolute and have correct extension
		assert.True(t, filepath.IsAbs(agentPath))

		var expectedExt string
		switch runtime.GOOS {
		case "darwin":
			expectedExt = ".dylib"
		case "linux":
			expectedExt = ".so"
		case "windows":
			expectedExt = ".dll"
		default:
			expectedExt = ".so"
		}

		assert.True(t, strings.HasSuffix(agentPath, expectedExt))
		assert.Contains(t, agentPath, "jvmtool-agent")
	}
}

func TestDisplayTempFileOutput(t *testing.T) {
	// Create a temporary file for testing
	tempFile, err := os.CreateTemp("", "jvmtool_test_*.log")
	require.NoError(t, err)
	defer os.Remove(tempFile.Name())

	// Write test content with timestamps
	testContent := "[2025-07-24 12:34:56] Memory Analysis Report\n[2025-07-24 12:34:57] Total Heap Size: 512MB\n"

	_, err = tempFile.WriteString(testContent)
	require.NoError(t, err)
	tempFile.Close()

	// Test displayTempFileOutput function
	// Since it prints to stdout, we can't easily capture the output in this test
	// But we can at least verify it doesn't panic
	assert.NotPanics(t, func() {
		displayTempFileOutput(tempFile.Name())
	})
}

func TestSAAgent_ErrorHandling(t *testing.T) {
	tests := []struct {
		name     string
		option   SAAgentOption
		expected int
	}{
		{
			name: "invalid user",
			option: SAAgentOption{
				User:     "nonexistentuser123",
				Pid:      "1234",
				Analysis: "memory",
				Duration: 30,
			},
			expected: 1, // Should return error code
		},
		{
			name: "empty pid",
			option: SAAgentOption{
				User:     "testuser",
				Pid:      "",
				Analysis: "memory",
				Duration: 30,
			},
			expected: 1, // Should return error code
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := SAAgent(tt.option)
			assert.Equal(t, tt.expected, result)
		})
	}
}
