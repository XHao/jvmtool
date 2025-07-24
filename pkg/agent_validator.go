package pkg

import (
	"bytes"
	"debug/elf"
	"debug/macho"
	"encoding/binary"
	"fmt"
	"runtime"
	"unsafe"
)

// AgentMetadata represents the metadata structure embedded in the agent library
type AgentMetadata struct {
	Magic     [16]byte // "JVMTOOLLOOTMVJ\0\0"
	Version   [32]byte // Version string
	Salt      [33]byte // Build-time salt (32 chars + null)
	BuildTime [32]byte // ISO 8601 timestamp
	Checksum  uint32   // Simple checksum of above fields
}

// Expected agent library metadata - these will be set during build
const (
	ExpectedAgentVersion  = "{{JVMTOOL_VERSION}}"
	ExpectedAgentSalt     = "{{JVMTOOL_SALT}}"
	ExpectedAgentBuild    = "{{JVMTOOL_BUILD}}"
	ExpectedAgentChecksum = 0 // {{JVMTOOL_CHECKSUM}}
)

// MetadataExtractor defines the interface for extracting metadata from agent libraries
type MetadataExtractor interface {
	ExtractMetadata(libPath string) (*AgentMetadata, error)
}

// AgentValidator handles agent library validation with configurable extractor
type AgentValidator struct {
	extractor MetadataExtractor
}

// NewAgentValidator creates a new validator with the given extractor
func NewAgentValidator(extractor MetadataExtractor) *AgentValidator {
	return &AgentValidator{extractor: extractor}
}

// NewDefaultAgentValidator creates a validator with the real metadata extractor
func NewDefaultAgentValidator() *AgentValidator {
	return &AgentValidator{extractor: &RealMetadataExtractor{}}
}

// validationRule defines a single validation rule
type validationRule struct {
	name     string
	actual   interface{}
	expected interface{}
}

// ValidateLibrary validates the agent library against expected build information
func (av *AgentValidator) ValidateLibrary(libPath string) error {
	// Extract metadata from the library
	metadata, err := av.extractor.ExtractMetadata(libPath)
	if err != nil {
		return fmt.Errorf("failed to extract agent metadata: %v", err)
	}

	// Define validation rules
	rules := []validationRule{
		{"version", metadata.GetVersion(), ExpectedAgentVersion},
		{"salt", metadata.GetSalt(), ExpectedAgentSalt},
		{"build time", metadata.GetBuildTime(), ExpectedAgentBuild},
		{"checksum", metadata.Checksum, ExpectedAgentChecksum},
	}

	// Validate all rules
	for _, rule := range rules {
		if rule.actual != rule.expected {
			return fmt.Errorf("agent %s mismatch: expected %v, got %v",
				rule.name, rule.expected, rule.actual)
		}
	}

	return nil
}

// getExpectedValues returns the expected values map for metadata info
func getExpectedValues() map[string]interface{} {
	return map[string]interface{}{
		"version":    ExpectedAgentVersion,
		"salt":       ExpectedAgentSalt,
		"build_time": ExpectedAgentBuild,
		"checksum":   ExpectedAgentChecksum,
	}
}

// GetMetadataInfo returns metadata information for debugging
func (av *AgentValidator) GetMetadataInfo(libPath string) (map[string]interface{}, error) {
	metadata, err := av.extractor.ExtractMetadata(libPath)
	if err != nil {
		return nil, err
	}

	return map[string]interface{}{
		"version":    metadata.GetVersion(),
		"salt":       metadata.GetSalt(),
		"build_time": metadata.GetBuildTime(),
		"checksum":   metadata.Checksum,
		"expected":   getExpectedValues(),
	}, nil
}

// RealMetadataExtractor implements MetadataExtractor using actual binary parsing
type RealMetadataExtractor struct{}

func (r *RealMetadataExtractor) ExtractMetadata(libPath string) (*AgentMetadata, error) {
	switch runtime.GOOS {
	case "darwin":
		return extractMetadataMachO(libPath)
	case "linux":
		return extractMetadataELF(libPath)
	default:
		// Fallback to pattern search for unsupported platforms
		return extractMetadataByPattern(libPath)
	}
}

// extractMetadataMachO extracts metadata from Mach-O binaries (macOS)
func extractMetadataMachO(libPath string) (*AgentMetadata, error) {
	file, err := macho.Open(libPath)
	if err != nil {
		return nil, fmt.Errorf("failed to open Mach-O file: %v", err)
	}
	defer file.Close()

	// Look for our custom section in __DATA segment
	for _, section := range file.Sections {
		if section.Seg == "__DATA" && section.Name == "__jvmtool" {
			data, err := section.Data()
			if err != nil {
				return nil, fmt.Errorf("failed to read section data: %v", err)
			}
			return parseMetadataFromBytes(data)
		}
	}

	return nil, fmt.Errorf("jvmtool metadata section not found in Mach-O file")
}

// extractMetadataELF extracts metadata from ELF binaries (Linux)
func extractMetadataELF(libPath string) (*AgentMetadata, error) {
	file, err := elf.Open(libPath)
	if err != nil {
		return nil, fmt.Errorf("failed to open ELF file: %v", err)
	}
	defer file.Close()

	// Look for our custom section
	section := file.Section(".jvmtool_meta")
	if section == nil {
		return nil, fmt.Errorf("jvmtool metadata section not found in ELF file")
	}

	data, err := section.Data()
	if err != nil {
		return nil, fmt.Errorf("failed to read section data: %v", err)
	}

	return parseMetadataFromBytes(data)
}

// extractMetadataByPattern searches for metadata using pattern matching (fallback)
func extractMetadataByPattern(libPath string) (*AgentMetadata, error) {
	// This is a fallback method that searches for the magic bytes in the file
	// It's less reliable but works on platforms without specific binary format support

	// For now, return an error - this can be implemented if needed
	return nil, fmt.Errorf("pattern-based metadata extraction not implemented yet")
}

// parseMetadataFromBytes parses the AgentMetadata structure from raw bytes
func parseMetadataFromBytes(data []byte) (*AgentMetadata, error) {
	if len(data) < int(unsafe.Sizeof(AgentMetadata{})) {
		return nil, fmt.Errorf("section data too small: got %d bytes, need at least %d",
			len(data), unsafe.Sizeof(AgentMetadata{}))
	}

	// Parse the struct from bytes
	reader := bytes.NewReader(data)
	metadata := &AgentMetadata{}

	if err := binary.Read(reader, binary.LittleEndian, metadata); err != nil {
		return nil, fmt.Errorf("failed to parse metadata structure: %v", err)
	}

	// Verify magic signature
	expectedMagic := "JVMTOOLLOOTMVJ\x00\x00"
	if !bytes.Equal(metadata.Magic[:], []byte(expectedMagic)) {
		return nil, fmt.Errorf("invalid magic signature: expected %q, got %q",
			expectedMagic, string(metadata.Magic[:]))
	}

	return metadata, nil
}

// String methods for metadata fields
func (m *AgentMetadata) GetVersion() string {
	return nullTerminatedString(m.Version[:])
}

func (m *AgentMetadata) GetSalt() string {
	return nullTerminatedString(m.Salt[:])
}

func (m *AgentMetadata) GetBuildTime() string {
	return nullTerminatedString(m.BuildTime[:])
}

// nullTerminatedString converts a null-terminated byte array to a string
func nullTerminatedString(data []byte) string {
	if i := bytes.IndexByte(data, 0); i >= 0 {
		return string(data[:i])
	}
	return string(data)
}
