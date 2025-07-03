package internal

import "os"

// globalLogger is the global logger instance used by the log and logInit functions.
var globalLogger *Logger

// logInit initializes the global logger with the specified output function.
// If outputFunc is nil, it defaults to pretty console output.
func logInit(outputFunc func(msg string)) {
	globalLogger = NewLogger(outputFunc)
}

// log logs a message using the global logger.
// If the global logger is not initialized, it initializes it with the default output.
var log = func(msg string) {
	if globalLogger == nil {
		globalLogger = NewLogger(nil)
	}
	globalLogger.Print(msg)
}

// Logger is a configurable logging utility. By default, it outputs to the console in a pretty format.
type Logger struct {
	outputFunc func(msg string)
}

// NewLogger creates a new Logger with the specified output function.
// If outputFunc is nil, it defaults to pretty console output.
func NewLogger(outputFunc func(msg string)) *Logger {
	if outputFunc == nil {
		outputFunc = func(msg string) {
			println(msg)
		}
	}
	return &Logger{outputFunc: outputFunc}
}

// Print logs a message using the configured output function.
func (l *Logger) Print(msg string) {
	l.outputFunc(msg)
}

// FileOutputFunc returns an output function that writes log messages to the specified file path, overwriting the file if it exists.
func FileOutputFunc(filePath string) func(msg string) {
	return func(msg string) {
		f, err := openLogFile(filePath)
		if err != nil {
			println("Logger error:", err.Error())
			println(msg)
			return
		}
		defer f.Close()
		f.WriteString(msg + "\n")
	}
}

// openLogFile opens or creates the log file in write-only mode, truncating it if it already exists.
func openLogFile(filePath string) (*os.File, error) {
	return os.OpenFile(filePath, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0644)
}
