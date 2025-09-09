package internal

import (
	"bufio"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"strings"
	"time"

	"github.com/XHao/jvmtool/pkg"
)

// Native message types (must match native/include/message.h)
const (
	nativeContentStatus = 0
	nativeContentError  = 1
	nativeContentData   = 2
)

// nativeMessageHeader mirrors the packed C++ struct layout
//
//	struct MessageHeader {
//	  uint8_t version;      // 1
//	  uint8_t agent_type;   // 0..4
//	  uint8_t content_type; // 0..2
//	  uint8_t reserved;     // 0
//	  uint32_t content_length; // little-endian host order
//	} __attribute__((packed));
type nativeMessageHeader struct {
	Version       uint8
	AgentType     uint8
	ContentType   uint8
	Reserved      uint8
	ContentLength uint32
}

// connectSocket dials the native agent Unix domain socket with retry.
func connectSocket(socketPath string, timeout time.Duration) (net.Conn, error) {
	deadline := time.Now().Add(timeout)
	var lastErr error
	for time.Now().Before(deadline) {
		conn, err := net.DialTimeout("unix", socketPath, 500*time.Millisecond)
		if err == nil {
			return conn, nil
		}
		lastErr = err
		time.Sleep(100 * time.Millisecond)
	}
	if lastErr == nil {
		lastErr = fmt.Errorf("timeout connecting to %s", socketPath)
	}
	return nil, lastErr
}

// readMessages reads binary messages and forwards text payloads to stdout/pkg.Log.
// Returns an error if the stream indicates an ERROR message or if IO fails.
func readMessages(conn net.Conn, until time.Time) error {
	defer conn.Close()
	reader := bufio.NewReader(conn)

	for {
		// Respect overall deadline
		if !until.IsZero() && time.Now().After(until) {
			return nil
		}

		// Set short read deadline to periodically check timeouts
		_ = conn.SetReadDeadline(time.Now().Add(1 * time.Second))

		var hdr nativeMessageHeader
		if err := binary.Read(reader, binary.LittleEndian, &hdr); err != nil {
			// Map temporary timeouts to continue waiting
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				continue
			}
			// EOF or other errors end the stream
			if errors.Is(err, net.ErrClosed) || strings.Contains(err.Error(), "use of closed network connection") {
				return nil
			}
			if strings.Contains(err.Error(), "EOF") {
				return nil
			}
			return fmt.Errorf("read header: %w", err)
		}

		if hdr.ContentLength == 0 {
			continue
		}

		buf := make([]byte, int(hdr.ContentLength))
		if _, err := io.ReadFull(reader, buf); err != nil {
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				// try next iteration; content may be partial, but in practice write is atomic
				continue
			}
			if strings.Contains(err.Error(), "EOF") {
				// best-effort print what we have
				if len(buf) > 0 {
					fmt.Print(string(buf))
				}
				return nil
			}
			return fmt.Errorf("read content: %w", err)
		}

		payload := string(buf)
		switch hdr.ContentType {
		case nativeContentStatus:
			pkg.Log(strings.TrimRight(payload, "\n"))
			// End-of-analysis marker from native
			if strings.Contains(payload, "=== End Analysis ===") {
				return nil
			}
		case nativeContentError:
			return fmt.Errorf("agent error: %s", strings.TrimSpace(payload))
		case nativeContentData:
			// Data may contain multiple lines; print as-is
			fmt.Print(payload)
		default:
			// Unknown type; ignore
		}
	}
}
