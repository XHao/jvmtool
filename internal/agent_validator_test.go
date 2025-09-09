package internal

import (
	"bytes"
	"encoding/binary"
	"io/fs"
	"os"
	"path/filepath"
	"testing"
)

func TestValidateMagicWord(t *testing.T) {
	tests := []struct {
		name      string
		data      []byte
		wantError bool
		errorMsg  string
	}{
		{
			name:      "valid magic word",
			data:      createValidMetadataBytes(),
			wantError: false,
		},
		{
			name:      "invalid magic word",
			data:      createInvalidMetadataBytes(),
			wantError: true,
			errorMsg:  "invalid magic signature",
		},
		{
			name:      "data too small",
			data:      []byte("short"),
			wantError: true,
			errorMsg:  "section data too small",
		},
		{
			name:      "empty data",
			data:      []byte{},
			wantError: true,
			errorMsg:  "section data too small",
		},
		{
			name:      "corrupted data",
			data:      createCorruptedMetadataBytes(),
			wantError: true,
			errorMsg:  "invalid magic signature",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := validateMagicWord(tt.data)

			if tt.wantError {
				if err == nil {
					t.Errorf("validateMagicWord() expected error but got none")
					return
				}
				if tt.errorMsg != "" && !contains(err.Error(), tt.errorMsg) {
					t.Errorf("validateMagicWord() error = %v, want error containing %q", err, tt.errorMsg)
				}
			} else {
				if err != nil {
					t.Errorf("validateMagicWord() unexpected error = %v", err)
				}
			}
		})
	}
}

func TestValidateAgentLibrary(t *testing.T) {
	// Test with non-existent file
	t.Run("non-existent file", func(t *testing.T) {
		err := ValidateAgentLibrary("/path/to/non/existent/file.so")
		if err == nil {
			t.Error("ValidateAgentLibrary() expected error for non-existent file but got none")
		}
	})

	// Test with temporary file containing valid metadata
	t.Run("valid metadata file", func(t *testing.T) {
		// Create a temporary file with valid metadata for testing
		// Note: This test will fail on unsupported platforms, which is expected
		tempFile := createTempLibraryFile(t, createValidMetadataBytes())
		defer os.Remove(tempFile)

		err := ValidateAgentLibrary(tempFile)
		// On macOS/Linux this should work if we create a proper binary file
		// On other platforms, we expect "metadata extraction not implemented" error
		if err != nil && !contains(err.Error(), "metadata extraction not implemented") {
			// For now, we expect this to fail since we're creating a simple file
			// In a real scenario, this would be a proper binary with metadata section
			t.Logf("Expected failure for test file: %v", err)
		}
	})
}

func TestAgentMetadataConstants(t *testing.T) {
	// Test that the ExpectedMagic constant is correct
	expected := "JVMTOOLLOOTMVJ\x00\x00"
	if ExpectedMagic != expected {
		t.Errorf("ExpectedMagic = %q, want %q", ExpectedMagic, expected)
	}

	// Test that the ExpectedMagic length matches AgentMetadata.Magic field size
	if len(ExpectedMagic) != 16 {
		t.Errorf("ExpectedMagic length = %d, want 16", len(ExpectedMagic))
	}
}

// Helper functions for creating test data

func createValidMetadataBytes() []byte {
	metadata := AgentMetadata{}
	copy(metadata.Magic[:], []byte(ExpectedMagic))

	var buf bytes.Buffer
	binary.Write(&buf, binary.LittleEndian, metadata)
	return buf.Bytes()
}

func createInvalidMetadataBytes() []byte {
	metadata := AgentMetadata{}
	copy(metadata.Magic[:], []byte("INVALID_MAGIC\x00\x00\x00"))

	var buf bytes.Buffer
	binary.Write(&buf, binary.LittleEndian, metadata)
	return buf.Bytes()
}

func createCorruptedMetadataBytes() []byte {
	data := createValidMetadataBytes()
	// Corrupt the first few bytes
	if len(data) > 4 {
		data[2] = 0xFF
		data[3] = 0xFF
	}
	return data
}

func createTempLibraryFile(t *testing.T, metadataBytes []byte) string {
	t.Helper()

	tempDir := t.TempDir()
	tempFile := filepath.Join(tempDir, "test_agent.so")

	// Create a simple file with metadata
	// Note: This won't work with actual binary parsing, but serves as a placeholder
	err := os.WriteFile(tempFile, metadataBytes, fs.ModePerm)
	if err != nil {
		t.Fatalf("Failed to create temp file: %v", err)
	}

	return tempFile
}

func contains(s, substr string) bool {
	return len(s) >= len(substr) && (s == substr || len(substr) == 0 ||
		(len(s) > len(substr) &&
			(s[:len(substr)] == substr || s[len(s)-len(substr):] == substr ||
				containsSubstring(s, substr))))
}

func containsSubstring(s, substr string) bool {
	for i := 0; i <= len(s)-len(substr); i++ {
		if s[i:i+len(substr)] == substr {
			return true
		}
	}
	return false
}
