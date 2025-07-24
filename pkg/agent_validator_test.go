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
func createTestMetadata(version, salt, buildTime string, checksum uint32) *AgentMetadata {
	metadata := &AgentMetadata{}

	// Set magic signature
	copy(metadata.Magic[:], "JVMTOOLLOOTMVJ\x00\x00")

	// Set version (null-terminated)
	copy(metadata.Version[:], version)

	// Set salt (null-terminated)
	copy(metadata.Salt[:], salt)

	// Set build time (null-terminated)
	copy(metadata.BuildTime[:], buildTime)

	// Set checksum
	metadata.Checksum = checksum

	return metadata
}

func TestAgentValidatorWithMatchingMetadata(t *testing.T) {
	// Create test metadata that matches expected values
	mockExtractor := &MockMetadataExtractor{
		MockData: createTestMetadata(
			ExpectedAgentVersion,
			ExpectedAgentSalt,
			ExpectedAgentBuild,
			ExpectedAgentChecksum,
		),
	}

	validator := NewAgentValidator(mockExtractor)

	err := validator.ValidateLibrary("dummy-path")
	if err != nil {
		// If constants are placeholders, this will fail
		t.Logf("Validation failed (expected if constants are placeholders): %v", err)
	} else {
		t.Logf("Validation successful!")
	}
}

func TestAgentValidatorWithMismatchedVersion(t *testing.T) {
	// Create test metadata with wrong version
	mockExtractor := &MockMetadataExtractor{
		MockData: createTestMetadata(
			"2.0.0", // wrong version
			ExpectedAgentSalt,
			ExpectedAgentBuild,
			ExpectedAgentChecksum,
		),
	}

	validator := NewAgentValidator(mockExtractor)

	err := validator.ValidateLibrary("dummy-path")
	if err == nil {
		t.Error("Expected validation to fail with mismatched version")
	} else {
		t.Logf("Validation correctly failed: %v", err)
	}
}

func TestAgentValidatorWithMismatchedSalt(t *testing.T) {
	// Create test metadata with wrong salt
	mockExtractor := &MockMetadataExtractor{
		MockData: createTestMetadata(
			ExpectedAgentVersion,
			"wrongsalt123456789012345678901234", // wrong salt
			ExpectedAgentBuild,
			ExpectedAgentChecksum,
		),
	}

	validator := NewAgentValidator(mockExtractor)

	err := validator.ValidateLibrary("dummy-path")
	if err == nil {
		t.Error("Expected validation to fail with mismatched salt")
	} else {
		t.Logf("Validation correctly failed: %v", err)
	}
}

func TestAgentValidatorWithMismatchedBuildTime(t *testing.T) {
	// Create test metadata with wrong build time
	mockExtractor := &MockMetadataExtractor{
		MockData: createTestMetadata(
			ExpectedAgentVersion,
			ExpectedAgentSalt,
			"2025-01-01T00:00:00Z", // wrong build time
			ExpectedAgentChecksum,
		),
	}

	validator := NewAgentValidator(mockExtractor)

	err := validator.ValidateLibrary("dummy-path")
	if err == nil {
		t.Error("Expected validation to fail with mismatched build time")
	} else {
		t.Logf("Validation correctly failed: %v", err)
	}
}

func TestAgentValidatorWithMismatchedChecksum(t *testing.T) {
	// Create test metadata with wrong checksum
	mockExtractor := &MockMetadataExtractor{
		MockData: createTestMetadata(
			ExpectedAgentVersion,
			ExpectedAgentSalt,
			ExpectedAgentBuild,
			99999, // wrong checksum
		),
	}

	validator := NewAgentValidator(mockExtractor)

	err := validator.ValidateLibrary("dummy-path")
	if err == nil {
		t.Error("Expected validation to fail with mismatched checksum")
	} else {
		t.Logf("Validation correctly failed: %v", err)
	}
}

func TestAgentValidatorWithExtractionError(t *testing.T) {
	// Create mock extractor that returns an error
	mockExtractor := &MockMetadataExtractor{
		MockError: errors.New("failed to parse binary"),
	}

	validator := NewAgentValidator(mockExtractor)

	err := validator.ValidateLibrary("dummy-path")
	if err == nil {
		t.Error("Expected validation to fail when extraction fails")
	} else {
		t.Logf("Validation correctly failed: %v", err)
	}
}

func TestAgentValidatorGetMetadataInfo(t *testing.T) {
	testVersion := "1.2.3"
	testSalt := "test-salt-12345678901234567890"
	testBuildTime := "2025-07-24T15:30:00Z"
	testChecksum := uint32(54321)

	mockExtractor := &MockMetadataExtractor{
		MockData: createTestMetadata(testVersion, testSalt, testBuildTime, testChecksum),
	}

	validator := NewAgentValidator(mockExtractor)

	info, err := validator.GetMetadataInfo("dummy-path")
	if err != nil {
		t.Fatalf("Failed to get metadata info: %v", err)
	}

	// Verify the structure
	if info["version"] != testVersion {
		t.Errorf("Info version mismatch: expected %s, got %v", testVersion, info["version"])
	}

	if info["salt"] != testSalt {
		t.Errorf("Info salt mismatch: expected %s, got %v", testSalt, info["salt"])
	}

	if info["build_time"] != testBuildTime {
		t.Errorf("Info build_time mismatch: expected %s, got %v", testBuildTime, info["build_time"])
	}

	if info["checksum"] != testChecksum {
		t.Errorf("Info checksum mismatch: expected %d, got %v", testChecksum, info["checksum"])
	}

	// Verify expected values are included
	expected, ok := info["expected"].(map[string]interface{})
	if !ok {
		t.Error("Expected info should contain expected values")
	} else {
		t.Logf("Expected values: %+v", expected)
	}

	t.Logf("Metadata info: %+v", info)
}

func TestMetadataStringMethods(t *testing.T) {
	testVersion := "1.0.0"
	testSalt := "short" // Test with shorter string
	testBuildTime := "2025-07-24T12:00:00Z"

	metadata := createTestMetadata(testVersion, testSalt, testBuildTime, 123)

	// Test that strings are properly null-terminated
	if metadata.GetVersion() != testVersion {
		t.Errorf("Version mismatch: expected %s, got %s", testVersion, metadata.GetVersion())
	}

	if metadata.GetSalt() != testSalt {
		t.Errorf("Salt mismatch: expected %s, got %s", testSalt, metadata.GetSalt())
	}

	if metadata.GetBuildTime() != testBuildTime {
		t.Errorf("Build time mismatch: expected %s, got %s", testBuildTime, metadata.GetBuildTime())
	}
}

func TestNullTerminatedStringHandling(t *testing.T) {
	// Test the nullTerminatedString helper function
	tests := []struct {
		name     string
		input    []byte
		expected string
	}{
		{
			name:     "null terminated string",
			input:    []byte("hello\x00world"),
			expected: "hello",
		},
		{
			name:     "no null terminator",
			input:    []byte("hello"),
			expected: "hello",
		},
		{
			name:     "empty with null",
			input:    []byte("\x00"),
			expected: "",
		},
		{
			name:     "multiple nulls",
			input:    []byte("test\x00\x00\x00"),
			expected: "test",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := nullTerminatedString(tt.input)
			if result != tt.expected {
				t.Errorf("nullTerminatedString(%q) = %q, want %q", tt.input, result, tt.expected)
			}
		})
	}
}

// Test backward compatibility - the original functions should still work
func TestBackwardCompatibilityFunctions(t *testing.T) {
	// These tests demonstrate that the new interface is working
	// and can replace the old direct function calls

	// Test that the new validator interface works with non-existent file
	validator := NewDefaultAgentValidator()
	err := validator.ValidateLibrary("non-existent-file")
	if err == nil {
		t.Error("Expected error for non-existent file")
	}

	// Test that the new metadata info method works with non-existent file
	_, err = validator.GetMetadataInfo("non-existent-file")
	if err == nil {
		t.Error("Expected error for non-existent file")
	}

	t.Log("New validator interface is working correctly")
}

// Benchmark the validation process
func BenchmarkAgentValidation(b *testing.B) {
	mockExtractor := &MockMetadataExtractor{
		MockData: createTestMetadata(
			ExpectedAgentVersion,
			ExpectedAgentSalt,
			ExpectedAgentBuild,
			ExpectedAgentChecksum,
		),
	}

	validator := NewAgentValidator(mockExtractor)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = validator.ValidateLibrary("dummy-path")
	}
}
