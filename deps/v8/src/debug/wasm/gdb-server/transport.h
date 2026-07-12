// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_WASM_GDB_SERVER_TRANSPORT_H_
#define V8_DEBUG_WASM_GDB_SERVER_TRANSPORT_H_

#include <sstream>

#include "src/base/macros.h"
#include "src/debug/wasm/gdb-server/gdb-remote-util.h"

#if _WIN32
#include <windows.h>
#include <winsock2.h>

typedef SOCKET SocketHandle;

#define CloseSocket closesocket
#define InvalidSocket INVALID_SOCKET
#define SocketGetLastError() WSAGetLastError()
static const int kErrInterrupt = WSAEINTR;
typedef int ssize_t;
typedef int socklen_t;

#else  // _WIN32

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

typedef int SocketHandle;

#define CloseSocket close
#define InvalidSocket (-1)
#define SocketGetLastError() errno
static const int kErrInterrupt = EINTR;

#endif  // _WIN32

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

class SocketTransport;

// Acts as a factory for Transport objects bound to a specified TCP port.
class SocketBinding {
 public:
  // Wrap existing socket handle.
  explicit SocketBinding(SocketHandle socket_handle);

  // Bind to the specified TCP port.
  static SocketBinding Bind(uint16_t tcp_port);

  bool IsValid() const { return socket_handle_ != InvalidSocket; }

  // Create a transport object from this socket binding
  std::unique_ptr<SocketTransport> CreateTransport();

  // Get port the socket is bound to.
  uint16_t GetBoundPort();

 private:
  SocketHandle socket_handle_;
};

class V8_EXPORT_PRIVATE TransportBase {
 public:
  virtual ~TransportBase() {}

  // Waits for an incoming connection on the bound port.
  virtual bool AcceptConnection() = 0;

  // Read {len} bytes from this transport, possibly blocking until enough data
  // is available.
  // {dst} must point to a buffer large enough to contain {len} bytes.
  // Returns true on success.
  // Returns false if the connection is closed; in that case the {dst} may have
  // been partially overwritten.
  virtual bool Read(char* dst, int32_t len) = 0;

  // Write {len} bytes to this transport.
  // Return true on success, false if the connection is closed.
  virtual bool Write(const char* src, int32_t len) = 0;

  // Return true if there is data to read.
  virtual bool IsDataAvailable() const = 0;

  // If we are connected to a debugger, gracefully closes the connection.
  // This should be called when a debugging session gets closed.
  virtual void Disconnect() = 0;

  // Shuts down this transport, gracefully closing the existing connection and
  // also closing the listening socket. This should be called when the GDB stub
  // shuts down, when the program terminates.
  virtual void Close() = 0;

  // Blocks waiting for one of these two events to occur:
  // - A network event (a new packet arrives, or the connection is dropped),
  // - A thread event is signaled (the execution stopped because of a trap or
  // breakpoint).
  virtual void WaitForDebugStubEvent() = 0;

  // Signal that this transport should leave an alertable wait state because
  // the execution of the debuggee was stopped because of a trap or breakpoint.
  virtual bool SignalThreadEvent() = 0;
};

class Transport : public TransportBase {
 public:
  explicit Transport(SocketHandle s);
  ~Transport() override;

  // TransportBase
  bool Read(char* dst, int32_t len) override;
  bool Write(const char* src, int32_t len) override;
  bool IsDataAvailable() const override;
  void Disconnect() override;
  void Close() override;

  static const int kBufSize = 4096;

 protected:
  // Copy buffered data to *dst up to len bytes and update dst and len.
  void CopyFromBuffer(char** dst, int32_t* len);

  // Read available data from the socket. Return false on EOF or error.
  virtual bool ReadSomeData() = 0;

  std::unique_ptr<char[]> buf_;
  int32_t pos_;
  int32_t size_;
  SocketHandle handle_bind_;
  SocketHandle handle_accept_;
};

#if _WIN32

class SocketTransport : public Transport {
 public:
  explicit SocketTransport(SocketHandle s);
  ~SocketTransport() override;
  SocketTransport(const SocketTransport&) = delete;
  SocketTransport& operator=(const SocketTransport&) = delete;

  // TransportBase
  bool AcceptConnection() override;
  void Disconnect() override;
  void WaitForDebugStubEvent() override;
  bool SignalThreadEvent() override;

 private:
  bool ReadSomeData() override;

  HANDLE socket_event_;
  HANDLE faulted_thread_event_;
};

#else  // _WIN32

class SocketTransport : public Transport {
 public:
  explicit SocketTransport(SocketHandle s);
  ~SocketTransport() override;
  SocketTransport(const SocketTransport&) = delete;
  SocketTransport& operator=(const SocketTransport&) = delete;

  // TransportBase
  bool AcceptConnection() override;
  void WaitForDebugStubEvent() override;
  bool SignalThreadEvent() override;

 private:
  bool ReadSomeData() override;

  int faulted_thread_fd_read_;
  int faulted_thread_fd_write_;
};

#endif  // _WIN32

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_TRANSPORT_H_
