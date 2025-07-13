package main

import (
	"fmt"
	"os"

	"github.com/XHao/jvmtool/internal"
)

// main is the entry point of the application.
func main() {
	os.Exit(run(os.Args))
}

// run parses arguments and dispatches commands.
// Returns exit code.
func run(args []string) int {
	if len(args) < 2 {
		printHelp()
		return 1
	}

	cmd := args[1]
	cmdArgs := args[2:]

	switch cmd {
	case "help", "-h", "--help":
		printHelp()
		return 0
	case "jps":
		return runJps(cmdArgs)
	case "jattach":
		return runJattach(cmdArgs)
	default:
		printError(fmt.Sprintf("unknown command: %s", cmd))
		printHelp()
		return 1
	}
}

// runJps handles the "jps" command.
func runJps(args []string) int {
	opt, err := internal.ParseJpsFlags(args)
	if err != nil {
		printError(fmt.Sprintf("failed to parse flags: %v", err))
		return 1
	}
	return internal.JpsList(opt)
}

// runJattach handles the "jattach" command.
func runJattach(args []string) int {
	opt, err := internal.ParseJattachFlags(args)
	if err != nil {
		printError(fmt.Sprintf("failed to parse flags: %v", err))
		return 1
	}
	return internal.Jattach(opt)
}

// printHelp prints the usage information for the command line tool.
func printHelp() {
	fmt.Print(`Usage: jvmtool <command> [options]

Commands:
  help                Show this help message.
  jps                 List Java processes for the current or specified user.
  jattach             Attach a Java agent to a running Java process.

jps options:
  -user <username>        Specify the user to list Java processes for. If not provided, uses the current user.
  -l                      Show the full package name or the path to the jar file.
  -v                      Show JVM arguments.
  -m                      Show main method arguments.
  -q                      Only show process id.

jattach options:
  -user <username>        Specify the user to attach to. If not provided, uses the current user.
  -pid <pid>              Specify the pid of the Java process to attach to. (required)
  -agentpath <path>       Specify the path to the Java agent jar. (required)
  -agentparams <params>   Specify the parameters for the Java agent. (optional)

Examples:
  jvmtool jps
  jvmtool jps -user alice
  jvmtool jps -l -v -m
  jvmtool jattach -pid 12345 -agentpath /path/to/agent.jar
  jvmtool jattach -user alice -pid 12345 -agentpath /path/to/agent.jar -agentparams "foo=bar"

`)
}

// printError prints error messages to stderr.
func printError(msg string) {
	fmt.Fprintln(os.Stderr, msg)
}
