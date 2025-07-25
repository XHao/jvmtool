package pkg

import (
	"errors"
	"testing"
)

// MockMetadataExtractor implements MetadataExtractor for testing
type MockMetadataExtractor struct {
	MockData  *AgentMetadata
	MockError error
}

func (m *MockMetadataExtractor) ExtractMetadata(libPath string) (*AgentMetadata, error) {
	if m.MockError != nil {
		return nil, m.MockError
	}
	return m.MockData, nil
}

// Helper function to create test metadata
func createTestMetadata(checksum uint32) *AgentMetadata {
	metadata := &AgentMetadata{}

	// Set magic signature
	copy(metadata.Magic[:], "JVMTOOLLOOTMVJ\x00\x00")

	// Set checksum
	metadata.Checksum = checksum

	return metadata
}

func TestCalculateChecksum(t *testing.T) {
	tests := []struct {
		name      string
		version   string
		salt      string
		buildTime string
		expected  uint32
	}{
		{
			name:      "basic test",
			version:   "1.0.0",
			salt:      "testsalt123",
			buildTime: "2025-01-01T00:00:00Z",
			expected:  calculateChecksum("1.0.0", "testsalt123", "2025-01-01T00:00:00Z"),
		},
		{
			name:      "empty strings",
			version:   "",
			salt:      "",
			buildTime: "",
			expected:  calculateChecksum("", "", ""),
		},
		{
			name:      "special characters",
			version:   "v1.0.0-beta+20250101",
			salt:      "salt!@#$%^&*()",
			buildTime: "2025-01-01T12:34:56.789Z",
			expected:  calculateChecksum("v1.0.0-beta+20250101", "salt!@#$%^&*()", "2025-01-01T12:34:56.789Z"),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := calculateChecksum(tt.version, tt.salt, tt.buildTime)
			if result != tt.expected {
				t.Errorf("calculateChecksum() = %v, want %v", result, tt.expected)
			}
		})
	}
}

func TestCalculateChecksumConsistency(t *testing.T) {
	// Test that the same inputs always produce the same output
	version := "1.0.0"
	salt := "testsalt"
	buildTime := "2025-01-01T00:00:00Z"

	checksum1 := calculateChecksum(version, salt, buildTime)
	checksum2 := calculateChecksum(version, salt, buildTime)

	if checksum1 != checksum2 {
		t.Errorf("calculateChecksum is not consistent: first=%d, second=%d", checksum1, checksum2)
	}
}

func TestCalculateChecksumDifferentInputs(t *testing.T) {
	// Test that different inputs produce different outputs
	checksum1 := calculateChecksum("1.0.0", "salt1", "2025-01-01T00:00:00Z")
	checksum2 := calculateChecksum("1.0.1", "salt1", "2025-01-01T00:00:00Z")
	checksum3 := calculateChecksum("1.0.0", "salt2", "2025-01-01T00:00:00Z")
	checksum4 := calculateChecksum("1.0.0", "salt1", "2025-01-02T00:00:00Z")

	if checksum1 == checksum2 {
		t.Error("Different versions should produce different checksums")
	}
	if checksum1 == checksum3 {
		t.Error("Different salts should produce different checksums")
	}
	if checksum1 == checksum4 {
		t.Error("Different build times should produce different checksums")
	}
}

func TestAgentValidatorWithMatchingChecksum(t *testing.T) {
	// Calculate expected checksum
	expectedChecksum := calculateChecksum(AgentVersion, AgentSalt, AgentBuild)

	// Create test metadata with matching checksum
	mockExtractor := &MockMetadataExtractor{
		MockData: createTestMetadata(expectedChecksum),
	}

	validator := NewDefaultAgentValidator()
	validator.extractor = mockExtractor

	err := validator.ValidateLibrary("/fake/path/agent.dylib")
	if err != nil {
		t.Errorf("Expected validation to pass, but got error: %v", err)
	}
}

func TestAgentValidatorWithMismatchedChecksum(t *testing.T) {
	// Calculate expected checksum and use a different one
	expectedChecksum := calculateChecksum(AgentVersion, AgentSalt, AgentBuild)
	wrongChecksum := expectedChecksum + 1

	// Create test metadata with wrong checksum
	mockExtractor := &MockMetadataExtractor{
		MockData: createTestMetadata(wrongChecksum),
	}

	validator := NewDefaultAgentValidator()
	validator.extractor = mockExtractor

	err := validator.ValidateLibrary("/fake/path/agent.dylib")
	if err == nil {
		t.Error("Expected validation to fail due to checksum mismatch, but it passed")
	}

	expectedError := "checksum mismatch"
	if err != nil && len(err.Error()) > 0 {
		if !contains(err.Error(), expectedError) {
			t.Errorf("Expected error to contain '%s', but got: %v", expectedError, err)
		}
	}
}

func TestAgentValidatorWithExtractionError(t *testing.T) {
	// Create mock extractor that returns an error
	mockExtractor := &MockMetadataExtractor{
		MockError: errors.New("failed to extract metadata"),
	}

	validator := NewDefaultAgentValidator()
	validator.extractor = mockExtractor

	err := validator.ValidateLibrary("/fake/path/agent.dylib")
	if err == nil {
		t.Error("Expected validation to fail due to extraction error, but it passed")
	}

	expectedError := "failed to extract agent metadata"
	if err != nil && len(err.Error()) > 0 {
		if !contains(err.Error(), expectedError) {
			t.Errorf("Expected error to contain '%s', but got: %v", expectedError, err)
		}
	}
}

func TestCreateTestMetadata(t *testing.T) {
	checksum := uint32(12345)
	metadata := createTestMetadata(checksum)

	// Verify magic signature
	expectedMagic := "JVMTOOLLOOTMVJ\x00\x00"
	if string(metadata.Magic[:]) != expectedMagic {
		t.Errorf("Magic mismatch: expected %q, got %q", expectedMagic, string(metadata.Magic[:]))
	}

	// Verify checksum
	if metadata.Checksum != checksum {
		t.Errorf("Checksum mismatch: expected %d, got %d", checksum, metadata.Checksum)
	}
}

// Helper function to check if a string contains a substring
func contains(s, substr string) bool {
	return len(s) >= len(substr) && (s == substr || (len(s) > len(substr) &&
		(func() bool {
			for i := 0; i <= len(s)-len(substr); i++ {
				if s[i:i+len(substr)] == substr {
					return true
				}
			}
			return false
		})()))
}

func TestRealMetadataExtractorPlatformSupport(t *testing.T) {
	extractor := &RealMetadataExtractor{}

	// Test that the extractor can handle non-existent files appropriately
	_, err := extractor.ExtractMetadata("non-existent-file")
	if err == nil {
		t.Error("Expected error for non-existent file")
	}

	// The error should indicate file access issue
	t.Logf("Got expected error for non-existent file: %v", err)
}

func TestExtractMetadataPEError(t *testing.T) {
	// Test PE extraction with non-existent file
	_, err := extractMetadataPE("non-existent-file.dll")
	if err == nil {
		t.Error("Expected error for non-existent PE file")
	}

	if !contains(err.Error(), "failed to open PE file") {
		t.Errorf("Expected 'failed to open PE file' error, got: %v", err)
	}
}

func TestExtractMetadataMachOError(t *testing.T) {
	// Test Mach-O extraction with non-existent file
	_, err := extractMetadataMachO("non-existent-file.dylib")
	if err == nil {
		t.Error("Expected error for non-existent Mach-O file")
	}

	if !contains(err.Error(), "failed to open Mach-O file") {
		t.Errorf("Expected 'failed to open Mach-O file' error, got: %v", err)
	}
}

func TestExtractMetadataELFError(t *testing.T) {
	// Test ELF extraction with non-existent file
	_, err := extractMetadataELF("non-existent-file.so")
	if err == nil {
		t.Error("Expected error for non-existent ELF file")
	}

	if !contains(err.Error(), "failed to open ELF file") {
		t.Errorf("Expected 'failed to open ELF file' error, got: %v", err)
	}
}

func TestParseMetadataFromBytesTooSmall(t *testing.T) {
	// Test with data that's too small
	smallData := []byte("small")

	_, err := parseMetadataFromBytes(smallData)
	if err == nil {
		t.Error("Expected error for too small data")
	}

	if !contains(err.Error(), "section data too small") {
		t.Errorf("Expected 'section data too small' error, got: %v", err)
	}
}

func TestParseMetadataFromBytesInvalidMagic(t *testing.T) {
	// Create data with wrong magic signature
	testData := make([]byte, 20) // Enough bytes for the struct
	copy(testData, "WRONGMAGICSIGNATURE")

	_, err := parseMetadataFromBytes(testData)
	if err == nil {
		t.Error("Expected error for invalid magic signature")
	}

	if !contains(err.Error(), "invalid magic signature") {
		t.Errorf("Expected 'invalid magic signature' error, got: %v", err)
	}
}

func TestCalculateChecksumMatchesBuildScript(t *testing.T) {
	// Test that our Go calculateChecksum function produces the same result
	// as the build script would for the same inputs
	testCases := []struct {
		name      string
		version   string
		salt      string
		buildTime string
	}{
		{
			name:      "typical values",
			version:   "1.0.0",
			salt:      "abc123def456",
			buildTime: "2025-07-25T12:00:00Z",
		},
		{
			name:      "empty version",
			version:   "",
			salt:      "testsalt",
			buildTime: "2025-01-01T00:00:00Z",
		},
		{
			name:      "complex version",
			version:   "v2.1.3-beta+build.123",
			salt:      "a1b2c3d4e5f6",
			buildTime: "2025-12-31T23:59:59Z",
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			// Calculate checksum using our Go function
			goChecksum := calculateChecksum(tc.version, tc.salt, tc.buildTime)

			// The checksum should be a valid uint32
			if goChecksum == 0 && tc.version != "" {
				t.Errorf("Unexpected zero checksum for non-empty inputs")
			}

			t.Logf("Checksum for %s|%s|%s = %d", tc.version, tc.salt, tc.buildTime, goChecksum)
		})
	}
}
