# JVMTool Project Documentation

A Go-based application with C++ JVMTI agents for comprehensive JVM monitoring and analysis.

## 🚀 Project Overview

**JVMTool** combines Go's ecosystem with native C++ JVMTI agents for JVM monitoring, profiling, and analysis.

### Core Features
- **Multi-command Interface**: `jps`, `jattach`, `sa` commands
- **Cross-platform**: Linux, macOS
- **JVMTI Integration**: Native JVM introspection
- **File-based Protocol**: Go ↔ C++ communication via `.jt` files
- **Modular Architecture**: Extensible agent system

## 🏗️ Architecture

### Application Structure
```
Go Layer (cmd/, pkg/, internal/)     Native Layer (native/)
├── Command Interface               ├── JVMTI Agent Framework
├── Public Libraries               ├── Analysis Modules  
├── Internal Implementation        └── Communication Protocol
└── Protocol Client                
```

### Key Components

**Go Application**:
- `cmd/main.go`: Command dispatch and execution
- `pkg/`: Cross-platform utilities (process, agent validation)
- `internal/`: Core logic (jps, jattach, sa, protocol)

**Native Agents**:
- `native/include/`: Agent framework headers
- `native/src/`: Core agent implementation
- `native/modules/`: Analysis modules (memory_sa_agent.cpp)

## 🔄 Communication Protocol

### Protocol Architecture
```
Go Application  <--  .jt files  -->  C++ JVMTI Agent
     ↓                                      ↓
JTProtocolReader                    FileProtocol
     ↓                                      ↓
AgentMessage                          Message
```

### Message Types
```cpp
// C++ Side
enum class MessageType {
    STATUS = 0, ERROR = 1, DATA = 2, PROGRESS = 3, RESULT = 4
};
enum class StatusCode {
    SUCCESS = 0, ERROR = 1, RUNNING = 2, COMPLETED = 3
};
```

```go
// Go Side
type AgentMessage struct {
    Type      MessageType
    Status    StatusCode
    Content   string
    Timestamp string
    Metadata  map[string]string
}
```

### Protocol Flow
1. Go creates unique `.jt` file path
2. C++ agent receives path via options
3. Agent writes structured messages
4. Go polls file for updates
5. Automatic cleanup on completion

## 🔧 Build & Dependencies

### Go Dependencies
```go
module github.com/XHao/jvmtool

require (
    github.com/shirou/gopsutil v2.21.11+incompatible  // Process info
    github.com/stretchr/testify v1.10.0               // Testing
    golang.org/x/sys v0.33.0                          // System calls
)
```

### Build Requirements
- **CMake 3.10+**, **JDK with JNI headers**, **C++11+ compiler**
- **GoogleTest** (auto-downloaded for testing)

### Build Commands
```bash
make all            # Build everything
make build-go       # Go application only  
make build-native   # C++ agents only
make test           # Run all tests
make package        # Create distribution
```

## 🎯 Core Functionality

### Commands
1. **`jps`**: Java process discovery with metadata
2. **`jattach`**: Dynamic JVMTI agent loading
3. **`sa`**: ServiceAbility agent for memory analysis

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

## 🧪 Testing & Development

### Test Coverage
- **Go Tests**: Unit tests (`*_test.go`) with testify framework
- **C++ Tests**: GoogleTest for native components  
- **Integration**: End-to-end command testing
- **Mocking**: Isolated component testing

### Development Workflow
1. `make dirs` → `make build` → `make test` → `make format`
2. **Extension Points**: New commands, agent modules, protocol types
3. **Platform Support**: OS-specific implementations

## 📋 Current Status

### Branch: `feature/agent` 
**Key Improvements**:
- ✅ Modular agent architecture with AgentModule interface
- ✅ Robust .jt file protocol for Go ↔ C++ communication  
- ✅ Cross-platform build system (Linux/macOS)
- ✅ Comprehensive testing with Go + GoogleTest
- ✅ Modern C++17 RAII patterns and Go 1.24+ generics

### Technical Highlights
- **Agent System**: Thread-safe module lifecycle with RawMonitor synchronization
- **Protocol**: Type-safe message parsing with metadata support
- **Build**: Unified Makefile + CMake with auto Java/JNI detection
- **Code Quality**: Strategic minimal English-only commenting philosophy

### Development Status (August 2025)
- ✅ Core functionality (jps, jattach, sa) working
- ✅ Agent architecture and protocol stability
- ✅ Testing coverage and documentation
- 🚧 Performance optimization and enhanced observability

---
**Updated**: 2025-08-01 | **Branch**: `feature/agent` | **Team**: XHao/jvmtool