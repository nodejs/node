// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/wasm/gdb-server/transport.h"

#include <fcntl.h>

#include "src/base/platform/wrappers.h"

#ifndef SD_BOTH
#define SD_BOTH 2
#endif

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

SocketBinding::SocketBinding(SocketHandle socket_handle)
    : socket_handle_(socket_handle) {}

// static
SocketBinding SocketBinding::Bind(uint16_t tcp_port) {
  SocketHandle socket_handle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_handle == InvalidSocket) {
    TRACE_GDB_REMOTE("Failed to create socket.\n");
    return SocketBinding(InvalidSocket);
  }
  struct sockaddr_in sockaddr;
  // Clearing sockaddr_in first appears to be necessary on Mac OS X.
  memset(&sockaddr, 0, sizeof(sockaddr));
  socklen_t addrlen = static_cast<socklen_t>(sizeof(sockaddr));
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sockaddr.sin_port = htons(tcp_port);

#if _WIN32
  // On Windows, SO_REUSEADDR has a different meaning than on POSIX systems.
  // SO_REUSEADDR allows hijacking of an open socket by another process.
  // The SO_EXCLUSIVEADDRUSE flag prevents this behavior.
  // See:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms740621(v=vs.85).aspx
  //
  // Additionally, unlike POSIX, TCP server sockets can be bound to
  // ports in the TIME_WAIT state, without setting SO_REUSEADDR.
  int exclusive_address = 1;
  if (setsockopt(socket_handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                 reinterpret_cast<char*>(&exclusive_address),
                 sizeof(exclusive_address))) {
    TRACE_GDB_REMOTE("Failed to set SO_EXCLUSIVEADDRUSE option.\n");
  }
#else
  // On POSIX, this is necessary to ensure that the TCP port is released
  // promptly when sel_ldr exits.  Without this, the TCP port might
  // only be released after a timeout, and later processes can fail
  // to bind it.
  int reuse_address = 1;
  if (setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<char*>(&reuse_address),
                 sizeof(reuse_address))) {
    TRACE_GDB_REMOTE("Failed to set SO_REUSEADDR option.\n");
  }
#endif

  if (bind(socket_handle, reinterpret_cast<struct sockaddr*>(&sockaddr),
           addrlen)) {
    TRACE_GDB_REMOTE("Failed to bind server.\n");
    return SocketBinding(InvalidSocket);
  }

  if (listen(socket_handle, 1)) {
    TRACE_GDB_REMOTE("Failed to listen.\n");
    return SocketBinding(InvalidSocket);
  }
  return SocketBinding(socket_handle);
}

std::unique_ptr<SocketTransport> SocketBinding::CreateTransport() {
  return std::make_unique<SocketTransport>(socket_handle_);
}

uint16_t SocketBinding::GetBoundPort() {
  struct sockaddr_in saddr;
  struct sockaddr* psaddr = reinterpret_cast<struct sockaddr*>(&saddr);
  // Clearing sockaddr_in first appears to be necessary on Mac OS X.
  memset(&saddr, 0, sizeof(saddr));
  socklen_t addrlen = static_cast<socklen_t>(sizeof(saddr));
  if (::getsockname(socket_handle_, psaddr, &addrlen)) {
    TRACE_GDB_REMOTE("Failed to retrieve bound address.\n");
    return 0;
  }
  return ntohs(saddr.sin_port);
}

// Do not delay sending small packets.  This significantly speeds up
// remote debugging.  Debug stub uses buffering to send outgoing packets
// so they are not split into more TCP packets than necessary.
void DisableNagleAlgorithm(SocketHandle socket) {
  int nodelay = 1;
  if (::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<char*>(&nodelay), sizeof(nodelay))) {
    TRACE_GDB_REMOTE("Failed to set TCP_NODELAY option.\n");
  }
}

Transport::Transport(SocketHandle s)
    : buf_(new char[kBufSize]),
      pos_(0),
      size_(0),
      handle_bind_(s),
      handle_accept_(InvalidSocket) {}

Transport::~Transport() {
  if (handle_accept_ != InvalidSocket) {
    CloseSocket(handle_accept_);
  }
}

void Transport::CopyFromBuffer(char** dst, int32_t* len) {
  int32_t copy_bytes = std::min(*len, size_ - pos_);
  base::Memcpy(*dst, buf_.get() + pos_, copy_bytes);
  pos_ += copy_bytes;
  *len -= copy_bytes;
  *dst += copy_bytes;
}

bool Transport::Read(char* dst, int32_t len) {
  if (pos_ < size_) {
    CopyFromBuffer(&dst, &len);
  }
  while (len > 0) {
    pos_ = 0;
    size_ = 0;
    if (!ReadSomeData()) {
      return false;
    }
    CopyFromBuffer(&dst, &len);
  }
  return true;
}

bool Transport::Write(const char* src, int32_t len) {
  while (len > 0) {
    ssize_t result = ::send(handle_accept_, src, len, 0);
    if (result > 0) {
      src += result;
      len -= result;
      continue;
    }
    if (result == 0) {
      return false;
    }
    if (SocketGetLastError() != kErrInterrupt) {
      return false;
    }
  }
  return true;
}

// Return true if there is data to read.
bool Transport::IsDataAvailable() const {
  if (pos_ < size_) {
    return true;
  }
  fd_set fds;

  FD_ZERO(&fds);
  FD_SET(handle_accept_, &fds);

  // We want a "non-blocking" check
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  // Check if this file handle can select on read
  int cnt = select(static_cast<int>(handle_accept_) + 1, &fds, 0, 0, &timeout);

  // If we are ready, or if there is an error.  We return true
  // on error, to let the next IO request fail.
  if (cnt != 0) return true;

  return false;
}

void Transport::Close() {
  ::shutdown(handle_bind_, SD_BOTH);
  CloseSocket(handle_bind_);
  Disconnect();
}

void Transport::Disconnect() {
  if (handle_accept_ != InvalidSocket) {
    // Shutdown the connection in both directions.  This should
    // always succeed, and nothing we can do if this fails.
    ::shutdown(handle_accept_, SD_BOTH);
    CloseSocket(handle_accept_);
    handle_accept_ = InvalidSocket;
  }
}

#if _WIN32

SocketTransport::SocketTransport(SocketHandle s) : Transport(s) {
  socket_event_ = WSA_INVALID_EVENT;
  faulted_thread_event_ = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  if (faulted_thread_event_ == NULL) {
    TRACE_GDB_REMOTE(
        "SocketTransport::SocketTransport: Failed to create event object for "
        "faulted thread\n");
  }
}

SocketTransport::~SocketTransport() {
  if (!CloseHandle(faulted_thread_event_)) {
    TRACE_GDB_REMOTE(
        "SocketTransport::~SocketTransport: Failed to close "
        "event\n");
  }

  if (socket_event_) {
    if (!::WSACloseEvent(socket_event_)) {
      TRACE_GDB_REMOTE(
          "SocketTransport::~SocketTransport: Failed to close "
          "socket event\n");
    }
  }
}

bool SocketTransport::AcceptConnection() {
  CHECK(handle_accept_ == InvalidSocket);
  handle_accept_ = ::accept(handle_bind_, NULL, 0);
  if (handle_accept_ != InvalidSocket) {
    DisableNagleAlgorithm(handle_accept_);

    // Create socket event
    socket_event_ = ::WSACreateEvent();
    if (socket_event_ == WSA_INVALID_EVENT) {
      TRACE_GDB_REMOTE(
          "SocketTransport::AcceptConnection: Failed to create socket event\n");
    }

    // Listen for close events in order to handle them correctly.
    // Additionally listen for read readiness as WSAEventSelect sets the socket
    // to non-blocking mode.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms738547(v=vs.85).aspx
    if (::WSAEventSelect(handle_accept_, socket_event_, FD_CLOSE | FD_READ) ==
        SOCKET_ERROR) {
      TRACE_GDB_REMOTE(
          "SocketTransport::AcceptConnection: Failed to bind event to "
          "socket\n");
    }
    return true;
  }
  return false;
}

bool SocketTransport::ReadSomeData() {
  while (true) {
    ssize_t result =
        ::recv(handle_accept_, buf_.get() + size_, kBufSize - size_, 0);
    if (result > 0) {
      size_ += result;
      return true;
    }
    if (result == 0) {
      return false;  // The connection was gracefully closed.
    }
    // WSAEventSelect sets socket to non-blocking mode. This is essential
    // for socket event notification to work, there is no workaround.
    // See remarks section at the page
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms741576(v=vs.85).aspx
    if (SocketGetLastError() == WSAEWOULDBLOCK) {
      if (::WaitForSingleObject(socket_event_, INFINITE) == WAIT_FAILED) {
        TRACE_GDB_REMOTE(
            "SocketTransport::ReadSomeData: Failed to wait on socket event\n");
      }
      if (!::ResetEvent(socket_event_)) {
        TRACE_GDB_REMOTE(
            "SocketTransport::ReadSomeData: Failed to reset socket event\n");
      }
      continue;
    }

    if (SocketGetLastError() != kErrInterrupt) {
      return false;
    }
  }
}

void SocketTransport::WaitForDebugStubEvent() {
  // Don't wait if we already have data to read.
  bool wait = !(pos_ < size_);

  HANDLE handles[2];
  handles[0] = faulted_thread_event_;
  handles[1] = socket_event_;
  int count = size_ < kBufSize ? 2 : 1;
  int result =
      WaitForMultipleObjects(count, handles, FALSE, wait ? INFINITE : 0);
  if (result == WAIT_OBJECT_0 + 1) {
    if (!ResetEvent(socket_event_)) {
      TRACE_GDB_REMOTE(
          "SocketTransport::WaitForDebugStubEvent: Failed to reset socket "
          "event\n");
    }
    return;
  } else if (result == WAIT_OBJECT_0) {
    if (!ResetEvent(faulted_thread_event_)) {
      TRACE_GDB_REMOTE(
          "SocketTransport::WaitForDebugStubEvent: Failed to reset event\n");
    }
    return;
  } else if (result == WAIT_TIMEOUT) {
    return;
  }
  TRACE_GDB_REMOTE(
      "SocketTransport::WaitForDebugStubEvent: Wait for events failed\n");
}

bool SocketTransport::SignalThreadEvent() {
  if (!SetEvent(faulted_thread_event_)) {
    return false;
  }
  return true;
}

void SocketTransport::Disconnect() {
  Transport::Disconnect();

  if (socket_event_ != WSA_INVALID_EVENT && !::WSACloseEvent(socket_event_)) {
    TRACE_GDB_REMOTE(
        "SocketTransport::~SocketTransport: Failed to close "
        "socket event\n");
  }
  socket_event_ = WSA_INVALID_EVENT;
  SignalThreadEvent();
}

#else  // _WIN32

SocketTransport::SocketTransport(SocketHandle s) : Transport(s) {
  int fds[2];
#if defined(__linux__)
  int ret = pipe2(fds, O_CLOEXEC);
#else
  int ret = pipe(fds);
#endif
  if (ret < 0) {
    TRACE_GDB_REMOTE(
        "SocketTransport::SocketTransport: Failed to allocate pipe for faulted "
        "thread\n");
  }
  faulted_thread_fd_read_ = fds[0];
  faulted_thread_fd_write_ = fds[1];
}

SocketTransport::~SocketTransport() {
  if (close(faulted_thread_fd_read_) != 0) {
    TRACE_GDB_REMOTE(
        "SocketTransport::~SocketTransport: Failed to close "
        "event\n");
  }
  if (close(faulted_thread_fd_write_) != 0) {
    TRACE_GDB_REMOTE(
        "SocketTransport::~SocketTransport: Failed to close "
        "event\n");
  }
}

bool SocketTransport::AcceptConnection() {
  CHECK(handle_accept_ == InvalidSocket);
  handle_accept_ = ::accept(handle_bind_, NULL, 0);
  if (handle_accept_ != InvalidSocket) {
    DisableNagleAlgorithm(handle_accept_);
    return true;
  }
  return false;
}

bool SocketTransport::ReadSomeData() {
  while (true) {
    ssize_t result =
        ::recv(handle_accept_, buf_.get() + size_, kBufSize - size_, 0);
    if (result > 0) {
      size_ += result;
      return true;
    }
    if (result == 0) {
      return false;  // The connection was gracefully closed.
    }
    if (SocketGetLastError() != kErrInterrupt) {
      return false;
    }
  }
}

void SocketTransport::WaitForDebugStubEvent() {
  // Don't wait if we already have data to read.
  bool wait = !(pos_ < size_);

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(faulted_thread_fd_read_, &fds);
  int max_fd = faulted_thread_fd_read_;
  if (size_ < kBufSize) {
    FD_SET(handle_accept_, &fds);
    max_fd = std::max(max_fd, handle_accept_);
  }

  int ret;
  // We don't need sleep-polling on Linux now, so we set either zero or infinite
  // timeout.
  if (wait) {
    ret = select(max_fd + 1, &fds, NULL, NULL, NULL);
  } else {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    ret = select(max_fd + 1, &fds, NULL, NULL, &timeout);
  }
  if (ret < 0) {
    TRACE_GDB_REMOTE(
        "SocketTransport::WaitForDebugStubEvent: Failed to wait for "
        "debug stub event\n");
  }

  if (ret > 0) {
    if (FD_ISSET(faulted_thread_fd_read_, &fds)) {
      char buf[16];
      if (read(faulted_thread_fd_read_, &buf, sizeof(buf)) < 0) {
        TRACE_GDB_REMOTE(
            "SocketTransport::WaitForDebugStubEvent: Failed to read from "
            "debug stub event pipe fd\n");
      }
    }
    if (FD_ISSET(handle_accept_, &fds)) ReadSomeData();
  }
}

bool SocketTransport::SignalThreadEvent() {
  // Notify the debug stub by marking the thread as faulted.
  char buf = 0;
  if (write(faulted_thread_fd_write_, &buf, sizeof(buf)) != sizeof(buf)) {
    TRACE_GDB_REMOTE(
        "SocketTransport:SignalThreadEvent: Can't send debug stub "
        "event\n");
    return false;
  }
  return true;
}

#endif  // _WIN32

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef SD_BOTH
