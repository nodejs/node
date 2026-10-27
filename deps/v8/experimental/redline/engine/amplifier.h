// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Crash amplification (DESIGN.md 5.8). Some crashes do not reproduce in a
// fresh target because they depend on timing: a profiler signal, a concurrent
// compile, a GC at a particular moment. The amplify step searches for a plain
// d8 command line that reproduces such a crash reliably, for example extra
// flags plus --stress-runs=K, which runs the script K times in one process.
// ClusterFuzz accepts a testcase together with its d8 flags, so an amplified
// crash can be uploaded. If the program throws an uncaught exception, the
// step wraps its statements in try/catch (param `wrap`), because d8 ends the
// --stress-runs loop at the first uncaught exception.
//
// Strategies (registry, configured with {"amplify": {"strategy": NAME,
// "params": {...}}}):
//   none   the step is off.
//   flags  candidates from stack rules, then the plain loop (default).

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_AMPLIFIER_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_AMPLIFIER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/base/json.h"
#include "experimental/redline/base/json_parser.h"
#include "experimental/redline/core/random.h"
#include "experimental/redline/core/selection.h"
#include "experimental/redline/engine/crash.h"
#include "experimental/redline/exec/target_options.h"

namespace redline {

// One crash to amplify.
struct AmplifyRequest {
  TargetOptions
      target;  // executable, args (with the session's flags), extra env
  std::string program;       // the crashing program, without the prelude
  std::string prelude;       // put in front of the program (empty: none)
  std::string script_path;   // where to write the script for the plain runs
  CrashSignature signature;  // the crash to reproduce
  std::string
      stderr_text;  // of the original crash (stack frames for the rules)
  uint64_t exec_time_us = 0;  // the longest execution seen for this program
  std::function<bool()> stopping;  // optional: true when the fuzzer stops
};

struct AmplifyResult {
  bool amplified = false;
  std::string args;   // extra d8 flags that reproduce it, with --stress-runs=K
  int hits = 0;       // crashes with the same signature, best candidate
  int processes = 0;  // processes run for the best candidate
  int total_processes = 0;  // all processes of the search
  double time_sec = 0;
  std::string note;  // one line: what was tried
  // Set if the step changed the program: its statements wrapped in
  // try/catch, because an uncaught exception ends the --stress-runs loop.
  // The args reproduce this program, not the original.
  std::string program;
};

class AmplifyStrategy {
 public:
  virtual ~AmplifyStrategy() = default;
  virtual std::string_view name() const = 0;
  virtual AmplifyResult Amplify(const AmplifyRequest& request, Random& rng) = 0;
  // The effective parameters, for the `start` event and --print-config.
  virtual JsonObject Params() const = 0;
  // How many times the step may run for one signature in one session.
  virtual int per_signature() const { return 1; }
};

// Builds a strategy from its name and params; nullptr with a message if the
// name or a parameter is unknown or out of range.
std::unique_ptr<AmplifyStrategy> CreateAmplifyStrategy(std::string_view name,
                                                       const JsonValue& params,
                                                       std::string* error);

// Name, parameters and documentation of every strategy, for --help-config.
struct AmplifyStrategyInfo {
  std::string_view name;
  std::string_view doc;
  std::vector<ParamSpec> params;
};
std::vector<AmplifyStrategyInfo> AmplifyStrategies();

// The --stress-runs count for one process: about loop_time_sec of runs,
// between 2 and max_loops.
int StressRunsFor(uint64_t exec_time_us, double loop_time_sec, int max_loops);

// `program` with its statements inside try { ... } catch (e) {} (a directive
// prologue stays in front), so that an uncaught exception does not end a
// --stress-runs loop. Empty if the program does not parse or is empty.
std::string WrapInTry(const std::string& program);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_AMPLIFIER_H_
