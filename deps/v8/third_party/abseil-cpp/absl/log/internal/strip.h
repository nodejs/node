// Copyright 2022 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: log/internal/strip.h
// -----------------------------------------------------------------------------

// SKIP_ABSL_INLINE_NAMESPACE_CHECK

#ifndef ABSL_LOG_INTERNAL_STRIP_H_
#define ABSL_LOG_INTERNAL_STRIP_H_

#include "absl/base/attributes.h"  // IWYU pragma: keep
#include "absl/base/log_severity.h"
#include "absl/log/internal/log_message.h"
#include "absl/log/internal/nullstream.h"

// `ABSL_LOGGING_INTERNAL_LOG_*` evaluates to a temporary `LogMessage` object or
// to a related object with a compatible API but different behavior.  This set
// of defines comes in three flavors: vanilla, plus two variants that strip some
// logging in subtly different ways for subtly different reasons (see below).
#if defined(STRIP_LOG) && STRIP_LOG

#define ABSL_LOGGING_INTERNAL_LOG_INFO ::absl::log_internal::NullStream()
#define ABSL_LOGGING_INTERNAL_LOG_WARNING ::absl::log_internal::NullStream()
#define ABSL_LOGGING_INTERNAL_LOG_ERROR ::absl::log_internal::NullStream()
#define ABSL_LOGGING_INTERNAL_LOG_FATAL ::absl::log_internal::NullStreamFatal()
#define ABSL_LOGGING_INTERNAL_LOG_QFATAL ::absl::log_internal::NullStreamFatal()
#define ABSL_LOGGING_INTERNAL_LOG_DFATAL \
  ::absl::log_internal::NullStreamMaybeFatal(::absl::kLogDebugFatal)
#define ABSL_LOGGING_INTERNAL_LOG_LEVEL(severity) \
  ::absl::log_internal::NullStreamMaybeFatal(absl_log_internal_severity)

// Fatal `DLOG`s expand a little differently to avoid being `[[noreturn]]`.
#define ABSL_LOGGING_INTERNAL_DLOG_FATAL \
  ::absl::log_internal::NullStreamMaybeFatal(::absl::LogSeverity::kFatal)
#define ABSL_LOGGING_INTERNAL_DLOG_QFATAL \
  ::absl::log_internal::NullStreamMaybeFatal(::absl::LogSeverity::kFatal)

#define ABSL_LOG_INTERNAL_CHECK(failure_message) ABSL_LOGGING_INTERNAL_LOG_FATAL
#define ABSL_LOG_INTERNAL_QCHECK(failure_message) \
  ABSL_LOGGING_INTERNAL_LOG_QFATAL

#else  // !defined(STRIP_LOG) || !STRIP_LOG

#define ABSL_LOGGING_INTERNAL_LOG_INFO \
  ::absl::log_internal::LogMessage(    \
      __FILE__, __LINE__, ::absl::log_internal::LogMessage::InfoTag{})
#define ABSL_LOGGING_INTERNAL_LOG_WARNING \
  ::absl::log_internal::LogMessage(       \
      __FILE__, __LINE__, ::absl::log_internal::LogMessage::WarningTag{})
#define ABSL_LOGGING_INTERNAL_LOG_ERROR \
  ::absl::log_internal::LogMessage(     \
      __FILE__, __LINE__, ::absl::log_internal::LogMessage::ErrorTag{})
#define ABSL_LOGGING_INTERNAL_LOG_FATAL \
  ::absl::log_internal::LogMessageFatal(__FILE__, __LINE__)
#define ABSL_LOGGING_INTERNAL_LOG_QFATAL \
  ::absl::log_internal::LogMessageQuietlyFatal(__FILE__, __LINE__)
#define ABSL_LOGGING_INTERNAL_LOG_DFATAL \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, ::absl::kLogDebugFatal)
#define ABSL_LOGGING_INTERNAL_LOG_LEVEL(severity)      \
  ::absl::log_internal::LogMessage(__FILE__, __LINE__, \
                                   absl_log_internal_severity)

// Fatal `DLOG`s expand a little differently to avoid being `[[noreturn]]`.
#define ABSL_LOGGING_INTERNAL_DLOG_FATAL \
  ::absl::log_internal::LogMessageDebugFatal(__FILE__, __LINE__)
#define ABSL_LOGGING_INTERNAL_DLOG_QFATAL \
  ::absl::log_internal::LogMessageQuietlyDebugFatal(__FILE__, __LINE__)

// These special cases dispatch to special-case constructors that allow us to
// avoid an extra function call and shrink non-LTO binaries by a percent or so.
#define ABSL_LOG_INTERNAL_CHECK(failure_message) \
  ::absl::log_internal::LogMessageFatal(__FILE__, __LINE__, failure_message)
#define ABSL_LOG_INTERNAL_QCHECK(failure_message)                  \
  ::absl::log_internal::LogMessageQuietlyFatal(__FILE__, __LINE__, \
                                               failure_message)
#endif  // !defined(STRIP_LOG) || !STRIP_LOG

// This part of a non-fatal `DLOG`s expands the same as `LOG`.
#define ABSL_LOGGING_INTERNAL_DLOG_INFO ABSL_LOGGING_INTERNAL_LOG_INFO
#define ABSL_LOGGING_INTERNAL_DLOG_WARNING ABSL_LOGGING_INTERNAL_LOG_WARNING
#define ABSL_LOGGING_INTERNAL_DLOG_ERROR ABSL_LOGGING_INTERNAL_LOG_ERROR
#define ABSL_LOGGING_INTERNAL_DLOG_DFATAL ABSL_LOGGING_INTERNAL_LOG_DFATAL
#define ABSL_LOGGING_INTERNAL_DLOG_LEVEL ABSL_LOGGING_INTERNAL_LOG_LEVEL

#define ABSL_LOGGING_INTERNAL_LOG_DO_NOT_SUBMIT ABSL_LOGGING_INTERNAL_LOG_ERROR

#endif  // ABSL_LOG_INTERNAL_STRIP_H_
