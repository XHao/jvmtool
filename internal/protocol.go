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

// readMessages reads binary messages and forwards them to the supplied handler.
// Returns an error if the stream indicates an ERROR message or if IO fails.
func readMessages(conn net.Conn, until time.Time, handler SAStreamHandler) error {
	defer conn.Close()
	if handler == nil {
		handler = newDefaultStreamHandler(SAAgentOption{})
	}

	reader := bufio.NewReader(conn)
	var streamErr error
	defer func() {
		_ = handler.OnComplete(streamErr)
	}()

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
			err = fmt.Errorf("read header: %w", err)
			streamErr = err
			return err
		}

		if hdr.ContentLength == 0 {
			continue
		}

		buf := make([]byte, int(hdr.ContentLength))
		n, err := io.ReadFull(reader, buf)
		if err != nil {
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				// try next iteration; content may be partial, but in practice write is atomic
				continue
			}
			if strings.Contains(err.Error(), "EOF") {
				// best-effort deliver what we have
				if n > 0 {
					if handlerErr := handler.OnData(string(buf[:n])); handlerErr != nil {
						streamErr = handlerErr
						return handlerErr
					}
				}
				return nil
			}
			err = fmt.Errorf("read content: %w", err)
			streamErr = err
			return err
		}

		payload := string(buf)
		switch hdr.ContentType {
		case nativeContentStatus:
			message := strings.TrimRight(payload, "\n")
			if err := handler.OnStatus(message); err != nil {
				streamErr = err
				return err
			}
			// End-of-analysis marker from native
			if strings.Contains(message, "=== End Analysis ===") {
				streamErr = nil
				return nil
			}
		case nativeContentError:
			err := fmt.Errorf("agent error: %s", strings.TrimSpace(payload))
			streamErr = err
			return err
		case nativeContentData:
			if err := handler.OnData(payload); err != nil {
				streamErr = err
				return err
			}
		default:
			// Unknown type; ignore
		}
	}
}
