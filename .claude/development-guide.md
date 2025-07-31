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

## 📁 Project Structure Details

```
jvmtool/
├── .claude/                    # 📋 Project documentation and analysis
│   ├── CLAUDE.md              # Project overview analysis
│   ├── jvmti.md               # JVMTI development guide
│   ├── improvements.md        # Improvement suggestions
│   └── decisions.md           # Architecture decision records
├── cmd/                       # 🚀 Application entry point
│   ├── main.go               # Main program and command dispatch
│   └── main_test.go          # Integration tests
├── internal/                  # 🔒 Internal implementation
│   ├── jattach.go            # Agent attachment functionality
│   ├── jps.go                # Java process listing
│   ├── sa_agent.go           # ServiceAbility agent
│   ├── jt_protocol.go        # File protocol implementation
│   ├── jvm*.go               # JVM interaction abstractions
│   └── *_test.go             # Unit tests
├── pkg/                       # 📦 Public libraries
│   ├── agent_validator*.go   # Agent validation
│   ├── java_process.go       # Java process management
│   ├── os*.go                # OS abstractions
│   └── user*.go              # User permission handling
├── native/                    # ⚡ C++ JVMTI agent
│   ├── include/              # Header files
│   │   ├── agent.h           # Agent framework
│   │   ├── file_protocol.h   # Protocol definitions
│   │   └── *.h               # Other interfaces
│   ├── src/                  # Implementation files
│   │   ├── agent.cpp         # Agent manager
│   │   ├── file_protocol.cpp # Protocol implementation
│   │   └── *.cpp             # Other implementations
│   ├── modules/              # Functional modules
│   │   └── memory_sa_agent.cpp
│   ├── test/                 # C++ tests
│   └── CMakeLists.txt        # Build configuration
├── build/                     # 🔨 Build output
├── scripts/                   # 📜 Build scripts
└── Makefile                  # 🎯 Main build file
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

## 🔧 Adding New Features

### 1. Adding New Go Commands

Add new command in `cmd/main.go`:

```go
switch cmd {
case "your-command":
    return runCommandWithFlags(internal.ParseYourCommandFlags, internal.YourCommand, cmdArgs)
}
```

Implement in `internal/`:

```go
// your_command.go
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

### 2. Adding New JVMTI Modules

Create header file `native/include/your_module.h`:

```cpp
#pragma once
#include "agent.h"

namespace jvmtool {
    class YourModule : public AgentModule {
    public:
        std::string getName() const override;
        bool initialize(jvmtiEnv* jvmti, const char* options) override;
        void shutdown() override;
        
    private:
        // Module state
    };
}
```

Create implementation file `native/modules/your_module.cpp`:

```cpp
#include "your_module.h"

namespace jvmtool {
    std::string YourModule::getName() const {
        return "your_module";
    }
    
    bool YourModule::initialize(jvmtiEnv* jvmti, const char* options) {
        // Initialization logic
        return true;
    }
    
    void YourModule::shutdown() {
        // Cleanup logic
    }
}

// Module registration
static YourModule your_module_instance;
static bool registered = []() {
    AgentManager::instance().registerModule(&your_module_instance);
    return true;
}();
```

Update `native/CMakeLists.txt` to add the new module.

### 3. Extending Communication Protocol

For new message types, update both:

**C++ side** (`native/include/file_protocol.h`):
```cpp
enum class MessageType {
    // Existing types...
    YOUR_NEW_TYPE = 5
};
```

**Go side** (`internal/jt_protocol.go`):
```go
const (
    // Existing types...
    YourNewMessage MessageType = 5
)
```

## 🎨 Code Style

### Comment Guidelines

**Philosophy**: Comments should be minimal and strategic, focusing only on essential explanations.

**Rules**:
- **Language**: All comments must be in English
- **Frequency**: Comment only at key locations, not every line
- **Purpose**: Explain "why" not "what" - the code should be self-explanatory
- **Quality over Quantity**: One good comment is better than ten redundant ones

**When to Comment**:
- Complex algorithms or business logic
- Non-obvious performance optimizations
- Error handling strategies
- Public API documentation
- Architecture decisions at critical junctions

**When NOT to Comment**:
- Self-evident code (e.g., `i++` or simple variable assignments)
- Repeating what the code already says
- Temporary debugging information
- Obvious getter/setter methods

### Go Code Style

- Use `gofmt` to format code
- Follow [Effective Go](https://golang.org/doc/effective_go.html) guidelines
- Use camelCase for function names
- Use lowercase for package names
- Add documentation comments for exported functions and types

```go
// ProcessJavaProcess handles JVM process attachment with retry mechanism.
// Returns attachment result and any critical errors encountered.
func ProcessJavaProcess(pid int, agentPath string) (AttachResult, error) {
    if agentPath == "" {
        return AttachResult{}, fmt.Errorf("agent path cannot be empty")
    }
    
    // Retry attachment up to 3 times for transient failures
    for attempt := 1; attempt <= maxRetries; attempt++ {
        result, err := attemptAttachment(pid, agentPath)
        if err == nil {
            return result, nil
        }
        
        if !isRetryableError(err) {
            return AttachResult{}, err
        }
        
        time.Sleep(time.Duration(attempt) * time.Second)
    }
    
    return AttachResult{}, fmt.Errorf("attachment failed after %d attempts", maxRetries)
}
```

### C++ Code Style

- Use 4 spaces for indentation
- Use PascalCase for class names
- Use camelCase for function names
- Use snake_case for variable names
- Use RAII for resource management
- Apply same comment philosophy as Go code

```cpp
class FileProtocol {
public:
    FileProtocol(const std::string& base_path) : base_path_(base_path) {}
    
    // Thread-safe message sending with automatic retry on transient failures
    bool sendMessage(const Message& msg) {
        std::lock_guard<std::mutex> lock(file_mutex_);
        
        try {
            auto formatted = formatter_->format(msg);
            *output_file_ << formatted << std::flush;
            return output_file_->good();
        } catch (const std::exception& e) {
            // Log critical errors but don't throw - caller handles failure
            return false;
        }
    }
    
private:
    std::string base_path_;
    std::unique_ptr<std::ofstream> output_file_;
    std::mutex file_mutex_;  // Protects file operations across threads
};
```

### Comment Examples

**Good Comments** (English, explain why/how):
```go
// Use exponential backoff to avoid overwhelming the target JVM
time.Sleep(time.Duration(attempt*attempt) * 100 * time.Millisecond)

// JVMTI requires this specific capability set for memory profiling
jvmtiCapabilities caps = {0};
caps.can_get_object_size = 1;
```

**Bad Comments** (avoid these):
```go
// Increment counter by 1
counter++

// Set name to empty string  
name = ""

// Call the function
result := processData()
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
