# JVMTool Project Analysis & Documentation

This document provides a comprehensive analysis of the JVMTool project, serving as a knowledge base for development and maintenance.

## 🚀 Project Overview

**JVMTool** is a sophisticated Go-based application that interfaces with native C++ JVMTI agents to provide comprehensive JVM monitoring, profiling, and analysis capabilities. It combines the performance of native code with the convenience of Go's ecosystem.

### Key Features
- **Multi-command Interface**: Supports `jps` (list Java processes), `jattach` (attach agents), and `sa` (ServiceAbility agent)
- **Cross-platform Support**: Linux, macOS, and Windows
- **JVMTI Integration**: Deep JVM introspection via native agents
- **File-based Protocol**: Robust communication between Go and C++ components
- **Modular Architecture**: Extensible agent system

## 🏗️ Project Structure

### Go Application Layer (`cmd/`, `pkg/`, `internal/`)

#### Command Interface (`cmd/`)
- **`main.go`**: Entry point with command dispatching
  - Generic command runner with type-safe flag parsing
  - Support for `jps`, `jattach`, and `sa` commands
  - Standardized error handling and exit codes

#### Public Libraries (`pkg/`)
- **`agent_validator.go`**: Cross-platform agent validation
- **`java_process.go`**: Java process discovery and management
- **`os.go`**: OS-specific utilities and abstractions
- **`user.go`**: User validation and privilege handling

#### Internal Implementation (`internal/`)
- **`jt_protocol.go`**: File-based communication protocol implementation
- **`jattach.go`**: Agent attachment functionality
- **`jps.go`**: Java process listing
- **`sa_agent.go`**: ServiceAbility agent operations
- **`jvm.go`**: JVM interaction utilities

### Native Agent Layer (`native/`)

#### Core Headers (`include/`)
- **`agent.h`**: JVMTI agent framework and module system
- **`file_protocol.h`**: Cross-language communication protocol
- **`message.h`**: Message serialization/deserialization
- **`writer.h`**: Efficient data output handling

#### Implementation (`src/`)
- **`agent.cpp`**: Main agent manager and module orchestration
- **`file_protocol.cpp`**: Protocol implementation
- **`message.cpp`**: Message handling
- **`build_info.cpp`**: Build metadata integration

#### Specialized Modules (`modules/`)
- **`memory_sa_agent.cpp`**: Memory analysis and ServiceAbility integration

### Build System
- **`Makefile`**: Cross-platform build orchestration
- **`CMakeLists.txt`**: Native code compilation with JNI integration
- **Custom CMake modules**: Java detection and JNI header resolution

## 🔄 Communication Protocol

### Architecture Overview
The project implements a sophisticated file-based IPC mechanism between Go and C++ components:

```
Go Application  <--  .jt files  -->  C++ JVMTI Agent
     ↓                                      ↓
JTProtocolReader                    FileProtocol
     ↓                                      ↓
AgentMessage                          Message
```

### Message Types & Status Codes

#### C++ Side (`file_protocol.h`)
```cpp
enum class MessageType {
    STATUS = 0,     // Agent lifecycle status
    ERROR = 1,      // Error reporting
    DATA = 2,       // Analysis data
    PROGRESS = 3,   // Operation progress
    RESULT = 4      // Final results
};

enum class StatusCode {
    SUCCESS = 0,    // Operation completed successfully
    ERROR = 1,      // Error occurred
    RUNNING = 2,    // Operation in progress
    COMPLETED = 3   // All operations finished
};
```

#### Go Side (`jt_protocol.go`)
```go
type AgentMessage struct {
    Type      MessageType
    Status    StatusCode
    Content   string
    Timestamp string
    Metadata  map[string]string
}
```

### Protocol Flow
1. **Initialization**: Go creates unique `.jt` file path
2. **Agent Launch**: C++ agent receives path via options
3. **Status Updates**: Agent writes structured messages
4. **Polling**: Go polls file for updates using `WaitForStatus`
5. **Data Exchange**: Bidirectional structured communication
6. **Cleanup**: Automatic file cleanup on completion

## 🔧 Build System & Dependencies

### Go Dependencies (`go.mod`)
```go
module github.com/XHao/jvmtool

require (
    github.com/shirou/gopsutil v2.21.11+incompatible  // System process info
    github.com/stretchr/testify v1.10.0               // Testing framework
    golang.org/x/sys v0.33.0                          // System calls
)
```

### Native Build Requirements
- **CMake 3.10+**: Cross-platform build system
- **JDK with JNI headers**: Java integration
- **C++11 compatible compiler**: Modern C++ features
- **GoogleTest**: Unit testing framework (auto-downloaded)

### Build Targets
```makefile
make all        # Build Go binary and native libraries
make build-go   # Go application only
make build-native  # C++ agents only
make test       # Run all tests
make package    # Create distribution packages
make install    # System-wide installation
```

## 🎯 Core Functionality

### 1. Java Process Discovery (`jps`)
- Cross-platform Java process enumeration
- User privilege validation
- Process metadata extraction
- Filtering and formatting options

### 2. Agent Attachment (`jattach`)
- Dynamic JVMTI agent loading
- Parameter passing and validation
- Attachment status monitoring
- Error handling and recovery

### 3. ServiceAbility Agent (`sa`)
- Memory analysis and profiling
- JVM state inspection
- Performance metrics collection
- Custom analysis modules

## 🔍 Key Implementation Details

### Agent Module System
```cpp
class AgentManager {
    static AgentManager& instance();
    void registerModule(AgentModule* module);
    void onAttach(JavaVM* vm, jvmtiEnv* jvmti, const char* options);
};
```

### Type-Safe Command Processing
```go
func runCommandWithFlags[T any](
    parseFunc func([]string) (T, error),
    execFunc func(T) int,
    args []string,
) int
```

### Cross-Platform Abstractions
- OS-specific implementations for Linux, macOS, Windows
- Unified interfaces for process management
- Platform-specific agent validation

## 🧪 Testing Strategy

### Test Coverage
- **Unit Tests**: Core logic and utilities (`*_test.go`)
- **Integration Tests**: End-to-end command testing
- **Native Tests**: C++ component validation (GoogleTest)
- **Mock Objects**: Isolated testing of components

### Test Organization
```
cmd/main_test.go              # Command interface tests
internal/*_test.go            # Internal logic tests
pkg/*_test.go                 # Public API tests
native/test/*_test.cpp        # C++ unit tests
```

## 🚀 Development Workflow

### Local Development
1. **Setup**: `make dirs` to create build directories
2. **Development**: Iterative `make build` cycles
3. **Testing**: `make test` for validation
4. **Formatting**: `make format` for code consistency

### Extension Points
- **New Commands**: Add to `main.go` switch statement
- **Agent Modules**: Implement `AgentModule` interface
- **Protocol Extensions**: Extend message types and handlers
- **Platform Support**: Add OS-specific implementations

## 📋 Project Status & Roadmap

### Current Branch: `feature/agent`
- Enhanced agent architecture
- Improved protocol stability
- Extended testing coverage

### Key Improvements Made
1. **Modular Agent System**: Pluggable analysis modules
2. **Robust Protocol**: Better error handling and status tracking
3. **Cross-Platform Support**: Unified build system
4. **Comprehensive Testing**: Unit and integration test suites
5. **Documentation**: Detailed API and usage documentation

---

**Last Updated**: 2025-07-31  
**Version**: Development Branch `feature/agent`  
**Maintainer**: XHao/jvmtool team