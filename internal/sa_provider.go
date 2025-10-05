package internal

import (
	"fmt"
	"strings"

	"github.com/XHao/jvmtool/pkg"
)

type SAProvider interface {
	Name() string
	Aliases() []string
	DefaultTask() string
	NormalizeTask(task string) (string, error)
	BuildParams(task string, opt SAAgentOption) string
	SocketPath(pid string) string
	StreamHandler(opt SAAgentOption) SAStreamHandler
}

type SAStreamHandler interface {
	OnStatus(message string) error
	OnData(message string) error
	OnComplete(err error) error
}

var (
	providerRegistry   = map[string]SAProvider{}
	providerAliasIndex = map[string]string{}
)

func registerProvider(p SAProvider) {
	name := p.Name()
	lowerName := strings.ToLower(name)
	providerRegistry[name] = p
	providerAliasIndex[lowerName] = name
	for _, alias := range p.Aliases() {
		providerAliasIndex[strings.ToLower(alias)] = name
	}
}

func newDefaultStreamHandler(opt SAAgentOption) SAStreamHandler {
	return &loggingHandler{option: opt}
}

type loggingHandler struct {
	option SAAgentOption
}

func (loggingHandler) OnStatus(message string) error {
	pkg.Log(message)
	return nil
}

func (loggingHandler) OnData(message string) error {
	fmt.Print(message)
	return nil
}

func (loggingHandler) OnComplete(err error) error {
	return nil
}

func resolveProvider(raw string) (SAProvider, error) {
	trimmed := strings.TrimSpace(raw)
	if trimmed == "" {
		return nil, fmt.Errorf("no SA providers")
	}
	key := strings.ToLower(trimmed)
	if name, ok := providerAliasIndex[key]; ok {
		return providerRegistry[name], nil
	}
	return nil, fmt.Errorf("module '%s' is not supported", raw)
}

func resolveModuleAndTask(module, task string) (SAProvider, string, error) {
	provider, err := resolveProvider(module)
	if err != nil {
		return nil, "", err
	}

	normalizedTask, err := provider.NormalizeTask(task)
	if err != nil {
		return nil, "", err
	}
	if normalizedTask == "" {
		normalizedTask = provider.DefaultTask()
	}

	return provider, normalizedTask, nil
}
