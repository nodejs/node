// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

// Author: kenton@google.com (Kenton Varda)

#include <google/protobuf/stubs/common.h>

#include <atomic>
#include <errno.h>
#include <sstream>
#include <stdio.h>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN  // We only need minimal includes
#include <windows.h>
#define snprintf _snprintf    // see comment in strutil.cc
#elif defined(HAVE_PTHREAD)
#include <pthread.h>
#else
#error "No suitable threading library available."
#endif
#if defined(__ANDROID__)
#include <android/log.h>
#endif

#include <google/protobuf/stubs/callback.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/mutex.h>
#include <google/protobuf/stubs/once.h>
#include <google/protobuf/stubs/status.h>
#include <google/protobuf/stubs/stringpiece.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/int128.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

namespace internal {

void VerifyVersion(int headerVersion,
                   int minLibraryVersion,
                   const char* filename) {
  if (GOOGLE_PROTOBUF_VERSION < minLibraryVersion) {
    // Library is too old for headers.
    GOOGLE_LOG(FATAL)
      << "This program requires version " << VersionString(minLibraryVersion)
      << " of the Protocol Buffer runtime library, but the installed version "
         "is " << VersionString(GOOGLE_PROTOBUF_VERSION) << ".  Please update "
         "your library.  If you compiled the program yourself, make sure that "
         "your headers are from the same version of Protocol Buffers as your "
         "link-time library.  (Version verification failed in \""
      << filename << "\".)";
  }
  if (headerVersion < kMinHeaderVersionForLibrary) {
    // Headers are too old for library.
    GOOGLE_LOG(FATAL)
      << "This program was compiled against version "
      << VersionString(headerVersion) << " of the Protocol Buffer runtime "
         "library, which is not compatible with the installed version ("
      << VersionString(GOOGLE_PROTOBUF_VERSION) <<  ").  Contact the program "
         "author for an update.  If you compiled the program yourself, make "
         "sure that your headers are from the same version of Protocol Buffers "
         "as your link-time library.  (Version verification failed in \""
      << filename << "\".)";
  }
}

string VersionString(int version) {
  int major = version / 1000000;
  int minor = (version / 1000) % 1000;
  int micro = version % 1000;

  // 128 bytes should always be enough, but we use snprintf() anyway to be
  // safe.
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%d.%d.%d", major, minor, micro);

  // Guard against broken MSVC snprintf().
  buffer[sizeof(buffer)-1] = '\0';

  return buffer;
}

}  // namespace internal

// ===================================================================
// emulates google3/base/logging.cc

// If the minimum logging level is not set, we default to logging messages for
// all levels.
#ifndef GOOGLE_PROTOBUF_MIN_LOG_LEVEL
#define GOOGLE_PROTOBUF_MIN_LOG_LEVEL LOGLEVEL_INFO
#endif

namespace internal {

#if defined(__ANDROID__)
inline void DefaultLogHandler(LogLevel level, const char* filename, int line,
                              const string& message) {
  if (level < GOOGLE_PROTOBUF_MIN_LOG_LEVEL) {
    return;
  }
  static const char* level_names[] = {"INFO", "WARNING", "ERROR", "FATAL"};

  static const int android_log_levels[] = {
      ANDROID_LOG_INFO,   // LOG(INFO),
      ANDROID_LOG_WARN,   // LOG(WARNING)
      ANDROID_LOG_ERROR,  // LOG(ERROR)
      ANDROID_LOG_FATAL,  // LOG(FATAL)
  };

  // Bound the logging level.
  const int android_log_level = android_log_levels[level];
  ::std::ostringstream ostr;
  ostr << "[libprotobuf " << level_names[level] << " " << filename << ":"
       << line << "] " << message.c_str();

  // Output the log string the Android log at the appropriate level.
  __android_log_write(android_log_level, "libprotobuf-native",
                      ostr.str().c_str());
  // Also output to std::cerr.
  fprintf(stderr, "%s", ostr.str().c_str());
  fflush(stderr);

  // Indicate termination if needed.
  if (android_log_level == ANDROID_LOG_FATAL) {
    __android_log_write(ANDROID_LOG_FATAL, "libprotobuf-native",
                        "terminating.\n");
  }
}

#else
void DefaultLogHandler(LogLevel level, const char* filename, int line,
                       const string& message) {
  if (level < GOOGLE_PROTOBUF_MIN_LOG_LEVEL) {
    return;
  }
  static const char* level_names[] = { "INFO", "WARNING", "ERROR", "FATAL" };

  // We use fprintf() instead of cerr because we want this to work at static
  // initialization time.
  fprintf(stderr, "[libprotobuf %s %s:%d] %s\n",
          level_names[level], filename, line, message.c_str());
  fflush(stderr);  // Needed on MSVC.
}
#endif

void NullLogHandler(LogLevel /* level */, const char* /* filename */,
                    int /* line */, const string& /* message */) {
  // Nothing.
}

static LogHandler* log_handler_ = &DefaultLogHandler;
static std::atomic<int> log_silencer_count_ = ATOMIC_VAR_INIT(0);

LogMessage& LogMessage::operator<<(const string& value) {
  message_ += value;
  return *this;
}

LogMessage& LogMessage::operator<<(const char* value) {
  message_ += value;
  return *this;
}

LogMessage& LogMessage::operator<<(const StringPiece& value) {
  message_ += value.ToString();
  return *this;
}

LogMessage& LogMessage::operator<<(const util::Status& status) {
  message_ += status.ToString();
  return *this;
}

LogMessage& LogMessage::operator<<(const uint128& value) {
  std::ostringstream str;
  str << value;
  message_ += str.str();
  return *this;
}

// Since this is just for logging, we don't care if the current locale changes
// the results -- in fact, we probably prefer that.  So we use snprintf()
// instead of Simple*toa().
#undef DECLARE_STREAM_OPERATOR
#define DECLARE_STREAM_OPERATOR(TYPE, FORMAT)                       \
  LogMessage& LogMessage::operator<<(TYPE value) {                  \
    /* 128 bytes should be big enough for any of the primitive */   \
    /* values which we print with this, but well use snprintf() */  \
    /* anyway to be extra safe. */                                  \
    char buffer[128];                                               \
    snprintf(buffer, sizeof(buffer), FORMAT, value);                \
    /* Guard against broken MSVC snprintf(). */                     \
    buffer[sizeof(buffer)-1] = '\0';                                \
    message_ += buffer;                                             \
    return *this;                                                   \
  }

DECLARE_STREAM_OPERATOR(char         , "%c" )
DECLARE_STREAM_OPERATOR(int          , "%d" )
DECLARE_STREAM_OPERATOR(unsigned int , "%u" )
DECLARE_STREAM_OPERATOR(long         , "%ld")
DECLARE_STREAM_OPERATOR(unsigned long, "%lu")
DECLARE_STREAM_OPERATOR(double       , "%g" )
DECLARE_STREAM_OPERATOR(void*        , "%p" )
DECLARE_STREAM_OPERATOR(long long         , "%" PROTOBUF_LL_FORMAT "d")
DECLARE_STREAM_OPERATOR(unsigned long long, "%" PROTOBUF_LL_FORMAT "u")
#undef DECLARE_STREAM_OPERATOR

LogMessage::LogMessage(LogLevel level, const char* filename, int line)
  : level_(level), filename_(filename), line_(line) {}
LogMessage::~LogMessage() {}

void LogMessage::Finish() {
  bool suppress = false;

  if (level_ != LOGLEVEL_FATAL) {
    suppress = log_silencer_count_ > 0;
  }

  if (!suppress) {
    log_handler_(level_, filename_, line_, message_);
  }

  if (level_ == LOGLEVEL_FATAL) {
#if PROTOBUF_USE_EXCEPTIONS
    throw FatalException(filename_, line_, message_);
#else
    abort();
#endif
  }
}

void LogFinisher::operator=(LogMessage& other) {
  other.Finish();
}

}  // namespace internal

LogHandler* SetLogHandler(LogHandler* new_func) {
  LogHandler* old = internal::log_handler_;
  if (old == &internal::NullLogHandler) {
    old = nullptr;
  }
  if (new_func == nullptr) {
    internal::log_handler_ = &internal::NullLogHandler;
  } else {
    internal::log_handler_ = new_func;
  }
  return old;
}

LogSilencer::LogSilencer() {
  ++internal::log_silencer_count_;
};

LogSilencer::~LogSilencer() {
  --internal::log_silencer_count_;
};

// ===================================================================
// emulates google3/base/callback.cc

Closure::~Closure() {}

namespace internal { FunctionClosure0::~FunctionClosure0() {} }

void DoNothing() {}

// ===================================================================
// emulates google3/util/endian/endian.h
//
// TODO(xiaofeng): PROTOBUF_LITTLE_ENDIAN is unfortunately defined in
// google/protobuf/io/coded_stream.h and therefore can not be used here.
// Maybe move that macro definition here in the furture.
uint32 ghtonl(uint32 x) {
  union {
    uint32 result;
    uint8 result_array[4];
  };
  result_array[0] = static_cast<uint8>(x >> 24);
  result_array[1] = static_cast<uint8>((x >> 16) & 0xFF);
  result_array[2] = static_cast<uint8>((x >> 8) & 0xFF);
  result_array[3] = static_cast<uint8>(x & 0xFF);
  return result;
}

// ===================================================================
// Shutdown support.

namespace internal {

struct ShutdownData {
  ~ShutdownData() {
    std::reverse(functions.begin(), functions.end());
    for (auto pair : functions) pair.first(pair.second);
  }

  static ShutdownData* get() {
    static auto* data = new ShutdownData;
    return data;
  }

  std::vector<std::pair<void (*)(const void*), const void*>> functions;
  Mutex mutex;
};

static void RunZeroArgFunc(const void* arg) {
  void (*func)() = reinterpret_cast<void (*)()>(const_cast<void*>(arg));
  func();
}

void OnShutdown(void (*func)()) {
  OnShutdownRun(RunZeroArgFunc, reinterpret_cast<void*>(func));
}

void OnShutdownRun(void (*f)(const void*), const void* arg) {
  auto shutdown_data = ShutdownData::get();
  MutexLock lock(&shutdown_data->mutex);
  shutdown_data->functions.push_back(std::make_pair(f, arg));
}

}  // namespace internal

void ShutdownProtobufLibrary() {
  // This function should be called only once, but accepts multiple calls.
  static bool is_shutdown = false;
  if (!is_shutdown) {
    delete internal::ShutdownData::get();
    is_shutdown = true;
  }
}

#if PROTOBUF_USE_EXCEPTIONS
FatalException::~FatalException() throw() {}

const char* FatalException::what() const throw() {
  return message_.c_str();
}
#endif

}  // namespace protobuf
}  // namespace google
