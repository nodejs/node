// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_EXEC_RUNNER_H_
#define V8_EXPERIMENTAL_REDLINE_EXEC_RUNNER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/exec/coverage.h"
#include "experimental/redline/exec/execution.h"
#include "experimental/redline/exec/reprl.h"
#include "experimental/redline/exec/target_options.h"

namespace redline {

// One target process plus its coverage bitmap: the unit a worker thread
// owns. Runs scripts and reports which edges were new.
//
// Policy (identical to Fuzzilli's): only executions that *succeed* are
// evaluated for new coverage; crashes are checked against a separate set of
// crash bits; failed and timed out executions never advance the coverage
// state.
class ScriptRunner {
 public:
  struct Result {
    Execution execution;
    // Edges that this execution reached for the first time (only filled for
    // successful executions).
    std::vector<uint32_t> new_edges;
    // For crashes: whether the crash reached edges no earlier crash reached.
    bool new_crash_edges = false;
    // Number of edges set in the bitmap by this execution, whatever the
    // outcome (informational).
    uint32_t edges_hit = 0;
  };

  struct Stats {
    uint64_t executions = 0;
    uint64_t succeeded = 0;
    uint64_t failed = 0;
    uint64_t crashed = 0;
    uint64_t timed_out = 0;
    uint64_t total_exec_time_us = 0;
    uint64_t infrastructure_failures = 0;
  };

  // `id` must be unique per process (it names the shared memory object).
  static std::unique_ptr<ScriptRunner> Create(const TargetOptions& options,
                                              int id, std::string* error);
  ~ScriptRunner();

  // Starts the target, verifies the REPRL handshake and that the target
  // publishes coverage information. Must be called once before Run().
  bool Initialize(std::string* error);

  // Runs `script` with the default timeout and evaluates coverage.
  bool Run(std::string_view script, Result* result, std::string* error);
  bool Run(std::string_view script, uint64_t timeout_us, Result* result,
           std::string* error);

  // Runs `script` without touching the coverage bookkeeping. The bitmap is
  // cleared before and left as is afterwards, so callers can inspect it via
  // coverage() (determinism checks, minimization probes).
  bool RunWithoutEvaluation(std::string_view script, uint64_t timeout_us,
                            Execution* execution, std::string* error);

  CoverageMap& coverage() { return *coverage_; }
  ReprlChild& child() { return *child_; }
  const TargetOptions& options() const { return options_; }
  const Stats& stats() const { return stats_; }
  int id() const { return id_; }

 private:
  ScriptRunner(TargetOptions options, int id);
  bool ExecuteWithRetry(std::string_view script, uint64_t timeout_us,
                        Execution* execution, std::string* error);
  void Account(const Execution& execution);

  TargetOptions options_;
  int id_;
  std::unique_ptr<CoverageMap> coverage_;
  std::unique_ptr<ReprlChild> child_;
  Stats stats_;
  bool initialized_ = false;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_EXEC_RUNNER_H_
