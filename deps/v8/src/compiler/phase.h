// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PHASE_H_
#define V8_COMPILER_PHASE_H_

#include "src/logging/runtime-call-stats.h"

#ifdef V8_RUNTIME_CALL_STATS
#define DECL_PIPELINE_PHASE_CONSTANTS_HELPER(Name, Kind, Mode)  \
  static constexpr PhaseKind kKind = Kind;                      \
  static const char* phase_name() { return "V8.TF" #Name; }     \
  static constexpr RuntimeCallCounterId kRuntimeCallCounterId = \
      RuntimeCallCounterId::kOptimize##Name;                    \
  static constexpr RuntimeCallStats::CounterMode kCounterMode = Mode;
#else  // V8_RUNTIME_CALL_STATS
#define DECL_PIPELINE_PHASE_CONSTANTS_HELPER(Name, Kind, Mode) \
  static constexpr PhaseKind kKind = Kind;                     \
  static const char* phase_name() { return "V8.TF" #Name; }
#endif  // V8_RUNTIME_CALL_STATS

#define DECL_PIPELINE_PHASE_CONSTANTS(Name)                        \
  DECL_PIPELINE_PHASE_CONSTANTS_HELPER(Name, PhaseKind::kTurbofan, \
                                       RuntimeCallStats::kThreadSpecific)

#define DECL_MAIN_THREAD_PIPELINE_PHASE_CONSTANTS(Name)            \
  DECL_PIPELINE_PHASE_CONSTANTS_HELPER(Name, PhaseKind::kTurbofan, \
                                       RuntimeCallStats::kExact)

namespace v8::internal::compiler {

enum class PhaseKind {
  kTurbofan,
  kTurboshaft,
};

}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_PHASE_H_
