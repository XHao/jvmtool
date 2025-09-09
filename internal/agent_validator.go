package internal

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"unsafe"
)

// AgentMetadata represents the metadata structure embedded in the agent library
type AgentMetadata struct {
	Magic [16]byte // "JVMTOOLLOOTMVJ\0\0"
}

// ExpectedMagic is the expected magic word in agent metadata
const ExpectedMagic = "JVMTOOLLOOTMVJ\x00\x00"

// ValidateAgentLibrary validates the agent library by checking the magic word
func ValidateAgentLibrary(libPath string) error {
	data, err := extractMetadataBytes(libPath)
	if err != nil {
		return fmt.Errorf("failed to extract agent metadata: %v", err)
	}

	return validateMagicWord(data)
}

// validateMagicWord checks if the data contains the expected magic word
func validateMagicWord(data []byte) error {
	if len(data) < int(unsafe.Sizeof(AgentMetadata{})) {
		return fmt.Errorf("section data too small: got %d bytes, need at least %d",
			len(data), int(unsafe.Sizeof(AgentMetadata{})))
	}

	// Parse only the magic field from bytes
	reader := bytes.NewReader(data)
	metadata := &AgentMetadata{}

	if err := binary.Read(reader, binary.LittleEndian, metadata); err != nil {
		return fmt.Errorf("failed to parse metadata structure: %v", err)
	}

	// Verify magic signature
	if !bytes.Equal(metadata.Magic[:], []byte(ExpectedMagic)) {
		return fmt.Errorf("invalid magic signature: expected %q, got %q",
			ExpectedMagic, string(metadata.Magic[:]))
	}

	return nil
}
