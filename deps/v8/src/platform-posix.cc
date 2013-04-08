// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "platform-posix.h"

#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#undef MAP_TYPE

#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
#define LOG_TAG "v8"
#include <android/log.h>
#endif

#include "v8.h"

#include "codegen.h"
#include "platform.h"

namespace v8 {
namespace internal {


// Maximum size of the virtual memory.  0 means there is no artificial
// limit.

intptr_t OS::MaxVirtualMemory() {
  struct rlimit limit;
  int result = getrlimit(RLIMIT_DATA, &limit);
  if (result != 0) return 0;
  return limit.rlim_cur;
}


intptr_t OS::CommitPageSize() {
  static intptr_t page_size = getpagesize();
  return page_size;
}


#ifndef __CYGWIN__
// Get rid of writable permission on code allocations.
void OS::ProtectCode(void* address, const size_t size) {
  mprotect(address, size, PROT_READ | PROT_EXEC);
}


// Create guard pages.
void OS::Guard(void* address, const size_t size) {
  mprotect(address, size, PROT_NONE);
}
#endif  // __CYGWIN__


void* OS::GetRandomMmapAddr() {
  Isolate* isolate = Isolate::UncheckedCurrent();
  // Note that the current isolate isn't set up in a call path via
  // CpuFeatures::Probe. We don't care about randomization in this case because
  // the code page is immediately freed.
  if (isolate != NULL) {
#ifdef V8_TARGET_ARCH_X64
    uint64_t rnd1 = V8::RandomPrivate(isolate);
    uint64_t rnd2 = V8::RandomPrivate(isolate);
    uint64_t raw_addr = (rnd1 << 32) ^ rnd2;
    // Currently available CPUs have 48 bits of virtual addressing.  Truncate
    // the hint address to 46 bits to give the kernel a fighting chance of
    // fulfilling our placement request.
    raw_addr &= V8_UINT64_C(0x3ffffffff000);
#else
    uint32_t raw_addr = V8::RandomPrivate(isolate);
    // The range 0x20000000 - 0x60000000 is relatively unpopulated across a
    // variety of ASLR modes (PAE kernel, NX compat mode, etc) and on macos
    // 10.6 and 10.7.
    raw_addr &= 0x3ffff000;
    raw_addr += 0x20000000;
#endif
    return reinterpret_cast<void*>(raw_addr);
  }
  return NULL;
}


// ----------------------------------------------------------------------------
// Math functions

double modulo(double x, double y) {
  return fmod(x, y);
}


#define UNARY_MATH_FUNCTION(name, generator)             \
static UnaryMathFunction fast_##name##_function = NULL;  \
void init_fast_##name##_function() {                     \
  fast_##name##_function = generator;                    \
}                                                        \
double fast_##name(double x) {                           \
  return (*fast_##name##_function)(x);                   \
}

UNARY_MATH_FUNCTION(sin, CreateTranscendentalFunction(TranscendentalCache::SIN))
UNARY_MATH_FUNCTION(cos, CreateTranscendentalFunction(TranscendentalCache::COS))
UNARY_MATH_FUNCTION(tan, CreateTranscendentalFunction(TranscendentalCache::TAN))
UNARY_MATH_FUNCTION(log, CreateTranscendentalFunction(TranscendentalCache::LOG))
UNARY_MATH_FUNCTION(exp, CreateExpFunction())
UNARY_MATH_FUNCTION(sqrt, CreateSqrtFunction())

#undef MATH_FUNCTION


void lazily_initialize_fast_exp() {
  if (fast_exp_function == NULL) {
    init_fast_exp_function();
  }
}


double OS::nan_value() {
  // NAN from math.h is defined in C99 and not in POSIX.
  return NAN;
}


int OS::GetCurrentProcessId() {
  return static_cast<int>(getpid());
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
  FILE* file = fopen(path, mode);
  if (file == NULL) return NULL;
  struct stat file_stat;
  if (fstat(fileno(file), &file_stat) != 0) return NULL;
  bool is_regular_file = ((file_stat.st_mode & S_IFREG) != 0);
  if (is_regular_file) return file;
  fclose(file);
  return NULL;
}


bool OS::Remove(const char* path) {
  return (remove(path) == 0);
}


FILE* OS::OpenTemporaryFile() {
  return tmpfile();
}


const char* const OS::LogFileOpenMode = "w";


void OS::Print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  VPrint(format, args);
  va_end(args);
}


void OS::VPrint(const char* format, va_list args) {
#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
  __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, format, args);
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
#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
  __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, format, args);
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
#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
  __android_log_vprint(ANDROID_LOG_ERROR, LOG_TAG, format, args);
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


#if defined(V8_TARGET_ARCH_IA32)
static OS::MemCopyFunction memcopy_function = NULL;
// Defined in codegen-ia32.cc.
OS::MemCopyFunction CreateMemCopyFunction();

// Copy memory area to disjoint memory area.
void OS::MemCopy(void* dest, const void* src, size_t size) {
  // Note: here we rely on dependent reads being ordered. This is true
  // on all architectures we currently support.
  (*memcopy_function)(dest, src, size);
#ifdef DEBUG
  CHECK_EQ(0, memcmp(dest, src, size));
#endif
}
#endif  // V8_TARGET_ARCH_IA32


void POSIXPostSetUp() {
#if defined(V8_TARGET_ARCH_IA32)
  memcopy_function = CreateMemCopyFunction();
#endif
  init_fast_sin_function();
  init_fast_cos_function();
  init_fast_tan_function();
  init_fast_log_function();
  // fast_exp is initialized lazily.
  init_fast_sqrt_function();
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

  int socket;
  do {
    socket = accept(socket_, NULL, NULL);
  } while (socket == -1 && errno == EINTR);

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
  do {
    status = connect(socket_, result->ai_addr, result->ai_addrlen);
  } while (status == -1 && errno == EINTR);
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
  if (len <= 0) return 0;
  int written = 0;
  while (written < len) {
    int status = send(socket_, data + written, len - written, 0);
    if (status == 0) {
      break;
    } else if (status > 0) {
      written += status;
    } else if (errno != EINTR) {
      return 0;
    }
  }
  return written;
}


int POSIXSocket::Receive(char* data, int len) const {
  if (len <= 0) return 0;
  int status;
  do {
    status = recv(socket_, data, len, 0);
  } while (status == -1 && errno == EINTR);
  return (status < 0) ? 0 : status;
}


bool POSIXSocket::SetReuseAddress(bool reuse_address) {
  int on = reuse_address ? 1 : 0;
  int status = setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  return status == 0;
}


bool Socket::SetUp() {
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
