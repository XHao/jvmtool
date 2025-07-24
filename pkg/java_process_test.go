package pkg

import (
	"os"
	"path/filepath"
	"strconv"
	"testing"
)

func TestJavaProcessValidator_ValidateJavaProcess(t *testing.T) {
	// Get current user for testing
	currentUser := getCurrentUsername(t)
	currentPid := os.Getpid()

	// Create a fake hsperfdata file for testing
	hsperfdataDir := GetHsperfdataDir(currentUser)
	err := os.MkdirAll(hsperfdataDir, 0755)
	if err != nil {
		t.Fatalf("Failed to create hsperfdata dir: %v", err)
	}
	defer os.RemoveAll(hsperfdataDir)

	hsperfdataFile := filepath.Join(hsperfdataDir, strconv.Itoa(currentPid))
	file, err := os.Create(hsperfdataFile)
	if err != nil {
		t.Fatalf("Failed to create hsperfdata file: %v", err)
	}
	file.Close()

	tests := []struct {
		name    string
		user    string
		pid     string
		wantErr bool
	}{
		{
			name:    "valid process should pass",
			user:    currentUser,
			pid:     strconv.Itoa(currentPid),
			wantErr: false,
		},
		{
			name:    "empty pid should fail",
			user:    currentUser,
			pid:     "",
			wantErr: true,
		},
		{
			name:    "invalid pid should fail",
			user:    currentUser,
			pid:     "invalid",
			wantErr: true,
		},
		{
			name:    "nonexistent pid should fail",
			user:    currentUser,
			pid:     "999999",
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			validator := &JavaProcessValidator{
				User: tt.user,
				Pid:  tt.pid,
			}
			err := validator.ValidateJavaProcess()
			if tt.wantErr {
				if err == nil {
					t.Errorf("ValidateJavaProcess() expected error but got none")
				}
			} else {
				if err != nil {
					t.Errorf("ValidateJavaProcess() unexpected error: %v", err)
				}
			}
		})
	}
}

func TestGetHsperfdataPath(t *testing.T) {
	tests := []struct {
		name     string
		username string
		pid      string
		want     string
	}{
		{
			name:     "normal case",
			username: "testuser",
			pid:      "12345",
			want:     filepath.Join(os.TempDir(), "hsperfdata_testuser", "12345"),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := GetHsperfdataPath(tt.username, tt.pid); got != tt.want {
				t.Errorf("GetHsperfdataPath() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestGetHsperfdataDir(t *testing.T) {
	tests := []struct {
		name     string
		username string
		want     string
	}{
		{
			name:     "normal case",
			username: "testuser",
			want:     filepath.Join(os.TempDir(), "hsperfdata_testuser"),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := GetHsperfdataDir(tt.username); got != tt.want {
				t.Errorf("GetHsperfdataDir() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestDiscoverJavaProcesses(t *testing.T) {
	username := "testuser"
	hsperfdataDir := GetHsperfdataDir(username)

	// Clean up any existing directory
	os.RemoveAll(hsperfdataDir)

	t.Run("no hsperfdata directory", func(t *testing.T) {
		pids, err := DiscoverJavaProcesses(username)
		if err != nil {
			t.Errorf("DiscoverJavaProcesses() unexpected error: %v", err)
		}
		if len(pids) != 0 {
			t.Errorf("DiscoverJavaProcesses() expected 0 pids, got %d", len(pids))
		}
	})

	t.Run("with fake processes", func(t *testing.T) {
		// Create hsperfdata directory with fake process files
		err := os.MkdirAll(hsperfdataDir, 0755)
		if err != nil {
			t.Fatalf("Failed to create hsperfdata dir: %v", err)
		}
		defer os.RemoveAll(hsperfdataDir)

		// Create fake process files
		testPids := []string{"12345", "67890", "invalid", "999"}
		for _, pid := range testPids {
			file, err := os.Create(filepath.Join(hsperfdataDir, pid))
			if err != nil {
				t.Fatalf("Failed to create fake process file: %v", err)
			}
			file.Close()
		}

		pids, err := DiscoverJavaProcesses(username)
		if err != nil {
			t.Errorf("DiscoverJavaProcesses() unexpected error: %v", err)
		}

		// Should find 3 valid numeric PIDs (excluding "invalid")
		expectedPids := []int32{12345, 67890, 999}
		if len(pids) != len(expectedPids) {
			t.Errorf("DiscoverJavaProcesses() expected %d pids, got %d", len(expectedPids), len(pids))
		}
	})
}

func TestToInt32(t *testing.T) {
	tests := []struct {
		name    string
		s       string
		want    int32
		wantErr bool
	}{
		{
			name:    "valid number",
			s:       "12345",
			want:    12345,
			wantErr: false,
		},
		{
			name:    "zero",
			s:       "0",
			want:    0,
			wantErr: false,
		},
		{
			name:    "negative number",
			s:       "-123",
			want:    -123,
			wantErr: false,
		},
		{
			name:    "invalid string",
			s:       "invalid",
			want:    0,
			wantErr: true,
		},
		{
			name:    "empty string",
			s:       "",
			want:    0,
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := Pid(tt.s)
			if tt.wantErr {
				if got != -1 {
					t.Errorf("ToInt32() expected error but got none")
				}
			} else {
				if got == -1 {
					t.Errorf("ToInt32() unexpected error")
				}
				if got != tt.want {
					t.Errorf("ToInt32() = %v, want %v", got, tt.want)
				}
			}
		})
	}
}
