//go:build !unix

package pkg

import (
	"fmt"
)

func (jp *JvmProcess) CheckSocket() error {
	return fmt.Errorf("check socket not implemented for this platform")
}

func (jp *JvmProcess) LoadAgent(_ string, _ string) error {
	return fmt.Errorf("load agent not implemented for this platform")
}
