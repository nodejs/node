// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_LOG_UTILS_H_
#define V8_LOGGING_LOG_UTILS_H_

#include <stdio.h>

#include <cstdarg>

#include "src/base/compiler-specific.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/flags/flags.h"
#include "src/utils/allocation.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

class Logger;
template <typename T>
class Vector;

enum class LogSeparator { kSeparator };

// Functions and data for performing output of log messages.
class Log {
 public:
  Log(Logger* log, const char* log_file_name);
  // Disables logging, but preserves acquired resources.
  void stop() { is_stopped_ = true; }

  static bool InitLogAtStart() {
    return FLAG_log || FLAG_log_api || FLAG_log_code || FLAG_log_handles ||
           FLAG_log_suspect || FLAG_ll_prof || FLAG_perf_basic_prof ||
           FLAG_perf_prof || FLAG_log_source_code || FLAG_gdbjit ||
           FLAG_log_internal_timer_events || FLAG_prof_cpp || FLAG_trace_ic ||
           FLAG_log_function_events;
  }

  // Frees all resources acquired in Initialize and Open... functions.
  // When a temporary file is used for the log, returns its stream descriptor,
  // leaving the file open.
  FILE* Close();

  // Returns whether logging is enabled.
  bool IsEnabled() { return !is_stopped_ && output_handle_ != nullptr; }

  // Size of buffer used for formatting log messages.
  static const int kMessageBufferSize = 2048;

  // This mode is only used in tests, as temporary files are automatically
  // deleted on close and thus can't be accessed afterwards.
  V8_EXPORT_PRIVATE static const char* const kLogToTemporaryFile;
  static const char* const kLogToConsole;

  // Utility class for formatting log messages. It escapes the given messages
  // and then appends them to the static buffer in Log.
  class MessageBuilder {
   public:
    // Create a message builder starting from position 0.
    // This acquires the mutex in the log as well.
    explicit MessageBuilder(Log* log);
    ~MessageBuilder() = default;

    void AppendString(String str,
                      base::Optional<int> length_limit = base::nullopt);
    void AppendString(Vector<const char> str);
    void AppendString(const char* str);
    void AppendString(const char* str, size_t length);
    void PRINTF_FORMAT(2, 3) AppendFormatString(const char* format, ...);
    void AppendCharacter(char c);
    void AppendSymbolName(Symbol symbol);

    // Delegate insertion to the underlying {log_}.
    // All appended strings are escaped to maintain one-line log entries.
    template <typename T>
    MessageBuilder& operator<<(T value) {
      log_->os_ << value;
      return *this;
    }

    // Finish the current log line an flush the it to the log file.
    void WriteToLogFile();

   private:
    // Prints the format string into |log_->format_buffer_|. Returns the length
    // of the result, or kMessageBufferSize if it was truncated.
    int PRINTF_FORMAT(2, 0)
        FormatStringIntoBuffer(const char* format, va_list args);

    void AppendSymbolNameDetails(String str, bool show_impl_info);

    void PRINTF_FORMAT(2, 3) AppendRawFormatString(const char* format, ...);
    void AppendRawCharacter(const char character);

    Log* log_;
    base::MutexGuard lock_guard_;
  };

 private:
  static FILE* CreateOutputHandle(const char* file_name);

  // Implementation of writing to a log file.
  int WriteToFile(const char* msg, int length) {
    DCHECK_NOT_NULL(output_handle_);
    os_.write(msg, length);
    DCHECK(!os_.bad());
    return length;
  }

  // Whether logging is stopped (e.g. due to insufficient resources).
  bool is_stopped_;

  // When logging is active output_handle_ is used to store a pointer to log
  // destination.  mutex_ should be acquired before using output_handle_.
  FILE* output_handle_;
  OFStream os_;

  // mutex_ is a Mutex used for enforcing exclusive
  // access to the formatting buffer and the log file or log memory buffer.
  base::Mutex mutex_;

  // Buffer used for formatting log messages. This is a singleton buffer and
  // mutex_ should be acquired before using it.
  char* format_buffer_;

  Logger* logger_;

  friend class Logger;
};

template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<LogSeparator>(
    LogSeparator separator);
template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<void*>(void* pointer);
template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<const char*>(
    const char* string);
template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<char>(char c);
template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<String>(String string);
template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<Symbol>(Symbol symbol);
template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<Name>(Name name);

}  // namespace internal
}  // namespace v8

#endif  // V8_LOGGING_LOG_UTILS_H_
