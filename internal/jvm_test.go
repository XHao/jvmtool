package internal

import (
	"testing"
	"time"

	"github.com/XHao/jvmtool/internal/testutil"
	"github.com/stretchr/testify/assert"
)

func TestLoadAgent(t *testing.T) {
	// Start a Java process using the testutil helper
	jp, cleanup, err := testutil.StartJavaProcess()
	if err != nil {
		t.Fatalf("failed to start Java process: %v", err)
	}
	defer cleanup()
	time.Sleep(time.Second)
	pid := int32(jp.Cmd.Process.Pid)
	jvmProc := JvmProcess{Pid: pid}
	err = jvmProc.checkSocket()
	assert.Nil(t, err)

	agentPath, cleanup2, err := testutil.CreateSimpleJavaAgent()
	if err != nil {
		t.Fatalf("failed to create Java agent: %v", err)
	}
	defer cleanup2()
	err = jvmProc.loadAgent(agentPath, "")
	assert.Nil(t, err)
}
