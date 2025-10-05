package internal

import (
	"encoding/binary"
	"errors"
	"net"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

type mockStreamHandler struct {
	statuses      []string
	data          []string
	completeErr   error
	completeCalls int
	statusErr     error
	dataErr       error
}

func (h *mockStreamHandler) OnStatus(message string) error {
	h.statuses = append(h.statuses, message)
	return h.statusErr
}

func (h *mockStreamHandler) OnData(message string) error {
	h.data = append(h.data, message)
	return h.dataErr
}

func (h *mockStreamHandler) OnComplete(err error) error {
	h.completeCalls++
	h.completeErr = err
	return nil
}

func TestReadMessagesDelegatesToHandler(t *testing.T) {
	client, server := net.Pipe()
	defer client.Close()

	handler := &mockStreamHandler{}
	done := make(chan error, 1)

	go func() {
		done <- readMessages(client, time.Now().Add(2*time.Second), handler)
	}()

	go func() {
		defer server.Close()
		writeNativeMessage(t, server, nativeContentStatus, "Starting analysis\n")
		writeNativeMessage(t, server, nativeContentData, "line1\nline2\n")
		writeNativeMessage(t, server, nativeContentStatus, "=== End Analysis ===\n")
	}()

	err := <-done
	require.NoError(t, err)
	assert.Equal(t, []string{"Starting analysis", "=== End Analysis ==="}, handler.statuses)
	assert.Equal(t, []string{"line1\nline2\n"}, handler.data)
	assert.Equal(t, 1, handler.completeCalls)
	assert.NoError(t, handler.completeErr)
}

func TestReadMessagesPropagatesHandlerError(t *testing.T) {
	client, server := net.Pipe()
	defer client.Close()
	defer server.Close()

	handler := &mockStreamHandler{statusErr: errors.New("handler failure")}
	done := make(chan error, 1)

	go func() {
		done <- readMessages(client, time.Now().Add(2*time.Second), handler)
	}()

	writeNativeMessage(t, server, nativeContentStatus, "Starting\n")

	err := <-done
	require.Error(t, err)
	assert.EqualError(t, err, "handler failure")
	assert.Equal(t, handler.completeErr, err)
}

func writeNativeMessage(t *testing.T, conn net.Conn, contentType uint8, payload string) {
	t.Helper()
	hdr := nativeMessageHeader{
		Version:       1,
		AgentType:     0,
		ContentType:   contentType,
		Reserved:      0,
		ContentLength: uint32(len(payload)),
	}

	deadline := time.Now().Add(500 * time.Millisecond)
	_ = conn.SetWriteDeadline(deadline)

	require.NoError(t, binary.Write(conn, binary.LittleEndian, hdr))
	if len(payload) > 0 {
		_, err := conn.Write([]byte(payload))
		require.NoError(t, err)
	}
}
