package internal

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"os/user"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"github.com/XHao/jvmtool/pkg"
	"github.com/shirou/gopsutil/process"
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
	if opt.User == "" {
		currentUser, err := user.Current()
		if err != nil {
			return err
		}
		opt.User = currentUser.Username
	} else {
		_, err := user.Lookup(opt.User)
		if err != nil {
			return err
		}
	}

	if opt.Pid == "" {
		return fmt.Errorf("pid is required")
	}

	_, err := process.NewProcess(toInt32(opt.Pid))
	if err != nil {
		return fmt.Errorf("process not found")
	}

	pidFile := os.TempDir() + "/hsperfdata_" + opt.User + "/" + fmt.Sprint(opt.Pid)
	if !pkg.PathExists(pidFile) {
		return fmt.Errorf("pid does not belong to the specified user")
	}

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
		log(err.Error())
		return 1
	}

	agentPath, err := findNativeAgent()
	if err != nil {
		log(fmt.Sprintf("Native agent not found (%v), falling back to Java agent", err))
		return 1
	}

	params := fmt.Sprintf("analysis=%s,duration=%d", option.Analysis, option.Duration)
	if option.Output != "" {
		params += fmt.Sprintf(",output=%s", option.Output)
	}

	jattachOpt := JattachOption{
		User:        option.User,
		Pid:         option.Pid,
		AgentPath:   agentPath,
		AgentParams: params,
	}

	log(fmt.Sprintf("Starting SA analysis for process %s (type: %s, duration: %ds)",
		option.Pid, option.Analysis, option.Duration))

	result := Jattach(jattachOpt)

	// If no output file was specified, we need to wait for and display the temporary file output
	if option.Output == "" && result == 0 {
		log("Waiting for analysis to complete...")
		time.Sleep(time.Duration(option.Duration+2) * time.Second)

		// Look for temporary output files
		tempPattern := fmt.Sprintf("/tmp/jvmtool_sa_%s*.log", option.Pid)
		if matches, err := filepath.Glob(tempPattern); err == nil && len(matches) > 0 {
			for _, tempFile := range matches {
				displayTempFileOutput(tempFile)
				// Clean up temp file
				os.Remove(tempFile)
			}
		}
	}

	return result
}

// findNativeAgent searches for the native agent library in various locations
func findNativeAgent() (string, error) {
	// Get the directory of the current executable
	execPath, err := os.Executable()
	if err != nil {
		return "", fmt.Errorf("failed to get executable path: %v", err)
	}
	execDir := filepath.Dir(execPath)

	// Detect OS and set library extension
	var libExt string
	switch runtime.GOOS {
	case "darwin":
		libExt = "dylib"
	case "linux":
		libExt = "so"
	case "windows":
		libExt = "dll"
	default:
		libExt = "so"
	}

	agentName := "jvmtool-agent." + libExt

	// Search paths in order of preference
	searchPath := filepath.Join(execDir, "..", "lib", agentName)
	absPath, err := filepath.Abs(searchPath)
	if err != nil {
		return "", fmt.Errorf("native agent library not found in any search paths")
	}
	return absPath, nil
}

// displayTempFileOutput reads and displays the content of a temporary output file
func displayTempFileOutput(tempFile string) {
	file, err := os.Open(tempFile)
	if err != nil {
		log(fmt.Sprintf("Error reading analysis output: %v", err))
		return
	}
	defer file.Close()

	log("Analysis Results:")
	log("================")

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
		log(fmt.Sprintf("Error reading analysis output: %v", err))
	}
}
