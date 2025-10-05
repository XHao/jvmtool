package internal

import (
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
			args: []string{"-user", "testuser", "-pid", "1234"},
			expected: SAAgentOption{
				User:     "testuser",
				Pid:      "1234",
				Module:   metaspaceProvider{},
				Task:     "metaspace",
				Duration: 30,
				Output:   "",
			},
			wantErr: false,
		},
		{
			name: "all flags provided",
			args: []string{"-user", "testuser", "-pid", "1234", "-module", "meta", "-task", "metaspace", "-duration", "60", "-output", "/tmp/test.log"},
			expected: SAAgentOption{
				User:     "testuser",
				Pid:      "1234",
				Module:   metaspaceProvider{},
				Task:     "metaspace",
				Duration: 60,
				Output:   "/tmp/test.log",
			},
			wantErr: false,
		},
		{
			name: "analysis preset",
			args: []string{"-user", "testuser", "-pid", "1234", "-analysis", "metaspace"},
			expected: SAAgentOption{
				User:     "testuser",
				Pid:      "1234",
				Module:   metaspaceProvider{},
				Task:     "metaspace",
				Duration: 30,
				Output:   "",
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

func TestResolveModuleAndTask(t *testing.T) {
	tests := []struct {
		name      string
		module    string
		task      string
		preset    string
		wantMod   string
		wantTask  string
		wantError bool
	}{
		{
			name:     "defaults",
			module:   "",
			task:     "",
			preset:   "",
			wantMod:  "meta",
			wantTask: "metaspace",
		},
		{
			name:     "explicit module and task",
			module:   "meta",
			task:     "metaspace",
			preset:   "",
			wantMod:  "meta",
			wantTask: "metaspace",
		},
		{
			name:     "module alias memory",
			module:   "memory",
			task:     "metaspace",
			preset:   "",
			wantMod:  "meta",
			wantTask: "metaspace",
		},
		{
			name:     "preset overrides",
			module:   "ignored",
			task:     "ignored",
			preset:   "metaspace",
			wantMod:  "meta",
			wantTask: "metaspace",
		},
		{
			name:     "preset memory alias",
			module:   "ignored",
			task:     "ignored",
			preset:   "memory",
			wantMod:  "meta",
			wantTask: "metaspace",
		},
		{
			name:      "unsupported preset",
			preset:    "thread",
			wantError: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			provider, task, err := resolveModuleAndTask(tt.module, tt.task)
			if tt.wantError {
				assert.Error(t, err)
				return
			}
			assert.NoError(t, err)
			require.NotNil(t, provider)
			assert.Equal(t, tt.wantMod, provider.Name())
			assert.Equal(t, tt.wantTask, task)
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
		default:
			expectedExt = ".so"
		}

		assert.True(t, strings.HasSuffix(agentPath, expectedExt))
		assert.Contains(t, agentPath, "jvmtool-agent")
	}
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
				Module:   metaspaceProvider{},
				Task:     "metaspace",
				Duration: 30,
			},
			expected: 1, // Should return error code
		},
		{
			name: "empty pid",
			option: SAAgentOption{
				User:     "testuser",
				Pid:      "",
				Module:   metaspaceProvider{},
				Task:     "metaspace",
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
