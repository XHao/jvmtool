package main

import (
	"testing"
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

// TestRunJps_InvalidArgs tests runJps with invalid arguments.
func TestRunJps_InvalidArgs(t *testing.T) {
	code := runJps([]string{"-notexist"})
	if code != 1 {
		t.Errorf("expected exit code 1 for invalid flag, got %d", code)
	}

	code = runJps([]string{"-user", "this_user_should_not_exist_12345"})
	if code != 1 {
		t.Errorf("expected exit code 1 for non-existent user, got %d", code)
	}
}

// TestRunJattach_InvalidArgs tests runJattach with invalid arguments.
func TestRunJattach_InvalidArgs(t *testing.T) {
	code := runJattach([]string{"-notexist"})
	if code != 1 {
		t.Errorf("expected exit code 1 for invalid flag, got %d", code)
	}

	code = runJattach([]string{"-pid", "12345"})
	if code != 1 {
		t.Errorf("expected exit code 1 for missing required agentpath, got %d", code)
	}

	code = runJattach([]string{"-agentpath", "/tmp/agent.jar"})
	if code != 1 {
		t.Errorf("expected exit code 1 for missing required pid, got %d", code)
	}

	code = runJattach([]string{"-user", "this_user_should_not_exist_12345", "-pid", "12345", "-agentpath", "/tmp/agent.jar"})
	if code != 1 {
		t.Errorf("expected exit code 1 for non-existent user, got %d", code)
	}
}
