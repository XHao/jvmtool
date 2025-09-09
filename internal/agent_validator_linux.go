//go:build linux

package internal

import (
	"debug/elf"
	"fmt"
)

// extractMetadataBytes extracts the metadata bytes from a Linux library
func extractMetadataBytes(libPath string) ([]byte, error) {
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

	return data, nil
}
