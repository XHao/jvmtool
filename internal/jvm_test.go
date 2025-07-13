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

	time.Sleep(time.Second)
	pid := int32(jp.cmd.Process.Pid)
	jvmProc := JvmProcess{Pid: pid}
	err = jvmProc.checkSocket()
	assert.Nil(t, err)

	{
		agentPath, cleanup2, err := createSimpleJavaAgent()
		if err != nil {
			t.Fatalf("failed to create Java agent: %v", err)
		}
		defer cleanup2()
		err = jvmProc.loadAgent(agentPath, "")
		assert.Nil(t, err)
	}

	{
		agentPath, cleanup2, err := createNoAgentMainJavaAgent()
		if err != nil {
			t.Fatalf("failed to create Java agent: %v", err)
		}
		defer cleanup2()
		err = jvmProc.loadAgent(agentPath, "")
		assert.EqualError(t, err, "agent load failed, code 102: No agentmain method or agentmain failed")
	}

	{

		agentPath, cleanup2, err := createManifestJavaAgent()
		if err != nil {
			t.Fatalf("failed to create Java agent: %v", err)
		}
		defer cleanup2()
		err = jvmProc.loadAgent(agentPath, "")
		assert.EqualError(t, err, "agent load failed, code 100: Agent JAR not found or no Agent-Class attribute")
	}

	cleanup()

	{
		agentPath, cleanup2, err := createSimpleJavaAgent()
		if err != nil {
			t.Fatalf("failed to create Java agent: %v", err)
		}
		defer cleanup2()
		err = jvmProc.loadAgent(agentPath, "")
		assert.NotNil(t, err)
	}
}
