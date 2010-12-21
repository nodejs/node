// Copyright 2009 the V8 project authors. All rights reserved.
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

// Platform specific code for POSIX goes here. This is not a platform on its
// own but contains the parts which are the same across POSIX platforms Linux,
// Mac OS, FreeBSD and OpenBSD.

#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#if defined(ANDROID)
#define LOG_TAG "v8"
#include <utils/Log.h>  // LOG_PRI_VA
#endif

#include "v8.h"

#include "platform.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Math functions

double modulo(double x, double y) {
  return fmod(x, y);
}


double OS::nan_value() {
  // NAN from math.h is defined in C99 and not in POSIX.
  return NAN;
}


// ----------------------------------------------------------------------------
// POSIX date/time support.
//

int OS::GetUserTime(uint32_t* secs,  uint32_t* usecs) {
  struct rusage usage;

  if (getrusage(RUSAGE_SELF, &usage) < 0) return -1;
  *secs = usage.ru_utime.tv_sec;
  *usecs = usage.ru_utime.tv_usec;
  return 0;
}


double OS::TimeCurrentMillis() {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) < 0) return 0.0;
  return (static_cast<double>(tv.tv_sec) * 1000) +
         (static_cast<double>(tv.tv_usec) / 1000);
}


int64_t OS::Ticks() {
  // gettimeofday has microsecond resolution.
  struct timeval tv;
  if (gettimeofday(&tv, NULL) < 0)
    return 0;
  return (static_cast<int64_t>(tv.tv_sec) * 1000000) + tv.tv_usec;
}


double OS::DaylightSavingsOffset(double time) {
  if (isnan(time)) return nan_value();
  time_t tv = static_cast<time_t>(floor(time/msPerSecond));
  struct tm* t = localtime(&tv);
  if (NULL == t) return nan_value();
  return t->tm_isdst > 0 ? 3600 * msPerSecond : 0;
}


int OS::GetLastError() {
  return errno;
}


// ----------------------------------------------------------------------------
// POSIX stdio support.
//

FILE* OS::FOpen(const char* path, const char* mode) {
  return fopen(path, mode);
}


const char* OS::LogFileOpenMode = "w";


void OS::Print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  VPrint(format, args);
  va_end(args);
}


void OS::VPrint(const char* format, va_list args) {
#if defined(ANDROID)
  LOG_PRI_VA(ANDROID_LOG_INFO, LOG_TAG, format, args);
#else
  vprintf(format, args);
#endif
}


void OS::FPrint(FILE* out, const char* format, ...) {
  va_list args;
  va_start(args, format);
  VFPrint(out, format, args);
  va_end(args);
}


void OS::VFPrint(FILE* out, const char* format, va_list args) {
#if defined(ANDROID)
  LOG_PRI_VA(ANDROID_LOG_INFO, LOG_TAG, format, args);
#else
  vfprintf(out, format, args);
#endif
}


void OS::PrintError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  VPrintError(format, args);
  va_end(args);
}


void OS::VPrintError(const char* format, va_list args) {
#if defined(ANDROID)
  LOG_PRI_VA(ANDROID_LOG_ERROR, LOG_TAG, format, args);
#else
  vfprintf(stderr, format, args);
#endif
}


int OS::SNPrintF(Vector<char> str, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VSNPrintF(str, format, args);
  va_end(args);
  return result;
}


int OS::VSNPrintF(Vector<char> str,
                  const char* format,
                  va_list args) {
  int n = vsnprintf(str.start(), str.length(), format, args);
  if (n < 0 || n >= str.length()) {
    // If the length is zero, the assignment fails.
    if (str.length() > 0)
      str[str.length() - 1] = '\0';
    return -1;
  } else {
    return n;
  }
}


// ----------------------------------------------------------------------------
// POSIX string support.
//

char* OS::StrChr(char* str, int c) {
  return strchr(str, c);
}


void OS::StrNCpy(Vector<char> dest, const char* src, size_t n) {
  strncpy(dest.start(), src, n);
}


// ----------------------------------------------------------------------------
// POSIX socket support.
//

class POSIXSocket : public Socket {
 public:
  explicit POSIXSocket() {
    // Create the socket.
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (IsValid()) {
      // Allow rapid reuse.
      static const int kOn = 1;
      int ret = setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                           &kOn, sizeof(kOn));
      ASSERT(ret == 0);
      USE(ret);
    }
  }
  explicit POSIXSocket(int socket): socket_(socket) { }
  virtual ~POSIXSocket() { Shutdown(); }

  // Server initialization.
  bool Bind(const int port);
  bool Listen(int backlog) const;
  Socket* Accept() const;

  // Client initialization.
  bool Connect(const char* host, const char* port);

  // Shutdown socket for both read and write.
  bool Shutdown();

  // Data Transimission
  int Send(const char* data, int len) const;
  int Receive(char* data, int len) const;

  bool SetReuseAddress(bool reuse_address);

  bool IsValid() const { return socket_ != -1; }

 private:
  int socket_;
};


bool POSIXSocket::Bind(const int port) {
  if (!IsValid())  {
    return false;
  }

  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  int status = bind(socket_,
                    BitCast<struct sockaddr *>(&addr),
                    sizeof(addr));
  return status == 0;
}


bool POSIXSocket::Listen(int backlog) const {
  if (!IsValid()) {
    return false;
  }

  int status = listen(socket_, backlog);
  return status == 0;
}


Socket* POSIXSocket::Accept() const {
  if (!IsValid()) {
    return NULL;
  }

  int socket = accept(socket_, NULL, NULL);
  if (socket == -1) {
    return NULL;
  } else {
    return new POSIXSocket(socket);
  }
}


bool POSIXSocket::Connect(const char* host, const char* port) {
  if (!IsValid()) {
    return false;
  }

  // Lookup host and port.
  struct addrinfo *result = NULL;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  int status = getaddrinfo(host, port, &hints, &result);
  if (status != 0) {
    return false;
  }

  // Connect.
  status = connect(socket_, result->ai_addr, result->ai_addrlen);
  freeaddrinfo(result);
  return status == 0;
}


bool POSIXSocket::Shutdown() {
  if (IsValid()) {
    // Shutdown socket for both read and write.
    int status = shutdown(socket_, SHUT_RDWR);
    close(socket_);
    socket_ = -1;
    return status == 0;
  }
  return true;
}


int POSIXSocket::Send(const char* data, int len) const {
  int status = send(socket_, data, len, 0);
  return status;
}


int POSIXSocket::Receive(char* data, int len) const {
  int status = recv(socket_, data, len, 0);
  return status;
}


bool POSIXSocket::SetReuseAddress(bool reuse_address) {
  int on = reuse_address ? 1 : 0;
  int status = setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  return status == 0;
}


bool Socket::Setup() {
  // Nothing to do on POSIX.
  return true;
}


int Socket::LastError() {
  return errno;
}


uint16_t Socket::HToN(uint16_t value) {
  return htons(value);
}


uint16_t Socket::NToH(uint16_t value) {
  return ntohs(value);
}


uint32_t Socket::HToN(uint32_t value) {
  return htonl(value);
}


uint32_t Socket::NToH(uint32_t value) {
  return ntohl(value);
}


Socket* OS::CreateSocket() {
  return new POSIXSocket();
}


} }  // namespace v8::internal
