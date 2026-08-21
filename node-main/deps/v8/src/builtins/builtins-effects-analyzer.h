// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_EFFECTS_ANALYZER_H_
#define V8_BUILTINS_BUILTINS_EFFECTS_ANALYZER_H_

#include "src/base/platform/mutex.h"
#include "src/builtins/builtins.h"
#include "src/execution/isolate.h"

namespace v8::internal {

// BuiltinsEffectsAnalyzer is used when compiling builtins to compute the side
// effects of builtins. Currently, it is only used to compute which builtins can
// allocate.
//
// When compiling each builtins, CsaEffectsComputationPhase computes the effects
// of this builtin and records them in BuiltinsEffectsAnalyzer by calling
// RegisterInitialEffects. At this stage, we don't have a definitive answer for
// all builtins, since some builtins call other builtins that may not yet be
// compiled and whose effects have yet to be computed.
//
// Once we've done this for all builtins, we call
// BuiltinsEffectsAnalyzer::Finalize to take into account calls to other
// builtins (eg, if builtin A doesn't directly allocate but it calls builtin B
// which can allocate, then A can allocate).
//
// Finally, BuiltinsEffectsAnalyzer::Write generates a C++ file containing a
// `bool CanAllocate(Builtin)` function, which is used in Turboshaft and Maglev
// to DCHECK that correct CanAllocate effects are used.

enum class BuiltinAllocateEffect {
  kUninitialized,
  kNo,                   // This builtin cannot allocate.
  kYes,                  // This builtin can allocate.
  kMaybeWithBuiltinCall  // This builtin cannot directly allocate, but it calls
                         // other builtins (and we don't know yet whether those
                         // other builtins allocate or not).
};

struct BuiltinEffects {
  BuiltinAllocateEffect can_allocate = BuiltinAllocateEffect::kUninitialized;
  std::unordered_set<Builtin> callees;
  bool is_finalized = false;
};

class BuiltinsEffectsAnalyzer {
 public:
  void RegisterInitialEffects(Builtin builtin, BuiltinAllocateEffect effects,
                              std::unordered_set<Builtin> callees);

  void Finalize();

  void Write(const char* file);

  bool is_finalized() const { return is_finalized_; }

  static BuiltinsEffectsAnalyzer* Setup(Isolate* isolate);
  static void TearDown(Isolate* isolate);

 private:
  std::array<BuiltinEffects, Builtins::kBuiltinCount> builtins_effects_;
  bool is_finalized_ = false;

  // Since builtins can be generated in parallel, updating {builtins_effects_}
  // is protected by a mutex.
  base::Mutex mutex_;
};

}  // namespace v8::internal

#endif  // V8_BUILTINS_BUILTINS_EFFECTS_ANALYZER_H_
