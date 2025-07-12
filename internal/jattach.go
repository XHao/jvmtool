package internal

import (
	"flag"
	"fmt"
	"os"
	"os/user"
	"strconv"

	"github.com/XHao/jvmtool/pkg"
	"github.com/shirou/gopsutil/process"
)

type JattachOption struct {
	User        string
	Pid         string
	AgentPath   string
	AgentParams string
}

// ParseJattachFlags parses flags for the "jattach" command and returns the corresponding JattachOption.
func ParseJattachFlags(args []string) (JattachOption, error) {
	jattachFlagSet := flag.NewFlagSet("jattach", flag.ExitOnError)
	user := jattachFlagSet.String("user", "", "specify the user to attach to")
	pid := jattachFlagSet.String("pid", "", "specify the pid of the Java process to attach to")
	agentPath := jattachFlagSet.String("agentpath", "", "specify the path to the Java agent jar")
	agentParams := jattachFlagSet.String("agentparams", "", "specify the parameters for the Java agent")
	if err := jattachFlagSet.Parse(args); err != nil {
		return JattachOption{}, err
	}
	return JattachOption{
		User:        *user,
		Pid:         *pid,
		AgentPath:   *agentPath,
		AgentParams: *agentParams,
	}, nil
}

// JattachValidate validates the JattachOption fields.
func (opt *JattachOption) JattachValidate() error {
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
	if opt.AgentPath == "" {
		return fmt.Errorf("agentpath is required")
	}
	return nil
}

// toInt32 converts a string to int32, returns 0 if conversion fails.
func toInt32(s string) int32 {
	n, _ := strconv.Atoi(s)
	return int32(n)
}

// Jattach performs the attach operation to a Java process specified by the JattachOption.
func Jattach(option JattachOption) {
	if err := option.JattachValidate(); err != nil {
		log(err.Error())
		return
	}

	jp := &JvmProcess{
		Pid: toInt32(option.Pid),
	}

	if err := jp.checkSocket(); err != nil {
		log(err.Error())
		return
	} else {
		if err := jp.loadAgent(option.AgentPath, option.AgentParams); err != nil {
			log(err.Error())
		}
	}
}
