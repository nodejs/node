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
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-known-node-aspects.h"

namespace v8::internal::maglev {

enum class TraceColor : uint8_t {
  kWhite = 97,
  kReset = 0,
  kGray = 90,
  kRed = 91,
  kGreen = 92,
  kYellow = 93,
  kBlue = 94,
  kMagenta = 95,
  kCyan = 96,
  kDarkGray = 30,
  kDarkRed = 31,
  kDarkGreen = 32,
  kDarkYellow = 33,
  kDarkBlue = 34,
  kDarkMagenta = 35,
  kDarkCyan = 36,
  kLightGray = 37,
  kBold = 1,
  kDim = 2,
  kInfo = kLightGray,
};

inline std::ostream& operator<<(std::ostream& os, const TraceColor color) {
  if (v8_flags.log_colour) {
    os << "\033[" << static_cast<int>(color) << "m";
  }
  return os;
}

struct TraceBytecode {
  interpreter::BytecodeArrayIterator& iterator;
};
struct TraceVirtualObjects {
  const VirtualObjectList& virtual_objects;
};
struct TraceKNA {
  const KnownNodeAspects& kna;
};

// Trace graph building helpers.
struct TraceNewNode {
  Node* node;
};
struct TraceNewControlNode {
  ControlNode* node;
};

// Trace inlining helpers.
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

// Random helpers.
struct TraceIdent {};
struct TraceNewline {};

class TraceLogger;
class Tracer {
 public:
  explicit Tracer(const MaglevCompilationInfo* info) : info_(info) {
    std::stringstream ss;
    if (v8_flags.trace_with_compilation_id) {
      ss << TraceColor::kGray << (info->is_turbolev() ? "[TLV:" : "[ML:")
         << info->trace_id() << "] " << TraceColor::kReset;
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
  explicit TraceLogger(const Tracer& tracer) : tracer_(tracer), os_(std::cout) {
    os_ << tracer.cached_prefix_;
  }

  ~TraceLogger() { os_ << TraceColor::kReset << std::endl; }

  template <typename T>
  TraceLogger& operator<<(const T& value) {
    os_ << value;
    return *this;
  }

  TraceLogger& operator<<(const TracePush& tag) {
    os_ << TraceColor::kYellow << "➕ PUSH   " << TraceColor::kReset
        << std::left << TraceColor::kCyan << std::setw(30)
        << tag.shared.object()->DebugNameCStr() << TraceColor::kReset << " ";
    return *this;
  }

  TraceLogger& operator<<(const TraceTry& tag) {
    os_ << TraceColor::kYellow << "🔍 TRY    " << TraceColor::kReset
        << std::left << TraceColor::kCyan << std::setw(30)
        << tag.shared.object()->DebugNameCStr() << TraceColor::kReset << " ";
    return *this;
  }

  TraceLogger& operator<<(const TraceSkip& tag) {
    os_ << TraceColor::kRed << "❌ SKIP   " << TraceColor::kReset << std::left
        << TraceColor::kCyan << std::setw(30)
        << tag.shared.object()->DebugNameCStr() << TraceColor::kReset << " ";
    return *this;
  }

  TraceLogger& operator<<(const TraceInlineEager& tag) {
    os_ << TraceColor::kGreen << "⚡ INLINE " << TraceColor::kReset << std::left
        << TraceColor::kCyan << std::setw(30)
        << tag.shared.object()->DebugNameCStr() << TraceColor::kReset << " ";
    return *this;
  }

  TraceLogger& operator<<(const TraceInline& tag) {
    os_ << TraceColor::kGreen << "✅ INLINE " << TraceColor::kReset << std::left
        << TraceColor::kCyan << std::setw(30)
        << tag.shared.object()->DebugNameCStr() << TraceColor::kReset << " ";
    return *this;
  }

  TraceLogger& operator<<(const TraceBytecode& bytecode) {
    bytecode.iterator.PrintCurrentBytecodeTo(os_);
    return *this;
  }

  TraceLogger& operator<<(const TraceNewNode& tag) {
    os_ << TraceColor::kDarkGreen << "+ " << tag.node << "  "
        << PrintNodeLabel(tag.node) << ": " << PrintNode(tag.node)
        << TraceColor::kReset;
    return *this;
  }

  TraceLogger& operator<<(const TraceNewControlNode& tag) {
    bool kHasRegallocData = false;
    bool kSkipTargets = true;
    os_ << TraceColor::kDarkGreen << "+ " << tag.node << "  "
        << PrintNodeLabel(tag.node) << ": "
        << PrintNode(tag.node, kHasRegallocData, kSkipTargets)
        << TraceColor::kReset;
    return *this;
  }

  TraceLogger& operator<<(const TraceVirtualObjects& tag) {
    for (const VirtualObject* vo : tag.virtual_objects) {
      GetCurrentGraphLabeller()->PrintNodeLabel(os_, vo, false);
      os_ << "; ";
    }
    return *this;
  }

  TraceLogger& operator<<(const TraceKNA& tag) {
    tag.kna.TraceLoadedProperties(this);
    return *this;
  }

  TraceLogger& operator<<(const TraceIdent& tag) {
    os_ << "          ";
    return *this;
  }

  TraceLogger& operator<<(const TraceNewline& tag) {
    os_ << "\n" << tracer_.cached_prefix_;
    return *this;
  }

 private:
  const Tracer& tracer_;
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
