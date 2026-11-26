// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_MUTATORS_FIXUP_H_
#define V8_EXPERIMENTAL_REDLINE_MUTATORS_FIXUP_H_

#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/mutators/explore.h"

namespace redline {

// The runtime-assisted "Fixup" mutator, after the guard removal of
// Fuzzilli's FixupMutator.
//
// Salvage wraps a statement that throws in `try { ... } catch {}`. A later
// mutation can remove the cause of the exception. The guard then stays, and
// it keeps the code out of the optimizations that a try block prevents.
// Fixup removes guards that no longer catch an exception:
//
//  1. Mutate(): puts the call `__redline_fixup("fN", 1)` at the start of
//     the body and the call `__redline_fixup("fN")` into the empty handler
//     of some guards (`try { ... } catch {}` or `catch (e) {}` without a
//     finally block). Mutate() does not change the guards in a function that
//     the program gives to `new Worker(...)`.
//  2. The worker executes the instrumented program with the runtime
//     prepended. A body that starts reports "FIXUP_RAN: <id>", and a
//     handler that runs reports "FIXUP_THREW: <id>".
//  3. Apply() removes the calls. A guard whose body ran without a run of
//     its handler becomes its body. If the body declares a lexical
//     binding (let, const, class, using, await using, a function), the body
//     stays in a block, so that the scope of the binding does not change. A
//     first statement that can become a directive also keeps the block.
class FixupMutator final : public RuntimeAssistedMutator {
 public:
  FixupMutator();
  bool Mutate(Builder& builder, MutationContext& ctx) override;
  std::string_view runtime() const override;
  int Apply(Builder& builder, const std::vector<ExploreProbe>& probes,
            const std::string& fuzzout, MutationContext& ctx,
            std::string* detail) override;

  static constexpr std::string_view kProbeFunction = "__redline_fixup";

  // True for `try { ... } catch {}` and `try { ... } catch (e) {}`.
  static bool IsGuard(const fast::Node* node);
  // Replaces the guard `guard` by its body (see Apply()).
  static void Unwrap(Builder& builder, fast::TryStatement* guard);
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_MUTATORS_FIXUP_H_
