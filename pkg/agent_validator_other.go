//go:build !darwin && !linux && !windows

package pkg

import (
	"fmt"
)

// platformSpecificExtractMetadata implements a fallback for unsupported platforms
func platformSpecificExtractMetadata(libPath string) (*AgentMetadata, error) {
	return nil, fmt.Errorf("metadata extraction not implemented for this platform")
}
