package internal

import (
	"fmt"
	"strings"
)

const (
	metaspaceModuleName = "meta"
	metaspaceTaskName   = "stats"
)

type metaspaceProvider struct{}

func (metaspaceProvider) Name() string { return metaspaceModuleName }

func (metaspaceProvider) Aliases() []string { return []string{"metaspace"} }

func (metaspaceProvider) DefaultTask() string { return metaspaceTaskName }

func (p metaspaceProvider) NormalizeTask(raw string) (string, error) {
	trimmed := strings.TrimSpace(raw)
	if trimmed == "" {
		return p.DefaultTask(), nil
	}
	return strings.ToLower(trimmed), nil
}

func (p metaspaceProvider) BuildParams(task string, opt SAAgentOption) string {
	params := fmt.Sprintf("analysis=%s,task_type=%s,duration=%d", p.Name(), task, opt.Duration)
	if opt.Output != "" {
		params += fmt.Sprintf(",output=%s", opt.Output)
	}
	return params
}

func (p metaspaceProvider) SocketPath(pid string) string {
	return fmt.Sprintf("/tmp/jvmtool_%s_%s.sock", p.Name(), pid)
}

func (metaspaceProvider) StreamHandler(opt SAAgentOption) SAStreamHandler {
	return newDefaultStreamHandler(opt)
}

func init() {
	registerProvider(metaspaceProvider{})
}
