// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The d8 flags of a crash reproducer (DESIGN.md 5.6). A plain d8 process runs
// the reproducer with a list of flags, and the crash signature tells if the
// crash stayed. Delta debugging then removes the flags that the crash does not
// need.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_FLAG_MINIMIZER_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_FLAG_MINIMIZER_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "experimental/redline/engine/crash.h"
#include "experimental/redline/exec/target_options.h"

namespace redline {

// Runs a d8 command line in fresh plain processes (no REPRL) and counts the
// runs that crash with one signature.
class PlainCrashRunner {
 public:
  PlainCrashRunner(const TargetOptions& target, CrashSignature signature,
                   uint64_t timeout_us);

  // True if at least `needed` of `runs` processes with `args` crash with the
  // signature. `args` ends with the script. The runs stop when the hits so
  // far decide the result, when a process does not start, or when the stop
  // function is true.
  bool Crashes(const std::vector<std::string>& args, int runs, int needed);
  // Runs `runs` processes with `args`, without an early stop on the hits, and
  // returns the processes that crashed with the signature. The runs stop
  // when a process does not start or when the stop function is true.
  int Count(const std::vector<std::string>& args, int runs);
  // The runner does not start a process when `stop` is true.
  void set_stop(std::function<bool()> stop) { stop_ = std::move(stop); }
  // All processes of this runner.
  int processes() const { return processes_; }
  // The processes of the last call of Crashes() or Count(), and those that
  // crashed with the signature.
  int last_runs() const { return last_runs_; }
  int last_hits() const { return last_hits_; }
  // Why a process of the last call of Crashes() or Count() did not start,
  // or empty.
  const std::string& error() const { return error_; }

 private:
  std::string executable_;
  std::vector<std::pair<std::string, std::string>> env_;
  CrashSignature signature_;
  uint64_t timeout_us_;
  std::function<bool()> stop_;
  int processes_ = 0;
  int last_runs_ = 0;
  int last_hits_ = 0;
  std::string error_;
};

struct FlagMinimization {
  std::vector<std::string> flags;  // the smallest list that kept the crash
  int tests = 0;                   // calls of the oracle
  bool budget_exhausted = false;   // the search stopped before its end
};

// Removes flags from `flags` while `oracle` stays true. It tries chunks of
// halving size, from the end of the list, down to single flags. Then it
// tries each single flag again, until a pass removes nothing. The flags for
// which `keep` is true stay. The search stops when `exhausted` is true.
// `oracle` must be true for `flags`.
FlagMinimization MinimizeFlags(
    const std::vector<std::string>& flags,
    const std::function<bool(const std::string& flag)>& keep,
    const std::function<bool(const std::vector<std::string>& candidate)>&
        oracle,
    const std::function<bool()>& exhausted);

// The steps of the minimization of a d8 bundle (DESIGN.md 5.6). `parts` ends
// with the crash part, which stays last. The steps remove the history parts
// that `crashes` does not need. Then `reduce(&parts, i)` reduces the crash
// part, and the steps remove history parts again. Then `reduce` reduces each
// history part, and the steps remove history parts again. The steps stop
// when `exhausted` is true. Returns the parts at the end.
std::vector<std::string> MinimizeBundleParts(
    std::vector<std::string> parts,
    const std::function<bool(const std::vector<std::string>& candidate)>&
        crashes,
    const std::function<void(std::vector<std::string>* parts, size_t i)>&
        reduce,
    const std::function<bool()>& exhausted);

// True if `flag` is the d8 flag `name` (for example "allow-natives-syntax"),
// also with underscores and with a value.
bool IsFlag(std::string_view flag, std::string_view name);

// True if `script` calls a runtime function with `%`, which needs the flag
// --allow-natives-syntax. Also true if `script` does not parse.
bool HasNativeCall(std::string_view script);

// True for a flag that the flag search of a reproducer keeps. If `natives`
// is true (the script calls a runtime function with `%`), it keeps
// --allow-natives-syntax. It always keeps --single-threaded and
// --random-seed, because they make a plain d8 run more deterministic. It
// also keeps --stress-runs, which an amplified crash needs.
bool KeepReproFlag(std::string_view flag, bool natives);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_FLAG_MINIMIZER_H_
