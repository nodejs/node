// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_MUTATORS_JIT_COMPOUND_H_
#define V8_EXPERIMENTAL_REDLINE_MUTATORS_JIT_COMPOUND_H_

#include <cstdint>
#include <optional>

#include "experimental/redline/mutators/mutator.h"

namespace redline {

// Compiler-state patterns that `JitCompoundMutator` applies to a function.
enum class JitCompoundPattern : uint8_t {
  kLocalAllocation = 0,
  kPolymorphicWarmup = 1,
  kInlinedDeopt = 2,
  kOsrLoopTransition = 3,
};

// Combines multiple interacting JIT conditions inside or around one function.
class JitCompoundMutator final : public Mutator {
 public:
  explicit JitCompoundMutator(bool natives = true);
  ~JitCompoundMutator() override = default;

  bool natives() const { return natives_; }
  void set_natives(bool natives) { natives_ = natives; }
  void SetNatives(bool natives) override { natives_ = natives; }

  std::optional<JitCompoundPattern> forced_pattern() const {
    return forced_pattern_;
  }
  void set_forced_pattern(std::optional<JitCompoundPattern> pattern) {
    forced_pattern_ = pattern;
  }

  bool Mutate(Builder& builder, MutationContext& ctx) override;

 private:
  bool natives_ = true;
  std::optional<JitCompoundPattern> forced_pattern_;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_MUTATORS_JIT_COMPOUND_H_
