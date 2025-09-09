package internal

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"github.com/XHao/jvmtool/pkg"
)

// SAAgentOption
type SAAgentOption struct {
	User     string
	Pid      string
	Analysis string // memory, thread, class, heap, all
	Duration int
	Output   string
}

// ParseSAAgentFlags
func ParseSAAgentFlags(args []string) (SAAgentOption, error) {
	saFlagSet := flag.NewFlagSet("sa", flag.ContinueOnError)
	user := saFlagSet.String("user", "", "specify the user")
	pid := saFlagSet.String("pid", "", "specify the pid of the Java process")
	analysis := saFlagSet.String("analysis", "all", "analysis type: memory, thread, class, heap, all")
	duration := saFlagSet.Int("duration", 30, "analysis duration in seconds")
	output := saFlagSet.String("output", "", "output file path")

	if err := saFlagSet.Parse(args); err != nil {
		return SAAgentOption{}, err
	}

	return SAAgentOption{
		User:     *user,
		Pid:      *pid,
		Analysis: *analysis,
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

	// Validate analysis type
	validTypes := map[string]bool{
		"memory": true,
		"thread": true,
		"class":  true,
		"heap":   true,
		"all":    true,
	}
	if !validTypes[opt.Analysis] {
		return fmt.Errorf("invalid analysis type: %s", opt.Analysis)
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

	// Map analysis to native module/task_type
	module := ""
	taskType := ""
	switch option.Analysis {
	case "memory", "all":
		module = "memory"
		taskType = "all"
	case "heap":
		module = "memory"
		taskType = "heap"
	case "gc":
		module = "memory"
		taskType = "gc"
	case "metaspace":
		module = "memory"
		taskType = "metaspace"
	default:
		pkg.Log(fmt.Sprintf("analysis type '%s' is not supported yet", option.Analysis))
		return 1
	}

	// Build parameters expected by native agent. Current native uses a Unix socket at /tmp/jvmtool_memory_<pid>.sock
	params := fmt.Sprintf("analysis=%s,task_type=%s,duration=%d", module, taskType, option.Duration)
	if option.Output != "" {
		params += fmt.Sprintf(",output=%s", option.Output)
	}

	jattachOpt := JattachOption{
		User:        option.User,
		Pid:         option.Pid,
		AgentPath:   agentPath,
		AgentParams: params,
	}

	pkg.Log(fmt.Sprintf("Starting SA analysis for process %s (type: %s, duration: %ds)",
		option.Pid, option.Analysis, option.Duration))

	result := Jattach(jattachOpt)
	if result != 0 {
		return result
	}

	// Connect to native Unix socket and stream messages
	// Note: current native implementation hardcodes memory module socket path
	socketPath := fmt.Sprintf("/tmp/jvmtool_%s_%s.sock", module, option.Pid)
	conn, err := connectSocket(socketPath, 5*time.Second)
	if err != nil {
		pkg.Log(fmt.Sprintf("Failed to connect to agent stream: %v", err))
		return 1
	}

	pkg.Log("Streaming analysis data...")
	deadline := time.Now().Add(time.Duration(option.Duration+2) * time.Second)
	if err := readMessages(conn, deadline); err != nil {
		pkg.Log(fmt.Sprintf("SA stream error: %v", err))
		return 1
	}

	return 0
}

// findNativeAgent searches for the native agent library in various locations
// following the project's installation and build structure
func findNativeAgent() (string, error) {
	// Detect OS and set library extension
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
			// Validate the agent library before returning
			if err := ValidateAgentLibrary(absPath); err != nil {
				pkg.Log(fmt.Sprintf("Agent validation failed for %s", absPath))
				continue
			}
			return absPath, nil
		}
	}

	// If not found, provide helpful error message
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

// displayTempFileOutput reads and displays the content of a temporary output file
func displayTempFileOutput(tempFile string) {
	file, err := os.Open(tempFile)
	if err != nil {
		pkg.Log(fmt.Sprintf("Error reading analysis output: %v", err))
		return
	}
	defer file.Close()

	pkg.Log("Analysis Results:")
	pkg.Log("================")

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		// Remove the timestamp prefix that was added in C++
		if len(line) > 21 && line[0] == '[' {
			if idx := strings.Index(line, "] "); idx != -1 && idx < 25 {
				line = line[idx+2:]
			}
		}
		fmt.Println(line)
	}

	if err := scanner.Err(); err != nil {
		pkg.Log(fmt.Sprintf("Error reading analysis output: %v", err))
	}
}
