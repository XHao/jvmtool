package internal

import (
	"flag"
	"fmt"

	"github.com/XHao/jvmtool/pkg"
)

type JattachOption struct {
	User        string
	Pid         string
	AgentPath   string
	AgentParams string
}

// ParseJattachFlags parses flags for the "jattach" command and returns the corresponding JattachOption.
func ParseJattachFlags(args []string) (JattachOption, error) {
	jattachFlagSet := flag.NewFlagSet("jattach", flag.ContinueOnError)
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
	if opt.AgentPath == "" {
		return fmt.Errorf("agentpath is required")
	}

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
	return validator.ValidateJavaProcess()
}

// Jattach performs the attach operation to a Java process specified by the JattachOption.
func Jattach(option JattachOption) int {
	if err := option.JattachValidate(); err != nil {
		log(err.Error())
		return 1
	}

	jp := &JvmProcess{
		Pid: pkg.Pid(option.Pid),
	}

	if err := jp.checkSocket(); err != nil {
		log(err.Error())
		return 1
	} else if err := jp.loadAgent(option.AgentPath, option.AgentParams); err != nil {
		log(err.Error())
		return 1
	}
	return 0
}
