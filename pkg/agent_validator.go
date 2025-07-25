package pkg

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"hash/crc32"
	"unsafe"
)

// AgentMetadata represents the metadata structure embedded in the agent library
type AgentMetadata struct {
	Magic    [16]byte // "JVMTOOLLOOTMVJ\0\0"
	Checksum uint32   // Simple checksum of above fields
}

// Expected agent library metadata - these will be set during build
const (
	AgentVersion = "{{JVMTOOL_VERSION}}"
	AgentSalt    = "{{JVMTOOL_SALT}}"
	AgentBuild   = "{{JVMTOOL_BUILD}}"
)

// MetadataExtractor defines the interface for extracting metadata from agent libraries
type MetadataExtractor interface {
	ExtractMetadata(libPath string) (*AgentMetadata, error)
}

// AgentValidator handles agent library validation with configurable extractor
type AgentValidator struct {
	extractor MetadataExtractor
}

// NewDefaultAgentValidator creates a validator with the real metadata extractor
func NewDefaultAgentValidator() *AgentValidator {
	return &AgentValidator{extractor: &RealMetadataExtractor{}}
}

// ValidateLibrary validates the agent library against expected build information
func (av *AgentValidator) ValidateLibrary(libPath string) error {
	// Extract metadata from the library
	metadata, err := av.extractor.ExtractMetadata(libPath)
	if err != nil {
		return fmt.Errorf("failed to extract agent metadata: %v", err)
	}

	expectedChecksum := calculateChecksum(AgentVersion, AgentSalt, AgentBuild)

	// Verify checksum matches
	if metadata.Checksum != expectedChecksum {
		return fmt.Errorf("checksum mismatch")
	}

	return nil
}

// calculateChecksum computes a simple checksum for the given metadata fields
func calculateChecksum(version, salt, buildTime string) uint32 {
	combined := version + "|" + salt + "|" + buildTime
	return crc32.ChecksumIEEE([]byte(combined))
}

// RealMetadataExtractor implements MetadataExtractor using actual binary parsing
type RealMetadataExtractor struct{}

func (r *RealMetadataExtractor) ExtractMetadata(libPath string) (*AgentMetadata, error) {
	return platformSpecificExtractMetadata(libPath)
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
