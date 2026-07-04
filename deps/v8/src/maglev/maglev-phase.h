// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_PHASE_H_
#define V8_MAGLEV_MAGLEV_PHASE_H_

#include "src/base/logging.h"

namespace v8::internal::maglev {

// WARNING: Phases in this list should be sorted by when they appear in the
// pipeline. This is then used by the printer/verifier to print/verify
// invariants that only hold (or don't hold) after some specific phases. For
// instance, after the AnyUseMarking phase, nodes have regalloc data that the
// printer can print.
enum MaglevPhase {
  kNone,
  kMaglevGraphBuilding,
  kInlining,
  kLoopPeeling,
  kTruncationPropagation,
  kTruncation,
  kPostOptimizer,
  kLoopOptimization,
  kPhiUntagging,
  kEscapeAnalysis,
  kRangeAnalysis,
  kAnyUseMarking,
  kDeadNodeSweeping,
  kDuringRegAlloc,
  kRegAlloc,
  kCodeGen
};

inline const char* PhaseName(MaglevPhase phase) {
  switch (phase) {
    case kNone:
      UNREACHABLE();
    case kMaglevGraphBuilding:
      return "Maglev graph building";
    case kInlining:
      return "Non-eager inlining";
    case kLoopPeeling:
      return "Non-eager loop peeling";
    case kTruncationPropagation:
      return "Truncation propagation";
    case kTruncation:
      return "Truncation";
    case kPostOptimizer:
      return "Post optimizer";
    case kLoopOptimization:
      return "Loop optimization (LICM)";
    case kPhiUntagging:
      return "Phi untagging";
    case kEscapeAnalysis:
      return "Escape analysis";
    case kRangeAnalysis:
      return "Range analysis";
    case kAnyUseMarking:
      return "AnyUseMarking";
    case kDeadNodeSweeping:
      return "Dead nodes sweeping";
    case kDuringRegAlloc:
      return "During register allocation";
    case kRegAlloc:
      return "Register allocation";
    case kCodeGen:
      return "Code generation";
  }
}

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_MAGLEV_PHASE_H_
