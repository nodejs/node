// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_PLATFORM_SOCKET_H_
#define V8_PLATFORM_SOCKET_H_

#include "globals.h"
#if V8_OS_WIN
#include "win32-headers.h"
#endif

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Socket
//

class Socket V8_FINAL {
 public:
  Socket();
  ~Socket() { Shutdown(); }

  // Server initialization.
  bool Bind(int port) V8_WARN_UNUSED_RESULT;
  bool Listen(int backlog) V8_WARN_UNUSED_RESULT;
  Socket* Accept() V8_WARN_UNUSED_RESULT;

  // Client initialization.
  bool Connect(const char* host, const char* port) V8_WARN_UNUSED_RESULT;

  // Shutdown socket for both read and write. This causes blocking Send and
  // Receive calls to exit. After |Shutdown()| the Socket object cannot be
  // used for any communication.
  bool Shutdown();

  // Data Transimission
  // Return 0 on failure.
  int Send(const char* buffer, int length) V8_WARN_UNUSED_RESULT;
  int Receive(char* buffer, int length) V8_WARN_UNUSED_RESULT;

  // Set the value of the SO_REUSEADDR socket option.
  bool SetReuseAddress(bool reuse_address);

  V8_INLINE bool IsValid() const {
    return native_handle_ != kInvalidNativeHandle;
  }

  static int GetLastError();

  // The implementation-defined native handle type.
#if V8_OS_POSIX
  typedef int NativeHandle;
  static const NativeHandle kInvalidNativeHandle = -1;
#elif V8_OS_WIN
  typedef SOCKET NativeHandle;
  static const NativeHandle kInvalidNativeHandle = INVALID_SOCKET;
#endif

  NativeHandle& native_handle() {
    return native_handle_;
  }
  const NativeHandle& native_handle() const {
    return native_handle_;
  }

 private:
  explicit Socket(NativeHandle native_handle) : native_handle_(native_handle) {}

  NativeHandle native_handle_;

  DISALLOW_COPY_AND_ASSIGN(Socket);
};

} }  // namespace v8::internal

#endif  // V8_PLATFORM_SOCKET_H_
