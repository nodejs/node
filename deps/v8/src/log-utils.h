// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOG_UTILS_H_
#define V8_LOG_UTILS_H_

#include <stdio.h>

#include <cstdarg>

#include "src/allocation.h"
#include "src/base/compiler-specific.h"
#include "src/base/platform/mutex.h"
#include "src/flags.h"

namespace v8 {
namespace internal {

class Logger;

// Functions and data for performing output of log messages.
class Log {
 public:
  // Performs process-wide initialization.
  void Initialize(const char* log_file_name);

  // Disables logging, but preserves acquired resources.
  void stop() { is_stopped_ = true; }

  static bool InitLogAtStart() {
    return FLAG_log || FLAG_log_api || FLAG_log_code || FLAG_log_gc ||
           FLAG_log_handles || FLAG_log_suspect || FLAG_ll_prof ||
           FLAG_perf_basic_prof || FLAG_perf_prof ||
           FLAG_log_internal_timer_events || FLAG_prof_cpp || FLAG_trace_ic;
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
  static const char* const kLogToConsole;

  // Utility class for formatting log messages. It fills the message into the
  // static buffer in Log.
  class MessageBuilder BASE_EMBEDDED {
   public:
    // Create a message builder starting from position 0.
    // This acquires the mutex in the log as well.
    explicit MessageBuilder(Log* log);
    ~MessageBuilder() { }

    // Append string data to the log message.
    void PRINTF_FORMAT(2, 3) Append(const char* format, ...);

    // Append string data to the log message.
    void PRINTF_FORMAT(2, 0) AppendVA(const char* format, va_list args);

    // Append a character to the log message.
    void Append(const char c);

    // Append double quoted string to the log message.
    void AppendDoubleQuotedString(const char* string);

    // Append a heap string.
    void Append(String* str);

    // Appends an address.
    void AppendAddress(Address addr);

    void AppendSymbolName(Symbol* symbol);

    void AppendDetailed(String* str, bool show_impl_info);

    // Append a portion of a string.
    void AppendStringPart(const char* str, int len);

    // Write the log message to the log file currently opened.
    void WriteToLogFile();

   private:
    Log* log_;
    base::LockGuard<base::Mutex> lock_guard_;
    int pos_;
  };

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
    DCHECK_NOT_NULL(output_handle_);
    size_t rv = fwrite(msg, 1, length, output_handle_);
    DCHECK_EQ(length, rv);
    USE(rv);
    fflush(output_handle_);
    return length;
  }

  // Whether logging is stopped (e.g. due to insufficient resources).
  bool is_stopped_;

  // When logging is active output_handle_ is used to store a pointer to log
  // destination.  mutex_ should be acquired before using output_handle_.
  FILE* output_handle_;

  // mutex_ is a Mutex used for enforcing exclusive
  // access to the formatting buffer and the log file or log memory buffer.
  base::Mutex mutex_;

  // Buffer used for formatting log messages. This is a singleton buffer and
  // mutex_ should be acquired before using it.
  char* message_buffer_;

  Logger* logger_;

  friend class Logger;
};


}  // namespace internal
}  // namespace v8

#endif  // V8_LOG_UTILS_H_
