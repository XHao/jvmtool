package internal

import (
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/XHao/jvmtool/pkg"
)

// captureLogs sets up a logger that captures log output into a slice and returns a function to retrieve the logs.
func captureLogs() (restore func(), getLogs func() []string, clearLogs func()) {
	origLogger := globalLogger
	var logs []string
	logInit(func(msg string) {
		logs = append(logs, msg)
	})
	return func() { globalLogger = origLogger }, func() []string { return logs }, func() { logs = nil }
}

// prepareHsperfdataFile creates a fake hsperfdata file for the given user and pid, returning the file path and a cleanup function.
func prepareHsperfdataFile(username string, pid int) (string, func(), error) {
	tempDir := os.TempDir()
	hsperfDir := filepath.Join(tempDir, "hsperfdata_"+username)
	if err := os.MkdirAll(hsperfDir, 0755); err != nil {
		return "", nil, err
	}
	hsperfFile := filepath.Join(hsperfDir, strconv.Itoa(pid))
	f, err := os.Create(hsperfFile)
	if err != nil {
		return "", nil, err
	}
	f.Close()
	cleanup := func() {
		os.RemoveAll(hsperfDir)
	}
	return hsperfFile, cleanup, nil
}

// TestJpsList_ValidUser tests JpsList with a valid user and a fake Java process.
func TestJpsList_ValidUser(t *testing.T) {
	restore, getLogs, clearLogs := captureLogs()
	defer restore()

	currentUser, err := user.Current()
	if err != nil {
		t.Fatalf("failed to get current user: %v", err)
	}

	pid := os.Getpid()
	if exist, _ := pkg.PidExists(int32(pid)); !exist {
		pid = 1
	}
	_, cleanup, err := prepareHsperfdataFile(currentUser.Username, pid)
	if err != nil {
		t.Fatalf("failed to create hsperfdata file: %v", err)
	}
	defer cleanup()

	clearLogs()
	opt := JpsOption{User: currentUser.Username}
	JpsList(opt)
	found := false
	for _, l := range getLogs() {
		if l != "" && l != "no java process" {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected to find at least one java process, got logs: %v", getLogs())
	}
}

// TestJpsList_InvalidUser tests JpsList with a non-existent user.
func TestJpsList_InvalidUser(t *testing.T) {
	restore, getLogs, clearLogs := captureLogs()
	defer restore()

	clearLogs()
	opt := JpsOption{User: "nonexistent_user_12345"}
	JpsList(opt)
	userErr := false
	for _, l := range getLogs() {
		if l == "user does not exist" {
			userErr = true
			break
		}
	}
	if !userErr {
		t.Errorf("expected 'user does not exist' error, got logs: %v", getLogs())
	}
}

// TestJpsList_NoJavaProcess tests JpsList when there are no hsperfdata files for the user.
func TestJpsList_NoJavaProcess(t *testing.T) {
	restore, getLogs, clearLogs := captureLogs()
	defer restore()

	currentUser, err := user.Current()
	if err != nil {
		t.Fatalf("failed to get current user: %v", err)
	}

	pid := os.Getpid()
	hsperfFile, cleanup, err := prepareHsperfdataFile(currentUser.Username, pid)
	if err != nil {
		t.Fatalf("failed to create hsperfdata file: %v", err)
	}
	os.Remove(hsperfFile)
	defer cleanup()

	clearLogs()
	opt := JpsOption{User: currentUser.Username}
	JpsList(opt)
	noProc := false
	for _, l := range getLogs() {
		if l == "no java process" {
			noProc = true
			break
		}
	}
	if !noProc {
		t.Errorf("expected 'no java process' log, got logs: %v", getLogs())
	}
}

// TestJpsList_ActualJavaProcess tests JpsList with an actual local Java process.
func TestJpsList_ActualJavaProcess(t *testing.T) {
	restore, getLogs, clearLogs := captureLogs()
	defer restore()

	currentUser, err := user.Current()
	if err != nil {
		t.Fatalf("failed to get current user: %v", err)
	}

	javaPath, err := exec.LookPath("java")
	if err != nil {
		t.Skip("java not found in PATH, skipping actual Java process test")
	}

	tmpDir := os.TempDir()
	javaFile := filepath.Join(tmpDir, "TestJpsList_ActualJavaProcess.java")
	javaSource := `public class TestJpsList_ActualJavaProcess { public static void main(String[] args) throws Exception { Thread.sleep(3000); } }`
	if err := os.WriteFile(javaFile, []byte(javaSource), 0644); err != nil {
		t.Fatalf("failed to write java file: %v", err)
	}
	defer os.Remove(javaFile)

	cmdCompile := exec.Command(javaPath, "c", javaFile)
	cmdCompile.Dir = tmpDir

	javacPath, err := exec.LookPath("javac")
	if err != nil {
		t.Skip("javac not found in PATH, skipping actual Java process test")
	}
	cmdCompile = exec.Command(javacPath, javaFile)
	cmdCompile.Dir = tmpDir
	if out, err := cmdCompile.CombinedOutput(); err != nil {
		t.Fatalf("failed to compile java file: %v, output: %s", err, string(out))
	}
	classFile := filepath.Join(tmpDir, "TestJpsList_ActualJavaProcess.class")
	defer os.Remove(classFile)

	cmdRun := exec.Command(javaPath, "-cp", tmpDir, "TestJpsList_ActualJavaProcess")
	cmdRun.Dir = tmpDir
	if err := cmdRun.Start(); err != nil {
		t.Fatalf("failed to start java process: %v", err)
	}
	defer func() {
		_ = cmdRun.Process.Kill()
		cmdRun.Wait()
	}()

	time.Sleep(2 * time.Second)

	clearLogs()
	opt := JpsOption{User: currentUser.Username}
	JpsList(opt)
	found := false
	for _, l := range getLogs() {
		if strings.Contains(l, "TestJpsList_ActualJavaProcess") {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected to find TestJpsList_ActualJavaProcess in logs, got: %v", getLogs())
	}
}
