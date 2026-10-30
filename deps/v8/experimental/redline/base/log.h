// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_BASE_LOG_H_
#define V8_EXPERIMENTAL_REDLINE_BASE_LOG_H_

#include <cstdarg>
#include <string>

namespace redline {

enum class LogLevel { kVerbose = 0, kInfo = 1, kWarning = 2, kError = 3 };

// Minimum level that is printed. Defaults to kInfo.
void SetLogLevel(LogLevel level);
LogLevel GetLogLevel();

// Thread-safe, printf-style logging to stderr with a timestamp and a
// component label, e.g. "[12:34:56.789] [REPRL] message".
void Log(LogLevel level, const char* component, const char* format, ...)
    __attribute__((format(printf, 3, 4)));
void LogV(LogLevel level, const char* component, const char* format,
          va_list args);

// Prints the message and aborts. Never returns.
[[noreturn]] void Fatal(const char* format, ...)
    __attribute__((format(printf, 1, 2)));

// Convenience wrappers.
#define REDLINE_LOG(component, ...) \
  ::redline::Log(::redline::LogLevel::kInfo, component, __VA_ARGS__)
#define REDLINE_VLOG(component, ...) \
  ::redline::Log(::redline::LogLevel::kVerbose, component, __VA_ARGS__)
#define REDLINE_WARN(component, ...) \
  ::redline::Log(::redline::LogLevel::kWarning, component, __VA_ARGS__)
#define REDLINE_ERROR(component, ...) \
  ::redline::Log(::redline::LogLevel::kError, component, __VA_ARGS__)

// printf into a std::string.
std::string StringPrintf(const char* format, ...)
    __attribute__((format(printf, 1, 2)));
std::string StringPrintfV(const char* format, va_list args);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_BASE_LOG_H_
