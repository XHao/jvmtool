package pkg

import (
	"bytes"
	"debug/elf"
	"debug/macho"
	"debug/pe"
	"encoding/binary"
	"fmt"
	"hash/crc32"
	"runtime"
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
	switch runtime.GOOS {
	case "darwin":
		return extractMetadataMachO(libPath)
	case "linux":
		return extractMetadataELF(libPath)
	case "windows":
		return extractMetadataPE(libPath)
	default:
		return nil, fmt.Errorf("pattern-based metadata extraction not implemented yet")
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

// extractMetadataPE extracts metadata from PE binaries (Windows)
func extractMetadataPE(libPath string) (*AgentMetadata, error) {
	file, err := pe.Open(libPath)
	if err != nil {
		return nil, fmt.Errorf("failed to open PE file: %v", err)
	}
	defer file.Close()

	sectionNames := []string{".jvmtool", ".data", ".rdata"}

	for _, sectionName := range sectionNames {
		section := file.Section(sectionName)
		if section != nil {
			data, err := section.Data()
			if err != nil {
				continue // Try next section
			}

			if metadata, err := parseMetadataFromBytes(data); err == nil {
				return metadata, nil
			}
		}
	}

	return nil, fmt.Errorf("jvmtool metadata not found in PE file sections")
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
