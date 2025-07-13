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
	if err := jpsFlagSet.Parse(args); err != nil {
		return JpsOption{}, err
	}

	return JpsOption{User: *user}, nil
}

type JpsOption struct {
	User string
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
		finded = append(finded, JvmProcess{Pid: p.Pid, Cmd: cmd})
	}

	for _, p := range finded {
		log(fmt.Sprintf("%d %s\n", p.Pid, p.Cmd))
	}
	return 0
}
