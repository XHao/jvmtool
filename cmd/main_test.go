package main

import (
	"testing"

	"github.com/XHao/jvmtool/internal"
)

func TestRun_Help(t *testing.T) {
	code := run([]string{"jvmtool", "help"})
	if code != 0 {
		t.Errorf("expected exit code 0, got %d", code)
	}
}

func TestRun_UnknownCommand(t *testing.T) {
	code := run([]string{"jvmtool", "unknown"})
	if code != 1 {
		t.Errorf("expected exit code 1, got %d", code)
	}
}

// TestRunJps_InvalidArgs tests jps command with invalid arguments.
func TestRunJps_InvalidArgs(t *testing.T) {
	code := runCommandWithFlags(internal.ParseJpsFlags, internal.JpsList, []string{"-notexist"})
	if code != 1 {
		t.Errorf("expected exit code 1 for invalid flag, got %d", code)
	}

	code = runCommandWithFlags(internal.ParseJpsFlags, internal.JpsList, []string{"-user", "this_user_should_not_exist_12345"})
	if code != 1 {
		t.Errorf("expected exit code 1 for non-existent user, got %d", code)
	}
}

// TestRunJattach_InvalidArgs tests jattach command with invalid arguments.
func TestRunJattach_InvalidArgs(t *testing.T) {
	code := runCommandWithFlags(internal.ParseJattachFlags, internal.Jattach, []string{"-notexist"})
	if code != 1 {
		t.Errorf("expected exit code 1 for invalid flag, got %d", code)
	}

	code = runCommandWithFlags(internal.ParseJattachFlags, internal.Jattach, []string{"-pid", "12345"})
	if code != 1 {
		t.Errorf("expected exit code 1 for missing required agentpath, got %d", code)
	}

	code = runCommandWithFlags(internal.ParseJattachFlags, internal.Jattach, []string{"-agentpath", "/tmp/agent.jar"})
	if code != 1 {
		t.Errorf("expected exit code 1 for missing required pid, got %d", code)
	}

	code = runCommandWithFlags(internal.ParseJattachFlags, internal.Jattach, []string{"-user", "this_user_should_not_exist_12345", "-pid", "12345", "-agentpath", "/tmp/agent.jar"})
	if code != 1 {
		t.Errorf("expected exit code 1 for non-existent user, got %d", code)
	}
}

// TestRunSAAgent_InvalidArgs tests sa command with invalid arguments.
func TestRunSAAgent_InvalidArgs(t *testing.T) {
	code := runCommandWithFlags(internal.ParseSAAgentFlags, internal.SAAgent, []string{"-notexist"})
	if code != 1 {
		t.Errorf("expected exit code 1 for invalid flag, got %d", code)
	}

	code = runCommandWithFlags(internal.ParseSAAgentFlags, internal.SAAgent, []string{"-analysis", "invalid_type"})
	if code != 1 {
		t.Errorf("expected exit code 1 for invalid analysis type, got %d", code)
	}
}

// TestRunCommandWithFlags_Help tests that help requests are handled correctly.
func TestRunCommandWithFlags_Help(t *testing.T) {
	code := runCommandWithFlags(internal.ParseJpsFlags, internal.JpsList, []string{"-h"})
	if code != 0 {
		t.Errorf("expected exit code 0 for help request, got %d", code)
	}

	code = runCommandWithFlags(internal.ParseJattachFlags, internal.Jattach, []string{"-h"})
	if code != 0 {
		t.Errorf("expected exit code 0 for help request, got %d", code)
	}

	code = runCommandWithFlags(internal.ParseSAAgentFlags, internal.SAAgent, []string{"-h"})
	if code != 0 {
		t.Errorf("expected exit code 0 for help request, got %d", code)
	}
}
