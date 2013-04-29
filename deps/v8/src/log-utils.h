// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#ifndef V8_LOG_UTILS_H_
#define V8_LOG_UTILS_H_

#include "allocation.h"

namespace v8 {
namespace internal {

class Logger;

// Functions and data for performing output of log messages.
class Log {
 public:
  // Performs process-wide initialization.
  void Initialize();

  // Disables logging, but preserves acquired resources.
  void stop() { is_stopped_ = true; }

  static bool InitLogAtStart() {
    return FLAG_log || FLAG_log_runtime || FLAG_log_api
        || FLAG_log_code || FLAG_log_gc || FLAG_log_handles || FLAG_log_suspect
        || FLAG_log_regexp || FLAG_ll_prof || FLAG_log_internal_timer_events;
  }

  // Frees all resources acquired in Initialize and Open... functions.
  // When a temporary file is used for the log, returns its stream descriptor,
  // leaving the file open.
  FILE* Close();

  // Returns whether logging is enabled.
  bool IsEnabled() {
    return !is_stopped_ && output_handle_ != NULL;
  }

  // Size of buffer used for formatting log messages.
  static const int kMessageBufferSize = 2048;

  // This mode is only used in tests, as temporary files are automatically
  // deleted on close and thus can't be accessed afterwards.
  static const char* const kLogToTemporaryFile;

 private:
  explicit Log(Logger* logger);

  // Opens stdout for logging.
  void OpenStdout();

  // Opens file for logging.
  void OpenFile(const char* name);

  // Opens a temporary file for logging.
  void OpenTemporaryFile();

  // Implementation of writing to a log file.
  int WriteToFile(const char* msg, int length) {
    ASSERT(output_handle_ != NULL);
    size_t rv = fwrite(msg, 1, length, output_handle_);
    ASSERT(static_cast<size_t>(length) == rv);
    USE(rv);
    fflush(output_handle_);
    return length;
  }

  // Whether logging is stopped (e.g. due to insufficient resources).
  bool is_stopped_;

  // When logging is active output_handle_ is used to store a pointer to log
  // destination.  mutex_ should be acquired before using output_handle_.
  FILE* output_handle_;

  // Used when low-level profiling is active.
  FILE* ll_output_handle_;

  // mutex_ is a Mutex used for enforcing exclusive
  // access to the formatting buffer and the log file or log memory buffer.
  Mutex* mutex_;

  // Buffer used for formatting log messages. This is a singleton buffer and
  // mutex_ should be acquired before using it.
  char* message_buffer_;

  Logger* logger_;

  friend class Logger;
  friend class LogMessageBuilder;
};


// Utility class for formatting log messages. It fills the message into the
// static buffer in Log.
class LogMessageBuilder BASE_EMBEDDED {
 public:
  // Create a message builder starting from position 0. This acquires the mutex
  // in the log as well.
  explicit LogMessageBuilder(Logger* logger);
  ~LogMessageBuilder() { }

  // Append string data to the log message.
  void Append(const char* format, ...);

  // Append string data to the log message.
  void AppendVA(const char* format, va_list args);

  // Append a character to the log message.
  void Append(const char c);

  // Append a heap string.
  void Append(String* str);

  // Appends an address.
  void AppendAddress(Address addr);

  void AppendDetailed(String* str, bool show_impl_info);

  // Append a portion of a string.
  void AppendStringPart(const char* str, int len);

  // Write the log message to the log file currently opened.
  void WriteToLogFile();

 private:
  Log* log_;
  ScopedLock sl;
  int pos_;
};

} }  // namespace v8::internal

#endif  // V8_LOG_UTILS_H_
