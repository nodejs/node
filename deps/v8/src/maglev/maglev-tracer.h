// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_TRACER_H_
#define V8_MAGLEV_MAGLEV_TRACER_H_

#include <iomanip>
#include <iostream>
#include <ostream>
#include <string>
#include <version>

#ifdef __cpp_lib_syncbuf
#include <syncstream>
#endif

#include "src/compiler/heap-refs.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/utils/ostreams.h"

namespace v8::internal::maglev {

#define C_RESET (v8_flags.log_colour ? "\033[0m" : "")
#define C_GRAY (v8_flags.log_colour ? "\033[90m" : "")
#define C_RED (v8_flags.log_colour ? "\033[91m" : "")
#define C_GREEN (v8_flags.log_colour ? "\033[92m" : "")
#define C_YELLOW (v8_flags.log_colour ? "\033[93m" : "")
#define C_BLUE (v8_flags.log_colour ? "\033[94m" : "")
#define C_MAGENTA (v8_flags.log_colour ? "\033[95m" : "")
#define C_CYAN (v8_flags.log_colour ? "\033[96m" : "")

struct TracePush {
  compiler::SharedFunctionInfoRef shared;
};

struct TraceTry {
  compiler::SharedFunctionInfoRef shared;
};

struct TraceSkip {
  compiler::SharedFunctionInfoRef shared;
};

struct TraceInlineEager {
  compiler::SharedFunctionInfoRef shared;
};

struct TraceInline {
  compiler::SharedFunctionInfoRef shared;
};

struct TraceIdent {};

class TraceLogger;
class Tracer {
 public:
  explicit Tracer(const MaglevCompilationInfo* info) : info_(info) {
    std::stringstream ss;
    if (v8_flags.trace_with_compilation_id) {
      ss << C_GRAY << (info->is_turbolev() ? "[TLV:" : "[ML:")
         << info->trace_id() << "] " << C_RESET;
    }
    cached_prefix_ = ss.str();
  }

  const MaglevCompilationInfo* info() const { return info_; }

  bool IsEnabled() const { return info_->is_tracing_enabled(); }

 private:
  const MaglevCompilationInfo* info_;
  std::string cached_prefix_;
  friend class TraceLogger;
};

class TraceLogger {
 public:
  explicit TraceLogger(const Tracer& tracer) : os_(std::cout) {
    os_ << tracer.cached_prefix_;
  }

  ~TraceLogger() { os_ << std::endl; }

  template <typename T>
  TraceLogger& operator<<(const T& value) {
    os_ << value;
    return *this;
  }

  TraceLogger& operator<<(const TracePush& tag) {
    os_ << C_YELLOW << "➕ PUSH   " << C_RESET << std::left << C_CYAN
        << std::setw(30) << tag.shared.object()->DebugNameCStr() << C_RESET
        << " ";
    return *this;
  }

  TraceLogger& operator<<(const TraceTry& tag) {
    os_ << C_YELLOW << "🔍 TRY    " << C_RESET << std::left << C_CYAN
        << std::setw(30) << tag.shared.object()->DebugNameCStr() << C_RESET
        << " ";
    return *this;
  }

  TraceLogger& operator<<(const TraceSkip& tag) {
    os_ << C_RED << "❌ SKIP   " << C_RESET << std::left << C_CYAN
        << std::setw(30) << tag.shared.object()->DebugNameCStr() << C_RESET
        << " ";
    return *this;
  }

  TraceLogger& operator<<(const TraceInlineEager& tag) {
    os_ << C_GREEN << "⚡ INLINE " << C_RESET << std::left << C_CYAN
        << std::setw(30) << tag.shared.object()->DebugNameCStr() << C_RESET
        << " ";
    return *this;
  }

  TraceLogger& operator<<(const TraceInline& tag) {
    os_ << C_GREEN << "✅ INLINE " << C_RESET << std::left << C_CYAN
        << std::setw(30) << tag.shared.object()->DebugNameCStr() << C_RESET
        << " ";
    return *this;
  }

  TraceLogger& operator<<(const TraceIdent& tag) {
    os_ << "          ";
    return *this;
  }

 private:
#ifdef __cpp_lib_syncbuf
  std::osyncstream os_;
#else
  // std::osyncstream is NOT available.
  // Falling back to interleaving IO.
  std::ostream& os_;
#endif
};

#define TRACE_IMPL(flag, tracer, ...)                                         \
  do {                                                                        \
    if (V8_UNLIKELY((tracer).info()->flags().flag && (tracer).IsEnabled())) { \
      TraceLogger((tracer)) << __VA_ARGS__;                                   \
    }                                                                         \
  } while (false)

// We assume that the class using this macro has a field tracer_.
#define TRACE_INLINING(...) TRACE_IMPL(trace_inlining, tracer_, __VA_ARGS__)

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_MAGLEV_TRACER_H_
