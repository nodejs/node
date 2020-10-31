/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_BASE_LOGGING_H_
#define INCLUDE_PERFETTO_BASE_LOGGING_H_

#include <errno.h>
#include <string.h>  // For strerror.

#include "perfetto/base/build_config.h"
#include "perfetto/base/compiler.h"
#include "perfetto/base/export.h"

// Ignore GCC warning about a missing argument for a variadic macro parameter.
#pragma GCC system_header

// TODO(primiano): move this to base/build_config.h, turn into
// PERFETTO_BUILDFLAG(DCHECK_IS_ON) and update call sites to use that instead.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
#define PERFETTO_DCHECK_IS_ON() 0
#else
#define PERFETTO_DCHECK_IS_ON() 1
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_FORCE_DLOG_ON)
#define PERFETTO_DLOG_IS_ON() 1
#elif PERFETTO_BUILDFLAG(PERFETTO_FORCE_DLOG_OFF)
#define PERFETTO_DLOG_IS_ON() 0
#else
#define PERFETTO_DLOG_IS_ON() PERFETTO_DCHECK_IS_ON()
#endif

#if defined(PERFETTO_ANDROID_ASYNC_SAFE_LOG)
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \
    !PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
#error "Async-safe logging is limited to Android tree builds"
#endif
// For binaries which need a very lightweight logging implementation.
// Note that this header is incompatible with android/log.h.
#include <async_safe/log.h>
#elif PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
// Normal android logging.
#include <android/log.h>
#endif

namespace perfetto {
namespace base {

// Constexpr functions to extract basename(__FILE__), e.g.: ../foo/f.c -> f.c .
constexpr const char* StrEnd(const char* s) {
  return *s ? StrEnd(s + 1) : s;
}

constexpr const char* BasenameRecursive(const char* s,
                                        const char* begin,
                                        const char* end) {
  return (*s == '/' && s < end)
             ? (s + 1)
             : ((s > begin) ? BasenameRecursive(s - 1, begin, end) : s);
}

constexpr const char* Basename(const char* str) {
  return BasenameRecursive(StrEnd(str), str, StrEnd(str));
}

enum LogLev { kLogDebug = 0, kLogInfo, kLogImportant, kLogError };

PERFETTO_EXPORT void LogMessage(LogLev,
                                const char* fname,
                                int line,
                                const char* fmt,
                                ...) PERFETTO_PRINTF_FORMAT(4, 5);

#if defined(PERFETTO_ANDROID_ASYNC_SAFE_LOG)
#define PERFETTO_XLOG(level, fmt, ...)                                        \
  do {                                                                        \
    async_safe_format_log((ANDROID_LOG_DEBUG + level), "perfetto",            \
                          "%s:%d " fmt, ::perfetto::base::Basename(__FILE__), \
                          __LINE__, ##__VA_ARGS__);                           \
  } while (0)
#else  // defined(PERFETTO_ANDROID_ASYNC_SAFE_LOG)
#define PERFETTO_XLOG(level, fmt, ...)                                      \
  ::perfetto::base::LogMessage(level, ::perfetto::base::Basename(__FILE__), \
                               __LINE__, fmt, ##__VA_ARGS__)
#endif  // defined(PERFETTO_ANDROID_ASYNC_SAFE_LOG)

#define PERFETTO_IMMEDIATE_CRASH() \
  do {                             \
    __builtin_trap();              \
    __builtin_unreachable();       \
  } while (0)

#if PERFETTO_BUILDFLAG(PERFETTO_VERBOSE_LOGS)
#define PERFETTO_LOG(fmt, ...) \
  PERFETTO_XLOG(::perfetto::base::kLogInfo, fmt, ##__VA_ARGS__)
#else  // PERFETTO_BUILDFLAG(PERFETTO_VERBOSE_LOGS)
#define PERFETTO_LOG(...) ::perfetto::base::ignore_result(__VA_ARGS__)
#endif  // PERFETTO_BUILDFLAG(PERFETTO_VERBOSE_LOGS)

#define PERFETTO_ILOG(fmt, ...) \
  PERFETTO_XLOG(::perfetto::base::kLogImportant, fmt, ##__VA_ARGS__)
#define PERFETTO_ELOG(fmt, ...) \
  PERFETTO_XLOG(::perfetto::base::kLogError, fmt, ##__VA_ARGS__)
#define PERFETTO_FATAL(fmt, ...)       \
  do {                                 \
    PERFETTO_PLOG(fmt, ##__VA_ARGS__); \
    PERFETTO_IMMEDIATE_CRASH();        \
  } while (0)

#define PERFETTO_PLOG(x, ...) \
  PERFETTO_ELOG(x " (errno: %d, %s)", ##__VA_ARGS__, errno, strerror(errno))

#if PERFETTO_DLOG_IS_ON()

#define PERFETTO_DLOG(fmt, ...) \
  PERFETTO_XLOG(::perfetto::base::kLogDebug, fmt, ##__VA_ARGS__)

#define PERFETTO_DPLOG(x, ...) \
  PERFETTO_DLOG(x " (errno: %d, %s)", ##__VA_ARGS__, errno, strerror(errno))

#else  // PERFETTO_DLOG_IS_ON()

#define PERFETTO_DLOG(...) ::perfetto::base::ignore_result(__VA_ARGS__)
#define PERFETTO_DPLOG(...) ::perfetto::base::ignore_result(__VA_ARGS__)

#endif  // PERFETTO_DLOG_IS_ON()

#if PERFETTO_DCHECK_IS_ON()

#define PERFETTO_DCHECK(x)                           \
  do {                                               \
    if (PERFETTO_UNLIKELY(!(x))) {                   \
      PERFETTO_PLOG("%s", "PERFETTO_CHECK(" #x ")"); \
      PERFETTO_IMMEDIATE_CRASH();                    \
    }                                                \
  } while (0)

#define PERFETTO_CHECK(x) PERFETTO_DCHECK(x)

#define PERFETTO_DFATAL(fmt, ...)      \
  do {                                 \
    PERFETTO_PLOG(fmt, ##__VA_ARGS__); \
    PERFETTO_IMMEDIATE_CRASH();        \
  } while (0)

#define PERFETTO_DFATAL_OR_ELOG(...) PERFETTO_DFATAL(__VA_ARGS__)

#else  // PERFETTO_DCHECK_IS_ON()

#define PERFETTO_DCHECK(x) \
  do {                     \
  } while (false && (x))

#define PERFETTO_CHECK(x)                            \
  do {                                               \
    if (PERFETTO_UNLIKELY(!(x))) {                   \
      PERFETTO_PLOG("%s", "PERFETTO_CHECK(" #x ")"); \
      PERFETTO_IMMEDIATE_CRASH();                    \
    }                                                \
  } while (0)

#define PERFETTO_DFATAL(...) ::perfetto::base::ignore_result(__VA_ARGS__)
#define PERFETTO_DFATAL_OR_ELOG(...) PERFETTO_ELOG(__VA_ARGS__)

#endif  // PERFETTO_DCHECK_IS_ON()

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_BASE_LOGGING_H_
