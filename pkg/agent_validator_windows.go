//go:build windows

package pkg

import (
	"debug/pe"
	"fmt"
)

// platformSpecificExtractMetadata implements the platform-specific metadata extraction for Windows
func platformSpecificExtractMetadata(libPath string) (*AgentMetadata, error) {
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
