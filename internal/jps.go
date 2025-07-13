package internal

import (
	"errors"
	"flag"
	"fmt"
	"os"
	"os/user"
	"path/filepath"
	"strconv"
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
	if opt.User != "" {
		_, err := user.Lookup(opt.User)
		if err != nil {
			return errors.New("user does not exist")
		}
	} else {
		if current, err := user.Current(); err != nil {
			return errors.New("current user check failed")
		} else {
			opt.User = current.Username
		}
	}
	return nil
}

// JpsList returns a list of Java process information for the current or specified user.
// @see sun.jvmstat.perfdata.monitor.protocol.local.LocalVmManager.activeVms()
func JpsList(option JpsOption) int {
	if err := option.JpsValidate(); err != nil {
		log(err.Error())
		return 1
	}

	finded := []JvmProcess{}
	tempDir := os.TempDir()

	namePatternPrefix := tempDir + "/hsperfdata_" + option.User + "/"
	fileNamePattern := namePatternPrefix + "*"
	pids := []int32{}

	files, err := filepath.Glob(fileNamePattern)
	if err != nil || len(files) == 0 {
		log("no java process")
		return 1
	}
	for _, file := range files {
		index := strings.LastIndex(file, "/") + 1

		if pid, err := strconv.Atoi(file[index:]); err != nil {
			continue
		} else if exist, _ := pkg.PidExists(int32(pid)); !exist {
			continue
		} else {
			pids = append(pids, int32(pid))
		}
	}

	if len(pids) == 0 {
		log("no java process")
		return 1
	}
	for _, pid := range pids {
		p, err := process.NewProcess(pid)
		if err != nil {
			continue
		}
		cmdSlice, _ := p.CmdlineSlice()
		cmd := strings.Join(cmdSlice, " ")
		mainClassOrJar, vmArgs, mainArgs := analyzeVmCmd(cmdSlice, option)
		finded = append(finded, JvmProcess{Pid: p.Pid, Cmd: cmd, mainClassOrJar: mainClassOrJar, vmArgs: vmArgs, mainArgs: mainArgs})
	}

	for _, p := range finded {
		printJps(p, option)
	}
	return 0
}

// printJps prints the information of a Java process according to the JpsOption.
func printJps(process JvmProcess, option JpsOption) {
	if option.Quiet {
		log(fmt.Sprintf("%d", process.Pid))
		return
	}
	output := fmt.Sprintf("%d", process.Pid)
	if option.ShowLong {
		output += fmt.Sprintf(" %s", process.Cmd)
	} else {
		output += fmt.Sprintf(" %s", process.mainClassOrJar)
	}
	if option.ShowVMArgs && process.vmArgs != "" {
		output += fmt.Sprintf(" %s", strings.TrimSpace(process.vmArgs))
	}
	if option.ShowArgs && process.mainArgs != "" {
		output += fmt.Sprintf(" %s", process.mainArgs)
	}
	log(output)
}

func analyzeVmCmd(cmdSlice []string, option JpsOption) (mainClassOrJar string, vmArgs string, mainArgs string) {
	if len(cmdSlice) < 2 {
		return
	}
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
				vmArgs += arg + " "
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
	return
}
