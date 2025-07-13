package internal

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

func TestLoadAgent(t *testing.T) {
	jp, cleanup, err := startJavaProcess()
	if err != nil {
		t.Fatalf("failed to start Java process: %v", err)
	}
	defer cleanup()
	time.Sleep(time.Second)
	pid := int32(jp.cmd.Process.Pid)
	jvmProc := JvmProcess{Pid: pid}
	err = jvmProc.checkSocket()
	assert.Nil(t, err)

	agentPath, cleanup2, err := createSimpleJavaAgent()
	if err != nil {
		t.Fatalf("failed to create Java agent: %v", err)
	}
	defer cleanup2()
	err = jvmProc.loadAgent(agentPath, "")
	assert.Nil(t, err)
}
