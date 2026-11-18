// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_MUTATORS_EXPLORE_H_
#define V8_EXPERIMENTAL_REDLINE_MUTATORS_EXPLORE_H_

#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/mutators/mutator.h"

namespace redline {

// The runtime-assisted "Explore" mutator, after Fuzzilli's ExplorationMutator.
//
// Static type guesses only go so far: for most variables the fuzzer does not
// know which properties and methods exist, and it never knows about objects
// coming from builtins it has no model of (d8.test.*, Intl, user classes,
// ...). Exploration asks the target instead:
//
//  1. Mutate(): inserts a few probes `__redline_explore("eN", v, [args])`
//     (wrapped in try/catch) at random statement positions, each exploring
//     one visible variable `v` with a few other visible variables and
//     literals as potential arguments. The probes are described in
//     MutationContext::probes.
//  2. The worker executes the instrumented program with kExploreRuntime
//     prepended. At runtime the probe inspects the value (typeof, property
//     names along the prototype chain, function arity, ...) picks an action
//     that makes sense for it (call a method, load/store a property, call or
//     construct a function, arithmetic/comparison for primitives, iterate,
//     ...), performs it to check that it does not throw, and reports it as
//     one JSON line "EXPLORE_ACTION: {...}" on the fuzzout channel.
//  3. ApplyExploreActions() replaces every probe that reported an action by
//     ordinary code performing exactly that action (`let rN = v.m(a, b);`),
//     and removes the probes that reported nothing. The result is a normal,
//     deterministic program that is executed and evaluated as usual.
//
// One exploration therefore costs one extra execution.

struct ExploreArg {
  enum class Kind {
    kVariable,
    kNumber,
    kString,
    kBoolean,
    kBigInt,
    kUndefined,
    kNull
  };
  Kind kind = Kind::kUndefined;
  std::string text;   // variable name, number/bigint digits or string value
  bool flag = false;  // boolean value
};

struct ExploreProbe {
  std::string id;        // "e<N>"/"p<N>", unique within the program
  std::string variable;  // explored/probed variable
  std::vector<ExploreArg> args;
  bool can_await = false;  // the probe sits in an async function
};

// A mutator in two halves: Mutate() only instruments the tree (and fills
// MutationContext::probes); the worker runs the instrumented program with
// runtime() prepended and hands the target's fuzzout to Apply(), which
// turns the probes into ordinary code.
class RuntimeAssistedMutator : public Mutator {
 public:
  using Mutator::Mutator;
  bool runtime_assisted() const override { return true; }
  // JavaScript prepended to the instrumented program.
  virtual std::string_view runtime() const = 0;
  // Rewrites the probes according to `fuzzout`. Returns the number of probes
  // turned into code (0: nothing useful happened; the mutation is a no-op).
  // Probes without an answer are removed. `detail` gets a short description.
  virtual int Apply(Builder& builder, const std::vector<ExploreProbe>& probes,
                    const std::string& fuzzout, MutationContext& ctx,
                    std::string* detail) = 0;

  // Finds the probe statement for `id` (the try statement wrapping the call
  // of `function`), or nullptr.
  static fast::Statement* FindProbe(Builder& builder, std::string_view function,
                                    const std::string& id);
  // The statement `try { function("id", v, extra...); } catch (e) {}`.
  static fast::Statement* MakeProbe(
      fast::Tree* tree, std::string_view function, const ExploreProbe& probe,
      const std::vector<fast::Expression*>& extra);
};

class ExploreMutator final : public RuntimeAssistedMutator {
 public:
  ExploreMutator();
  bool Mutate(Builder& builder, MutationContext& ctx) override;
  std::string_view runtime() const override;
  int Apply(Builder& builder, const std::vector<ExploreProbe>& probes,
            const std::string& fuzzout, MutationContext& ctx,
            std::string* detail) override;

  // Name of the runtime helper function the probes call.
  static constexpr std::string_view kProbeFunction = "__redline_explore";
};

// JavaScript defining kProbeFunction; prepended to instrumented programs.
extern const char kExploreRuntime[];

// Rewrites the probes in `builder`'s tree according to the actions in the
// target's fuzzout output (see ExploreMutator::Apply).
int ApplyExploreActions(Builder& builder,
                        const std::vector<ExploreProbe>& probes,
                        const std::string& fuzzout, std::string* detail);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_MUTATORS_EXPLORE_H_
