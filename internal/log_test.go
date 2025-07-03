package internal

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// TestNewLogger_DefaultOutput tests the default output function of Logger.
func TestNewLogger_DefaultOutput(t *testing.T) {
	logger := NewLogger(nil)
	if logger == nil {
		t.Fatal("Expected non-nil logger")
	}
}

// TestLogger_Print_CustomOutput tests Logger.Print with a custom output function.
func TestLogger_Print_CustomOutput(t *testing.T) {
	var output strings.Builder
	logger := NewLogger(func(msg string) {
		output.WriteString(msg)
	})
	testMsg := "hello log"
	logger.Print(testMsg)
	if output.String() != testMsg {
		t.Errorf("Expected '%s', got '%s'", testMsg, output.String())
	}
}

// TestFileOutputFunc_WritesToFile tests that FileOutputFunc writes log messages to a file.
func TestFileOutputFunc_WritesToFile(t *testing.T) {
	dir := t.TempDir()
	logFile := filepath.Join(dir, "test.log")
	outputFunc := FileOutputFunc(logFile)
	testMsg := "file log message"
	outputFunc(testMsg)

	data, err := os.ReadFile(logFile)
	if err != nil {
		t.Fatalf("Failed to read log file: %v", err)
	}
	content := string(data)
	if !strings.Contains(content, testMsg) {
		t.Errorf("Expected log file to contain '%s', got '%s'", testMsg, content)
	}
}

// TestFileOutputFunc_OverwriteFile tests that FileOutputFunc overwrites the file on each call.
func TestFileOutputFunc_OverwriteFile(t *testing.T) {
	dir := t.TempDir()
	logFile := filepath.Join(dir, "overwrite.log")
	outputFunc := FileOutputFunc(logFile)
	msg1 := "first"
	msg2 := "second"
	outputFunc(msg1)
	outputFunc(msg2)

	data, err := os.ReadFile(logFile)
	if err != nil {
		t.Fatalf("Failed to read log file: %v", err)
	}
	content := string(data)
	// Since FileOutputFunc overwrites the file, only the last message should be present.
	if strings.Contains(content, msg1) {
		t.Errorf("Expected log file to not contain the first message, got '%s'", content)
	}
	if !strings.Contains(content, msg2) {
		t.Errorf("Expected log file to contain only the second message, got '%s'", content)
	}
}

// TestOpenLogFile_CreatesFile tests that openLogFile creates a new file if it does not exist.
func TestOpenLogFile_CreatesFile(t *testing.T) {
	dir := t.TempDir()
	logFile := filepath.Join(dir, "newfile.log")
	f, err := openLogFile(logFile)
	if err != nil {
		t.Fatalf("openLogFile failed: %v", err)
	}
	defer f.Close()
	info, err := os.Stat(logFile)
	if err != nil {
		t.Fatalf("Expected file to exist: %v", err)
	}
	if info.IsDir() {
		t.Errorf("Expected a file, got a directory")
	}
}
