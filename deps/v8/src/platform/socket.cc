// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/socket.h"

#if V8_OS_POSIX
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netdb.h>

#include <unistd.h>
#endif

#include <errno.h>

#include "checks.h"
#include "once.h"

namespace v8 {
namespace internal {

#if V8_OS_WIN

static V8_DECLARE_ONCE(initialize_winsock) = V8_ONCE_INIT;


static void InitializeWinsock() {
  WSADATA wsa_data;
  int result = WSAStartup(MAKEWORD(1, 0), &wsa_data);
  CHECK_EQ(0, result);
}

#endif  // V8_OS_WIN


Socket::Socket() {
#if V8_OS_WIN
  // Be sure to initialize the WinSock DLL first.
  CallOnce(&initialize_winsock, &InitializeWinsock);
#endif  // V8_OS_WIN

  // Create the native socket handle.
  native_handle_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}


bool Socket::Bind(int port) {
  ASSERT_GE(port, 0);
  ASSERT_LT(port, 65536);
  if (!IsValid()) return false;
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sin.sin_port = htons(static_cast<uint16_t>(port));
  int result = ::bind(
      native_handle_, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin));
  return result == 0;
}


bool Socket::Listen(int backlog) {
  if (!IsValid()) return false;
  int result = ::listen(native_handle_, backlog);
  return result == 0;
}


Socket* Socket::Accept() {
  if (!IsValid()) return NULL;
  while (true) {
    NativeHandle native_handle = ::accept(native_handle_, NULL, NULL);
    if (native_handle == kInvalidNativeHandle) {
#if V8_OS_POSIX
      if (errno == EINTR) continue;  // Retry after signal.
#endif
      return NULL;
    }
    return new Socket(native_handle);
  }
}


bool Socket::Connect(const char* host, const char* port) {
  ASSERT_NE(NULL, host);
  ASSERT_NE(NULL, port);
  if (!IsValid()) return false;

  // Lookup host and port.
  struct addrinfo* info = NULL;
  struct addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_INET;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_protocol = IPPROTO_TCP;
  int result = ::getaddrinfo(host, port, &hint, &info);
  if (result != 0) {
    return false;
  }

  // Connect to the host on the given port.
  for (struct addrinfo* ai = info; ai != NULL; ai = ai->ai_next) {
    // Try to connect using this addr info.
    while (true) {
      result = ::connect(
          native_handle_, ai->ai_addr, static_cast<int>(ai->ai_addrlen));
      if (result == 0) {
        freeaddrinfo(info);
        return true;
      }
#if V8_OS_POSIX
      if (errno == EINTR) continue;  // Retry after signal.
#endif
      break;
    }
  }
  freeaddrinfo(info);
  return false;
}


bool Socket::Shutdown() {
  if (!IsValid()) return false;
  // Shutdown socket for both read and write.
#if V8_OS_POSIX
  int result = ::shutdown(native_handle_, SHUT_RDWR);
  ::close(native_handle_);
#elif V8_OS_WIN
  int result = ::shutdown(native_handle_, SD_BOTH);
  ::closesocket(native_handle_);
#endif
  native_handle_ = kInvalidNativeHandle;
  return result == 0;
}


int Socket::Send(const char* buffer, int length) {
  ASSERT(length <= 0 || buffer != NULL);
  if (!IsValid()) return 0;
  int offset = 0;
  while (offset < length) {
    int result = ::send(native_handle_, buffer + offset, length - offset, 0);
    if (result == 0) {
      break;
    } else if (result > 0) {
      ASSERT(result <= length - offset);
      offset += result;
    } else {
#if V8_OS_POSIX
      if (errno == EINTR) continue;  // Retry after signal.
#endif
      return 0;
    }
  }
  return offset;
}


int Socket::Receive(char* buffer, int length) {
  if (!IsValid()) return 0;
  if (length <= 0) return 0;
  ASSERT_NE(NULL, buffer);
  while (true) {
    int result = ::recv(native_handle_, buffer, length, 0);
    if (result < 0) {
#if V8_OS_POSIX
      if (errno == EINTR) continue;  // Retry after signal.
#endif
      return 0;
    }
    return result;
  }
}


bool Socket::SetReuseAddress(bool reuse_address) {
  if (!IsValid()) return 0;
  int v = reuse_address ? 1 : 0;
  int result = ::setsockopt(native_handle_, SOL_SOCKET, SO_REUSEADDR,
                            reinterpret_cast<char*>(&v), sizeof(v));
  return result == 0;
}


// static
int Socket::GetLastError() {
#if V8_OS_POSIX
  return errno;
#elif V8_OS_WIN
  // Be sure to initialize the WinSock DLL first.
  CallOnce(&initialize_winsock, &InitializeWinsock);

  // Now we can safely perform WSA calls.
  return ::WSAGetLastError();
#endif
}

} }  // namespace v8::internal
