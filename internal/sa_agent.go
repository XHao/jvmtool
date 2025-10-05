package internal

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"time"

	"github.com/XHao/jvmtool/pkg"
)

// SAAgentOption
type SAAgentOption struct {
	User     string
	Pid      string
	Module   SAProvider
	Task     string
	Duration int
	Output   string
}

// ParseSAAgentFlags
func ParseSAAgentFlags(args []string) (SAAgentOption, error) {
	saFlagSet := flag.NewFlagSet("sa", flag.ContinueOnError)
	user := saFlagSet.String("user", "", "specify the user")
	pid := saFlagSet.String("pid", "", "specify the pid of the Java process")
	module := saFlagSet.String("module", "", "analysis module (currently only 'meta')")
	task := saFlagSet.String("task", "", "module-specific task type")
	duration := saFlagSet.Int("duration", 30, "analysis duration in seconds")
	output := saFlagSet.String("output", "", "output file path")

	if err := saFlagSet.Parse(args); err != nil {
		return SAAgentOption{}, err
	}

	provider, resolvedTask, err := resolveModuleAndTask(*module, *task)
	if err != nil {
		return SAAgentOption{}, err
	}

	return SAAgentOption{
		User:     *user,
		Pid:      *pid,
		Module:   provider,
		Task:     resolvedTask,
		Duration: *duration,
		Output:   *output,
	}, nil
}

// SAAgentValidate
func (opt *SAAgentOption) SAAgentValidate() error {
	// Validate user
	user, err := pkg.ValidateUser(opt.User)
	if err != nil {
		return err
	}
	opt.User = user

	// Validate Java process
	validator := &pkg.JavaProcessValidator{
		User: opt.User,
		Pid:  opt.Pid,
	}
	if err := validator.ValidateJavaProcess(); err != nil {
		return err
	}

	return nil
}

// SAAgent
func SAAgent(option SAAgentOption) int {
	if err := option.SAAgentValidate(); err != nil {
		pkg.Log(err.Error())
		return 1
	}

	agentPath, err := findNativeAgent()
	if err != nil {
		pkg.Log(fmt.Sprintf("Native agent not found (%v)", err))
		return 1
	}

	params := option.Module.BuildParams(option.Task, option)

	jattachOpt := JattachOption{
		User:        option.User,
		Pid:         option.Pid,
		AgentPath:   agentPath,
		AgentParams: params,
	}

	pkg.Log(fmt.Sprintf("Starting SA analysis for process %s (module: %s, task: %s, duration: %ds)",
		option.Pid, option.Module, option.Task, option.Duration))

	handler := option.Module.StreamHandler(option)

	result := Jattach(jattachOpt)
	if result != 0 {
		return result
	}

	socketPath := option.Module.SocketPath(option.Pid)
	conn, err := connectSocket(socketPath, 5*time.Second)
	if err != nil {
		pkg.Log(fmt.Sprintf("Failed to connect to agent stream: %v", err))
		return 1
	}

	pkg.Log("Streaming analysis data...")
	deadline := time.Now().Add(time.Duration(option.Duration+2) * time.Second)
	if err := readMessages(conn, deadline, handler); err != nil {
		pkg.Log(fmt.Sprintf("SA stream error: %v", err))
		return 1
	}

	return 0
}

// findNativeAgent searches for the native agent library in various locations
// following the project's installation and build structure
func findNativeAgent() (string, error) {
	var libExt string
	switch runtime.GOOS {
	case "darwin":
		libExt = "dylib"
	case "linux":
		libExt = "so"
	default:
		libExt = "so"
	}

	agentName := "jvmtool-agent." + libExt

	// Get the directory of the current executable
	execPath, err := os.Executable()
	if err != nil {
		return "", fmt.Errorf("failed to get executable path: %v", err)
	}
	execDir := filepath.Dir(execPath)

	// Search paths in order of preference
	searchPaths := []string{
		// 1. Same installation prefix as binary (if installed via make install)
		// If binary is at PREFIX/bin/jvmtool, look for PREFIX/lib/agent
		filepath.Join(filepath.Dir(execDir), "lib", agentName),

		// 2. Standard system installation paths (following FHS)
		filepath.Join("/usr/local/lib", agentName),
		filepath.Join("/usr/lib", agentName),
		filepath.Join("/opt/local/lib", agentName), // MacPorts
	}

	// Search for the agent library
	for _, path := range searchPaths {
		absPath, err := filepath.Abs(path)
		if err != nil {
			continue
		}

		if pkg.PathExists(absPath) {
			if err := ValidateAgentLibrary(absPath); err != nil {
				pkg.Log(fmt.Sprintf("Agent validation failed for %s", absPath))
				continue
			}
			return absPath, nil
		}
	}

	return "", fmt.Errorf("native agent library '%s' not found in any of the search paths:\n%s",
		agentName, joinSearchPaths(searchPaths))
}

// joinSearchPaths joins paths with newlines for error messages
func joinSearchPaths(paths []string) string {
	result := ""
	for i, path := range paths {
		if i > 0 {
			result += "\n"
		}
		result += path
	}
	return result
}
