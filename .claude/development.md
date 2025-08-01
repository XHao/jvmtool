# JVMTool Development Guide

Welcome to JVMTool development! This guide will help you quickly understand the project structure, development workflow, and best practices.

## 🚀 Quick Start

### Environment Requirements

#### Required Software
- **Go 1.24+**: Check version with `go version`
- **JDK 8+**: Complete JDK installation (including JNI headers)
- **CMake 3.10+**: C++ build system
- **Make**: Project build tool
- **Git**: Version control

#### Recommended Development Tools
- **VS Code**: With Go and C++ extensions
- **GoLand**: JetBrains Go IDE
- **CLion**: JetBrains C++ IDE

### Initial Setup

```bash
# 1. Clone the project
git clone https://github.com/XHao/jvmtool.git
cd jvmtool

# 2. Check environment
java -version          # Verify JDK installation
echo $JAVA_HOME       # Verify JAVA_HOME setting
go version            # Verify Go version
cmake --version       # Verify CMake version

# 3. Create build directories
make dirs

# 4. Build the project
make build

# 5. Run tests
make test

# 6. Verify installation
./build/jvmtool help
```

## 📁 Project Structure

```
jvmtool/
├── .claude/                    # Project documentation
├── .github/                   # GitHub workflows
├── .vscode/                   # VS Code settings
├── cmd/                       # Application entry point
├── internal/                  # Internal implementation
├── pkg/                       # Public libraries
├── native/                    # C++ JVMTI agent
│   ├── include/              # Header files
│   ├── src/                  # Implementation files
│   ├── modules/              # Functional modules
│   ├── test/                 # C++ tests
│   └── demo/                 # Demo applications
├── build/                     # Build output
├── dist/                      # Distribution packages
├── scripts/                   # Build scripts
├── test/                      # Integration tests
├── go.mod                     # Go module definition
├── Makefile                   # Build configuration
└── README.md                  # Project documentation
```

## 🛠️ Development Workflow

### Daily Development Process

```bash
# 1. Create feature branch
git checkout -b feature/your-feature-name

# 2. Develop
# Edit code...

# 3. Format code
make format

# 4. Run tests
make test

# 5. Build project
make build

# 6. Commit changes
git add .
git commit -m "feat: add your feature description"

# 7. Push branch
git push origin feature/your-feature-name
```

### Make Command Reference

```bash
make help           # Show all available commands
make all            # Build all components
make build          # Build Go and C++ components
make build-go       # Build Go application only
make build-native   # Build C++ agent only
make test           # Run all tests
make clean          # Clean build files
make format         # Format code
make lint           # Code linting
make package        # Create release package
make install        # System installation
```

## 🧪 Testing Strategy

### Go Tests

```bash
# Run all Go tests
go test ./...

# Run specific package tests
go test ./internal

# Verbose output
go test -v ./...

# Test coverage
go test -cover ./...

# Generate coverage report
go test -coverprofile=coverage.out ./...
go tool cover -html=coverage.out
```

### C++ Tests

```bash
# Build and run C++ tests
cd native/build
make test

# Or use CTest
ctest --verbose
```

### Integration Tests

```bash
# Run end-to-end tests
make test-integration

# Manual command testing
./build/jvmtool jps
./build/jvmtool jattach -pid 12345 -agentpath ./build/lib/jvmtool-agent.so
```

## 📐 Code Standards and Guidelines

### Strategic Minimal Commenting Philosophy

The project follows a **minimal English-only commenting strategy** designed for global accessibility:

#### Core Principles
1. **Self-Documenting Code**: Write code that explains itself through clear naming
2. **Strategic Comments**: Comment only when code purpose isn't immediately clear
3. **English Only**: All comments and documentation in English for international collaboration
4. **Function-Level Focus**: Prioritize high-level function documentation over line-by-line comments

#### Go Code Standards

**Documentation**:
- Function-level comments explaining purpose, parameters, and return values
- Type definitions with clear purpose description
- Avoid obvious inline comments

**Example**:
```go
// ParseJpsFlags parses command line arguments for jps command.
// Returns JpsOption with validated settings or error if parsing fails.
func ParseJpsFlags(args []string) (JpsOption, error) {
    // Implementation...
}

// AgentMessage represents a parsed message from JVMTI agent communication.
type AgentMessage struct {
    Type      MessageType   // Message category
    Status    StatusCode    // Operation status
    Content   string        // Message content
    Timestamp string        // Creation timestamp
}
```

#### C++ Code Standards

**Documentation**:
- Class-level documentation explaining purpose and usage
- Public method documentation with parameters and behavior
- RAII patterns for resource management

**Modern C++ Practices**:
- **Prefer modern C++17/20 features** when appropriate for better performance and readability
- Use `constexpr` functions for compile-time evaluation
- Leverage structured bindings `auto [a, b] = function()` instead of separate assignments
- Use aggregate initialization `return {"name", "description"}` over verbose constructors
- Apply `auto` for type deduction where it improves clarity
- Prefer smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw pointers when possible
- Use range-based for loops and STL algorithms over traditional loops

**Example**:
```cpp
// Modern C++ approach - using structured bindings and constexpr
struct ErrorInfo {
    const char* name;
    const char* description;
    constexpr ErrorInfo(const char* n, const char* d) : name(n), description(d) {}
};

// Constexpr function for compile-time evaluation
constexpr ErrorInfo getErrorInfo(int code) {
    switch (code) {
        case 100: return {"NULL_POINTER", "Pointer parameter is NULL"};
        case 101: return {"OUT_OF_MEMORY", "Insufficient memory"};
        default:  return {"UNKNOWN", "Undefined error"};
    }
}

void logError(int error_code) {
    // Modern structured binding instead of separate assignments
    const auto [name, description] = getErrorInfo(error_code);
    std::cerr << "Error " << name << ": " << description << std::endl;
}

// AgentModule provides abstract base for JVMTI analysis modules.
// Handles lifecycle management, thread safety, and resource cleanup.
class AgentModule {
public:
    virtual ~AgentModule() = default;
    
    // Initialize module with JVM environment and options.
    virtual jvmtiError initialize(JavaVM* java_vm, jvmtiEnv* jvmti, 
                                 const char* options);
    virtual void onAttach(const char* options) = 0;
    virtual const char* getName() const = 0;

protected:
    jvmtiEnv* jvmti_ = nullptr;
    jrawMonitorID module_monitor_ = nullptr;
};
```

### Naming Conventions

#### Go Naming
```go
// Package names: lowercase, single word
package internal

// Types: PascalCase
type AgentMessage struct{}
type MessageType int

// Functions: PascalCase (public), camelCase (private)
func ParseJpsFlags() {}    // Public
func parseArgument() {}    // Private

// Variables: camelCase
var protocolReader *JTProtocolReader
var isInitialized bool

// Constants: PascalCase or SCREAMING_SNAKE_CASE for groups
const DefaultTimeout = 30 * time.Second
const (
    STATUS_SUCCESS = 0
    STATUS_ERROR   = 1
)
```

#### C++ Naming
```cpp
// Classes and types: PascalCase
class AgentModule {};
enum class MessageType {};

// Methods and functions: camelCase
void initialize();
bool isInitialized() const;

// Member variables: snake_case with trailing underscore
class AgentModule {
private:
    jvmtiEnv* jvmti_;
    JavaVM* vm_;
    jrawMonitorID module_monitor_;
};

// Constants: SCREAMING_SNAKE_CASE
static const int MAX_RETRY_COUNT = 3;
static const char* DEFAULT_AGENT_NAME = "jvmtool-agent";
```

### Project Structure Guidelines

#### Directory Organization
The project follows a standard Go project layout with native C++ components:

```
├── .claude/                   # Development documentation and guides
├── cmd/                       # Application entry points (main packages)
├── internal/                  # Private application code (not importable)
├── pkg/                       # Public library code (importable by others)
├── native/                    # C++ JVMTI agent implementation
├── build/                     # Build artifacts
├── test/                      # Integration tests
└── scripts/                   # Build and utility scripts
```

#### File Organization Principles
1. **Separation of Concerns**: Each package has a single, well-defined responsibility
2. **Dependency Direction**: Dependencies flow inward (cmd → internal → pkg)
3. **Platform Abstraction**: OS-specific code isolated in separate files
4. **Test Colocation**: Tests alongside the code they test
5. **Documentation Centralization**: All development docs in `.claude/`

### Code Quality Standards

#### Error Handling Patterns

**Go**: Explicit error handling with context
```go
func parseProtocolFile(path string) (*AgentMessage, error) {
    file, err := os.Open(path)
    if err != nil {
        return nil, fmt.Errorf("failed to open protocol file %s: %w", path, err)
    }
    defer file.Close()
    
    message, err := parseMessage(file)
    if err != nil {
        return nil, fmt.Errorf("failed to parse message from %s: %w", path, err)
    }
    
    return message, nil
}
```

**C++**: RAII and explicit error codes
```cpp
jvmtiError AgentModule::initialize(JavaVM* java_vm, jvmtiEnv* jvmti, 
                                  const char* options) {
    if (java_vm == nullptr || jvmti == nullptr) {
        return JVMTI_ERROR_NULL_POINTER;
    }

    jvmti_ = jvmti;
    vm_ = java_vm;

    if (!initializeMonitor()) {
        return JVMTI_ERROR_INTERNAL;
    }

    return JVMTI_ERROR_NONE;
}
```

#### Testing Standards

**Go**: Table-driven tests
```go
func TestParseJpsFlags(t *testing.T) {
    tests := []struct {
        name        string
        args        []string
        expected    JpsOption
        expectError bool
    }{
        {"default options", []string{}, JpsOption{ShowPID: true}, false},
        {"long format", []string{"-l"}, JpsOption{ShowPID: true, ShowMainClass: true}, false},
        {"invalid flag", []string{"--invalid"}, JpsOption{}, true},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            result, err := ParseJpsFlags(tt.args)
            if tt.expectError {
                assert.Error(t, err)
            } else {
                assert.NoError(t, err)
                assert.Equal(t, tt.expected, result)
            }
        })
    }
}
```

**C++**: GoogleTest with descriptive test names
```cpp
TEST(AgentModuleTest, InitializeWithValidParameters) {
    MockJavaVM vm;
    MockJVMTIEnv jvmti;
    TestAgentModule module;
    
    EXPECT_CALL(jvmti, CreateRawMonitor(_, _))
        .WillOnce(Return(JVMTI_ERROR_NONE));
    
    jvmtiError result = module.initialize(&vm, &jvmti, nullptr);
    
    EXPECT_EQ(JVMTI_ERROR_NONE, result);
    EXPECT_TRUE(module.isInitialized());
}
```

## 🔧 Development Best Practices

### Adding New Go Commands

1. **Add command dispatch** in `cmd/main.go`:
```go
switch cmd {
case "your-command":
    return runCommandWithFlags(internal.ParseYourCommandFlags, internal.YourCommand, cmdArgs)
}
```

2. **Implement in `internal/`**:
```go
type YourCommandOption struct {
    // Define option fields
}

func ParseYourCommandFlags(args []string) (YourCommandOption, error) {
    // Implement argument parsing
}

func YourCommand(opt YourCommandOption) int {
    // Implement command logic
    return 0
}
```

### Adding New JVMTI Modules

1. **Create header** (`native/include/your_module.h`):
```cpp
#pragma once
#include "agent.h"

namespace jvmtool {
    class YourModule : public AgentModule {
    public:
        const char* getName() const override;
        jvmtiError initialize(JavaVM* java_vm, jvmtiEnv* jvmti, 
                             const char* options) override;
        void onAttach(const char* options) override;
    };
}
```

2. **Create implementation** (`native/modules/your_module.cpp`):
```cpp
#include "your_module.h"

namespace jvmtool {
    const char* YourModule::getName() const {
        return "your_module";
    }
    
    jvmtiError YourModule::initialize(JavaVM* java_vm, jvmtiEnv* jvmti, 
                                     const char* options) {
        jvmtiError result = AgentModule::initialize(java_vm, jvmti, options);
        if (result != JVMTI_ERROR_NONE) {
            return result;
        }
        
        // Module-specific initialization
        return JVMTI_ERROR_NONE;
    }
    
    void YourModule::onAttach(const char* options) {
        // Module logic
    }
}

// Module registration
static YourModule your_module_instance;
static bool registered = []() {
    AgentManager::instance().registerModule(&your_module_instance);
    return true;
}();
```

3. **Update `native/CMakeLists.txt`** to include the new module

### Extending Communication Protocol

Update message types in both languages:

**C++** (`native/include/message.h`):
```cpp
enum class MessageType {
    YOUR_NEW_TYPE = 5
};
```

**Go** (`internal/jt_protocol.go`):
```go
const (
    YourNewMessage MessageType = 5
)
```

## 🐛 Debugging Guide

### Go Program Debugging

```bash
# Use delve debugger
go install github.com/go-delve/delve/cmd/dlv@latest

# Debug application
dlv debug ./cmd -- jps

# Debug tests
dlv test ./internal -- -test.run TestFunction
```

### C++ Agent Debugging

```bash
# Use GDB
gdb --args java -agentpath:./build/lib/jvmtool-agent.so YourJavaApp

# Or use LLDB (macOS)
lldb -- java -agentpath:./build/lib/jvmtool-agent.dylib YourJavaApp
```

### Protocol Debugging

```bash
# Monitor protocol files
tail -f /tmp/jvmtool-*.jt

# Or use watch command
watch -n 1 cat /tmp/jvmtool-*.jt
```

## 📊 Performance Analysis

### Go Performance Analysis

```bash
# CPU profiling
go test -cpuprofile=cpu.prof ./...
go tool pprof cpu.prof

# Memory analysis
go test -memprofile=mem.prof ./...
go tool pprof mem.prof

# Online analysis
go tool pprof http://localhost:6060/debug/pprof/profile
```

### C++ Performance Analysis

```bash
# Use perf (Linux)
perf record -g java -agentpath:./build/lib/jvmtool-agent.so YourApp
perf report

# Use Instruments (macOS)
# Open Instruments in Xcode, select Time Profiler
```

## 📚 Learning Resources

### JVMTI Development
- [JVMTI Specification](https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html)
- [JNI Programming Guide](https://docs.oracle.com/javase/8/docs/technotes/guides/jni/)
- [Project JVMTI Best Practices](./.claude/jvmti.md)

### Go Development
- [Go Language Tutorial](https://tour.golang.org/)
- [Effective Go](https://golang.org/doc/effective_go.html)
- [Go Blog](https://blog.golang.org/)

### C++ Development
- [C++ Core Guidelines](https://github.com/isocpp/CppCoreGuidelines)
- [Modern C++ Features](https://github.com/AnthonyCalandra/modern-cpp-features)

## 🤝 Contributing Guidelines

### Submitting Pull Requests

1. **Fork the project** and create a feature branch
2. **Write tests** covering new functionality
3. **Ensure tests pass** with `make test`
4. **Format code** with `make format`
5. **Write clear commit messages**
6. **Create Pull Request**

### Commit Message Format

```
type(scope): short description

Longer description if needed

Fixes #issue_number
```

Type examples:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation update
- `test`: Test-related changes
- `refactor`: Code refactoring
- `perf`: Performance optimization

### Reporting Issues

When reporting issues using GitHub Issues, please include:
- Operating system and version
- Go and JDK versions
- Steps to reproduce
- Error messages and logs
- Expected behavior

## 🆘 Getting Help

- **Project Documentation**: Check documents in `.claude/` directory
- **Code Examples**: Reference `*_test.go` files
- **GitHub Issues**: Search existing issues or create new ones
- **Code Reviews**: Get feedback through Pull Requests

---

**Happy Coding! 🎉**

For any questions or suggestions, feel free to participate in project discussions through GitHub Issues or Pull Requests.
