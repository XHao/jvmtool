package internal

import (
	"flag"
	"fmt"
	"strings"

	"github.com/XHao/jvmtool/pkg"
	"github.com/shirou/gopsutil/process"
)

// ParseJpsFlags parses flags for the "jps" command and returns the corresponding JpsOption.
func ParseJpsFlags(args []string) (JpsOption, error) {
	jpsFlagSet := flag.NewFlagSet("jps", flag.ContinueOnError)
	user := jpsFlagSet.String("user", "", "specify the user to list Java processes for")
	showLong := jpsFlagSet.Bool("l", false, "show the full package name or the path to the jar file")
	showVMArgs := jpsFlagSet.Bool("v", false, "show JVM arguments")
	showArgs := jpsFlagSet.Bool("m", false, "show main method arguments")
	quiet := jpsFlagSet.Bool("q", false, "only show process id")
	if err := jpsFlagSet.Parse(args); err != nil {
		return JpsOption{}, err
	}
	return JpsOption{
		User:       *user,
		ShowLong:   *showLong,
		ShowVMArgs: *showVMArgs,
		ShowArgs:   *showArgs,
		Quiet:      *quiet,
	}, nil
}

type JpsOption struct {
	User       string
	ShowLong   bool // -l
	ShowVMArgs bool // -v
	ShowArgs   bool // -m
	Quiet      bool // -q
}

// JpsValidate checks if the JpsOption fields are valid.
// Currently, it validates the User field if provided.
func (opt *JpsOption) JpsValidate() error {
	user, err := pkg.ValidateUser(opt.User)
	if err != nil {
		return err
	}
	opt.User = user
	return nil
}

// JpsList returns a list of Java process information for the current or specified user.
// @see sun.jvmstat.perfdata.monitor.protocol.local.LocalVmManager.activeVms()
func JpsList(option JpsOption) int {
	if err := option.JpsValidate(); err != nil {
		log(err.Error())
		return 1
	}

	pids, err := pkg.DiscoverJavaProcesses(option.User)
	if err != nil {
		log(fmt.Sprintf("failed to discover java processes: %v", err))
		return 1
	}

	if len(pids) == 0 {
		if !option.Quiet {
			log("no java processes found for user: " + option.User)
		}
		return 0
	}

	processes := collectProcessInfo(pids, option)
	for _, p := range processes {
		printJps(p, option)
	}
	return 0
}

// collectProcessInfo collects detailed information for the given PIDs
func collectProcessInfo(pids []int32, option JpsOption) []JvmProcess {
	var processes []JvmProcess
	for _, pid := range pids {
		p, err := process.NewProcess(pid)
		if err != nil {
			continue
		}
		cmdSlice, err := p.CmdlineSlice()
		if err != nil {
			continue
		}
		cmd := strings.Join(cmdSlice, " ")
		mainClassOrJar, vmArgs, mainArgs := analyzeVmCmd(cmdSlice, option)
		processes = append(processes, JvmProcess{
			Pid:            p.Pid,
			Cmd:            cmd,
			mainClassOrJar: mainClassOrJar,
			vmArgs:         vmArgs,
			mainArgs:       mainArgs,
		})
	}
	return processes
}

// printJps prints the information of a Java process according to the JpsOption.
func printJps(process JvmProcess, option JpsOption) {
	if option.Quiet {
		log(fmt.Sprintf("%d", process.Pid))
		return
	}

	var parts []string
	parts = append(parts, fmt.Sprintf("%d", process.Pid))

	if option.ShowLong {
		parts = append(parts, process.Cmd)
	} else {
		parts = append(parts, process.mainClassOrJar)
	}

	if option.ShowVMArgs && process.vmArgs != "" {
		parts = append(parts, strings.TrimSpace(process.vmArgs))
	}

	if option.ShowArgs && process.mainArgs != "" {
		parts = append(parts, process.mainArgs)
	}

	log(strings.Join(parts, " "))
}

func analyzeVmCmd(cmdSlice []string, option JpsOption) (mainClassOrJar string, vmArgs string, mainArgs string) {
	if len(cmdSlice) < 2 {
		return
	}

	var vmArgsList []string
	skipNext := false

	for i := 1; i < len(cmdSlice); i++ {
		arg := cmdSlice[i]
		if skipNext {
			skipNext = false
			continue
		}
		if arg == "-cp" || arg == "-classpath" {
			skipNext = true
			continue
		}
		if arg == "-jar" && i+1 < len(cmdSlice) {
			mainClassOrJar = cmdSlice[i+1]
			if option.ShowArgs && i+2 < len(cmdSlice) {
				mainArgs = strings.Join(cmdSlice[i+2:], " ")
			}
			break
		}
		if strings.HasPrefix(arg, "-") {
			if option.ShowVMArgs {
				vmArgsList = append(vmArgsList, arg)
			}
			continue
		}
		if mainClassOrJar == "" {
			mainClassOrJar = arg
			if option.ShowArgs && i+1 < len(cmdSlice) {
				mainArgs = strings.Join(cmdSlice[i+1:], " ")
			}
			break
		}
	}

	if option.ShowVMArgs && len(vmArgsList) > 0 {
		vmArgs = strings.Join(vmArgsList, " ")
	}

	return
}
