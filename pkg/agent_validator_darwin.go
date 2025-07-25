//go:build darwin

package pkg

import (
	"debug/macho"
	"fmt"
)

// platformSpecificExtractMetadata implements the platform-specific metadata extraction for Darwin
func platformSpecificExtractMetadata(libPath string) (*AgentMetadata, error) {
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
