// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_MINIMIZER_MINIMIZER_H_
#define V8_EXPERIMENTAL_REDLINE_MINIMIZER_MINIMIZER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/core/component.h"
#include "experimental/redline/fast/tree.h"

namespace redline {

class SyntaxChecker;

// Answers whether a candidate program still has the property that is being
// preserved: reaches a given set of edges, still crashes, ... Called once
// per minimization test, typically running the program in the target.
using MinimizationOracle = std::function<bool(const std::string& source)>;

struct MinimizerOptions {
  // Oracle calls per minimization. Executions are the expensive part, so
  // this bounds the cost of adding one program to the corpus.
  int max_tests = 300;
  // Wall-clock bound per minimization (0 = none).
  uint64_t max_time_ms = 30000;
  // Passes over all reducers (a pass that changes nothing ends early).
  int max_rounds = 3;
  // Adaptive budget: the minimizer caps tests to about `target_time_ms` /
  // execution time, never below `min_tests`. 0 disables scaling so raw
  // `MinimizerOptions` and crash minimization stay uncapped.
  uint64_t target_time_ms = 0;
  int min_tests = 16;
  // The part of the remaining test budget that the `expressions` reducer
  // can use each time that it starts, from 0 to 1. 1 sets no limit and
  // disables early stopping. Most tests of this reducer keep no reduction.
  double expressions_share = 1.0;
  // Run time of the original program in microseconds, set by `ForProgram`.
  // The minimizer uses this estimate until probes measure the real time.
  uint64_t initial_exec_time_us = 0;

  // The test budget for a program that executes in `exec_time_us`.
  int TestsFor(uint64_t exec_time_us) const;
  // These options with `initial_exec_time_us` set to `exec_time_us`.
  MinimizerOptions ForProgram(uint64_t exec_time_us) const;
};

// Why a minimization stopped.
enum class MinimizationStop {
  kNone,        // the minimizer did not run
  kFixedPoint,  // a round of all reducers kept no more reductions
  kBudget,      // the minimizer used up the test budget or the time budget
  kRounds,      // the minimizer ran max_rounds rounds
  kStopped,     // the caller stopped the minimization (the minimizer does
                // not set this value)
};

// The name of `stop` for reports and events: "skipped", "fixed_point",
// "budget", "rounds" or "stopped".
const char* MinimizationStopName(MinimizationStop stop);

struct MinimizationResult {
  int tests = 0;            // oracle calls
  int accepted = 0;         // ... that kept the property (reductions kept)
  int syntax_rejected = 0;  // candidates the V8 parser refused (not executed)
  size_t nodes_before = 0;
  size_t nodes_after = 0;
  uint64_t time_us = 0;
  bool budget_exhausted = false;
  MinimizationStop stop = MinimizationStop::kNone;
};

class Reducer;

// Shared state of one minimization: the tree, the oracle, the budget and
// the statistics. Reducers edit the tree and call Test(); on a false return
// they must undo their edit.
class MinimizationHelper {
 public:
  MinimizationHelper(fast::Tree* tree, const MinimizationOracle& oracle,
                     SyntaxChecker* checker, const MinimizerOptions& options);

  fast::Tree* tree() { return tree_; }
  // Prints the tree and asks the oracle whether the property still holds.
  // Returns false without testing once the budget is exhausted.
  bool Test();
  bool BudgetExhausted() const;
  // The tests that remain in the adaptive test budget.
  int TestsLeft() const;
  // The current test cap after adaptive execution-time scaling.
  int EffectiveMaxTests() const;
  // The current execution time estimate in microseconds for budget scaling.
  uint64_t EstimatedExecTimeUs() const;
  const MinimizerOptions& options() const { return options_; }
  void set_reducer(Reducer* reducer) { reducer_ = reducer; }
  const MinimizationResult& result() const { return result_; }
  MinimizationResult* mutable_result() { return &result_; }

 private:
  fast::Tree* tree_;
  const MinimizationOracle& oracle_;
  SyntaxChecker* checker_;
  const MinimizerOptions& options_;
  Reducer* reducer_ = nullptr;
  MinimizationResult result_;
  uint64_t start_us_;
  uint64_t total_probe_time_us_ = 0;
  uint64_t last_kept_probe_time_us_ = 0;
  int kept_probes_ = 0;
};

// One minimization strategy (statement removal, block unwrapping, ...).
// Reducers are Components: `redline minimize --reducer=NAME` runs one alone.
class Reducer : public Component {
 public:
  Reducer(std::string name, std::string_view description)
      : Component(std::move(name)), description_(description) {}
  std::string_view description() const override { return description_; }

  // Applies all reductions this reducer can find, keeping the ones the
  // oracle accepts. Returns the number of accepted reductions.
  virtual int Reduce(MinimizationHelper& helper) = 0;

 private:
  std::string_view description_;
};

class Minimizer {
 public:
  explicit Minimizer(MinimizerOptions options = MinimizerOptions());
  ~Minimizer();

  // Every reducer (the components), in registration order.
  static std::vector<std::unique_ptr<Reducer>> CreateDefaultReducers();
  // The reducers Minimize() runs when no order is configured, in order:
  // `dataflow` first, `truncate` not.
  static const std::vector<std::string_view>& DefaultOrder();
  // The reducers of a crash minimization without a configured order
  // (crash_minimizer.reducers), in order: all reducers, `dataflow` first.
  static const std::vector<std::string_view>& CrashOrder();

  const MinimizerOptions& options() const { return options_; }
  const std::vector<std::unique_ptr<Reducer>>& reducers() const {
    return reducers_;
  }
  Reducer* Find(std::string_view name) const;
  std::vector<std::string> Names() const;
  std::vector<const Component*> Components() const;

  // Minimizes `tree` in place with all reducers. `checker` (optional)
  // filters candidates the V8 parser rejects before they reach the oracle.
  MinimizationResult Minimize(fast::Tree* tree,
                              const MinimizationOracle& oracle,
                              SyntaxChecker* checker) const;
  MinimizationResult Minimize(fast::Tree* tree,
                              const MinimizationOracle& oracle,
                              SyntaxChecker* checker,
                              const MinimizerOptions& options) const;
  // Same with an explicit reducer sequence.
  MinimizationResult MinimizeWith(const std::vector<Reducer*>& reducers,
                                  fast::Tree* tree,
                                  const MinimizationOracle& oracle,
                                  SyntaxChecker* checker) const;
  MinimizationResult MinimizeWith(const std::vector<Reducer*>& reducers,
                                  fast::Tree* tree,
                                  const MinimizationOracle& oracle,
                                  SyntaxChecker* checker,
                                  const MinimizerOptions& options) const;

 private:
  MinimizerOptions options_;
  std::vector<std::unique_ptr<Reducer>> reducers_;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_MINIMIZER_MINIMIZER_H_
