// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/base/log.h"

#include <sys/time.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>

namespace redline {

namespace {

std::atomic<int> g_log_level{static_cast<int>(LogLevel::kInfo)};
std::mutex& LogMutex() {
  static std::mutex* mutex = new std::mutex();
  return *mutex;
}

const char* LevelTag(LogLevel level) {
  switch (level) {
    case LogLevel::kVerbose:
      return "V";
    case LogLevel::kInfo:
      return "I";
    case LogLevel::kWarning:
      return "W";
    case LogLevel::kError:
      return "E";
  }
  return "?";
}

void FormatTimestamp(char* buffer, size_t size) {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  struct tm tm_buf;
  localtime_r(&tv.tv_sec, &tm_buf);
  size_t n = strftime(buffer, size, "%H:%M:%S", &tm_buf);
  snprintf(buffer + n, size - n, ".%03d", static_cast<int>(tv.tv_usec / 1000));
}

}  // namespace

void SetLogLevel(LogLevel level) {
  g_log_level.store(static_cast<int>(level), std::memory_order_relaxed);
}

LogLevel GetLogLevel() {
  return static_cast<LogLevel>(g_log_level.load(std::memory_order_relaxed));
}

void LogV(LogLevel level, const char* component, const char* format,
          va_list args) {
  if (static_cast<int>(level) < g_log_level.load(std::memory_order_relaxed)) {
    return;
  }
  std::string message = StringPrintfV(format, args);
  char timestamp[32];
  FormatTimestamp(timestamp, sizeof(timestamp));
  std::scoped_lock<std::mutex> guard(LogMutex());
  fprintf(stderr, "[%s %s] [%s] %s\n", timestamp, LevelTag(level), component,
          message.c_str());
  fflush(stderr);
}

void Log(LogLevel level, const char* component, const char* format, ...) {
  va_list args;
  va_start(args, format);
  LogV(level, component, format, args);
  va_end(args);
}

void Fatal(const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::string message = StringPrintfV(format, args);
  va_end(args);
  {
    std::scoped_lock<std::mutex> guard(LogMutex());
    fprintf(stderr, "[FATAL] %s\n", message.c_str());
    fflush(stderr);
  }
  abort();
}

std::string StringPrintfV(const char* format, va_list args) {
  va_list copy;
  va_copy(copy, args);
  char stack_buffer[512];
  int needed = vsnprintf(stack_buffer, sizeof(stack_buffer), format, copy);
  va_end(copy);
  if (needed < 0) {
    return std::string();
  }
  if (static_cast<size_t>(needed) < sizeof(stack_buffer)) {
    return std::string(stack_buffer, static_cast<size_t>(needed));
  }
  std::string result(static_cast<size_t>(needed) + 1, '\0');
  vsnprintf(result.data(), result.size(), format, args);
  result.resize(static_cast<size_t>(needed));
  return result;
}

std::string StringPrintf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::string result = StringPrintfV(format, args);
  va_end(args);
  return result;
}

}  // namespace redline
