// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_LOG_UTILS_H_
#define V8_LOGGING_LOG_UTILS_H_

#include <stdio.h>

#include <atomic>
#include <cstdarg>
#include <memory>

#include "src/base/compiler-specific.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/common/assert-scope.h"
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
  explicit Log(Logger* logger, std::string log_file_name);

  V8_EXPORT_PRIVATE static bool IsLoggingToConsole(std::string file_name);
  V8_EXPORT_PRIVATE static bool IsLoggingToTemporaryFile(std::string file_name);

  // Frees all resources acquired in Initialize and Open... functions.
  // When a temporary file is used for the log, returns its stream descriptor,
  // leaving the file open.
  FILE* Close();

  std::string file_name() const;

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
    ~MessageBuilder() = default;

    void AppendString(String str,
                      base::Optional<int> length_limit = base::nullopt);
    void AppendString(Vector<const char> str);
    void AppendString(const char* str);
    void AppendString(const char* str, size_t length, bool is_one_byte = true);
    void PRINTF_FORMAT(2, 3) AppendFormatString(const char* format, ...);
    void AppendCharacter(char c);
    void AppendTwoByteCharacter(char c1, char c2);
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
    // Create a message builder starting from position 0.
    // This acquires the mutex in the log as well.
    explicit MessageBuilder(Log* log);

    // Prints the format string into |log_->format_buffer_|. Returns the length
    // of the result, or kMessageBufferSize if it was truncated.
    int PRINTF_FORMAT(2, 0)
        FormatStringIntoBuffer(const char* format, va_list args);

    void AppendSymbolNameDetails(String str, bool show_impl_info);

    void PRINTF_FORMAT(2, 3) AppendRawFormatString(const char* format, ...);
    void AppendRawCharacter(const char character);

    Log* log_;
    NoGarbageCollectionMutexGuard lock_guard_;

    friend class Log;
  };

  // Use this method to create an instance of Log::MessageBuilder. This method
  // will return null if logging is disabled.
  std::unique_ptr<Log::MessageBuilder> NewMessageBuilder();

 private:
  static FILE* CreateOutputHandle(std::string file_name);
  base::Mutex* mutex() { return &mutex_; }

  void WriteLogHeader();

  Logger* logger_;

  std::string file_name_;

  // When logging is active output_handle_ is used to store a pointer to log
  // destination.  mutex_ should be acquired before using output_handle_.
  FILE* output_handle_;

  OFStream os_;

  // mutex_ is a Mutex used for enforcing exclusive
  // access to the formatting buffer and the log file or log memory buffer.
  base::Mutex mutex_;

  // Buffer used for formatting log messages. This is a singleton buffer and
  // mutex_ should be acquired before using it.
  std::unique_ptr<char[]> format_buffer_;

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
