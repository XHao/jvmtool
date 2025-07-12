package main

import (
	"fmt"
	"os"

	"github.com/XHao/jvmtool/internal"
)

// main is the entry point of the application.
func main() {
	if len(os.Args) < 2 {
		printHelp()
		os.Exit(1)
	}

	cmd := os.Args[1]
	args := os.Args[2:]

	switch cmd {
	case "help", "-h", "--help":
		printHelp()
	case "jps":
		opt, err := internal.ParseJpsFlags(args)
		if err != nil {
			fmt.Fprintf(os.Stderr, "failed to parse flags: %v\n", err)
			os.Exit(1)
		}
		internal.JpsList(opt)
	case "jattach":
		opt, err := internal.ParseJattachFlags(args)
		if err != nil {
			fmt.Fprintf(os.Stderr, "failed to parse flags: %v\n", err)
			os.Exit(1)
		}
		internal.Jattach(opt)
	default:
		fmt.Fprintf(os.Stderr, "unknown command: %s\n", cmd)
		printHelp()
		os.Exit(1)
	}
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

jattach options:
  -user <username>        Specify the user to attach to. If not provided, uses the current user.
  -pid <pid>              Specify the pid of the Java process to attach to. (required)
  -agentpath <path>       Specify the path to the Java agent jar. (required)
  -agentparams <params>   Specify the parameters for the Java agent. (optional)

Examples:
  jvmtool jps
  jvmtool jps -user alice
  jvmtool jattach -pid 12345 -agentpath /path/to/agent.jar
  jvmtool jattach -user alice -pid 12345 -agentpath /path/to/agent.jar -agentparams "foo=bar"

`)
}
