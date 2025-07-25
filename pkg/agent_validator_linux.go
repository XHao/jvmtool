//go:build linux

package pkg

import (
	"debug/elf"
	"fmt"
)

// platformSpecificExtractMetadata implements the platform-specific metadata extraction for Linux
func platformSpecificExtractMetadata(libPath string) (*AgentMetadata, error) {
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
