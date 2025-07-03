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

	switch os.Args[1] {
	case "help", "-h", "--help":
		printHelp()
	case "jps":
		opt, err := internal.ParseJpsFlags(os.Args[2:])
		if err != nil {
			fmt.Fprintf(os.Stderr, "failed to parse flags: %v\n", err)
			os.Exit(1)
		}
		internal.JpsList(opt)
	default:
		fmt.Printf("unknown command: %s\n", os.Args[1])
		printHelp()
		os.Exit(1)
	}
}

// printHelp prints the usage information for the command line tool.
func printHelp() {
	fmt.Print(`Usage: jvmtool <command> [options]

Commands:
  help            Show this help message.
  jps             List Java processes for the current or specified user.

jps options:
  -user <username>    Specify the user to list Java processes for. If not provided, uses the current user.

Examples:
  jvmtool jps
  jvmtool jps -user alice

`)
}
