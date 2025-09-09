//go:build !darwin && !linux

package internal

import (
	"fmt"
)

// extractMetadataBytes implements a fallback for unsupported platforms
func extractMetadataBytes(libPath string) ([]byte, error) {
	return nil, fmt.Errorf("metadata extraction not implemented for this platform")
}
