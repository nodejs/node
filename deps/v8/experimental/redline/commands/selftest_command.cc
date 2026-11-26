// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "redline selftest": end-to-end checks of the execution layer against a
// real target. Modeled on Fuzzilli's startup tests, extended with the
// behaviours the fuzzer relies on (timeouts, respawning, output capture,
// coverage evaluation and edge determinism). Prints one PASS/FAIL line per
// check and exits non-zero if any check failed.

#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <thread>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/base/v8_runtime.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/commands/flags_command.h"
#include "experimental/redline/commands/selftest_checker.h"
#include "experimental/redline/commands/selftest_selection.h"
#include "experimental/redline/core/environment.h"
#include "experimental/redline/core/program.h"
#include "experimental/redline/core/random.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/corpus/corpus.h"
#include "experimental/redline/corpus/feature_weights.h"
#include "experimental/redline/corpus/material.h"
#include "experimental/redline/engine/amplifier.h"
#include "experimental/redline/engine/config.h"
#include "experimental/redline/engine/crash.h"
#include "experimental/redline/engine/d8_flags.h"
#include "experimental/redline/engine/flag_minimizer.h"
#include "experimental/redline/engine/fuzzer.h"
#include "experimental/redline/engine/importer.h"
#include "experimental/redline/engine/prelude.h"
#include "experimental/redline/engine/salvage.h"
#include "experimental/redline/engine/shared_coverage.h"
#include "experimental/redline/exec/coverage.h"
#include "experimental/redline/exec/process_run.h"
#include "experimental/redline/exec/runner.h"
#include "experimental/redline/exec/target_options.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/parser.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/minimizer/minimizer.h"
#include "experimental/redline/mutators/explore.h"
#include "experimental/redline/mutators/mutator.h"

namespace redline {

namespace {

constexpr const char* kLog = "selftest";

std::string Describe(const Execution& e) {
  return StringPrintf("%s (exit code %d, signal %d, %.1f ms)",
                      OutcomeName(e.outcome), e.exit_code, e.signal,
                      static_cast<double>(e.exec_time_us) / 1000.0);
}

// Runs `script` and returns false on infrastructure failure (reported as
// a failed check).
bool RunOrFail(Checker* checker, ScriptRunner* runner, const char* name,
               const std::string& script, ScriptRunner::Result* result) {
  std::string error;
  if (!runner->Run(script, result, &error)) {
    checker->Check(name, false, "execution failed: " + error);
    return false;
  }
  return true;
}

// experimental/redline/testdata, found from the location of the binary, or
// empty. Look for testdata/selection so we do not pick up unrelated
// out/<dir>/testdata directories created by other build targets.
std::string DefaultTestdataDir() {
  std::string selection = RedlineDataDir("testdata/selection");
  if (!selection.empty()) {
    return selection.substr(0, selection.rfind('/'));
  }
  return RedlineDataDir("testdata");
}

// Offline: the `fuzzilli` and `fuzzilli-wasm` profiles of the random d8
// flags.
void RunFuzzilliFlagProfileChecks(Checker* checker) {
  auto name_of = [](const std::string& flag) {
    return flag.substr(0, flag.find('='));
  };
  auto value_of = [](const std::string& flag) -> int64_t {
    size_t eq = flag.find('=');
    return eq == std::string::npos
               ? -1
               : std::strtoll(flag.c_str() + eq + 1, nullptr, 10);
  };
  // The `fuzzilli` profiles mirror `v8ProcessArgs` without inter-flag
  // sanitization, and emit both polarities of `chooseBooleanFlagRandomly`.
  // Only `fuzzilli-wasm` draws the Wasm flags.
  auto is_wasm_flag = [](const std::string& name) {
    for (const char* prefix :
         {"--wasm-", "--no-wasm-", "--stress-wasm-", "--no-liftoff",
          "--turboshaft-wasm-", "--no-turboshaft-wasm-"}) {
      if (name.rfind(prefix, 0) == 0) return true;
    }
    return false;
  };
  std::string bad_fuzzilli;
  bool saw_pos_bool = false;
  bool saw_neg_bool = false;
  bool saw_wasm = false;
  for (D8FlagProfile profile :
       {D8FlagProfile::kFuzzilli, D8FlagProfile::kFuzzilliWasm}) {
    const bool with_wasm = profile == D8FlagProfile::kFuzzilliWasm;
    Random fz_rng(11);
    for (int i = 0; i < 500 && bad_fuzzilli.empty(); i++) {
      std::set<std::string> seen;
      int64_t size = -1;
      auto has = [&](const char* name) { return seen.count(name) > 0; };
      for (const std::string& flag : RandomD8Flags(fz_rng, profile)) {
        const std::string name = name_of(flag);
        const int64_t value = value_of(flag);
        std::string why;
        if (name == "--maglev-as-top-tier" && !has("--no-turbofan")) {
          why = "without --no-turbofan";
        } else if (name == "--no-maglev" && has("--maglev-as-top-tier")) {
          why = "with --maglev-as-top-tier";
        } else if ((name == "--turbolev" ||
                    name == "--always-osr-from-maglev" ||
                    name == "--optimize-maglev-optimizes-to-turbofan" ||
                    name == "--stress-concurrent-inlining") &&
                   has("--no-turbofan")) {
          why = "with --no-turbofan";
        } else if ((name == "--maglev-non-eager-inlining" ||
                    name == "--always-osr-from-maglev") &&
                   has("--no-maglev")) {
          why = "with --no-maglev";
        } else if (name == "--max-maglev-inlined-bytecode-size-small" &&
                   !has("--maglev-non-eager-inlining")) {
          why = "without --maglev-non-eager-inlining";
        } else if (name == "--turbolev-future" && !has("--turbolev")) {
          why = "without --turbolev";
        } else if (name == "--max-inlined-bytecode-size-small" &&
                   (value == 0
                        ? !has("--turbolev-future")
                        : !has("--turbolev") ||
                              has("--max-inlined-bytecode-size-small"))) {
          why = "without its parent flag";
        } else if (name == "--max-inlined-bytecode-size") {
          size = value;
        } else if (name == "--max-inlined-bytecode-size-cumulative" &&
                   (size < 0 || value < size)) {
          why = "without a smaller --max-inlined-bytecode-size";
        } else if (name == "--scavenger-chaos-mode-threshold" &&
                   !has("--scavenger-chaos-mode")) {
          why = "without --scavenger-chaos-mode";
        } else if (name == "--proto-assign-seq-opt-count" &&
                   !has("--proto-assign-seq-lazy-func-opt")) {
          why = "without --proto-assign-seq-lazy-func-opt";
        } else if (name == "--no-use-ic" && has("--stress-ic")) {
          why = "with --stress-ic";
        } else if (name == "--stress-concurrent-inlining" &&
                   has("--assert-types")) {
          why = "with --assert-types";
        } else if (name == "--stress-concurrent-inlining-attach-code" &&
                   !has("--stress-concurrent-inlining")) {
          why = "without --stress-concurrent-inlining";
        } else if (name == "--wasm-assert-types" && !has("--no-liftoff")) {
          why = "without --no-liftoff";
        } else if (name == "--wasm-sync-tier-up" && has("--no-liftoff")) {
          why = "with --no-liftoff";
        } else if (name == "--stress-wasm-code-gc" && !has("--wasm-code-gc")) {
          why = "without --wasm-code-gc";
        } else if (is_wasm_flag(name) && !with_wasm) {
          why = "in the non-Wasm profile";
        } else if (name == "--fuzzilli-enable-optimized-code-coverage" ||
                   name == "--harmony-struct" || name == "--maglev-licm" ||
                   name == "--maglev-pretenure-store-values" ||
                   name == "--turbo-splitting" ||
                   name == "--shared-string-table" ||
                   name == "--maglev-untagged-phis") {
          why = "is not in the table";
        }
        if (!why.empty()) {
          bad_fuzzilli =
              std::string(D8FlagProfileName(profile)) + ": " + flag + " " + why;
          break;
        }
        seen.insert(name);
        saw_pos_bool |= name == "--always-sparkplug";
        saw_neg_bool |= name == "--no-always-sparkplug";
        saw_wasm |= with_wasm && is_wasm_flag(name);
      }
    }
  }
  checker->Check(
      "random flags: the fuzzilli profiles keep the conditions of Fuzzilli, "
      "both boolean polarities and the Wasm gate",
      bad_fuzzilli.empty() && saw_pos_bool && saw_neg_bool && saw_wasm,
      bad_fuzzilli);

  // The marginal frequencies of the `fuzzilli` profiles are the
  // probabilities of `v8ProcessArgs` (tools/d8_flag_sync.py computes them).
  struct Marginal {
    D8FlagProfile profile;
    const char* name;
    double expected;
  };
  const Marginal marginals[] = {
      {D8FlagProfile::kFuzzilli, "--turbolev", 0.63},
      {D8FlagProfile::kFuzzilli, "--no-maglev", 0.095},
      {D8FlagProfile::kFuzzilli, "--no-use-ic", 0.09},
      {D8FlagProfile::kFuzzilli, "--stress-concurrent-inlining", 0.2025},
      {D8FlagProfile::kFuzzilli, "--max-inlined-bytecode-size-small", 0.202482},
      {D8FlagProfile::kFuzzilli, "--maglev-range-verification", 0.126},
      {D8FlagProfile::kFuzzilliWasm, "--no-liftoff", 0.5},
      {D8FlagProfile::kFuzzilliWasm, "--wasm-sync-tier-up", 0.1},
  };
  constexpr int kMarginalDraws = 20000;
  std::map<std::string, int> fz_counts;
  std::map<std::string, int> wasm_counts;
  Random marginal_rng(29);
  for (int i = 0; i < kMarginalDraws; i++) {
    for (D8FlagProfile profile :
         {D8FlagProfile::kFuzzilli, D8FlagProfile::kFuzzilliWasm}) {
      std::set<std::string> names;
      for (const std::string& flag : RandomD8Flags(marginal_rng, profile)) {
        names.insert(name_of(flag));
      }
      for (const std::string& name : names) {
        (profile == D8FlagProfile::kFuzzilli ? fz_counts : wasm_counts)[name]++;
      }
    }
  }
  std::string bad_marginal;
  for (const Marginal& m : marginals) {
    const int count =
        (m.profile == D8FlagProfile::kFuzzilli ? fz_counts
                                               : wasm_counts)[m.name];
    const double observed = static_cast<double>(count) / kMarginalDraws;
    const double sigma =
        std::sqrt(m.expected * (1 - m.expected) / kMarginalDraws);
    if (std::fabs(observed - m.expected) > 5 * sigma) {
      bad_marginal += StringPrintf(" %s=%.4f (expected %.4f)", m.name, observed,
                                   m.expected);
    }
  }
  checker->Check(
      "random flags: the fuzzilli profiles draw the marginal probabilities "
      "of Fuzzilli",
      bad_marginal.empty(), bad_marginal);
}

// Offline: the profiles of the random d8 flags. The Turbolev profile must not
// draw the flags that only Maglev reads, and the Maglev profile must not draw
// the Turbolev and Turboshaft flags.
void RunD8FlagProfileChecks(Checker* checker) {
  // Returns the first flag of 500 draws that starts with one of `prefixes`
  // and is not in `allowed`.
  auto draws_any = [](D8FlagProfile profile,
                      std::initializer_list<const char*> prefixes,
                      std::initializer_list<const char*> allowed = {}) {
    Random rng(7);
    std::string found;
    for (int i = 0; i < 500 && found.empty(); i++) {
      for (const std::string& flag : RandomD8Flags(rng, profile)) {
        if (std::any_of(allowed.begin(), allowed.end(),
                        [&](const char* name) { return flag == name; })) {
          continue;
        }
        for (const char* prefix : prefixes) {
          if (flag.rfind(prefix, 0) == 0) {
            found = flag;
          }
        }
      }
    }
    return found;
  };
  D8FlagProfile parsed = D8FlagProfile::kMaglev;
  checker->Check(
      "random flags: the profile names parse",
      ParseD8FlagProfile("turbolev", &parsed) &&
          parsed == D8FlagProfile::kTurbolev &&
          ParseD8FlagProfile("maglev", &parsed) &&
          parsed == D8FlagProfile::kMaglev &&
          ParseD8FlagProfile("fuzzilli", &parsed) &&
          parsed == D8FlagProfile::kFuzzilli &&
          std::string(D8FlagProfileName(parsed)) == "fuzzilli" &&
          ParseD8FlagProfile("fuzzilli-wasm", &parsed) &&
          parsed == D8FlagProfile::kFuzzilliWasm &&
          std::string(D8FlagProfileName(parsed)) == "fuzzilli-wasm" &&
          ParseD8FlagProfile("runtime", &parsed) &&
          parsed == D8FlagProfile::kRuntime &&
          std::string(D8FlagProfileName(parsed)) == "runtime" &&
          !ParseD8FlagProfile("turbofan", &parsed),
      "");
  // Turbolev builds its graph with the Maglev graph builder, so it also reads
  // the Maglev assertions and the allowed --no-maglev- flags. Only Turbolev
  // reads --maglev-range-analysis.
  std::string maglev_only = draws_any(
      D8FlagProfile::kTurbolev,
      {"--maglev-future", "--maglev-non-eager", "--max-maglev",
       "--optimize-on-next-call-optimizes-to-maglev", "--no-maglev-",
       "--maglev-as-top-tier", "--maglev-inline-api-calls"},
      {"--no-maglev-inlining", "--no-maglev-poly-calls",
       "--no-maglev-escape-analysis", "--no-maglev-optimistic-peeled-loops",
       "--no-maglev-truncation", "--no-maglev-cse",
       "--no-maglev-range-analysis"});
  checker->Check("random flags: the turbolev profile draws no Maglev-only flag",
                 maglev_only.empty(), maglev_only);
  const bool turbolev_has_maglev_assert =
      !draws_any(D8FlagProfile::kTurbolev, {"--maglev-assert"},
                 {"--maglev-assert-types"})
           .empty() &&
      !draws_any(D8FlagProfile::kTurbolev, {"--maglev-assert-types"}).empty();
  checker->Check("random flags: the turbolev profile draws Maglev assertions",
                 turbolev_has_maglev_assert, "");
  std::string turbolev_only = draws_any(
      D8FlagProfile::kMaglev,
      {"--no-turbolev", "--no-turboshaft", "--turboshaft-",
       "--verify-turboshaft", "--maglev-range-verification",
       "--always-osr-from-maglev", "--no-additive-safe-int-feedback",
       "--max-inlined-bytecode-size", "--no-maglev-range-analysis",
       "--no-turbo-", "--turbo-", "--optimize-maglev-optimizes-to-turbofan",
       "--stress-concurrent-inlining", "--concurrent-recompilation-"});
  checker->Check("random flags: the maglev profile draws no Turbolev flag",
                 turbolev_only.empty(), turbolev_only);
  // The verifiers and stress flags of the V8 profile of Fuzzilli.
  std::string missing_new;
  for (const char* flag :
       {"--turbo-verify", "--turbo-verify-allocation", "--efficiency-mode",
        "--battery-saver-mode", "--no-short-builtin-calls",
        "--maglev-assert-stack-size", "--stress-background-compile",
        "--lazy-new-space-shrinking"}) {
    if (draws_any(D8FlagProfile::kTurbolev, {flag}).empty()) {
      missing_new += std::string(" turbolev:") + flag;
    }
    if (strncmp(flag, "--turbo-", 8) == 0) continue;
    if (draws_any(D8FlagProfile::kMaglev, {flag}).empty()) {
      missing_new += std::string(" maglev:") + flag;
    }
  }
  checker->Check(
      "random flags: the maglev and turbolev profiles draw the Fuzzilli "
      "verifier and stress flags",
      missing_new.empty(), missing_new);
  // The runtime profile draws no JIT flag and no flag of the base d8 flags.
  std::string runtime_jit = draws_any(
      D8FlagProfile::kRuntime,
      {"--maglev", "--no-maglev", "--turbo", "--no-turbo", "--max-",
       "--optimize-", "--assert-types", "--jit-fuzzing", "--always-",
       "--sparkplug", "--no-sparkplug", "--deopt-every", "--no-enable-",
       "--stress-concurrent", "--verify-turboshaft", "--concurrent-",
       "--stress-gc-during-compilation", "--no-baseline-batch-compilation",
       "--disable-loop-stack-checks"});
  for (const std::string& base : TargetOptions::DefaultArgs()) {
    std::string found = draws_any(D8FlagProfile::kRuntime, {base.c_str()});
    if (!found.empty() && runtime_jit.empty()) runtime_jit = found;
  }
  const bool runtime_draws =
      !draws_any(D8FlagProfile::kRuntime, {"--stress-ic"}).empty() &&
      !draws_any(D8FlagProfile::kRuntime, {"--no-use-ic"}).empty() &&
      !draws_any(D8FlagProfile::kRuntime, {"--stress-lazy"}).empty() &&
      !draws_any(D8FlagProfile::kRuntime, {"--stress-compaction"}).empty() &&
      !draws_any(D8FlagProfile::kRuntime, {"--efficiency-mode"}).empty();
  checker->Check(
      "random flags: the runtime profile draws runtime flags and no JIT or "
      "base flag",
      runtime_jit.empty() && runtime_draws, runtime_jit);

  // The rules of the draw. A draw names each flag at most once. A flag that
  // needs another flag comes only with that flag. The cumulative inlining
  // limit is not smaller than the limit for one function.
  auto name_of = [](const std::string& flag) {
    return flag.substr(0, flag.find('='));
  };
  auto value_of = [](const std::string& flag) -> int64_t {
    size_t eq = flag.find('=');
    return eq == std::string::npos
               ? -1
               : std::strtoll(flag.c_str() + eq + 1, nullptr, 10);
  };
  std::string bad_draw;
  for (D8FlagProfile profile :
       {D8FlagProfile::kMaglev, D8FlagProfile::kTurbolev}) {
    Random rng(11);
    for (int i = 0; i < 500 && bad_draw.empty(); i++) {
      std::vector<std::string> flags = RandomD8Flags(rng, profile);
      std::set<std::string> names;
      int64_t size = -1;
      bool chaos = false;
      for (const std::string& flag : flags) {
        const std::string name = name_of(flag);
        if (!names.insert(name).second) {
          bad_draw = "two " + name;
        } else if (name == "--max-inlined-bytecode-size") {
          size = value_of(flag);
        } else if (name == "--max-inlined-bytecode-size-cumulative" &&
                   (size < 0 || value_of(flag) < size)) {
          bad_draw = flag + " without a smaller --max-inlined-bytecode-size";
        } else if (name == "--scavenger-chaos-mode") {
          chaos = true;
        } else if (name == "--scavenger-chaos-mode-threshold" && !chaos) {
          bad_draw = flag + " without --scavenger-chaos-mode";
        }
      }
      if (bad_draw.empty()) {
        if (names.count("--verify-bytecode-full") > 0 &&
            names.count("--verify-bytecode-light") > 0) {
          bad_draw = "both --verify-bytecode-full and --verify-bytecode-light";
        } else if (names.count("--no-sparkplug") > 0 &&
                   names.count("--always-sparkplug") > 0) {
          bad_draw = "both --no-sparkplug and --always-sparkplug";
        } else if (names.count("--assert-types") > 0 &&
                   (names.count("--stress-concurrent-inlining") > 0 ||
                    names.count("--stress-concurrent-inlining-attach-code") >
                        0)) {
          bad_draw = "--assert-types with --stress-concurrent-inlining";
        }
      }
    }
  }
  checker->Check(
      "random flags: a draw names each flag once, with the flags it needs",
      bad_draw.empty(), bad_draw);

  // Fixed flags win in `fuzzilli`: sandbox mode suppresses non-sandbox draws,
  // and `SanitizeFuzzilliFixedClashes` drops flags that clash with fixed args.
  struct FuzzilliFixedCase {
    const char* label;
    std::vector<std::string> fixed;
    std::vector<std::string> forbidden;
  };
  const FuzzilliFixedCase fz_fixed_cases[] = {
      {"--sandbox-fuzzing",
       {"--sandbox-fuzzing"},
       {"--verify-heap", "--stress-lazy", "--maglev-assert-types",
        "--turbo-verify-allocation", "--turbo-verify", "--assert-types"}},
      {"--no-maglev",
       {"--no-maglev"},
       {"--maglev-non-eager-inlining", "--maglev-future",
        "--maglev-as-top-tier"}},
      {"--maglev-as-top-tier",
       {"--maglev-as-top-tier"},
       {"--turbolev-future", "--no-maglev", "--stress-concurrent-inlining",
        "--stress-concurrent-inlining-attach-code"}},
      {"--no-turbofan",
       {"--no-turbofan"},
       {"--turbolev", "--always-osr-from-maglev",
        "--optimize-maglev-optimizes-to-turbofan",
        "--stress-concurrent-inlining"}},
      {"--assert-types", {"--assert-types"}, {"--stress-concurrent-inlining"}},
      {"--single-threaded",
       {"--single-threaded"},
       {"--stress-concurrent-inlining",
        "--stress-concurrent-inlining-attach-code"}},
      {"--stress-concurrent-inlining",
       {"--stress-concurrent-inlining"},
       {"--assert-types", "--maglev-as-top-tier"}},
      {"--jitless",
       {"--jitless"},
       {"--maglev-as-top-tier", "--maglev-future", "--turbolev-future",
        "--jit-fuzzing", "--verify-turboshaft", "--always-osr-from-maglev",
        "--optimize-maglev-optimizes-to-turbofan",
        "--stress-concurrent-inlining",
        "--stress-concurrent-inlining-attach-code"}},
      {"--disable-optimizing-compilers",
       {"--disable-optimizing-compilers"},
       {"--verify-turboshaft", "--always-osr-from-maglev",
        "--stress-concurrent-inlining"}},
      {"--max-lazy", {"--max-lazy"}, {"--jit-fuzzing", "--no-lazy"}},
      {"--empty-shared-heap",
       {"--empty-shared-heap"},
       {"--shared-strings", "--shared-string-table"}},
      {"--predictable-gc-schedule",
       {"--predictable-gc-schedule"},
       {"--stress-compaction",
        "--stress-scavenger-conservative-object-pinning-random"}},
  };
  std::string bad_fz_fixed;
  for (const FuzzilliFixedCase& c : fz_fixed_cases) {
    Random rng(19);
    for (int i = 0; i < 300 && bad_fz_fixed.empty(); i++) {
      for (const std::string& flag :
           RandomD8Flags(rng, D8FlagProfile::kFuzzilli, c.fixed)) {
        const std::string name = name_of(flag);
        for (const std::string& forbid : c.forbidden) {
          if (name == forbid) {
            bad_fz_fixed = std::string(c.label) + " drew " + flag;
            break;
          }
        }
        if (!bad_fz_fixed.empty()) break;
      }
    }
  }
  checker->Check(
      "random flags: the fuzzilli profile respects fixed flags and sandbox "
      "mode",
      bad_fz_fixed.empty(), bad_fz_fixed);
  // --single-threaded and --assert-types stop the extra jobs of
  // --stress-concurrent-inlining.
  TunableRegistry& registry = TunableRegistry::Instance();
  std::string error;
  bool set =
      registry.Set("flags.single_threaded", 1, &error) &&
      registry.Set("flags.stress_concurrent_inlining", 1, &error) &&
      registry.Set("flags.stress_concurrent_inlining_attach_code", 1, &error);
  std::string concurrent = set ? draws_any(D8FlagProfile::kTurbolev,
                                           {"--stress-concurrent-inlining"})
                               : error;
  registry.Reset();
  checker->Check(
      "random flags: --single-threaded comes without "
      "--stress-concurrent-inlining",
      set && concurrent.empty(), concurrent);
  bool set_assert =
      registry.Set("flags.assert_types", 1, &error) &&
      registry.Set("flags.stress_concurrent_inlining", 1, &error) &&
      registry.Set("flags.stress_concurrent_inlining_attach_code", 1, &error);
  std::string assert_concurrent =
      set_assert ? draws_any(D8FlagProfile::kTurbolev,
                             {"--stress-concurrent-inlining"})
                 : error;
  registry.Reset();
  checker->Check(
      "random flags: --assert-types comes without "
      "--stress-concurrent-inlining",
      set_assert && assert_concurrent.empty(), assert_concurrent);

  // A fixed --single-threaded or --assert-types also removes the
  // --stress-concurrent-inlining flags. Without a fixed flag, the same
  // tunables give these flags in each draw.
  auto fixed_draws = [](const std::vector<std::string>& fixed) {
    Random rng(19);
    int with_concurrent = 0;
    for (int i = 0; i < 100; i++) {
      for (const std::string& flag :
           RandomD8Flags(rng, D8FlagProfile::kTurbolev, fixed)) {
        if (flag.rfind("--stress-concurrent-inlining", 0) == 0) {
          with_concurrent++;
          break;
        }
      }
    }
    return with_concurrent;
  };
  const bool set_concurrent =
      registry.Set("flags.stress_concurrent_inlining", 1, &error) &&
      registry.Set("flags.stress_concurrent_inlining_attach_code", 1, &error);
  const int no_fixed = fixed_draws({});
  const int fixed_st = fixed_draws({"--single-threaded"});
  const int fixed_assert = fixed_draws({"--assert-types"});
  registry.Reset();
  checker->Check(
      "random flags: a fixed --single-threaded or --assert-types removes "
      "--stress-concurrent-inlining",
      set_concurrent && no_fixed == 100 && fixed_st == 0 && fixed_assert == 0,
      StringPrintf("no fixed flag %d, --single-threaded %d, --assert-types %d",
                   no_fixed, fixed_st, fixed_assert));

  // --maglev-as-top-tier turns off Turbofan and Turbolev, and turns on Maglev.
  // Like Fuzzilli, the Maglev profile draws it only with a fixed
  // --no-turbofan. A fixed or drawn --turbofan, --turbolev or --no-maglev
  // removes it.
  auto top_tier_draws = [](const std::vector<std::string>& fixed) {
    Random rng(23);
    int with_top_tier = 0;
    for (int i = 0; i < 100; i++) {
      const std::vector<std::string> flags =
          RandomD8Flags(rng, D8FlagProfile::kMaglev, fixed);
      if (std::find(flags.begin(), flags.end(), "--maglev-as-top-tier") !=
          flags.end()) {
        with_top_tier++;
      }
    }
    return with_top_tier;
  };
  auto sanitized_has_top_tier = [](const char* other) {
    std::vector<std::string> flags = {"--maglev-as-top-tier", other};
    SanitizeD8Flags(&flags);
    return std::find(flags.begin(), flags.end(), "--maglev-as-top-tier") !=
           flags.end();
  };
  const bool set_top_tier = registry.Set("flags.maglev_as_top_tier", 1, &error);
  const int top_no_fixed = top_tier_draws({});
  const int top_no_turbofan = top_tier_draws({"--no-turbofan"});
  const int top_turbolev = top_tier_draws({"--no-turbofan", "--turbolev"});
  const int top_no_maglev = top_tier_draws({"--no-turbofan", "--no-maglev"});
  const int top_disabled =
      top_tier_draws({"--no-turbofan", "--disable-optimizing-compilers"});
  registry.Reset();
  const bool drawn_removed = !sanitized_has_top_tier("--turbofan") &&
                             !sanitized_has_top_tier("--turbolev") &&
                             !sanitized_has_top_tier("--no-maglev") &&
                             sanitized_has_top_tier("--no-turbofan");
  checker->Check(
      "random flags: --maglev-as-top-tier needs --no-turbofan and no clash",
      set_top_tier && top_no_fixed == 0 && top_no_turbofan == 100 &&
          top_turbolev == 0 && top_no_maglev == 0 && top_disabled == 0 &&
          drawn_removed,
      StringPrintf("no fixed flag %d, --no-turbofan %d, --turbolev %d, "
                   "--no-maglev %d, --disable-optimizing-compilers %d, "
                   "drawn %d",
                   top_no_fixed, top_no_turbofan, top_turbolev, top_no_maglev,
                   top_disabled, drawn_removed ? 1 : 0));

  // `redline flags` draws reproducible flag sets and stops when random `d8`
  // flags are off.
  auto run_flags = [](std::initializer_list<const char*> args,
                      std::vector<std::string>* out, std::string* err) {
    std::vector<const char*> argv = {"redline", "flags"};
    argv.insert(argv.end(), args.begin(), args.end());
    Flags parsed_flags =
        Flags::Parse(static_cast<int>(argv.size()), argv.data(), 2);
    return DrawD8FlagLines(parsed_flags, out, err);
  };
  std::vector<std::string> maglev_lines, maglev_again, turbolev_lines,
      fuzzilli_lines, wasm_lines, runtime_lines, builtins_lines, bad_out;
  std::string flags_err, bad_err, disabled_err;
  const bool maglev_ok = run_flags({"--preset=maglev", "--n=20", "--seed=7"},
                                   &maglev_lines, &flags_err) &&
                         run_flags({"--preset=maglev", "--n=20", "--seed=7"},
                                   &maglev_again, &flags_err);
  const bool turbolev_ok = run_flags(
      {"--preset=turbolev", "--n=20", "--seed=7"}, &turbolev_lines, &flags_err);
  const bool fuzzilli_ok = run_flags(
      {"--preset=fuzzilli", "--n=20", "--seed=7"}, &fuzzilli_lines, &flags_err);
  const bool subsystem_ok =
      run_flags({"--preset=wasm", "--n=20", "--seed=7"}, &wasm_lines,
                &flags_err) &&
      run_flags({"--preset=runtime", "--n=20", "--seed=7"}, &runtime_lines,
                &flags_err) &&
      run_flags({"--preset=builtins", "--n=20", "--seed=7"}, &builtins_lines,
                &flags_err);
  const bool bad_rejected =
      !run_flags({"--preset=maglev", "--n=0"}, &bad_out, &bad_err);
  const bool disabled_rejected =
      !run_flags({"--preset=default"}, &bad_out, &disabled_err) &&
      disabled_err.find("randomize_d8_flags") != std::string::npos;
  registry.Reset();
  const bool saw_top_tier = std::any_of(
      maglev_lines.begin(), maglev_lines.end(), [](const std::string& line) {
        return line.find("--maglev-as-top-tier") != std::string::npos;
      });
  const bool saw_turbolev_flag = std::any_of(
      turbolev_lines.begin(), turbolev_lines.end(),
      [](const std::string& line) {
        return line.find("--verify-turboshaft") != std::string::npos ||
               line.find("--stress-concurrent-inlining") != std::string::npos;
      });
  const bool saw_fuzzilli_tier =
      std::any_of(fuzzilli_lines.begin(), fuzzilli_lines.end(),
                  [](const std::string& line) {
                    return line.find("--turbolev") != std::string::npos;
                  });
  auto any_line_has = [](const std::vector<std::string>& lines,
                         const char* text) {
    return std::any_of(lines.begin(), lines.end(), [&](const std::string& l) {
      return l.find(text) != std::string::npos;
    });
  };
  checker->Check(
      "random flags: the wasm, runtime and builtins presets draw their "
      "profiles",
      subsystem_ok && wasm_lines.size() == 20 && runtime_lines.size() == 20 &&
          builtins_lines.size() == 20 && any_line_has(wasm_lines, "--wasm-") &&
          any_line_has(wasm_lines, "--turbolev") &&
          !any_line_has(runtime_lines, "--maglev") &&
          !any_line_has(builtins_lines, "--turbo") &&
          any_line_has(builtins_lines, "--stress-lazy"),
      flags_err);
  checker->Check(
      "random flags: the flags command draws preset flag sets",
      maglev_ok && turbolev_ok && fuzzilli_ok && bad_rejected &&
          disabled_rejected && maglev_lines.size() == 20 &&
          turbolev_lines.size() == 20 && fuzzilli_lines.size() == 20 &&
          maglev_lines == maglev_again && maglev_lines != turbolev_lines &&
          fuzzilli_lines != turbolev_lines && saw_top_tier &&
          saw_turbolev_flag && saw_fuzzilli_tier,
      flags_err);
}

// Offline: crash signatures. A recursion that overflows the stack must get
// the same signature wherever the stack ran out; other crashes keep their
// first frames.
void RunCrashSignatureChecks(Checker* checker) {
  auto frame = [](const char* mangled) {
    return StringPrintf("./d8(%s+0x10)[0x5555]\n", mangled);
  };
  auto trace = [&](const char* top, int depth) {
    std::string text = "Received signal 11 SEGV_MAPERR 7ffd00000ff8\n\n";
    text += "==== C stack trace ===============================\n\n";
    text += frame(top);
    const char* cycle[] = {"_ZN2v88internal3FooEv", "_ZN2v88internal3BarEv",
                           "_ZN2v88internal3BazEv"};
    for (int i = 0; i < depth; i++) {
      text += frame(cycle[i % 3]);
    }
    return text + "[end of stack trace]\n";
  };
  CrashSignature a =
      CrashSignature::FromOutput(11, 0, trace("_ZN2v88internal4Top1Ev", 57));
  CrashSignature b =
      CrashSignature::FromOutput(11, 0, trace("_ZN2v88internal4Top2Ev", 55));
  checker->Check(
      "crash signature: a stack overflow does not depend on the faulting frame",
      a == b, a.text + " vs " + b.text);
  checker->Check("crash signature: a stack overflow names its recursion",
                 a.text ==
                     "sig11 SEGV_MAPERR stack-overflow v8::internal::Bar | "
                     "v8::internal::Baz | v8::internal::Foo",
                 a.text);
  CrashSignature c =
      CrashSignature::FromOutput(11, 0, trace("_ZN2v88internal4Top1Ev", 2));
  checker->Check("crash signature: other crashes keep their first three frames",
                 c.text ==
                     "sig11 SEGV_MAPERR v8::internal::Top1 < v8::internal::Foo "
                     "< v8::internal::Bar",
                 c.text);
  CrashSignature d = CrashSignature::FromOutput(
      5, 0,
      "abort: CSA_DCHECK failed: Torque assert 'Is<A>(o)' failed "
      "[src/builtins/cast.tq:1025] [gen/torque-generated/src/builtins/"
      "regexp-test-tq-csa.cc:174]\n\nStacktrace:\n");
  checker->Check(
      "crash signature: a failed CSA_DCHECK names its builtin and message",
      d.text == "csa regexp-test-tq-csa.cc:174 Torque assert 'Is<A>(o)' failed",
      d.text);
}

// Offline: the d8 bundle of a state-dependent crash (MakeReprlBundle).
void RunReprlBundleChecks(Checker* checker) {
  std::string bundle;
  bool ok = MakeReprlBundle("// header\n//\n", {"let x = 1;", "let x = 2;\n"},
                            &bundle);
  checker->Check("bundle: one script part for each script, in order",
                 ok && bundle ==
                           "// header\n//\n"
                           "// JS_BUNDLE_SCRIPT\nlet x = 1;\n"
                           "// JS_BUNDLE_SCRIPT\nlet x = 2;\n",
                 bundle);
  checker->Check(
      "bundle: a script with the marker prefix is refused",
      !MakeReprlBundle("", {"s = '// JS_BUNDLE_MODULE:a.mjs';"}, &bundle), "");
  checker->Check("bundle: a header line that is not a comment is refused",
                 !MakeReprlBundle("// a\nvar b;\n", {"1;"}, &bundle), "");
}

// Offline: the coverage header (experimental/redline/d8/cov.h) on synthetic
// shared memory.
void RunCoverageHeaderChecks(Checker* checker) {
  std::string error;
  std::unique_ptr<CoverageMap> map = CoverageMap::Create(9998, &error);
  if (!checker->Check("coverage header: create a coverage map", map != nullptr,
                      error)) {
    return;
  }
  int fd = shm_open(map->shm_name().c_str(), O_RDWR, 0);
  void* mapping = fd < 0 ? MAP_FAILED
                         : mmap(nullptr, CoverageMap::kShmSize,
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fd >= 0) {
    close(fd);
  }
  const bool mapped = mapping != MAP_FAILED;
  if (!checker->Check("coverage header: map the shared memory", mapped,
                      mapped ? "" : strerror(errno))) {
    return;
  }
  uint8_t* shm = static_cast<uint8_t*>(mapping);
  // A target with inline-bool-flag, 80 code edges and 10 builtins blocks
  // writes this header.
  cov::Header header = {};
  header.magic = cov::kMagic;
  header.version = cov::kVersion;
  header.header_size = cov::kHeaderSize;
  header.shm_size = static_cast<uint32_t>(CoverageMap::kShmSize);
  header.mode = static_cast<uint32_t>(cov::Mode::kInlineBoolFlag);
  header.bits_per_edge = 1;
  header.num_edges = 90;
  header.max_edges = cov::MaxEdges(header.shm_size, 1);
  header.num_regions = 2;
  header.regions[0] = {static_cast<uint32_t>(cov::RegionKind::kCode), 1, 80};
  header.regions[1] = {static_cast<uint32_t>(cov::RegionKind::kBuiltins), 81,
                       10};
  header.build_id_size = 2;
  header.build_id[0] = 0xab;
  header.build_id[1] = 0x01;
  memcpy(shm, &header, sizeof(header));
  bool ok = map->FinishInitialization(&error);
  checker->Check("coverage header: read a valid header", ok, error);
  if (ok) {
    checker->Check(
        "coverage header: geometry",
        map->num_edges() == 90 &&
            strcmp(map->mode_name(), "inline-bool-flag") == 0 &&
            map->build_id() == "ab01",
        StringPrintf("%u edges, mode %s, build id %s", map->num_edges(),
                     map->mode_name(), map->build_id().c_str()));
    // The bitmap starts after the header.
    uint8_t* bits = shm + cov::kHeaderSize;
    bits[5 / 8] |= static_cast<uint8_t>(1u << (5 % 8));
    bits[85 / 8] |= static_cast<uint8_t>(1u << (85 % 8));
    const std::vector<uint32_t> expected = {5, 85};
    checker->Check("coverage header: bits after the header",
                   map->CurrentEdges() == expected,
                   StringPrintf("%zu edges", map->CurrentEdges().size()));
    // A new target process with other edges is another binary.
    cov::Header other = header;
    other.num_edges = 91;
    other.regions[1].count = 11;
    memcpy(shm, &other, sizeof(other));
    std::string changed;
    checker->Check("coverage header: a changed header is an error",
                   !map->FinishInitialization(&changed) &&
                       changed.find("mixing binaries") != std::string::npos,
                   changed);
  }
  // Headers that this Redline rejects.
  struct BadHeader {
    const char* name;
    cov::Header header;
  };
  BadHeader bad[4] = {{"no magic", header},
                      {"a region beyond the edges", header},
                      {"no instrumentation", header},
                      {"a wrong edge size", header}};
  bad[0].header.magic = 0;
  bad[1].header.regions[1].count = 11;
  bad[2].header.mode = static_cast<uint32_t>(cov::Mode::kNone);
  bad[3].header.mode = static_cast<uint32_t>(cov::Mode::kInline8bitCounters);
  for (const BadHeader& b : bad) {
    std::unique_ptr<CoverageMap> fresh = CoverageMap::Create(9997, &error);
    if (fresh == nullptr) {
      checker->Check("coverage header: create a coverage map", false, error);
      break;
    }
    int bad_fd = shm_open(fresh->shm_name().c_str(), O_RDWR, 0);
    if (bad_fd >= 0) {
      if (pwrite(bad_fd, &b.header, sizeof(b.header), 0) !=
          static_cast<ssize_t>(sizeof(b.header))) {
        bad_fd = -1;
      }
      close(bad_fd);
    }
    std::string reason;
    checker->Check(StringPrintf("coverage header: reject %s", b.name).c_str(),
                   bad_fd >= 0 && !fresh->FinishInitialization(&reason),
                   reason);
  }
  munmap(mapping, CoverageMap::kShmSize);
}

// Offline: the counters layout of the coverage (exec/coverage.h) on a
// synthetic header, and the feature weights (corpus/feature_weights.h).
void RunCoverageCounterChecks(Checker* checker) {
  const uint32_t buckets[][2] = {{1, 0},   {2, 1},   {3, 2},  {4, 3},  {7, 3},
                                 {8, 4},   {15, 4},  {16, 5}, {31, 5}, {32, 6},
                                 {127, 6}, {128, 7}, {255, 7}};
  bool buckets_ok = true;
  for (const auto& [count, bucket] : buckets) {
    buckets_ok = buckets_ok &&
                 CoverageMap::Bucket(static_cast<uint8_t>(count)) == bucket;
  }
  checker->Check("coverage counters: AFL bucket table", buckets_ok, "");

  std::string error;
  std::unique_ptr<CoverageMap> map = CoverageMap::Create(9999, &error);
  if (!checker->Check("coverage counters: create a coverage map",
                      map != nullptr, error)) {
    return;
  }
  // A target with 80 code edges, 10 builtins blocks and 10 optimized code
  // features writes this header.
  int fd = shm_open(map->shm_name().c_str(), O_RDWR, 0);
  void* mapping = fd < 0 ? MAP_FAILED
                         : mmap(nullptr, CoverageMap::kShmSize,
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fd >= 0) {
    close(fd);
  }
  const bool mapped = mapping != MAP_FAILED;
  if (!checker->Check("coverage counters: map the shared memory", mapped,
                      mapped ? "" : strerror(errno))) {
    return;
  }
  uint8_t* shm = static_cast<uint8_t*>(mapping);
  cov::Header header = {};
  header.magic = cov::kMagic;
  header.version = cov::kVersion;
  header.header_size = cov::kHeaderSize;
  header.shm_size = static_cast<uint32_t>(CoverageMap::kShmSize);
  header.mode = static_cast<uint32_t>(cov::Mode::kInline8bitCounters);
  header.bits_per_edge = 8;
  header.num_edges = 100;
  header.max_edges = cov::MaxEdges(header.shm_size, 8);
  header.num_regions = 3;
  header.regions[0] = {static_cast<uint32_t>(cov::RegionKind::kCode), 1, 80};
  header.regions[1] = {static_cast<uint32_t>(cov::RegionKind::kBuiltins), 81,
                       10};
  header.regions[2] = {static_cast<uint32_t>(cov::RegionKind::kOptimized), 91,
                       10};
  memcpy(shm, &header, sizeof(header));
  CoverageMap::Watch watch;
  watch.edges = {7, 5};
  map->SetWatch(watch);
  bool ok = map->FinishInitialization(&error);
  checker->Check("coverage counters: read the header", ok, error);
  if (ok) {
    checker->Check(
        "coverage counters: geometry",
        map->counters_mode() && map->real_edges() == 100 &&
            map->counted_edges() == 90 && map->watched().size() == 12 &&
            map->num_edges() == 100 + 12 * CoverageMap::kBuckets,
        StringPrintf("features %u, counted %u, watched %zu", map->num_edges(),
                     map->counted_edges(), map->watched().size()));
    uint8_t* counters = shm + cov::kHeaderSize;
    counters[5] = 3;
    counters[50] = 1;
    counters[91] = 200;
    map->Refresh();
    // Edge 5 is watched edge 0 (bucket 2), edge 91 is watched edge 2
    // (bucket 7). Bucket features start at 101.
    const std::vector<uint32_t> expected = {5, 50, 91, 103, 124};
    checker->Check(
        "coverage counters: hit bits and bucket features",
        map->CurrentEdges() == expected && map->CountCurrentEdges() == 2,
        StringPrintf("%zu features, %u counted edges",
                     map->CurrentEdges().size(), map->CountCurrentEdges()));
    // Several counters in one 8-byte word. Also a watched edge in the middle
    // of the optimized region: watched edge 10, bucket 4 for a count of 9.
    map->ClearBitmap();
    counters[8] = 1;
    counters[12] = 255;
    counters[15] = 128;
    counters[99] = 9;
    map->Refresh();
    const std::vector<uint32_t> packed = {8, 12, 15, 99, 185};
    checker->Check("coverage counters: packed hit bits",
                   map->CurrentEdges() == packed,
                   StringPrintf("%zu features", map->CurrentEdges().size()));
    // Crash deduplication counts only the edges. Edge 100 and the bucket
    // features of edge 5 (101 to 108) are in the same 8-byte word.
    SharedCoverage shared;
    const bool shared_ok =
        shared.Initialize(map->num_edges(), map->bitmap_size(), &error,
                          map->counted_edges(), map->real_edges());
    map->ResetState();
    std::string crash_results;
    auto crash = [&](std::initializer_list<std::pair<uint32_t, uint8_t>> hits) {
      map->ClearBitmap();
      for (const auto& [edge, count] : hits) {
        counters[edge] = count;
      }
      map->Refresh();
      const bool local = map->EvaluateCrash();
      const bool session = shared.ClaimNewCrashEdges(map->bitmap());
      crash_results += StringPrintf("%s%d%d", crash_results.empty() ? "" : " ",
                                    local, session);
    };
    // A new edge, the same edge with a different hit count, a new edge.
    crash({{5, 1}});
    crash({{5, 2}});
    crash({{5, 2}, {100, 1}});
    checker->Check("coverage counters: crash edges ignore bucket features",
                   shared_ok && crash_results == "11 00 11", crash_results);
    // A flaky feature that is not a counted edge (an optimized code feature
    // or a bucket feature) does not change coverage_fraction(). The packed
    // bitmap has 3 counted edges: 8, 12 and 15.
    SharedCoverage flaky;
    const bool flaky_ok =
        flaky.Initialize(map->num_edges(), map->bitmap_size(), &error,
                         map->counted_edges(), map->real_edges());
    map->ClearBitmap();
    counters[8] = 1;
    counters[12] = 255;
    counters[15] = 128;
    counters[99] = 9;
    map->Refresh();
    const bool claimed_packed = flaky.ClaimNewEdges(map->bitmap()) == packed;
    const double denominator = static_cast<double>(map->counted_edges() + 1);
    flaky.RecordFlaky(99, 1);
    flaky.RecordFlaky(185, 1);
    const double three_edges = flaky.coverage_fraction();
    flaky.RecordFlaky(12, 1);
    const double two_edges = flaky.coverage_fraction();
    checker->Check(
        "coverage counters: only flaky counted edges reduce the coverage",
        flaky_ok && claimed_packed && flaky.found_edges() == 3 &&
            flaky.suppressed_flaky_edges() == 1 &&
            three_edges == 3 / denominator && two_edges == 2 / denominator,
        StringPrintf(
            "found %u, suppressed %llu, coverage %g then %g edges",
            flaky.found_edges(),
            static_cast<unsigned long long>(flaky.suppressed_flaky_edges()),
            three_edges * denominator, two_edges * denominator));
    map->ClearBitmap();
    counters[5] = 3;
    counters[50] = 1;
    counters[91] = 200;
    map->Refresh();
    FeatureWeights::Params params;
    std::unique_ptr<FeatureWeights> weights =
        FeatureWeights::Build(*map, params, {{5, 1.0}, {50, 0.3}});
    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-6; };
    checker->Check(
        "coverage counters: feature weights",
        near(weights->Of(5), 21.0) && near(weights->Of(50), 7.0) &&
            near(weights->Of(85), 0.5) && near(weights->Of(91), 5.0) &&
            near(weights->Of(101), 0.0) && near(weights->Of(103), 10.5) &&
            near(weights->Of(124), 2.5) && near(weights->Of(10), 1.0) &&
            weights->target_edges() == std::vector<uint32_t>{5, 50},
        StringPrintf("%g %g %g %g %g %g %g", weights->Of(5), weights->Of(50),
                     weights->Of(85), weights->Of(91), weights->Of(101),
                     weights->Of(103), weights->Of(124)));
    // A focus on code edges 40 to 60. The focus edges, the target edges and
    // the buckets of the targets are in the focus. Optimized code features are
    // excluded by default and included when optimized_in_focus is true.
    std::unique_ptr<FeatureWeights> focused = FeatureWeights::Build(
        *map, params, {{5, 1.0}}, FeatureWeights::EdgeRanges{{40, 60}});
    FeatureWeights::Params opt_params = params;
    opt_params.optimized_in_focus = true;
    std::unique_ptr<FeatureWeights> focused_opt = FeatureWeights::Build(
        *map, opt_params, {{5, 1.0}}, FeatureWeights::EdgeRanges{{40, 60}});
    std::unique_ptr<FeatureWeights> weights_opt =
        FeatureWeights::Build(*map, opt_params, {{5, 1.0}, {50, 0.3}});
    checker->Check(
        "coverage counters: focus",
        focused->has_focus() && focused->filters_admission() &&
            focused->focus_edges() == 21 && focused->InFocus(40) &&
            focused->InFocus(60) && !focused->InFocus(61) &&
            focused->InFocus(5) && !focused->InFocus(95) &&
            focused_opt->InFocus(95) && focused->InFocus(103) &&
            !focused->InFocus(10) && !focused->InFocus(85) &&
            near(focused->Of(10), 0.1) && near(focused->Of(50), 1.0) &&
            near(focused->Of(85), 0.05) && near(focused->Of(5), 21.0) &&
            !weights->has_focus() && weights->filters_admission() &&
            weights->InFocus(10) && !weights->InFocus(95) &&
            !weights_opt->filters_admission() && weights_opt->InFocus(95),
        StringPrintf("focus edges %u, w(10) %g, w(85) %g",
                     focused->focus_edges(), focused->Of(10), focused->Of(85)));
    // Target features stay active in the rarity scheduler past rare_edge_limit.
    // Slow programs get less energy than fast ones, and partner picks favour
    // target programs in change-based fuzzing.
    FeatureWeights::SetActive(weights.get());
    auto& sel_reg = SelectorRegistry<const Program>::Instance();
    std::string sel_err;
    auto seeds =
        sel_reg.Create(SelectionConfig{"rarity", JsonValue()}, &sel_err);
    auto partners = sel_reg.Create(
        SelectionConfig{"rarity-partner", JsonValue()}, &sel_err);
    Corpus rarity_corpus(std::move(seeds), std::move(partners));
    ProgramPtr last_target, last_non_target;
    for (int i = 0; i < 10; i++) {
      Lineage l;
      auto pt = Program::Parse("let a = 1;", l, &sel_err);
      pt->edges = {5};
      if (i == 9) {
        for (uint32_t e = 15; e < 155; e++) {
          if (e != 50) {
            pt->edges.push_back(e);
          }
        }
      }
      pt->exec_time_us = i == 9 ? 1000 : 100000;
      last_target = rarity_corpus.Add(pt);
      auto pn = Program::Parse("let b = 2;", l, &sel_err);
      pn->edges = {10};
      pn->exec_time_us = 1000;
      last_non_target = rarity_corpus.Add(pn);
    }
    Random rng(42);
    bool partner_picked_target = false;
    for (int i = 0; i < 8; i++) {
      ProgramPtr p = rarity_corpus.PickPartner(*last_non_target, rng);
      if (p != nullptr && p->edges == std::vector<uint32_t>{5}) {
        partner_picked_target = true;
      }
    }
    FeatureWeights::SetActive(nullptr);
    // Without active targets, Pick() pops the max-energy seed first: the fast
    // program (1 ms) beats the slow one (100 ms) with equal edge rarity.
    auto time_seeds =
        sel_reg.Create(SelectionConfig{"rarity", JsonValue()}, &sel_err);
    auto time_partners = sel_reg.Create(
        SelectionConfig{"uniform-partner", JsonValue()}, &sel_err);
    Corpus time_corpus(std::move(time_seeds), std::move(time_partners));
    Lineage l;
    auto slow_p = Program::Parse("let x = 1;", l, &sel_err);
    slow_p->edges = {20};
    slow_p->exec_time_us = 100000;
    time_corpus.Add(slow_p);
    auto fast_p = Program::Parse("let y = 1;", l, &sel_err);
    fast_p->edges = {21};
    fast_p->exec_time_us = 1000;
    ProgramPtr fast_added = time_corpus.Add(fast_p);
    ProgramPtr first_pick = time_corpus.Pick(rng);
    const bool kept_target_5 = std::find(last_target->rare_edges.begin(),
                                         last_target->rare_edges.end(),
                                         5u) != last_target->rare_edges.end();
    checker->Check(
        "coverage counters: rarity target retention and time weighting",
        kept_target_5 && last_target->rare_edges.size() == 128 &&
            last_non_target->rare_edges.empty() && partner_picked_target &&
            first_pick == fast_added,
        StringPrintf("target rare_edges %zu, non-target %zu",
                     last_target->rare_edges.size(),
                     last_non_target->rare_edges.size()));

    std::vector<std::string> base_flags = {
        "--single-threaded",
        "--max-maglev-eager-inlined-bytecode-size=0",
        "--max-maglev-inlined-bytecode-size-small=0",
        "--always-sparkplug",
        "--verify-bytecode-light",
        "--stress-concurrent-inlining"};
    std::vector<std::string> extra_flags = {
        "--single-threaded", "--maglev-assert", "--no-sparkplug",
        "--verify-bytecode-full", "--assert-types"};
    // Keep all base flags, so that only SanitizeD8Flags removes flags.
    std::string keep_error;
    const bool set_keep = TunableRegistry::Instance().Set(
        "flags.target_keep_chance", 1.0, &keep_error);
    std::vector<std::string> combined =
        CombineD8Flags(base_flags, extra_flags, rng);
    TunableRegistry::Instance().Reset();
    const bool stripped_orphan =
        std::find(combined.begin(), combined.end(),
                  "--max-maglev-eager-inlined-bytecode-size=0") ==
            combined.end() &&
        std::find(combined.begin(), combined.end(),
                  "--max-maglev-inlined-bytecode-size-small=0") ==
            combined.end() &&
        std::find(combined.begin(), combined.end(), "--always-sparkplug") ==
            combined.end() &&
        std::find(combined.begin(), combined.end(),
                  "--verify-bytecode-light") == combined.end() &&
        std::find(combined.begin(), combined.end(),
                  "--stress-concurrent-inlining") == combined.end();
    const bool has_assert =
        std::find(combined.begin(), combined.end(), "--maglev-assert") !=
            combined.end() &&
        std::find(combined.begin(), combined.end(), "--no-sparkplug") !=
            combined.end() &&
        std::find(combined.begin(), combined.end(), "--verify-bytecode-full") !=
            combined.end();
    const ptrdiff_t st_count =
        std::count(combined.begin(), combined.end(), "--single-threaded");
    checker->Check(
        "d8 flags: CombineD8Flags deduplication and orphan stripping",
        set_keep && stripped_orphan && has_assert && st_count == 1,
        StringPrintf("size %zu, st_count %td", combined.size(), st_count));

    const bool formula_ok =
        std::abs(EffectiveTargetGuidedChance(0, 0) - 0.75) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(3, 3) - 0.75) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(4, 0) - 0.75) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(10, 2) - 0.60) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(8, 4) - 0.375) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(10, 8) - 0.15) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(4, 4) - 0.0) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(20, 20) - 0.0) < 1e-9;
    checker->Check("d8 flags: EffectiveTargetGuidedChance warmup and scaling",
                   formula_ok, "");

    auto count_sentinel_draws = [](uint64_t completed, uint64_t hit,
                                   bool populate_pool) {
      FuzzConfig cfg;
      cfg.randomize_d8_flags = true;
      Fuzzer fuzzer(std::move(cfg));
      fuzzer.SetTargetsForTest({{5, 100.0}});
      if (populate_pool) {
        fuzzer.RecordTargetFlags({"--sentinel-pool-flag"}, 100.0);
      }
      for (uint64_t i = 0; i < completed; i++) {
        fuzzer.RecordTargetChild(i < hit);
      }
      Random draw_rng(12345);
      int sentinel_count = 0;
      for (int i = 0; i < 400; i++) {
        std::vector<std::string> drawn = fuzzer.DrawD8Flags(draw_rng);
        if (std::find(drawn.begin(), drawn.end(), "--sentinel-pool-flag") !=
            drawn.end()) {
          sentinel_count++;
        }
      }
      return sentinel_count;
    };
    TunableRegistry& tunable_reg = TunableRegistry::Instance();
    std::string tunable_err;
    const bool set_no_mutate =
        tunable_reg.Set("flags.target_mutate_chance", 0.0, &tunable_err);
    const int empty_pool_draws = count_sentinel_draws(10, 0, false);
    const int warmup_draws = count_sentinel_draws(3, 3, true);
    const int zero_hit_draws = count_sentinel_draws(20, 0, true);
    const int low_hit_draws = count_sentinel_draws(20, 2, true);
    const int high_hit_draws = count_sentinel_draws(20, 18, true);
    const int all_hit_draws = count_sentinel_draws(20, 20, true);
    tunable_reg.Reset();
    checker->Check(
        "d8 flags: Fuzzer scales target-guided draws by child hit rate",
        set_no_mutate && empty_pool_draws == 0 && warmup_draws > 240 &&
            zero_hit_draws > 240 && low_hit_draws > 200 && high_hit_draws > 0 &&
            high_hit_draws < 80 && low_hit_draws > high_hit_draws + 120 &&
            all_hit_draws == 0,
        StringPrintf("empty %d, warmup %d, zero %d, low %d, high %d, all %d",
                     empty_pool_draws, warmup_draws, zero_hit_draws,
                     low_hit_draws, high_hit_draws, all_hit_draws));

    // DrawD8Flags gives the d8 flags of the configuration to the new draws
    // and to the combined draws. A fixed --single-threaded thus removes
    // --stress-concurrent-inlining from all draws. A fixed
    // --maglev-non-eager-inlining keeps the inlining size 0 of the pool
    // flags in the combined draws. The Turbolev profile does not draw
    // --maglev-non-eager-inlining.
    const bool set_combined_st =
        tunable_reg.Set("flags.stress_concurrent_inlining", 1.0,
                        &tunable_err) &&
        tunable_reg.Set("flags.stress_concurrent_inlining_attach_code", 1.0,
                        &tunable_err) &&
        tunable_reg.Set("flags.target_mutate_chance", 1.0, &tunable_err) &&
        tunable_reg.Set("flags.target_keep_chance", 1.0, &tunable_err);
    FuzzConfig st_cfg;
    st_cfg.randomize_d8_flags = true;
    st_cfg.d8_flag_profile = "turbolev";
    st_cfg.target.args = {"--single-threaded", "--maglev-non-eager-inlining"};
    Fuzzer st_fuzzer(std::move(st_cfg));
    st_fuzzer.SetTargetsForTest({{5, 100.0}});
    st_fuzzer.RecordTargetFlags(
        {"--sentinel-pool-flag", "--max-maglev-eager-inlined-bytecode-size=0"},
        100.0);
    st_fuzzer.RecordTargetChild(false);
    Random st_rng(4242);
    int st_sentinel = 0;
    int st_size_zero = 0;
    bool st_had_concurrent = false;
    for (int i = 0; i < 100; i++) {
      std::vector<std::string> drawn = st_fuzzer.DrawD8Flags(st_rng);
      if (std::find(drawn.begin(), drawn.end(), "--sentinel-pool-flag") !=
          drawn.end()) {
        st_sentinel++;
        if (std::find(drawn.begin(), drawn.end(),
                      "--max-maglev-eager-inlined-bytecode-size=0") !=
            drawn.end()) {
          st_size_zero++;
        }
      }
      for (const std::string& flag : drawn) {
        if (flag.rfind("--stress-concurrent-inlining", 0) == 0) {
          st_had_concurrent = true;
        }
      }
    }
    tunable_reg.Reset();
    checker->Check(
        "d8 flags: Fuzzer draws keep the fixed d8 flags",
        set_combined_st && st_sentinel > 40 && st_sentinel < 100 &&
            st_size_zero == st_sentinel && !st_had_concurrent,
        StringPrintf("sentinel %d, size 0 %d, concurrent %d", st_sentinel,
                     st_size_zero, st_had_concurrent ? 1 : 0));

    Minimizer min_check;
    auto min_prog = Program::Parse(
        "let u0 = 1;\nlet u1 = 2;\nlet keep = 1;\n"
        "let u2 = 3;\nlet u3 = 4;\nkeep.m(1);\n",
        l, &sel_err);
    auto min_tree = min_prog->CopyTree();
    MinimizationResult min_res = min_check.Minimize(
        min_tree.get(),
        [](const std::string& src) {
          return src.find("let keep = 1") != std::string::npos &&
                 src.find("keep.m(1)") != std::string::npos;
        },
        nullptr);
    const uint64_t stmt_invocations =
        min_check.Find("statements")->stats().invocations;
    checker->Check(
        "minimizer: unreferenced declarations and fixed-point stopping",
        min_res.accepted == 1 && min_res.tests <= 8 && stmt_invocations == 1 &&
            min_res.stop == MinimizationStop::kFixedPoint,
        StringPrintf("tests %d, accepted %d, stmt_invocations %llu, stop %s",
                     min_res.tests, min_res.accepted,
                     static_cast<unsigned long long>(stmt_invocations),
                     MinimizationStopName(min_res.stop)));
  }
  munmap(mapping, CoverageMap::kShmSize);
}

// The oracle of the crash minimizer checks. The script "crashes" if its
// top-level code calls `crash`, directly or through the top-level helpers
// `var NAME = function ...`. The calls of other names do nothing.
bool ReachesCrash(const std::string& script) {
  std::unique_ptr<fast::Tree> tree =
      fast::Parser::ParseScript(script, fast::ParseOptions(), nullptr);
  if (tree == nullptr) {
    return false;
  }
  std::map<std::string, fast::Node*, std::less<>> helpers;
  std::vector<fast::Node*> work;  // the code that runs
  for (fast::Statement* statement : tree->root()->body) {
    auto* declaration = statement->As<fast::VariableDeclaration>();
    fast::VariableDeclarator* declarator =
        declaration != nullptr && declaration->declarations.size() == 1
            ? declaration->declarations[0]
            : nullptr;
    const fast::Identifier* name =
        declarator != nullptr && declarator->target != nullptr
            ? declarator->target->As<fast::Identifier>()
            : nullptr;
    if (name != nullptr && declarator->init != nullptr &&
        declarator->init->Is<fast::FunctionExpression>()) {
      helpers[std::string(name->name)] = declarator->init;
    } else {
      work.push_back(statement);
    }
  }
  std::set<std::string, std::less<>> called;
  bool crashed = false;
  while (!work.empty() && !crashed) {
    fast::Node* code = work.back();
    work.pop_back();
    fast::Walk(code, [&](fast::Node* node) {
      const auto* call = node->As<fast::CallExpression>();
      const fast::Identifier* callee =
          call != nullptr ? call->callee->As<fast::Identifier>() : nullptr;
      if (callee == nullptr) {
        return true;
      }
      if (callee->name == "crash") {
        crashed = true;
      }
      auto helper = helpers.find(callee->name);
      if (helper != helpers.end() && called.insert(helper->first).second) {
        work.push_back(helper->second);
      }
      return true;
    });
  }
  return crashed;
}

// The names of the top-level `var` declarations of `tree`, in order.
std::string TopLevelVars(const fast::Tree& tree) {
  std::string names;
  for (fast::Statement* statement : tree.root()->body) {
    const auto* declaration = statement->As<fast::VariableDeclaration>();
    if (declaration == nullptr) {
      continue;
    }
    for (const fast::VariableDeclarator* declarator :
         declaration->declarations) {
      const fast::Identifier* name =
          declarator->target != nullptr
              ? declarator->target->As<fast::Identifier>()
              : nullptr;
      if (name != nullptr) {
        names += (names.empty() ? "" : " ") + std::string(name->name);
      }
    }
  }
  return names;
}

// Offline: the crash minimizer reduces one tree of the prelude and the
// program, with the crash order of the reducers. The program calls one
// helper of the prelude, and that helper calls a second helper. The other
// helpers call each other, but the program does not reach them.
void RunCrashMinimizerChecks(Checker* checker) {
  const char* kPrelude =
      "var h0 = function(a) { return a; };\n"
      "var h1 = function(a) { return h0(a) + 1; };\n"
      "var h2 = function(a) { return crash(a); };\n"
      "var h3 = function(a) { return h2(a); };\n"
      "var h4 = function(a) { return h1(a) * 2; };\n"
      "var h5 = function(a) { return h5(a - 1); };\n"
      "var unused = 7;\n";
  std::string error;
  std::unique_ptr<ScriptTree> script =
      ScriptTree::Parse(kPrelude, "h3(1);\n", &error);
  if (!checker->Check(
          "crash minimizer: the prelude and the program parse into one tree",
          script != nullptr && script->prelude_statements() == 7 &&
              ReachesCrash(fast::Print(script->tree()->root())),
          error)) {
    return;
  }
  Minimizer minimizer;
  std::vector<Reducer*> reducers;
  for (std::string_view name : Minimizer::CrashOrder()) {
    reducers.push_back(minimizer.Find(name));
  }
  const MinimizationResult result =
      minimizer.MinimizeWith(reducers, script->tree(), ReachesCrash, nullptr,
                             FuzzConfig().crash_minimizer);
  const std::string kept = TopLevelVars(*script->tree());
  std::string program = script->PrintProgram();
  std::replace(program.begin(), program.end(), '\n', ' ');
  checker->Check(
      "crash minimizer: the program keeps exactly the prelude helpers that it "
      "reaches",
      kept == "h2 h3" && script->PreludeStatementsNow() == 2 &&
          program.find("h3(") != std::string::npos &&
          program.find("var ") == std::string::npos &&
          result.stop == MinimizationStop::kFixedPoint,
      StringPrintf("helpers \"%s\", %zu prelude statements, program \"%s\", "
                   "stop %s, %d tests",
                   kept.c_str(), script->PreludeStatementsNow(),
                   program.c_str(), MinimizationStopName(result.stop),
                   result.tests));
  // `var a = h0` and `(1);` parse as one statement: `var a = h0(1);`.
  std::unique_ptr<ScriptTree> joined =
      ScriptTree::Parse("var a = h0", "(1);\n", &error);
  checker->Check(
      "crash minimizer: no tree for a prelude that continues in the program",
      joined == nullptr, joined == nullptr ? error : "one tree");
}

// A coverage bitmap of 256 bits (4 words) with the bits of `edges` set,
// for SharedCoverage::ClaimNewEdges.
std::vector<uint8_t> EdgeBitmap(std::initializer_list<uint32_t> edges) {
  std::vector<uint8_t> bitmap(32, 0);
  for (uint32_t edge : edges) {
    bitmap[edge / 8] =
        static_cast<uint8_t>(bitmap[edge / 8] | (1u << (edge % 8)));
  }
  return bitmap;
}

// A checked candidate with `edges` for the ChainCommits checks.
CheckedCandidate CoalesceCandidate(const char* source,
                                   std::vector<uint32_t> edges) {
  std::string error;
  CheckedCandidate candidate;
  candidate.program = Program::Parse(source, Lineage(), &error);
  candidate.edges = edges;
  candidate.credit.edges = std::move(edges);
  candidate.reruns = 2;
  return candidate;
}

// The 1-based line of the first line of `text` that contains `needle`.
int LineOf(const std::string& text, std::string_view needle) {
  const size_t at = text.find(needle);
  if (at == std::string::npos) {
    return 0;
  }
  return 1 + static_cast<int>(std::count(
                 text.begin(), text.begin() + static_cast<std::ptrdiff_t>(at),
                 '\n'));
}

// Offline: the flaky bits of SharedCoverage and the determinism check with
// fewer reruns for clean candidates. Also the coalesced commit of a chain,
// and the salvage retry that a throw in the last top-level statement skips.
void RunCoalesceChecks(Checker* checker) {
  std::string error;
  {
    // AnyFlaky() reads the bits that RecordFlaky() sets, also from many
    // threads at the same time. Edge 0 and edges after num_edges have no bit.
    SharedCoverage none;
    none.RecordFlaky(3, 1);
    SharedCoverage shared;
    const bool ok = shared.Initialize(255, 32, &error);
    const bool before = shared.AnyFlaky({3, 70});
    shared.RecordFlaky(70, 50);
    const bool after = shared.AnyFlaky({3, 70});
    const bool other = shared.AnyFlaky({3, 71, 0, 999});
    std::vector<std::thread> threads;
    for (uint32_t t = 0; t < 4; t++) {
      threads.emplace_back([&shared, t] {
        for (uint32_t edge = 100 + t; edge <= 255; edge += 4) {
          shared.RecordFlaky(edge, 50);
        }
      });
    }
    for (std::thread& thread : threads) {
      thread.join();
    }
    int set = 0;
    for (uint32_t edge = 100; edge <= 255; edge++) {
      set += shared.AnyFlaky({edge}) ? 1 : 0;
    }
    checker->Check("coalesce: AnyFlaky reads the flaky bits of RecordFlaky",
                   ok && !none.AnyFlaky({3}) && !before && after && !other &&
                       !shared.AnyFlaky({}) && set == 156 &&
                       shared.suppressed_flaky_edges() == 0,
                   StringPrintf("before %d, after %d, other %d, %d of 156 bits",
                                before, after, other, set));
  }
  CheckRules rules;  // min_reruns 5, clean_min_reruns 2
  auto check = [&](SharedCoverage& shared, std::vector<uint32_t> claimed,
                   std::vector<CheckRun> runs, bool first_held) {
    size_t next = 0;
    return RunDeterminismCheck(
        shared, rules, std::move(claimed), first_held,
        [&](const std::vector<uint32_t>& edges, CheckRun* run) {
          if (next < runs.size()) {
            *run = runs[next++];
          } else {
            run->succeeded = true;
          }
          // Only the edges that are still in the set can be missing.
          std::vector<uint32_t> missing;
          std::set_intersection(edges.begin(), edges.end(),
                                run->missing.begin(), run->missing.end(),
                                std::back_inserter(missing));
          run->missing = std::move(missing);
          return true;
        });
  };
  CheckRun clean_run;
  clean_run.succeeded = true;
  {
    // A clean candidate stops after 2 reruns. A candidate with a flaky edge
    // from an earlier check, or with an edge that a rerun loses, runs
    // min_reruns times.
    SharedCoverage shared;
    const bool ok = shared.Initialize(255, 32, &error);
    shared.MarkSeen({10, 11, 12, 13, 20, 21});
    const CheckResult clean = check(shared, {10, 11}, {}, true);
    shared.RecordFlaky(12, 50);
    const CheckResult history = check(shared, {12, 13}, {}, true);
    CheckRun lose;
    lose.succeeded = true;
    lose.missing = {21};
    const CheckResult dropped = check(shared, {20, 21}, {lose}, true);
    checker->Check(
        "coalesce: a clean check converges after clean_min_reruns",
        ok && clean.survived && clean.clean && clean.reruns == 2 &&
            history.survived && !history.clean && history.reruns == 5 &&
            dropped.survived && !dropped.clean && dropped.reruns == 5 &&
            dropped.edges == std::vector<uint32_t>{20} && !shared.IsSeen(21) &&
            shared.AnyFlaky({21}) && shared.IsSeen(20),
        StringPrintf("reruns: clean %d, flaky history %d, dropped edge %d",
                     clean.reruns, history.reruns, dropped.reruns));
  }
  {
    // The parts of the Finalize executions add up.
    const std::vector<uint64_t> split = SplitByEdges(10, {3, 1});
    const std::vector<uint64_t> even = SplitByEdges(5, {0, 0});
    checker->Check(
        "coalesce: the Finalize executions are split by the new edges",
        split == std::vector<uint64_t>{8, 2} &&
            even == std::vector<uint64_t>{3, 2} && SplitByEdges(7, {}).empty(),
        StringPrintf("split %llu+%llu, even %llu+%llu",
                     static_cast<unsigned long long>(split[0]),
                     static_cast<unsigned long long>(split[1]),
                     static_cast<unsigned long long>(even[0]),
                     static_cast<unsigned long long>(even[1])));
  }
  {
    // A candidate that reached the pending edges in all runs merges. A
    // candidate that missed 1 pending edge, or a merge after the limit,
    // finalizes the pending commit first.
    std::vector<PendingCommit> finalized;
    CoalesceLimits limits;
    limits.max_merges = 2;
    ChainCommits commits(limits, [&](PendingCommit commit) {
      finalized.push_back(std::move(commit));
    });
    using Offered = ChainCommits::Offered;
    const Offered first =
        commits.Offer(CoalesceCandidate("a = 1;", {1, 2}), true);
    const Offered merged =
        commits.Offer(CoalesceCandidate("a = 1; b = 2;", {3}), true);
    const std::vector<uint32_t> pending = *commits.pending_edges();
    const size_t before_break = finalized.size();
    const Offered missed =
        commits.Offer(CoalesceCandidate("a = 1; c = 3;", {4}), false);
    const Offered merged_again =
        commits.Offer(CoalesceCandidate("a = 1; c = 3; d = 4;", {5}), true);
    const Offered merged_twice =
        commits.Offer(CoalesceCandidate("a = 1; c = 3; d = 5;", {6}), true);
    const Offered limited =
        commits.Offer(CoalesceCandidate("a = 1; e = 6;", {7}), true);
    commits.Flush();
    const bool shapes =
        finalized.size() == 3 &&
        finalized[0].candidate.edges == std::vector<uint32_t>{1, 2, 3} &&
        finalized[0].credits.size() == 2 && finalized[0].merges == 1 &&
        finalized[0].candidate.reruns == 4 &&
        finalized[0].candidate.program->source().find("b = 2") !=
            std::string::npos &&
        finalized[1].candidate.edges == std::vector<uint32_t>{4, 5, 6} &&
        finalized[1].credits.size() == 3 && finalized[1].merges == 2 &&
        finalized[1].credits[2].edges == std::vector<uint32_t>{6} &&
        finalized[2].candidate.edges == std::vector<uint32_t>{7};
    checker->Check(
        "coalesce: merge if all runs reach the pending edges, else finalize",
        first == Offered::kStarted && merged == Offered::kMerged &&
            pending == std::vector<uint32_t>{1, 2, 3} && before_break == 0 &&
            missed == Offered::kReplaced && merged_again == Offered::kMerged &&
            merged_twice == Offered::kMerged && limited == Offered::kReplaced &&
            shapes && !commits.has_pending(),
        StringPrintf("%zu commits finalized", finalized.size()));
  }
  {
    // A crash or a stop request in the middle of a chain leaves the scope of
    // ChainCommits. Its destructor publishes the pending commit, thus no
    // claimed edge stays without a program.
    auto chain = [&](bool crash, size_t* finalized, uint32_t* published) {
      SharedCoverage shared;
      shared.Initialize(255, 32, &error);
      *finalized = 0;
      *published = 0;
      std::set<uint32_t> edges;
      {
        ChainCommits commits(CoalesceLimits(), [&](PendingCommit commit) {
          (*finalized)++;
          edges.insert(commit.candidate.edges.begin(),
                       commit.candidate.edges.end());
        });
        const std::vector<std::vector<uint8_t>> runs = {
            EdgeBitmap({5, 6}), EdgeBitmap({5, 6, 7}),
            EdgeBitmap({5, 6, 7, 8})};
        for (size_t round = 0; round < runs.size(); round++) {
          if (!crash && round == 2) {
            break;  // a stop request before the 3rd link
          }
          std::vector<uint32_t> claimed =
              shared.ClaimNewEdges(runs[round].data());
          const CheckResult result =
              check(shared, std::move(claimed), {}, true);
          commits.Offer(CoalesceCandidate("a = 1;", result.edges),
                        result.pending_held);
          if (crash && round == 1) {
            break;  // the 2nd link crashes after its check
          }
        }
      }
      *published = static_cast<uint32_t>(edges.size());
      return shared.found_edges();
    };
    size_t crash_commits = 0;
    size_t stop_commits = 0;
    uint32_t crash_published = 0;
    uint32_t stop_published = 0;
    const uint32_t crash_found = chain(true, &crash_commits, &crash_published);
    const uint32_t stop_found = chain(false, &stop_commits, &stop_published);
    checker->Check(
        "coalesce: a crash or a stop in a chain publishes the pending commit",
        crash_commits == 1 && crash_published == 3 && crash_found == 3 &&
            stop_commits == 1 && stop_published == 3 && stop_found == 3,
        StringPrintf("crash: %zu commits, %u of %u edges; stop: %zu commits, "
                     "%u of %u edges",
                     crash_commits, crash_published, crash_found, stop_commits,
                     stop_published, stop_found));
  }
  {
    // A flaky candidate after a pending commit: its runs miss pending edge
    // 41 and its own edge 43. Only edge 43 is flaky and released. The
    // pending edges stay claimed and are not flaky.
    SharedCoverage shared;
    const bool ok = shared.Initialize(255, 32, &error);
    const std::vector<uint8_t> first = EdgeBitmap({40, 41});
    const std::vector<uint32_t> pending = shared.ClaimNewEdges(first.data());
    const std::vector<uint8_t> second = EdgeBitmap({40, 41, 42, 43});
    std::vector<uint32_t> claimed = shared.ClaimNewEdges(second.data());
    CheckRun lose;
    lose.succeeded = true;
    lose.missing = {43};
    lose.pending_held = false;
    const CheckResult result = check(shared, claimed, {lose}, true);
    // A candidate that fails a rerun is dropped. It still does not touch
    // the pending edges.
    const std::vector<uint8_t> third = EdgeBitmap({40, 41, 44});
    std::vector<uint32_t> failing = shared.ClaimNewEdges(third.data());
    CheckRun fail;
    fail.succeeded = false;
    const CheckResult failed = check(shared, failing, {fail}, false);
    std::vector<PendingCommit> finalized;
    ChainCommits::Offered offered;
    {
      ChainCommits commits(CoalesceLimits(), [&](PendingCommit commit) {
        finalized.push_back(std::move(commit));
      });
      commits.Offer(CoalesceCandidate("a = 1;", pending), true);
      offered = commits.Offer(CoalesceCandidate("a = 1; b = 2;", result.edges),
                              result.pending_held);
    }
    checker->Check(
        "coalesce: a flaky candidate does not mark the pending edges flaky",
        ok && pending == std::vector<uint32_t>{40, 41} && result.survived &&
            !result.pending_held && result.edges == std::vector<uint32_t>{42} &&
            !shared.AnyFlaky(pending) && shared.IsSeen(40) &&
            shared.IsSeen(41) && shared.AnyFlaky({43}) && !shared.IsSeen(43) &&
            !failed.survived && !shared.IsSeen(44) && !shared.AnyFlaky({44}) &&
            offered == ChainCommits::Offered::kReplaced &&
            finalized.size() == 2 && finalized[0].candidate.edges == pending,
        StringPrintf("kept %zu edges, pending held %d, %zu commits",
                     result.edges.size(), result.pending_held,
                     finalized.size()));
  }
  {
    // The worker skips the salvage retry only for a throw directly in the
    // last top-level statement. A throw in a nested list, in a called
    // function or in a function that an earlier statement calls can have
    // code after it.
    auto last = [](const char* source, std::string_view inner,
                   std::string_view outer) {
      fast::ParseError err;
      std::unique_ptr<fast::Tree> tree =
          fast::Parser::ParseScript(source, fast::ParseOptions(), &err);
      if (tree == nullptr) {
        return false;
      }
      const std::string printed = fast::Print(tree->root());
      ExceptionLines lines;
      lines.innermost = LineOf(printed, inner);
      lines.outermost = LineOf(printed, outer);
      return ThrowsInLastTopLevelStatement(tree.get(), lines);
    };
    const bool tail = last("let a = 1;\nnull.x;\n", "null.x", "null.x");
    const bool head = last("null.x;\nlet a = 1;\n", "null.x", "null.x");
    const bool nested = last(
        "let a = 1;\nfor (let i = 0; i < 2; i++) {\n  null.x;\n  a++;\n}\n",
        "null.x", "null.x");
    const bool call =
        last("function f() {\n  null.x;\n  g();\n}\nf();\n", "null.x", "f();");
    const bool earlier_call = last(
        "f();\nlet b = 2;\nfunction f() {\n  null.x;\n}\n", "null.x", "f();");
    checker->Check(
        "coalesce: the salvage tail skip needs a throw directly in the last "
        "top-level statement",
        tail && !head && !nested && !call && !earlier_call &&
            !ThrowsInLastTopLevelStatement(nullptr, ExceptionLines{1, 1}),
        StringPrintf("tail %d, head %d, nested %d, call %d, earlier call %d",
                     tail, head, nested, call, earlier_call));
  }
}

// Offline: the parts of a commit that save executions. The probe timeout
// tightens after a kept reduction. The hit edges come from the runs of the
// check or of the minimization. The default order starts with `dataflow`,
// and `expressions` uses only a part of the budget.
void RunCommitCostChecks(Checker* checker) {
  {
    FuzzConfig config;
    config.target.timeout_us = 1000000;
    const uint64_t first = MinimizerProbeTimeout(config, 100000);
    const uint64_t tight = TightenedProbeTimeout(config, first, 20000);
    const uint64_t floor = TightenedProbeTimeout(config, first, 0);
    const uint64_t capped = TightenedProbeTimeout(config, first, 400000);
    config.probe_timeout_tighten_factor = 0;
    const uint64_t off = TightenedProbeTimeout(config, first, 20000);
    checker->Check(
        "commit: a kept reduction tightens the probe timeout to 2.5 times its "
        "run time plus 50 ms, between 50 ms and the first timeout",
        first == 520000 && tight == 100000 && floor == 50000 &&
            capped == first && off == first,
        StringPrintf("first %llu, tight %llu, floor %llu, capped %llu, off "
                     "%llu us",
                     static_cast<unsigned long long>(first),
                     static_cast<unsigned long long>(tight),
                     static_cast<unsigned long long>(floor),
                     static_cast<unsigned long long>(capped),
                     static_cast<unsigned long long>(off)));
  }
  {
    // A program that the minimizer did not change: the edges of all runs of
    // the check. The probes of a rejected minimization do not apply.
    HitEdges kept;
    kept.RecordRun({5, 3, 1, 2});
    kept.RecordRun({7, 1, 5, 2});
    kept.RecordProbe({1, 9});
    // A minimized program: the last kept probe and the verification run.
    // The runs of the original program do not apply.
    HitEdges minimized;
    minimized.RecordRun({1, 2, 3});
    minimized.RecordRun({1, 2, 3});
    minimized.RecordProbe({1, 4, 8});
    minimized.RecordProbe({6, 4, 1});
    minimized.RecordVerifiedRun({9, 6, 4, 1});
    // A verification without a probe gives one run. Publish then runs the
    // program once more.
    HitEdges alone;
    alone.RecordRun({1, 2});
    alone.RecordVerifiedRun({2, 3});
    checker->Check(
        "commit: the hit edges come from the check runs, or from the last "
        "kept probe and the verification of the minimized program",
        kept.runs() == 2 && kept.edges() == std::vector<uint32_t>{1, 2, 5} &&
            minimized.runs() == 2 &&
            minimized.edges() == std::vector<uint32_t>{1, 4, 6} &&
            alone.runs() == 1 && alone.edges() == std::vector<uint32_t>{2, 3} &&
            minimized.Take() == std::vector<uint32_t>{1, 4, 6} &&
            minimized.runs() == 0);
  }
  checker->Check(
      "minimizer: the default order starts with `dataflow`, the crash order "
      "keeps `expressions`",
      Minimizer::DefaultOrder() ==
              std::vector<std::string_view>{"dataflow", "statements", "unwrap",
                                            "lists", "expressions"} &&
          std::find(Minimizer::CrashOrder().begin(),
                    Minimizer::CrashOrder().end(),
                    "expressions") != Minimizer::CrashOrder().end());
  // `dataflow` removes the unused declarations with one test, and then each
  // unused group of top-level statements with one test.
  {
    std::string message;
    std::shared_ptr<Program> program = Program::Parse(
        "var u0 = 0; var u1 = 1;\n"
        "var a = 1; var b = a + 1;\nvar c = 2; var d = c * 2;\n"
        "var keep = 3; print(keep);\n",
        Lineage(), &message);
    if (checker->Check("minimizer: the dataflow program parses",
                       program != nullptr, message)) {
      std::unique_ptr<fast::Tree> tree = program->CopyTree();
      Minimizer minimizer;
      MinimizationOracle oracle = [](const std::string& source) {
        return source.find("print(keep)") != std::string::npos &&
               source.find("keep = 3") != std::string::npos;
      };
      const MinimizationResult result = minimizer.Minimize(
          tree.get(), oracle, nullptr, FuzzConfig().minimizer);
      std::string printed = fast::Print(tree->root());
      std::replace(printed.begin(), printed.end(), '\n', ' ');
      const ComponentStats& dataflow = minimizer.Find("dataflow")->stats();
      checker->Check(
          "minimizer: the default order runs `dataflow` and keeps its cuts",
          dataflow.succeeded == 3 && dataflow.executed == 4 &&
              printed.find("var u") == std::string::npos &&
              printed.find("var a") == std::string::npos &&
              printed.find("var c") == std::string::npos &&
              printed.find("print(keep)") != std::string::npos,
          StringPrintf("%llu of %llu dataflow cuts kept, %d tests, program "
                       "\"%s\"",
                       static_cast<unsigned long long>(dataflow.succeeded),
                       static_cast<unsigned long long>(dataflow.executed),
                       result.tests, printed.c_str()));
    }
  }
  // `expressions` uses only its share of the budget when early stopping is off,
  // and stops after `minimizer.expressions.max_consecutive_failures` rejected
  // probes in a row when `expressions_share` is below 1. Crash minimization
  // (`expressions_share` == 1) stays uncapped.
  {
    std::string source;
    for (int i = 0; i < 60; i++) {
      source += StringPrintf("f(a%d + b%d, c%d * d%d);\n", i, i, i, i);
    }
    std::string message;
    std::shared_ptr<Program> program =
        Program::Parse(source, Lineage(), &message);
    auto tests_with = [&](double share, const MinimizationOracle& oracle) {
      std::unique_ptr<fast::Tree> tree = program->CopyTree();
      Minimizer minimizer;
      MinimizerOptions options;
      options.max_tests = 200;
      options.max_rounds = 1;
      options.target_time_ms = 0;
      options.expressions_share = share;
      std::vector<Reducer*> reducers = {minimizer.Find("expressions")};
      return minimizer.MinimizeWith(reducers, tree.get(), oracle, nullptr,
                                    options);
    };
    MinimizationOracle never = [](const std::string&) { return false; };
    std::string tun_err;
    TunableRegistry::Instance().Set(
        "minimizer.expressions.max_consecutive_failures", 0, &tun_err);
    const int quarter = program != nullptr ? tests_with(0.25, never).tests : -1;
    const int all = program != nullptr ? tests_with(1.0, never).tests : -1;
    const int none = program != nullptr ? tests_with(0.0, never).tests : -1;
    TunableRegistry::Instance().Reset();
    checker->Check(
        "minimizer: `expressions` uses only its share of the remaining tests",
        tun_err.empty() && quarter == 50 && all == 200 && none == 0,
        StringPrintf("%d tests with 0.25, %d with 1, %d with 0 (%s%s)", quarter,
                     all, none, message.c_str(), tun_err.c_str()));

    const int early = program != nullptr ? tests_with(0.25, never).tests : -1;
    const int crash_full =
        program != nullptr ? tests_with(1.0, never).tests : -1;
    // Reset test: accept every 5th probe so consecutive failures stay below 10.
    int probe_num = 0;
    MinimizationOracle periodic = [&](const std::string&) {
      probe_num++;
      return (probe_num % 5) == 0;
    };
    const MinimizationResult reset_res =
        program != nullptr ? tests_with(0.25, periodic) : MinimizationResult();
    checker->Check(
        "minimizer: `expressions` stops early on consecutive failures for "
        "corpus programs and stays full for crashes",
        early == 10 && crash_full == 200 && reset_res.tests == 50 &&
            reset_res.accepted >= 5,
        StringPrintf("early %d, crash %d, reset %d (%d accepted)", early,
                     crash_full, reset_res.tests, reset_res.accepted));

    // When `expressions_share` is 1, `expressions` does not freeze a low
    // initial cap if an accepted cut recovers budget.
    MinimizationResult share_recover;
    if (program != nullptr) {
      std::unique_ptr<fast::Tree> tree = program->CopyTree();
      Minimizer minimizer;
      MinimizerOptions options;
      options.max_tests = 100;
      options.max_rounds = 1;
      options.target_time_ms = 1500;
      options.min_tests = 16;
      options.expressions_share = 1.0;
      bool accepted_first = false;
      std::vector<Reducer*> reducers = {minimizer.Find("expressions")};
      share_recover = minimizer.MinimizeWith(
          reducers, tree.get(),
          [&](const std::string&) {
            if (!accepted_first) {
              accepted_first = true;
              return true;
            }
            return false;
          },
          nullptr, options.ForProgram(200'000));
    }
    checker->Check(
        "minimizer: `expressions` with share 1 uses recovered budget",
        share_recover.tests == 100 && share_recover.accepted == 1,
        StringPrintf("tests %d, accepted %d", share_recover.tests,
                     share_recover.accepted));

    // Syntax rejections do not count toward `max_consecutive_failures`.
    // Untagging a tagged template with an invalid escape fails V8 parsing.
    std::string v8_err;
    const bool v8_ok = InitializeV8(FuzzConfig().v8_flags, &v8_err);
    std::string tag_src;
    for (int i = 0; i < 15; i++) {
      tag_src += StringPrintf("tag%d`\\unicode`;\n", i);
    }
    std::string tag_err;
    std::shared_ptr<Program> tag_prog =
        Program::Parse(tag_src, Lineage(), &tag_err);
    MinimizationResult syntax_res;
    if (v8_ok && tag_prog != nullptr) {
      SyntaxChecker syntax;
      std::unique_ptr<fast::Tree> tree = tag_prog->CopyTree();
      Minimizer minimizer;
      MinimizerOptions options;
      options.max_tests = 200;
      options.max_rounds = 1;
      options.target_time_ms = 0;
      options.expressions_share = 0.5;
      std::vector<Reducer*> reducers = {minimizer.Find("expressions")};
      syntax_res =
          minimizer.MinimizeWith(reducers, tree.get(), never, &syntax, options);
    }
    checker->Check(
        "minimizer: `expressions` ignores syntax rejections in consecutive "
        "failure counts",
        v8_ok && tag_prog != nullptr && syntax_res.syntax_rejected >= 10 &&
            syntax_res.tests == 10,
        StringPrintf("syntax_rejected %d, tests %d (%s%s)",
                     syntax_res.syntax_rejected, syntax_res.tests,
                     v8_err.c_str(), tag_err.c_str()));
  }
  // Adaptive slow-program budget (MP-2). Corpus tests scale to execution
  // duration down to `min_tests`. Budget recovers when kept cuts speed up
  // execution. Crash minimization stays uncapped.
  {
    const FuzzConfig defaults;
    const MinimizerOptions& corpus_opt = defaults.minimizer;
    const MinimizerOptions& crash_opt = defaults.crash_minimizer;
    const int fast_cap = corpus_opt.TestsFor(5'000);     // 5 ms -> 300
    const int slow25_cap = corpus_opt.TestsFor(25'000);  // 25 ms -> 60
    const int slow50_cap = corpus_opt.TestsFor(50'000);  // 50 ms -> 30
    const int floor_cap = corpus_opt.TestsFor(200'000);  // 200 ms -> 16 floor
    const int crash_cap = crash_opt.TestsFor(200'000);   // uncapped -> 2000

    // Edge cases for `TestsFor`: non-positive caps, huge target time, high or
    // negative `min_tests`, and zero run time.
    MinimizerOptions edge;
    edge.max_tests = 0;
    edge.target_time_ms = 1500;
    const int zero_max = edge.TestsFor(50'000);
    edge.max_tests = -5;
    const int neg_max = edge.TestsFor(50'000);
    edge.max_tests = 20;
    edge.min_tests = 500;
    const int high_floor = edge.TestsFor(500'000);
    edge.min_tests = -10;
    const int neg_floor = edge.TestsFor(1'000'000);
    edge.min_tests = 4;
    edge.target_time_ms = std::numeric_limits<uint64_t>::max();
    const int huge_target = edge.TestsFor(1'000);
    edge.target_time_ms = 1500;
    const int zero_time = edge.TestsFor(0);
    checker->Check(
        "minimizer: `TestsFor` handles edge caps and large target times",
        zero_max == 0 && neg_max == 0 && high_floor == 20 && neg_floor == 1 &&
            huge_target == 20 && zero_time == 20,
        StringPrintf("zero=%d neg=%d high_floor=%d neg_floor=%d huge=%d "
                     "zero_time=%d",
                     zero_max, neg_max, high_floor, neg_floor, huge_target,
                     zero_time));

    std::string source;
    for (int i = 0; i < 40; i++) {
      source += StringPrintf("g%d();\n", i);
    }
    std::string message;
    std::shared_ptr<Program> program =
        Program::Parse(source, Lineage(), &message);
    // Slow initial time with no accepted reductions stays capped at 16 tests.
    MinimizationResult slow_res;
    // Slow probe 1 followed by fast probe 2 recovers budget at once.
    MinimizationResult two_step_res;
    if (program != nullptr) {
      std::unique_ptr<fast::Tree> slow_tree = program->CopyTree();
      Minimizer minimizer;
      std::vector<Reducer*> stmts = {minimizer.Find("statements")};
      slow_res = minimizer.MinimizeWith(
          stmts, slow_tree.get(), [](const std::string&) { return false; },
          nullptr, corpus_opt.ForProgram(200'000));

      std::unique_ptr<fast::Tree> recovery_tree = program->CopyTree();
      MinimizerOptions rec_opt;
      rec_opt.max_tests = 50;
      rec_opt.max_rounds = 1;
      rec_opt.target_time_ms = 120;
      rec_opt.min_tests = 4;
      int call = 0;
      two_step_res = minimizer.MinimizeWith(
          stmts, recovery_tree.get(),
          [&](const std::string&) {
            call++;
            if (call == 1) {
              // Slow accepted cut keeps the cap at the floor of 4 tests.
              std::this_thread::sleep_for(std::chrono::milliseconds(40));
              return true;
            }
            if (call == 2) {
              // Fast accepted cut replaces `last_kept_probe_time_us_`.
              return true;
            }
            return false;
          },
          nullptr, rec_opt.ForProgram(200'000));
    }
    checker->Check(
        "minimizer: adaptive slow-program budget caps slow corpus programs, "
        "recovers after a slow cut, and leaves crashes uncapped",
        fast_cap == 300 && slow25_cap == 60 && slow50_cap == 30 &&
            floor_cap == 16 && crash_cap == 2000 && slow_res.tests == 16 &&
            slow_res.budget_exhausted && two_step_res.tests == 14 &&
            !two_step_res.budget_exhausted,
        StringPrintf("caps 5ms=%d 25ms=%d 50ms=%d 200ms=%d crash=%d; slow=%d, "
                     "two_step=%d (exhausted=%d)",
                     fast_cap, slow25_cap, slow50_cap, floor_cap, crash_cap,
                     slow_res.tests, two_step_res.tests,
                     two_step_res.budget_exhausted ? 1 : 0));
  }
  // Adaptive delta-debugging chunking on `statements` (MP-4). Sparse large
  // lists shrink in few probes, and dead nested blocks clear in 1 probe.
  // Small lists skip chunking, dense lists stop early, and all ASTs stay
  // valid.
  {
    auto parse_ok = [](const std::string& src) {
      fast::ParseError err;
      return fast::Parser::ParseScript(src, fast::ParseOptions(), &err) !=
             nullptr;
    };
    bool all_valid = true;
    auto run_stmts = [&](const std::string& src,
                         const MinimizationOracle& inner,
                         std::string* printed_out) {
      std::string err;
      std::shared_ptr<Program> prog = Program::Parse(src, Lineage(), &err);
      std::unique_ptr<fast::Tree> tree = prog->CopyTree();
      Minimizer minimizer;
      MinimizerOptions options;
      options.max_tests = 200;
      options.max_rounds = 1;
      options.target_time_ms = 0;
      std::vector<Reducer*> reducers = {minimizer.Find("statements")};
      MinimizationResult res = minimizer.MinimizeWith(
          reducers, tree.get(),
          [&](const std::string& candidate) {
            if (!parse_ok(candidate)) {
              all_valid = false;
            }
            return inner(candidate);
          },
          nullptr, options);
      *printed_out = fast::Print(tree->root());
      return res;
    };

    // 1. Sparse 32-statement list with only 1 essential statement.
    std::string sparse_src;
    for (int i = 0; i < 32; i++) {
      sparse_src += StringPrintf("h(%d);\n", i);
    }
    std::string sparse_out;
    const MinimizationResult sparse_res = run_stmts(
        sparse_src,
        [](const std::string& s) {
          return s.find("h(0);") != std::string::npos;
        },
        &sparse_out);

    // 2. Dense 32-statement irreducible list stops chunking after 2 empty
    // levels: 2 + 4 + 32 = 38 probes instead of 62.
    std::string dense_out;
    const MinimizationResult dense_res = run_stmts(
        sparse_src, [](const std::string&) { return false; }, &dense_out);

    // 3. Small 4-statement irreducible list skips chunking: 4 probes, not 6.
    std::string small_src = "h(0);\nh(1);\nh(2);\nh(3);\n";
    std::string small_out;
    const MinimizationResult small_res = run_stmts(
        small_src, [](const std::string&) { return false; }, &small_out);

    // 4. Dead 10-statement nested block clears in 1 probe (plus 1 top-level
    // probe on the function statement itself).
    std::string nested_src = "function keep() {\n";
    for (int i = 0; i < 10; i++) {
      nested_src += StringPrintf("  h(%d);\n", i);
    }
    nested_src += "}\n";
    std::string nested_out;
    const MinimizationResult nested_res = run_stmts(
        nested_src,
        [](const std::string& s) {
          return s.find("function keep()") != std::string::npos;
        },
        &nested_out);

    // 5. Non-power-of-2 lists (11 and 33 statements) keep balanced blocks and
    // avoid duplicate remainder probes.
    std::string odd11_src;
    for (int i = 0; i < 11; i++) {
      odd11_src += StringPrintf("h(%d);\n", i);
    }
    std::string odd11_out;
    const MinimizationResult odd11_res = run_stmts(
        odd11_src,
        [](const std::string& s) {
          return s.find("h(5);") != std::string::npos;
        },
        &odd11_out);

    std::string odd33_src;
    for (int i = 0; i < 33; i++) {
      odd33_src += StringPrintf("h(%d);\n", i);
    }
    std::string odd33_sparse_out;
    const MinimizationResult odd33_sparse_res = run_stmts(
        odd33_src,
        [](const std::string& s) {
          return s.find("h(17);") != std::string::npos;
        },
        &odd33_sparse_out);
    std::string odd33_dense_out;
    const MinimizationResult odd33_dense_res = run_stmts(
        odd33_src, [](const std::string&) { return false; }, &odd33_dense_out);

    checker->Check(
        "minimizer: `statements` uses adaptive block chunking with valid ASTs",
        all_valid && sparse_res.tests <= 8 &&
            sparse_out.find("h(0);") != std::string::npos &&
            sparse_out.find("h(1);") == std::string::npos &&
            dense_res.tests == 38 && small_res.tests == 4 &&
            nested_res.tests == 2 &&
            nested_out.find("h(0)") == std::string::npos &&
            odd11_res.tests < 11 &&
            odd11_out.find("h(5);") != std::string::npos &&
            odd11_out.find("h(4);") == std::string::npos &&
            odd33_sparse_res.tests <= 12 &&
            odd33_sparse_out.find("h(17);") != std::string::npos &&
            odd33_sparse_out.find("h(16);") == std::string::npos &&
            odd33_dense_res.tests == 39,
        StringPrintf("valid=%d sparse=%d dense=%d small=%d nested=%d "
                     "odd11=%d odd33_s=%d odd33_d=%d",
                     all_valid ? 1 : 0, sparse_res.tests, dense_res.tests,
                     small_res.tests, nested_res.tests, odd11_res.tests,
                     odd33_sparse_res.tests, odd33_dense_res.tests));
  }
}

// Offline: the search of the d8 flags of NAME.repro.js (MinimizeFlags). The
// oracle needs 2 flags of 7, and --allow-natives-syntax must stay.
void RunFlagMinimizerChecks(Checker* checker) {
  const std::vector<std::string> flags = {
      "--a", "--b", "--allow_natives_syntax", "--c=1", "--d", "--e", "--f"};
  int calls = 0;
  auto oracle = [&calls](const std::vector<std::string>& candidate) {
    calls++;
    auto has = [&candidate](const char* flag) {
      return std::find(candidate.begin(), candidate.end(), flag) !=
             candidate.end();
    };
    return has("--c=1") && has("--f");
  };
  auto keep = [](const std::string& flag) {
    return IsFlag(flag, "allow-natives-syntax");
  };
  const FlagMinimization result =
      MinimizeFlags(flags, keep, oracle, [] { return false; });
  std::string kept;
  for (const std::string& flag : result.flags) {
    kept += (kept.empty() ? "" : " ") + flag;
  }
  checker->Check(
      "crash flags: the search keeps the needed flags and the kept flags",
      kept == "--allow_natives_syntax --c=1 --f" && result.tests == calls &&
          !result.budget_exhausted,
      StringPrintf("kept \"%s\", %d tests", kept.c_str(), result.tests));
  int tests = 0;
  const FlagMinimization stopped =
      MinimizeFlags(flags, keep, oracle, [&tests] { return tests++ >= 2; });
  checker->Check("crash flags: the search stops at the end of the budget",
                 stopped.budget_exhausted && stopped.tests == 2,
                 StringPrintf("%d tests, budget_exhausted %d", stopped.tests,
                              stopped.budget_exhausted ? 1 : 0));
  // HasNativeCall is also true for a script that does not parse. Thus the
  // script with the call must parse.
  const char* call_in_function =
      "function f() { return %DebugPrint(1); }\nf();";
  const bool in_function =
      fast::Parser::ParseScript(call_in_function, fast::ParseOptions(),
                                nullptr) != nullptr &&
      HasNativeCall(call_in_function);
  const bool plain = HasNativeCall("print(1);");
  const bool not_parsing = HasNativeCall("let = ;");
  checker->Check(
      "crash flags: % calls need --allow-natives-syntax",
      in_function && !plain && not_parsing &&
          IsFlag("--allow_natives_syntax=1", "allow-natives-syntax") &&
          !IsFlag("--allow-natives-syntax-x", "allow-natives-syntax"),
      StringPrintf("call in a function %d, no call %d, no parse %d",
                   in_function ? 1 : 0, plain ? 1 : 0, not_parsing ? 1 : 0));
  checker->Check(
      "crash flags: the search keeps the flags for a deterministic run",
      KeepReproFlag("--single-threaded", false) &&
          KeepReproFlag("--random-seed=7", false) &&
          KeepReproFlag("--allow-natives-syntax", true) &&
          !KeepReproFlag("--allow-natives-syntax", false) &&
          !KeepReproFlag("--jit-fuzzing", true),
      "");
}

// Offline: the plain test of an amplified crash. Its processes follow the
// crash rate of the amplify step. Its flags have the flags of the amplify
// step instead of --stress-runs and --random-seed. Also the decisions of the
// rate checks of NAME.repro.js, and the cause of a stop in their notes.
void RunAmplifiedTestChecks(Checker* checker) {
  const std::string runs = StringPrintf(
      "%d %d %d %d %d %d", AmplifiedRuns(5, 5), AmplifiedRuns(4, 5),
      AmplifiedRuns(1, 2), AmplifiedRuns(1, 5), AmplifiedRuns(0, 5),
      AmplifiedRuns(3, 0));
  checker->Check("amplified crashes: the processes of a test follow the rate",
                 runs == "2 3 5 9 10 10", runs);
  std::string flags;
  for (const std::string& flag :
       AmplifiedFlags({"--a", "--random-seed=5", "--stress_runs=3", "--b"},
                      "--c --stress-runs=24")) {
    flags += (flags.empty() ? "" : " ") + flag;
  }
  // The rate check: a bar of 4 of 5.
  auto decide = [](RateCount kept, const RateCount* all) {
    const RateDecision d = DecideAmplifiedRate(4, 5, kept, all);
    return StringPrintf("%s%s %d/%d", d.keep ? "keep" : "drop",
                        d.all_flags ? " all" : "", d.rate.hits, d.rate.runs);
  };
  const RateCount all_pass{5, 5}, all_fail{5, 3}, all_cut{2, 2};
  const std::string decisions =
      decide({5, 4}, nullptr) + ", " + decide({5, 3}, &all_pass) + ", " +
      decide({5, 3}, &all_fail) + ", " + decide({3, 3}, nullptr) + ", " +
      decide({5, 3}, &all_cut) + ", " + decide({5, 2}, nullptr);
  checker->Check(
      "amplified crashes: the rate check keeps only a complete count that "
      "passes",
      decisions ==
          "keep 4/5, keep all 5/5, drop 3/5, drop 3/3, drop 2/2, "
          "drop 2/5",
      decisions);
  // The rate check of a crash that reproduces alone: the bar is 5 of 5, and
  // NAME.repro.js always stays.
  auto decide_alone = [](RateCount kept, const RateCount* all) {
    const RateDecision d =
        DecideReproducibleRate(5, kept, all, "the check stopped");
    return StringPrintf("%s%s %d/%d", d.keep ? "keep" : "drop",
                        d.all_flags ? " all" : "", d.rate.hits, d.rate.runs);
  };
  const std::string alone =
      decide_alone({5, 5}, nullptr) + ", " + decide_alone({5, 4}, &all_pass) +
      ", " + decide_alone({5, 4}, &all_fail) + ", " +
      decide_alone({5, 4}, nullptr) + ", " + decide_alone({2, 2}, nullptr) +
      ", " + decide_alone({5, 4}, &all_cut);
  // The note gives the cause of a count that stopped early.
  const std::string cut_note =
      DecideAmplifiedRate(4, 5, {0, 0}, nullptr, "cannot start d8: x").note;
  const std::string alone_note =
      DecideReproducibleRate(5, {5, 4}, &all_cut, "the session stopped").note;
  checker->Check(
      "crash flags: the rate check of a crash that reproduces alone keeps "
      "the flags of the search only after 5 of 5",
      alone == "keep 5/5, keep all 5/5, keep all 3/5, keep all 4/5, keep all "
               "2/2, keep all 2/2" &&
          cut_note ==
              "the bar is 4 of 5, not checked after 0 of 5 runs: cannot "
              "start d8: x, so Redline wrote no NAME.repro.js" &&
          alone_note ==
              "the bar is 5 of 5, the kept flags gave 4 of 5, the check of "
              "all flags stopped after 2 of 5 runs: the session stopped",
      alone + "; " + cut_note + "; " + alone_note);
  checker->Check("amplified crashes: the flags of a test",
                 flags == "--a --b --c --stress-runs=24" &&
                     KeepReproFlag("--stress-runs=24", false),
                 flags);
}

// Offline: the steps of the bundle minimization (MinimizeBundleParts). In
// the first case, the test needs the part "needed" and the crash part last.
// The time limit ends after the first step, so no part gets a reduction. The
// second case has no time limit, and the reductions change the needed parts.
void RunBundleStepChecks(Checker* checker) {
  int tests = 0;
  int reductions = 0;
  const std::vector<std::string> kept = MinimizeBundleParts(
      {"unneeded", "needed", "other", "crash"},
      [&tests](const std::vector<std::string>& candidate) {
        tests++;
        return std::find(candidate.begin(), candidate.end(), "needed") !=
                   candidate.end() &&
               candidate.back() == "crash";
      },
      [&reductions](std::vector<std::string>*, size_t) { reductions++; },
      [&tests] { return tests >= 4; });
  std::string text;
  for (const std::string& part : kept) {
    text += (text.empty() ? "" : " ") + part;
  }
  checker->Check(
      "bundles: the steps keep the needed part and the crash part last",
      text == "needed crash" && reductions == 0,
      StringPrintf("parts \"%s\", %d tests, %d reductions", text.c_str(), tests,
                   reductions));
  // Without a time limit. The reduction of the crash part makes the part
  // "a" unnecessary, and the reduction of "b" gives "b2". The removal after
  // each step must remove the part that is not necessary.
  int reduced = 0;
  const std::vector<std::string> again = MinimizeBundleParts(
      {"a", "b", "crash"},
      [](const std::vector<std::string>& candidate) {
        auto has = [&candidate](const char* part) {
          return std::find(candidate.begin(), candidate.end(), part) !=
                 candidate.end();
        };
        if (candidate.back() == "crash") {
          return has("a") && has("b");
        }
        return candidate.back() == "crash2" && (has("b") || has("b2"));
      },
      [&reduced](std::vector<std::string>* parts, size_t i) {
        reduced++;
        std::string& part = (*parts)[i];
        part = part == "crash" ? "crash2" : part == "b" ? "b2" : part;
      },
      [] { return false; });
  std::string again_text;
  for (const std::string& part : again) {
    again_text += (again_text.empty() ? "" : " ") + part;
  }
  checker->Check(
      "bundles: a reduced crash part lets the steps remove a history part",
      again_text == "b2 crash2" && reduced == 2,
      StringPrintf("parts \"%s\", %d reductions", again_text.c_str(), reduced));
}

// The value of the line `key: value` of a crash report, or "".
std::string ReportValue(const std::string& report, const std::string& key) {
  const std::string prefix = key + ": ";
  size_t start = 0;
  while (start < report.size()) {
    size_t end = report.find('\n', start);
    if (end == std::string::npos) {
      end = report.size();
    }
    if (report.compare(start, prefix.size(), prefix) == 0) {
      return report.substr(start + prefix.size(), end - start - prefix.size());
    }
    start = end + 1;
  }
  return "";
}

// The time that the session of a crash check can take. The crash minimizer
// stops after crash_minimizer.max_time_ms.
constexpr double kCrashSessionSeconds = 300;

// The time that a plain d8 run of a NAME.repro.js file can take.
constexpr uint64_t kPlainRunUs = 60'000'000;

// Removes `path`. For a directory, also removes all of its contents.
void RemoveTree(const std::string& path) {
  struct stat st;
  if (lstat(path.c_str(), &st) != 0) {
    return;
  }
  if (!S_ISDIR(st.st_mode)) {
    unlink(path.c_str());
    return;
  }
  std::vector<std::string> names;
  if (DIR* dir = opendir(path.c_str())) {
    while (const struct dirent* entry = readdir(dir)) {
      const std::string entry_name = entry->d_name;
      if (entry_name != "." && entry_name != "..") {
        names.push_back(entry_name);
      }
    }
    closedir(dir);
  }
  for (const std::string& entry_name : names) {
    RemoveTree(path + "/" + entry_name);
  }
  rmdir(path.c_str());
}

// The result of a short session that imports crash programs.
struct CrashSession {
  std::string dir;     // the files of the session, or "" if there are none
  bool saved = false;  // the session saved a crash
  // Why the wait stopped: "crash saved", "session ended", "imports did not
  // crash" or "time limit".
  std::string stop;
  double seconds = 0;
  std::string report;  // the report of the crash with NAME.repro.js, or ""
  std::string repro;   // its NAME.repro.js, or ""
  std::string error;   // why the session did not run, or ""
};

// Runs a short session with the d8 flags of `target` and `session_flags`. The
// session imports `programs`, in an order that the seed of the session sets.
// The wait stops when the session saved a crash, when Run() returns, when
// the imports did not crash or after kCrashSessionSeconds. The caller
// removes `dir` with RemoveTree().
CrashSession RunCrashSession(const TargetOptions& target,
                             const std::vector<std::string>& session_flags,
                             const std::vector<std::string>& programs,
                             const std::string& report_key = "repro") {
  CrashSession session;
  char dir_template[] = "/tmp/redline-selftest.XXXXXX";
  if (mkdtemp(dir_template) == nullptr) {
    session.error = StringPrintf("mkdtemp: %s", strerror(errno));
    return session;
  }
  session.dir = dir_template;
  const std::string import_dir = session.dir + "/import";
  if (!MakeDirectories(import_dir, &session.error)) {
    return session;
  }
  for (size_t i = 0; i < programs.size(); i++) {
    if (!WriteStringToFile(StringPrintf("%s/%02zu.js", import_dir.c_str(), i),
                           programs[i], &session.error)) {
      return session;
    }
  }
  std::string args;
  for (const std::string& arg : target.args) {
    args += (args.empty() ? "" : " ") + arg;
  }
  std::vector<std::string> all_flags = {
      "redline",          "fuzz",    "--jobs=1",         "--seed=1",
      "--no-introspect",  "--quiet", "--no-save-corpus", "--no-component-stats",
      "--allow-no-dcheck"};
  all_flags.push_back("--d8=" + target.executable);
  all_flags.push_back(std::string("--target-kind=") +
                      TargetKindName(target.kind));
  all_flags.push_back("--d8-args=" + args);
  // The d8 environment of the selftest, for example LD_PRELOAD.
  std::string env;
  for (const auto& [key, value] : target.extra_env) {
    env += (env.empty() ? "" : ",") + key + "=" + value;
  }
  if (!env.empty()) {
    all_flags.push_back("--d8-env=" + env);
  }
  all_flags.push_back("--import=" + import_dir);
  all_flags.push_back("--out=" + session.dir + "/out");
  all_flags.insert(all_flags.end(), session_flags.begin(), session_flags.end());
  std::vector<const char*> argv;
  for (const std::string& flag : all_flags) {
    argv.push_back(flag.c_str());
  }
  Flags flags = Flags::Parse(static_cast<int>(argv.size()), argv.data(), 2);
  FuzzConfig config;
  if (!FuzzConfig::Load(flags, &config, &session.error)) {
    return session;
  }
  // The session logs to stderr. Show only its errors.
  const LogLevel log_level = GetLogLevel();
  SetLogLevel(LogLevel::kError);
  const std::string crashes = session.dir + "/out/crashes";
  Stopwatch clock;
  {
    Fuzzer fuzzer(std::move(config));
    if (!fuzzer.Initialize(&session.error)) {
      SetLogLevel(log_level);
      return session;
    }
    std::atomic<bool> ended{false};
    std::thread thread([&fuzzer, &ended] {
      fuzzer.Run();
      ended.store(true);
    });
    // The session writes signatures.txt after the files of a crash. The
    // worker processes a crash of an import before it starts to fuzz. Thus
    // a fuzz execution without signatures.txt shows that the imports did not
    // crash.
    while (session.stop.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      // Read the two flags before the file, so that a crash that the
      // session saves in the meantime counts as saved.
      const bool session_ended = ended.load();
      const bool fuzzing = fuzzer.stats().fuzz_executions.load() > 0;
      session.saved = FileExists(crashes + "/signatures.txt");
      if (session.saved) {
        session.stop = "crash saved";
      } else if (session_ended) {
        session.stop = "session ended";
      } else if (fuzzing) {
        session.stop = "imports did not crash";
      } else if (clock.ElapsedSeconds() >= kCrashSessionSeconds) {
        session.stop = "time limit";
      }
    }
    fuzzer.RequestStop();
    thread.join();
  }
  SetLogLevel(log_level);
  session.seconds = clock.ElapsedSeconds();
  // The report of an imported program: the report with the line
  // `report_key`, or else the first report. A crash of the fuzzing after the
  // imports gets no NAME.repro.js, because the stop request interrupts it.
  std::string error, first;
  for (const std::string& path : ListFiles(crashes, ".txt")) {
    std::string text;
    if (!BaseName(path).starts_with("crash_") ||
        !ReadFileToString(path, &text, &error)) {
      continue;
    }
    if (first.empty()) {
      first = text;
    }
    if (!ReportValue(text, report_key).empty()) {
      session.report = text;
    }
  }
  if (session.report.empty()) {
    session.report = first;
  }
  const std::string repro_name = ReportValue(session.report, "repro");
  // The report and NAME.repro.js are also the result of a session with
  // another `report_key`.
  if (!repro_name.empty() &&
      !ReadFileToString(crashes + "/" + repro_name, &session.repro, &error)) {
    session.error = error;
  }
  return session;
}

// The words of the `// run:` line of a NAME.repro.js file: the d8, its
// flags and the file. Empty if there is no run line.
std::vector<std::string> RunLineWords(const std::string& repro) {
  const std::string prefix = "\n// run: ";
  const size_t start = repro.find(prefix);
  if (start == std::string::npos) {
    return {};
  }
  const size_t end = repro.find('\n', start + 1);
  const std::string line =
      repro.substr(start + prefix.size(), end - start - prefix.size());
  std::vector<std::string> words;
  size_t position = 0;
  while (position < line.size()) {
    const size_t space = std::min(line.find(' ', position), line.size());
    if (space > position) {
      words.push_back(line.substr(position, space - position));
    }
    position = space + 1;
  }
  return words;
}

// Runs the `// run:` line of a NAME.repro.js file in a plain d8 process.
// Returns the signature of the crash. If there is no crash, returns the
// reason in parentheses.
std::string RunReproLine(const std::string& repro,
                         const TargetOptions& target) {
  const std::vector<std::string> words = RunLineWords(repro);
  if (words.empty()) {
    return "(no run line)";
  }
  const std::vector<std::string> args(words.begin() + 1, words.end());
  const ProcessResult run =
      RunProcess(words[0], args, target.PlainEnvironment(), kPlainRunUs);
  if (!run.started) {
    return "(cannot start d8: " + run.error + ")";
  }
  if (run.timed_out) {
    return "(timed out)";
  }
  if (run.signal == 0 && run.exit_code == 0) {
    return "(no crash)";
  }
  return CrashSignature::FromOutput(run.signal, run.exit_code, run.stderr_text)
      .text;
}

// A crash behind the default prelude, in a short session. The crash
// minimizer reduces the tree of the prelude and the program. NAME.repro.js
// must keep the 2 prelude helpers that the program reaches, and no other
// prelude statement. Its `// run:` line must have absolute paths, and it
// must crash a plain d8 process with the signature of the crash. The node
// counts of `minimized:` are the counts of the program alone.
void RunCrashReproCheck(Checker* checker, const TargetOptions& target,
                        bool fuzzilli_target) {
  const char* name =
      "crash minimizer: a crash behind the default prelude gives "
      "NAME.repro.js without the unused helpers";
  // redline('REDLINE_CRASH', 1) or fuzzilli('FUZZILLI_CRASH', 1) is a CHECK
  // failure (see the crash checks).
  const char* crash_call =
      fuzzilli_target ? "fuzzilli('FUZZILLI_CRASH'" : "redline('REDLINE_CRASH'";
  const CrashSession session = RunCrashSession(
      target, {}, {StringPrintf("assertThrows(() => %s, 1));\n", crash_call)});
  const std::string& repro = session.repro;
  // Two header lines, then the script.
  const size_t first_line = repro.find('\n');
  const size_t second_line = first_line == std::string::npos
                                 ? first_line
                                 : repro.find('\n', first_line + 1);
  const bool header = repro.starts_with("// signature: ") &&
                      second_line != std::string::npos &&
                      repro.compare(first_line + 1, 8, "// run: ") == 0;
  std::unique_ptr<fast::Tree> tree =
      header ? fast::Parser::ParseScript(repro.substr(second_line + 1),
                                         fast::ParseOptions(), nullptr)
             : nullptr;
  const std::string helpers = tree != nullptr ? TopLevelVars(*tree) : "";
  const std::string& report = session.report;
  const std::string signature = ReportValue(report, "signature");
  const std::string plain = header ? RunReproLine(repro, target) : "";
  const std::vector<std::string> words = RunLineWords(repro);
  const bool absolute = words.size() >= 2 && words[0].starts_with("/") &&
                        words.back().starts_with("/");
  // The nodes of the program in the file NAME + `suffix` of the crash, or 0.
  // NAME.orig.js has the program alone before the minimization, and NAME.js
  // has it after the minimization.
  const std::string repro_name = ReportValue(report, "repro");
  auto file_nodes = [&](const char* suffix) -> size_t {
    const std::string repro_suffix = ".repro.js";
    std::string text, error;
    if (!repro_name.ends_with(repro_suffix) ||
        !ReadFileToString(
            session.dir + "/out/crashes/" +
                repro_name.substr(0, repro_name.size() - repro_suffix.size()) +
                suffix,
            &text, &error)) {
      return 0;
    }
    std::unique_ptr<fast::Tree> file_tree =
        fast::Parser::ParseScript(text, fast::ParseOptions(), nullptr);
    return file_tree != nullptr ? fast::CountNodes(file_tree->root()) : 0;
  };
  const size_t orig_nodes = file_nodes(".orig.js");
  const size_t js_nodes = file_nodes(".js");
  RemoveTree(session.dir);
  const std::string min_tree = ReportValue(report, "min_tree");
  const std::string prelude = ReportValue(report, "min_prelude_statements");
  const std::string stop = ReportValue(report, "min_stop");
  const std::string minimized = ReportValue(report, "minimized");
  const std::string script_nodes = ReportValue(report, "min_script_nodes");
  size_t nodes_before = 0, nodes_after = 0, script_before = 0, script_after = 0;
  const bool counts =
      sscanf(minimized.c_str(), "yes (%*d tests, %zu -> %zu nodes)",
             &nodes_before, &nodes_after) == 2 &&
      sscanf(script_nodes.c_str(), "%zu -> %zu", &script_before,
             &script_after) == 2;
  const bool ok = tree != nullptr && helpers == "__redline_call assertThrows" &&
                  min_tree == "prelude and program" &&
                  prelude.ends_with("-> 2") && !stop.empty() &&
                  stop != "skipped" && counts && orig_nodes > 0 &&
                  nodes_before == orig_nodes && nodes_after == js_nodes &&
                  script_before > nodes_before && absolute &&
                  ReportValue(report, "repro_rate") == "5/5" &&
                  !signature.empty() && plain == signature;
  checker->Check(
      name, ok,
      StringPrintf("%s, %.0f s, helpers \"%s\", min_tree %s, "
                   "min_prelude_statements %s, min_stop %s, minimized %s, "
                   "min_script_nodes %s, NAME.orig.js %zu nodes, NAME.js %zu "
                   "nodes, run line "
                   "\"%s ... %s\", repro_rate %s, plain d8 run \"%s\", "
                   "signature \"%s\"%s%s",
                   session.stop.c_str(), session.seconds, helpers.c_str(),
                   min_tree.c_str(), prelude.c_str(), stop.c_str(),
                   minimized.c_str(), script_nodes.c_str(), orig_nodes,
                   js_nodes, words.empty() ? "" : words[0].c_str(),
                   words.empty() ? "" : words.back().c_str(),
                   ReportValue(report, "repro_rate").c_str(), plain.c_str(),
                   signature.c_str(), session.error.empty() ? "" : ", ",
                   session.error.c_str()));
}

// The crash steps behind an on-demand prelude (`prelude.on_demand`), in a
// short session. The tree of the minimizer has the part of the prelude that
// the program got, not the whole prelude. NAME.repro.js must crash a plain
// d8 process alone, and NAME.js must crash again behind its part of the
// prelude. The report names the on-demand prelude.
void RunCrashOnDemandCheck(Checker* checker, const TargetOptions& target,
                           bool fuzzilli_target) {
  const char* name =
      "crash minimizer: a crash behind an on-demand prelude gives "
      "NAME.repro.js that crashes alone";
  const char* crash_call =
      fuzzilli_target ? "fuzzilli('FUZZILLI_CRASH'" : "redline('REDLINE_CRASH'";
  const std::string program =
      StringPrintf("assertThrows(() => %s, 1));\n", crash_call);
  const CrashSession session =
      RunCrashSession(target, {"--tunable=prelude.on_demand=1"}, {program});
  // The session set the tunable of this process.
  TunableRegistry::Instance().Reset();
  // The prelude statements that the program gets. The crash needs 2 of
  // them: `assertThrows` and the helper that it calls.
  std::string build_error;
  const std::unique_ptr<OnDemandPrelude> on_demand = OnDemandPrelude::Build(
      DefaultPrelude(), /*fallback_rules=*/true, &build_error);
  const size_t part =
      on_demand != nullptr ? on_demand->Select(program).count : 0;
  const std::string& repro = session.repro;
  const size_t first_line = repro.find('\n');
  const size_t second_line = first_line == std::string::npos
                                 ? first_line
                                 : repro.find('\n', first_line + 1);
  const bool header =
      repro.starts_with("// signature: ") && second_line != std::string::npos;
  std::unique_ptr<fast::Tree> tree =
      header ? fast::Parser::ParseScript(repro.substr(second_line + 1),
                                         fast::ParseOptions(), nullptr)
             : nullptr;
  const std::string helpers = tree != nullptr ? TopLevelVars(*tree) : "";
  const std::string& report = session.report;
  const std::string signature = ReportValue(report, "signature");
  const std::string plain = header ? RunReproLine(repro, target) : "";
  RemoveTree(session.dir);
  const std::string min_tree = ReportValue(report, "min_tree");
  const std::string statements = ReportValue(report, "min_prelude_statements");
  const std::string verified = ReportValue(report, "min_program_verified");
  const std::string prelude_line = ReportValue(report, "prelude");
  const std::string rate = ReportValue(report, "repro_rate");
  checker->Check(
      name,
      tree != nullptr && helpers == "__redline_call assertThrows" &&
          min_tree == "prelude and program" && part > 2 &&
          part < on_demand->statements() &&
          statements == StringPrintf("%zu -> 2", part) && verified == "yes" &&
          prelude_line ==
              StringPrintf("on demand (prelude.on_demand=1), %zu of %zu "
                           "statements",
                           part, on_demand->statements()) &&
          rate == "5/5" && !signature.empty() && plain == signature,
      StringPrintf("%s, %.0f s, helpers \"%s\", min_tree %s, "
                   "min_prelude_statements %s, min_program_verified %s, "
                   "prelude \"%s\", repro_rate %s, plain d8 run \"%s\", "
                   "signature \"%s\"%s%s",
                   session.stop.c_str(), session.seconds, helpers.c_str(),
                   min_tree.c_str(), statements.c_str(), verified.c_str(),
                   prelude_line.c_str(), rate.c_str(), plain.c_str(),
                   signature.c_str(), session.error.empty() ? "" : ", ",
                   session.error.c_str()));
}

// A crash behind a prelude whose text matters: the program reads the text
// of a prelude function. The printer removes the comment in the function,
// so the printed tree of the prelude and the program does not crash. Then the
// minimizer reduces the program alone, behind the exact prelude. Redline writes
// no NAME.repro.js, and `min_tree` gives the reason.
void RunCrashFallbackCheck(Checker* checker, const TargetOptions& target,
                           bool fuzzilli_target) {
  const char* name =
      "crash minimizer: a prelude whose text matters gives the program alone "
      "and no NAME.repro.js";
  char dir_template[] = "/tmp/redline-selftest-prelude.XXXXXX";
  std::string error, prelude_dir;
  if (mkdtemp(dir_template) == nullptr) {
    error = StringPrintf("mkdtemp: %s", strerror(errno));
  } else {
    prelude_dir = dir_template;
  }
  const std::string prelude = prelude_dir + "/prelude.js";
  if (error.empty()) {
    WriteStringToFile(prelude,
                      "var f = function() { /*keep*/ return 1; };\n"
                      "var unused = 2;\n",
                      &error);
  }
  CrashSession session;
  if (error.empty()) {
    const char* crash_call = fuzzilli_target ? "fuzzilli('FUZZILLI_CRASH'"
                                             : "redline('REDLINE_CRASH'";
    session = RunCrashSession(
        target, {"--prelude=" + prelude},
        {StringPrintf("if (f.toString().includes(\"/*keep*/\")) %s, 1);\n",
                      crash_call)});
    error = session.error;
    RemoveTree(session.dir);
  }
  if (!prelude_dir.empty()) {
    RemoveTree(prelude_dir);
  }
  const std::string& report = session.report;
  const std::string min_tree = ReportValue(report, "min_tree");
  const bool ok = error.empty() && !ReportValue(report, "signature").empty() &&
                  min_tree.starts_with("program alone: the printed script") &&
                  ReportValue(report, "repro").empty() && session.repro.empty();
  checker->Check(
      name, ok,
      StringPrintf("%s, %.0f s, min_tree \"%s\", repro \"%s\"%s%s",
                   session.stop.c_str(), session.seconds, min_tree.c_str(),
                   ReportValue(report, "repro").c_str(),
                   error.empty() ? "" : ", ", error.c_str()));
}

// A crash that needs 1 flag of the session, in a short session with the
// default prelude. The search of the d8 flags of NAME.repro.js keeps only
// this flag and the flags that KeepReproFlag keeps. The time budget is
// large, so only the process limit stops the search. The `// run:` line has
// these flags, and it crashes a plain d8 process with the signature. The
// rate check keeps these flags, and no temporary file stays.
void RunCrashFlagsCheck(Checker* checker, const TargetOptions& target,
                        bool fuzzilli_target) {
  const char* name =
      "crash flags: a crash that needs 1 flag of the session keeps only this "
      "flag";
  // Crash kind 7 crashes only with --hole-fuzzing.
  const char* crash_call =
      fuzzilli_target ? "fuzzilli('FUZZILLI_CRASH'" : "redline('REDLINE_CRASH'";
  const CrashSession session =
      RunCrashSession(target,
                      {"--d8-extra-args=--hole-fuzzing",
                       R"(--config={"crash_flag_budget_sec": 1000})"},
                      {StringPrintf("%s, 7);\n", crash_call)});
  // The flags that the search keeps, in the order of the target.
  std::vector<std::string> expected;
  for (const std::string& arg : target.args) {
    if (KeepReproFlag(arg, /*natives=*/false)) {
      expected.push_back(arg);
    }
  }
  expected.push_back("--hole-fuzzing");
  std::string expected_text;
  for (const std::string& flag : expected) {
    expected_text += (expected_text.empty() ? "" : " ") + flag;
  }
  const std::string flags = ReportValue(session.report, "repro_flags");
  const std::vector<std::string> words = RunLineWords(session.repro);
  const std::string plain =
      words.empty() ? "" : RunReproLine(session.repro, target);
  const size_t temporary =
      ListFiles(session.dir + "/out/crashes", ".tmp").size();
  RemoveTree(session.dir);
  const std::string signature = ReportValue(session.report, "signature");
  const std::string note = ReportValue(session.report, "repro_flags_note");
  const std::string rate = ReportValue(session.report, "repro_rate");
  const bool ok =
      flags == expected_text && words.size() == expected.size() + 2 &&
      std::equal(expected.begin(), expected.end(), words.begin() + 1) &&
      note.find(", budget used up") == std::string::npos &&
      note.find(", the session stopped") == std::string::npos &&
      rate == "5/5" && temporary == 0 && !signature.empty() &&
      plain == signature;
  checker->Check(
      name, ok,
      StringPrintf("%s, %.0f s, repro_flags \"%s\" (expected \"%s\"), %s, "
                   "repro_rate %s (%s), %zu temporary files, %zu words in the "
                   "run line, plain d8 run \"%s\", signature \"%s\"%s%s",
                   session.stop.c_str(), session.seconds, flags.c_str(),
                   expected_text.c_str(), note.c_str(), rate.c_str(),
                   ReportValue(session.report, "repro_rate_note").c_str(),
                   temporary, words.size(), plain.c_str(), signature.c_str(),
                   session.error.empty() ? "" : ", ", session.error.c_str()));
}

// A program that changes one more protector of the isolate in each run. The
// protectors stay changed in the next runs of the same process, so the 5th
// run in one process crashes. `type` names its class, so that two programs
// can have different sources. The last statement is an unused declaration.
// The dataflow reducer tests the later statements first, so the first test
// of the minimizer removes this declaration, and the test passes.
std::string ProtectorProgram(const char* type, bool fuzzilli_target) {
  const char* crash_call =
      fuzzilli_target ? "fuzzilli('FUZZILLI_CRASH'" : "redline('REDLINE_CRASH'";
  return StringPrintf(
      "class %s {}\n"
      "if (!%%TypedArraySpeciesProtector()) {\n"
      "  %s, 1);\n"
      "} else if (!%%RegExpSpeciesProtector()) {\n"
      "  Object.defineProperty(Uint8Array, Symbol.species, {value: %s});\n"
      "} else if (!%%PromiseSpeciesProtector()) {\n"
      "  Object.defineProperty(RegExp, Symbol.species, {value: %s});\n"
      "} else if (!%%ArraySpeciesProtector()) {\n"
      "  Object.defineProperty(Promise, Symbol.species, {value: %s});\n"
      "} else {\n"
      "  Object.defineProperty(Array, Symbol.species, {value: %s});\n"
      "}\n"
      "var unused = 0;\n",
      type, crash_call, type, type, type, type);
}

// An amplified crash, in a short session without the prelude, with
// harvest_crashes. Each run of the program changes one more protector of the
// isolate. The protectors stay changed in the next runs of the same process,
// and the 5th run crashes. Thus a fresh target with crash_reruns runs does
// not crash, and the amplify step finds --stress-runs. The minimizer reduces
// the program with the amplified flags in plain d8 processes. The `// run:`
// line of NAME.repro.js keeps --stress-runs, and it crashes a plain d8
// process with the signature.
void RunCrashAmplifiedCheck(Checker* checker, const TargetOptions& target,
                            bool fuzzilli_target) {
  const char* name =
      "amplified crashes: an amplified crash gets NAME.repro.js with "
      "--stress-runs";
  // With min_reruns 5, the 4th determinism rerun of the first program is the
  // 5th run, and HarvestCrash takes its crash. The second program, with a
  // different source, makes the test independent of the number of reruns.
  // The time limit of 2 s stops the minimization before its end. The time
  // limit starts after the start check, and the first test of the minimizer
  // removes the unused declaration (ProtectorProgram). Thus the minimizer
  // accepts this test also on a loaded machine, and the last check must run
  // after the time limit.
  const CrashSession session = RunCrashSession(
      target,
      {"--prelude=none",
       R"(--config={"crash_history": 0, "crash_amplified_budget_sec": 2, "harvest_crashes": true})"},
      {ProtectorProgram("C0", fuzzilli_target),
       ProtectorProgram("C1", fuzzilli_target)});
  const std::string& report = session.report;
  const std::vector<std::string> words = RunLineWords(session.repro);
  const std::string plain =
      words.empty() ? "" : RunReproLine(session.repro, target);
  RemoveTree(session.dir);
  const std::string signature = ReportValue(report, "signature");
  const bool stress_runs = std::any_of(
      words.begin(), words.end(),
      [](const std::string& word) { return IsFlag(word, "stress-runs"); });
  const std::string stop = ReportValue(report, "min_stop");
  const bool ok = ReportValue(report, "amplified") == "yes" &&
                  ReportValue(report, "reproducible") == "no" &&
                  ReportValue(report, "min_oracle").starts_with("1 of ") &&
                  stop == "budget" &&
                  !ReportValue(report, "repro_rate").empty() && stress_runs &&
                  !signature.empty() && plain == signature;
  checker->Check(
      name, ok,
      StringPrintf(
          "%s, %.0f s, amplified_rate %s, min_oracle \"%s\", min_stop %s, "
          "min_tree \"%s\", min_script_nodes %s, repro_flags \"%s\", %s, "
          "repro_rate %s (%s), plain d8 run \"%s\", signature \"%s\"%s%s",
          session.stop.c_str(), session.seconds,
          ReportValue(report, "amplified_rate").c_str(),
          ReportValue(report, "min_oracle").c_str(), stop.c_str(),
          ReportValue(report, "min_tree").c_str(),
          ReportValue(report, "min_script_nodes").c_str(),
          ReportValue(report, "repro_flags").c_str(),
          ReportValue(report, "repro_flags_note").c_str(),
          ReportValue(report, "repro_rate").c_str(),
          ReportValue(report, "repro_rate_note").c_str(), plain.c_str(),
          signature.c_str(), session.error.empty() ? "" : ", ",
          session.error.c_str()));
}

// A state-dependent crash, in a short session without the prelude and
// without the amplify step. The programs are the programs of
// RunCrashAmplifiedCheck. The history replay finds the earlier runs, and
// Redline writes NAME.bundle.js. The minimizer writes NAME.bundle.min.js
// with fewer parts. It crashes as often as NAME.bundle.js, and it crashes a
// plain d8 process with the signature.
void RunCrashBundleCheck(Checker* checker, const TargetOptions& target,
                         bool fuzzilli_target) {
  const char* name =
      "bundles: a state-dependent crash gets a smaller NAME.bundle.min.js";
  const ProcessResult probe =
      RunProcess(target.executable, {"--bundle-reprl", "/dev/null"},
                 target.PlainEnvironment(), kPlainRunUs);
  if (!probe.started || probe.timed_out || probe.signal != 0 ||
      probe.exit_code != 0 ||
      probe.stderr_text.find("unknown flag --bundle-reprl") !=
          std::string::npos) {
    checker->Info(name, "skipped (the target d8 has no --bundle-reprl flag)");
    return;
  }
  const CrashSession session = RunCrashSession(
      target,
      {"--prelude=none",
       R"(--config={"amplify": {"strategy": "none"}, "crash_bundle_min_budget_sec": 60})"},
      {ProtectorProgram("C0", fuzzilli_target),
       ProtectorProgram("C1", fuzzilli_target)},
      "bundle_min");
  const std::string& report = session.report;
  const std::string crashes = session.dir + "/out/crashes/";
  const std::string bundle_name = ReportValue(report, "bundle");
  const std::string min_name = ReportValue(report, "bundle_min");
  std::string bundle, min, error;
  const bool read = !bundle_name.empty() && !min_name.empty() &&
                    ReadFileToString(crashes + bundle_name, &bundle, &error) &&
                    ReadFileToString(crashes + min_name, &min, &error);
  // One plain d8 run of the minimized bundle.
  std::string plain;
  if (read) {
    std::vector<std::string> args = target.args;
    args.push_back("--bundle-reprl");
    args.push_back(crashes + min_name);
    const ProcessResult run = RunProcess(
        target.executable, args, target.PlainEnvironment(), kPlainRunUs);
    plain = !run.started || run.timed_out ? "(no run)"
            : run.signal == 0 && run.exit_code == 0
                ? "(no crash)"
                : CrashSignature::FromOutput(run.signal, run.exit_code,
                                             run.stderr_text)
                      .text;
  }
  RemoveTree(session.dir);
  auto parts = [](const std::string& text) {
    size_t count = 0;
    for (size_t at = text.find(kBundleScriptMarker); at != std::string::npos;
         at = text.find(kBundleScriptMarker, at + 1)) {
      count++;
    }
    return count;
  };
  // The hits of a rate "h/n".
  auto hits = [](const std::string& rate) {
    return rate.empty() ? -1 : atoi(rate.c_str());
  };
  const std::string signature = ReportValue(report, "signature");
  const std::string rate = ReportValue(report, "bundle_rate");
  const std::string min_rate = ReportValue(report, "bundle_min_rate");
  const bool ok = ReportValue(report, "state_dependent") == "yes" && read &&
                  parts(min) < parts(bundle) && hits(min_rate) >= hits(rate) &&
                  hits(rate) > 0 && !signature.empty() && plain == signature;
  checker->Check(
      name, ok,
      StringPrintf("%s, %.0f s, bundle_rate %s, bundle_min_rate %s, "
                   "bundle_min_note \"%s\", %zu -> %zu parts, plain d8 run "
                   "\"%s\", signature \"%s\"%s%s",
                   session.stop.c_str(), session.seconds, rate.c_str(),
                   min_rate.c_str(),
                   ReportValue(report, "bundle_min_note").c_str(),
                   parts(bundle), parts(min), plain.c_str(), signature.c_str(),
                   error.empty() ? "" : ", ", error.c_str()));
}

// Offline: optimized code features with one bit per edge. As with counters,
// they do not count for the coverage, they weigh in the rarity strategy and
// they do not admit a program.
void RunCoverageBitsChecks(Checker* checker) {
  std::string error;
  std::unique_ptr<CoverageMap> map = CoverageMap::Create(9996, &error);
  if (!checker->Check("coverage bits: create a coverage map", map != nullptr,
                      error)) {
    return;
  }
  int fd = shm_open(map->shm_name().c_str(), O_RDWR, 0);
  void* mapping = fd < 0 ? MAP_FAILED
                         : mmap(nullptr, CoverageMap::kShmSize,
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fd >= 0) {
    close(fd);
  }
  const bool mapped = mapping != MAP_FAILED;
  if (!checker->Check("coverage bits: map the shared memory", mapped,
                      mapped ? "" : strerror(errno))) {
    return;
  }
  // A target with 80 code edges, 10 builtins blocks and 10 optimized code
  // features (inline-bool-flag) writes this header.
  cov::Header header = {};
  header.magic = cov::kMagic;
  header.version = cov::kVersion;
  header.header_size = cov::kHeaderSize;
  header.shm_size = static_cast<uint32_t>(CoverageMap::kShmSize);
  header.mode = static_cast<uint32_t>(cov::Mode::kInlineBoolFlag);
  header.bits_per_edge = 1;
  header.num_edges = 100;
  header.max_edges = cov::MaxEdges(header.shm_size, 1);
  header.num_regions = 3;
  header.regions[0] = {static_cast<uint32_t>(cov::RegionKind::kCode), 1, 80};
  header.regions[1] = {static_cast<uint32_t>(cov::RegionKind::kBuiltins), 81,
                       10};
  header.regions[2] = {static_cast<uint32_t>(cov::RegionKind::kOptimized), 91,
                       10};
  memcpy(mapping, &header, sizeof(header));
  const bool ok = map->FinishInitialization(&error);
  checker->Check("coverage bits: read the header", ok, error);
  if (ok) {
    FeatureWeights::Params params;
    std::unique_ptr<FeatureWeights> weights =
        FeatureWeights::Build(*map, params, {});
    checker->Check(
        "coverage bits: optimized code features",
        !map->counters_mode() && map->num_edges() == 100 &&
            map->counted_edges() == 90 &&
            map->KindOf(95) == CoverageMap::FeatureKind::kOptimized &&
            weights->Of(95) == params.optimized &&
            weights->Of(85) == params.builtins && weights->Of(10) == 1.0 &&
            weights->InFocus(10) && !weights->InFocus(95),
        StringPrintf("counted %u, w(95) %g, w(85) %g, admits 95: %d",
                     map->counted_edges(), weights->Of(95), weights->Of(85),
                     weights->InFocus(95)));
  }
  munmap(mapping, CoverageMap::kShmSize);
}

// The first 5 columns of the row of `signature_text` in signatures.txt
// (signature, crashes, saved, minimized, reproduced), or "".
std::string SignatureRow(const std::string& summary,
                         const std::string& signature_text) {
  const size_t start = summary.find(signature_text + " |");
  if (start == std::string::npos) {
    return "";
  }
  size_t end = start;
  for (int column = 0; column < 5 && end != std::string::npos; column++) {
    end = summary.find(" | ", end + 1);
  }
  return summary.substr(
      start, end == std::string::npos ? std::string::npos : end - start);
}

// Offline: the minimize slots of a crash signature. At most
// `minimize_crashes_per_signature` crashes of one signature hold a minimize
// slot at the same time. A crash whose minimization fails gives back its
// slot. A crash without a slot gives back nothing.
void RunCrashSlotChecks(Checker* checker) {
  const char* name =
      "crash slots: only a crash with a minimize slot gives back a slot";
  char dir_template[] = "/tmp/redline-selftest.XXXXXX";
  if (mkdtemp(dir_template) == nullptr) {
    checker->Check(name, false, StringPrintf("mkdtemp: %s", strerror(errno)));
    return;
  }
  const std::string dir = dir_template;
  const std::string summary_path = dir + "/crashes/signatures.txt";
  std::string error;
  std::string all_succeed, first_fails, summary;
  bool read = false;
  if (MakeDirectories(dir + "/crashes", &error)) {
    FuzzConfig config;
    config.out_dir = dir;
    config.max_crashes_per_signature = 5;
    config.minimize_crashes_per_signature = 2;
    Fuzzer fuzzer(std::move(config));
    // The crashes that got a minimize slot, of 6 crashes of one signature.
    // The minimization fails for the crashes in `failures`.
    auto slots = [&fuzzer](int line, uint64_t first_hash,
                           const std::set<int>& failures) {
      const CrashSignature signature = CrashSignature::FromOutput(
          6, 0,
          StringPrintf("#\n# Fatal error in a.cc, line %d\n# Check failed: "
                       "x.\n",
                       line));
      std::string crashes;
      for (int i = 0; i < 6; i++) {
        if (fuzzer.RecordCrashForTest(first_hash + static_cast<uint64_t>(i),
                                      signature, failures.count(i) == 0)) {
          crashes += StringPrintf("%s%d", crashes.empty() ? "" : " ", i);
        }
      }
      return crashes;
    };
    all_succeed = slots(1, 100, {});
    first_fails = slots(2, 200, {0});
    read = ReadFileToString(summary_path, &summary, &error);
  }
  // The columns: signature | crashes | saved | minimized | reproduced.
  const std::string row1 = SignatureRow(summary, "fatal a.cc:1 x.");
  const std::string row2 = SignatureRow(summary, "fatal a.cc:2 x.");
  const bool ok = read && all_succeed == "0 1" && first_fails == "0 1 2" &&
                  row1 == "fatal a.cc:1 x. | 6 | 5 | 2 | 6" &&
                  row2 == "fatal a.cc:2 x. | 6 | 5 | 2 | 6";
  checker->Check(
      name, ok,
      StringPrintf("slots \"%s\" and \"%s\", rows \"%s\" and \"%s\"%s%s",
                   all_succeed.c_str(), first_fails.c_str(), row1.c_str(),
                   row2.c_str(), error.empty() ? "" : ", ", error.c_str()));
  std::remove(summary_path.c_str());
  rmdir((dir + "/crashes").c_str());
  rmdir(dir.c_str());
}

// Offline: the Fuzzilli shared-memory layout (src/fuzzilli/cov.cc). A
// 4-byte edge count at offset 0 is followed by the edge bitmap at offset 4.
void RunFuzzilliCoverageChecks(Checker* checker) {
  std::string error;
  std::unique_ptr<CoverageMap> map = CoverageMap::Create(9995, &error);
  if (!checker->Check("fuzzilli coverage: create a coverage map",
                      map != nullptr, error)) {
    return;
  }
  int fd = shm_open(map->shm_name().c_str(), O_RDWR, 0);
  void* mapping = fd < 0 ? MAP_FAILED
                         : mmap(nullptr, CoverageMap::kShmSize,
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fd >= 0) {
    close(fd);
  }
  const bool mapped = mapping != MAP_FAILED;
  if (!checker->Check("fuzzilli coverage: map the shared memory", mapped,
                      mapped ? "" : strerror(errno))) {
    return;
  }
  uint8_t* shm = static_cast<uint8_t*>(mapping);
  memset(shm, 0, CoverageMap::kShmSize);
  const uint32_t num_edges = 120;
  memcpy(shm, &num_edges, sizeof(num_edges));
  bool ok = map->FinishInitialization(&error);
  checker->Check("fuzzilli coverage: detect and read the Fuzzilli layout", ok,
                 error);
  if (ok) {
    checker->Check(
        "fuzzilli coverage: geometry and single code region",
        map->is_fuzzilli() && map->target_kind() == TargetKind::kFuzzilli &&
            map->num_edges() == 120 && map->real_edges() == 120 &&
            map->counted_edges() == 120 &&
            strcmp(map->mode_name(), "fuzzilli") == 0 &&
            map->build_id().empty() && map->regions().size() == 1 &&
            map->regions()[0].kind == CoverageMap::RegionKind::kCode &&
            map->regions()[0].first == 1 && map->regions()[0].count == 120,
        StringPrintf("%u edges, mode %s", map->num_edges(), map->mode_name()));
    // The bitmap starts at offset 4, right after `num_edges`. Edge 1 and edge
    // `num_edges` are valid edges, and `CoverageMap` ignores bit `num_edges +
    // 1`.
    uint8_t* bits = shm + CoverageMap::kFuzzilliHeaderSize;
    for (uint32_t bit :
         {uint32_t{1}, uint32_t{5}, uint32_t{115}, num_edges, num_edges + 1}) {
      bits[bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
    }
    const std::vector<uint32_t> expected = {1, 5, 115, num_edges};
    checker->Check(
        "fuzzilli coverage: bits after the 4-byte edge count",
        map->CurrentEdges() == expected && map->EvaluateNewEdges() == expected,
        StringPrintf("%zu edges", map->CurrentEdges().size()));
    // ClearBitmap clears the bitmap at offset 4 and keeps num_edges at 0..3.
    map->ClearBitmap();
    uint32_t kept_edges = 0;
    memcpy(&kept_edges, shm, sizeof(kept_edges));
    checker->Check(
        "fuzzilli coverage: ClearBitmap keeps the edge count at offset 0",
        map->CurrentEdges().empty() && kept_edges == 120,
        StringPrintf("num_edges=%u", kept_edges));
    // A respawned process with a different edge count is an error.
    const uint32_t other_edges = 121;
    memcpy(shm, &other_edges, sizeof(other_edges));
    std::string changed;
    checker->Check("fuzzilli coverage: a changed edge count is an error",
                   !map->FinishInitialization(&changed) &&
                       changed.find("mixing binaries") != std::string::npos,
                   changed);
  }
  // --target-kind=redline rejects the Fuzzilli layout, and --targets is
  // rejected in Fuzzilli mode.
  memcpy(shm, &num_edges, sizeof(num_edges));
  std::unique_ptr<CoverageMap> forced_redline =
      CoverageMap::Create(9994, &error);
  if (forced_redline != nullptr) {
    forced_redline->SetTargetKind(TargetKind::kRedline);
    int rfd = shm_open(forced_redline->shm_name().c_str(), O_RDWR, 0);
    if (rfd >= 0) {
      if (pwrite(rfd, &num_edges, sizeof(num_edges), 0) !=
          static_cast<ssize_t>(sizeof(num_edges))) {
        rfd = -1;
      }
      close(rfd);
    }
    std::string reason;
    checker->Check(
        "fuzzilli coverage: --target-kind=redline rejects the Fuzzilli layout",
        rfd >= 0 && !forced_redline->FinishInitialization(&reason), reason);
  }
  std::unique_ptr<CoverageMap> with_targets = CoverageMap::Create(9993, &error);
  if (with_targets != nullptr) {
    CoverageMap::Watch watch;
    watch.edges = {5};
    with_targets->SetWatch(watch);
    int tfd = shm_open(with_targets->shm_name().c_str(), O_RDWR, 0);
    if (tfd >= 0) {
      if (pwrite(tfd, &num_edges, sizeof(num_edges), 0) !=
          static_cast<ssize_t>(sizeof(num_edges))) {
        tfd = -1;
      }
      close(tfd);
    }
    std::string reason;
    checker->Check(
        "fuzzilli coverage: --targets is rejected with a clear message",
        tfd >= 0 && !with_targets->FinishInitialization(&reason) &&
            reason.find("--targets") != std::string::npos,
        reason);
  }
  munmap(mapping, CoverageMap::kShmSize);
}

// Offline: the optimized code region of the Fuzzilli layout. A process
// with --fuzzilli-enable-optimized-code-coverage reports
// CoverageMap::kFuzzilliNumOptimizedCodeEdges more edges.
void RunFuzzilliOptimizedCoverageChecks(Checker* checker) {
  constexpr uint32_t kCode = 120;
  constexpr uint32_t kAll = kCode + CoverageMap::kFuzzilliNumOptimizedCodeEdges;
  checker->Check(
      "fuzzilli optcov: the last optimized code coverage flag applies",
      CoverageMap::RequestsOptimizedCoverage(
          {"--fuzzilli_enable_optimized_code_coverage"}) &&
          !CoverageMap::RequestsOptimizedCoverage(
              {"--fuzzilli-enable-optimized-code-coverage",
               "--no-fuzzilli-enable-optimized-code-coverage"}) &&
          !CoverageMap::RequestsOptimizedCoverage({"--maglev"}),
      "");
  // Writes `edges` as the edge count of a new process and initializes `map`.
  auto start = [](CoverageMap* map, uint32_t edges, bool requested,
                  bool possible, std::string* reason) {
    int fd = shm_open(map->shm_name().c_str(), O_RDWR, 0);
    if (fd < 0) {
      *reason = strerror(errno);
      return false;
    }
    const bool written = pwrite(fd, &edges, sizeof(edges), 0) ==
                         static_cast<ssize_t>(sizeof(edges));
    close(fd);
    map->SetOptimizedCoverage(requested, possible);
    return written && map->FinishInitialization(reason);
  };
  std::string error;
  std::unique_ptr<CoverageMap> map = CoverageMap::Create(9992, &error);
  if (!checker->Check("fuzzilli optcov: create a coverage map", map != nullptr,
                      error)) {
    return;
  }
  // Random d8 flags: the first process runs without the flag, but the map
  // has the region.
  const bool ok = start(map.get(), kCode, false, true, &error);
  const auto& regions = map->regions();
  checker->Check(
      "fuzzilli optcov: a kOptimized region follows the code edges",
      ok && map->num_edges() == kAll && map->real_edges() == kAll &&
          map->counted_edges() == kCode && regions.size() == 2 &&
          regions[1].kind == CoverageMap::RegionKind::kOptimized &&
          regions[1].first == kCode + 1 &&
          regions[1].count == CoverageMap::kFuzzilliNumOptimizedCodeEdges &&
          map->KindOf(kCode) == CoverageMap::FeatureKind::kCode &&
          map->KindOf(kCode + 1) == CoverageMap::FeatureKind::kOptimized,
      StringPrintf("%u edges, %u counted: %s", map->num_edges(),
                   map->counted_edges(), error.c_str()));
  if (!ok) {
    return;
  }
  // One code edge and one optimized code feature: only the code edge counts
  // for the coverage, like in a process without the flag.
  int fd = shm_open(map->shm_name().c_str(), O_RDWR, 0);
  uint8_t bits[64] = {};
  bits[5 / 8] = 1u << (5 % 8);
  bits[(kCode + 3) / 8] |= 1u << ((kCode + 3) % 8);
  const bool written = fd >= 0 && pwrite(fd, bits, sizeof(bits),
                                         CoverageMap::kFuzzilliHeaderSize) ==
                                      static_cast<ssize_t>(sizeof(bits));
  if (fd >= 0) {
    close(fd);
  }
  SharedCoverage shared;
  const bool shared_ok =
      shared.Initialize(map->num_edges(), map->bitmap_size(), &error,
                        map->counted_edges(), map->real_edges());
  const std::vector<uint32_t> fresh = map->EvaluateNewEdges();
  const std::vector<uint32_t> claimed = shared.ClaimNewEdges(map->bitmap());
  const std::vector<uint32_t> expected = {5, kCode + 3};
  checker->Check(
      "fuzzilli optcov: optimized code features do not count as coverage",
      written && shared_ok && fresh == expected && claimed == expected &&
          map->found_edges() == 1 && shared.found_edges() == 1 &&
          map->coverage_fraction() == 1.0 / (kCode + 1) &&
          shared.coverage_fraction() == 1.0 / (kCode + 1),
      StringPrintf("found %u and %u", map->found_edges(),
                   shared.found_edges()));
  // Later processes: with and without the flag. Any other edge count is
  // another binary.
  std::string with_flag, without_flag, other, unrequested;
  const bool respawns = start(map.get(), kAll, true, true, &with_flag) &&
                        start(map.get(), kCode, false, true, &without_flag);
  const bool other_rejected = !start(map.get(), kCode + 1, false, true, &other);
  const bool unrequested_rejected =
      !start(map.get(), kAll, false, true, &unrequested);
  checker->Check(
      "fuzzilli optcov: N and N + 32768 edges are accepted, other counts not",
      respawns && map->num_edges() == kAll && other_rejected &&
          other.find("mixing binaries") != std::string::npos &&
          unrequested_rejected &&
          unrequested.find("mixing binaries") != std::string::npos,
      with_flag + without_flag + " | " + other + " | " + unrequested);
  // Fixed d8 flags without the flag: no region, and a process that reports
  // the optimized code features is an error.
  std::unique_ptr<CoverageMap> fixed = CoverageMap::Create(9991, &error);
  std::string grown, requested;
  const bool fixed_ok =
      fixed != nullptr && start(fixed.get(), kCode, false, false, &error);
  checker->Check("fuzzilli optcov: without optcov the layout stays strict",
                 fixed_ok && fixed->num_edges() == kCode &&
                     fixed->regions().size() == 1 &&
                     !start(fixed.get(), kAll, false, false, &grown) &&
                     !start(fixed.get(), kAll, true, true, &requested) &&
                     grown.find("mixing binaries") != std::string::npos &&
                     requested.find("mixing binaries") != std::string::npos,
                 error + " | " + grown + " | " + requested);
  // A fixed flag: the first process already reports the optimized features.
  std::unique_ptr<CoverageMap> first = CoverageMap::Create(9990, &error);
  std::string too_few;
  const bool first_ok =
      first != nullptr && start(first.get(), kAll, true, true, &error);
  std::unique_ptr<CoverageMap> small = CoverageMap::Create(9989, &error);
  checker->Check(
      "fuzzilli optcov: a first process with the flag sets the code edges",
      first_ok && first->counted_edges() == kCode &&
          first->num_edges() == kAll && small != nullptr &&
          !start(small.get(), 100, true, true, &too_few) &&
          too_few.find("only 100 edges") != std::string::npos,
      error + " | " + too_few);
  checker->Check("fuzzilli optcov: K-of-N admission of optimized code features",
                 OptimizedAdmitWorkers(0.0, 8) == 0 &&
                     OptimizedAdmitWorkers(0.25, 1) == 1 &&
                     OptimizedAdmitWorkers(0.25, 4) == 1 &&
                     OptimizedAdmitWorkers(0.25, 8) == 2 &&
                     OptimizedAdmitWorkers(0.3, 10) == 3 &&
                     OptimizedAdmitWorkers(1.0, 3) == 3,
                 "");
}

// Offline: find retention. A determinism rerun that does not succeed does
// not make edges flaky. Suppressed edges leave Program::hit_edges. Crash
// reruns get a longer timeout, and saved corpus files parse again.
void RunRetentionChecks(Checker* checker) {
  std::string error;
  CheckRules rules;
  auto check = [&](SharedCoverage& shared, std::vector<uint32_t> claimed,
                   std::vector<CheckRun> runs) {
    size_t next = 0;
    return RunDeterminismCheck(
        shared, rules, std::move(claimed), true,
        [&](const std::vector<uint32_t>& /*edges*/, CheckRun* run) {
          if (next < runs.size()) {
            *run = runs[next++];
          } else {
            run->succeeded = true;
          }
          return true;
        });
  };
  {
    // A rerun that times out or crashes (succeeded is false) after a clean
    // rerun: the edges go back, without a flaky count.
    SharedCoverage shared;
    const bool ok = shared.Initialize(255, 32, &error);
    const std::vector<uint8_t> bitmap = EdgeBitmap({50, 51});
    std::vector<uint32_t> claimed = shared.ClaimNewEdges(bitmap.data());
    CheckRun clean;
    clean.succeeded = true;
    CheckRun timeout;
    const CheckResult result = check(shared, claimed, {clean, timeout});
    checker->Check(
        "retention: a rerun timeout or crash does not mark edges flaky",
        ok && claimed == std::vector<uint32_t>{50, 51} && !result.survived &&
            !result.deterministic && result.reruns == 2 &&
            !shared.AnyFlaky({50, 51}) && !shared.IsSeen(50) &&
            !shared.IsSeen(51) && shared.found_edges() == 0,
        StringPrintf("reruns %d, found %u", result.reruns,
                     shared.found_edges()));
  }
  {
    // A check that does not converge: only the edges that a successful rerun
    // lost are flaky. The edge that all runs reached is not.
    SharedCoverage shared;
    const bool ok = shared.Initialize(255, 32, &error);
    const std::vector<uint8_t> bitmap = EdgeBitmap({60, 61, 62, 63});
    std::vector<uint32_t> claimed = shared.ClaimNewEdges(bitmap.data());
    rules.max_reruns = 3;
    std::vector<CheckRun> runs(3);
    for (uint32_t i = 0; i < 3; i++) {
      runs[i].succeeded = true;
      runs[i].missing = {60 + i};
    }
    const CheckResult result = check(shared, claimed, runs);
    rules = CheckRules();
    checker->Check(
        "retention: no convergence marks only the lost edges flaky",
        ok && !result.survived && !result.converged && shared.AnyFlaky({60}) &&
            shared.AnyFlaky({61}) && shared.AnyFlaky({62}) &&
            !shared.AnyFlaky({63}) && !shared.IsSeen(63),
        StringPrintf("reruns %d, dropped %zu", result.reruns, result.dropped));
  }
  {
    // A suppressed edge leaves the hit edges. RecordFlaky ignores the edges
    // outside 1..num_edges.
    SharedCoverage shared;
    const bool ok = shared.Initialize(255, 32, &error);
    const bool first = shared.RecordFlaky(70, 2);
    const bool second = shared.RecordFlaky(70, 2);
    const bool outside = shared.RecordFlaky(0, 1) || shared.RecordFlaky(256, 1);
    std::vector<uint32_t> hit = {69, 70, 71};
    shared.RemoveSuppressed(&hit);
    checker->Check(
        "retention: suppressed flaky edges leave the hit edges",
        ok && !first && second && !outside && shared.IsSuppressed(70) &&
            !shared.IsSuppressed(69) && !shared.IsSuppressed(256) &&
            !shared.AnyFlaky({256}) && shared.suppressed_flaky_edges() == 1 &&
            hit == std::vector<uint32_t>{69, 71},
        StringPrintf("suppressed %llu", static_cast<unsigned long long>(
                                            shared.suppressed_flaky_edges())));
  }
  {
    // The crash rerun timeout: from the configuration, at least 1x.
    const char* argv[] = {"redline",
                          "fuzz",
                          "--timeout-ms=500",
                          "--crash-rerun-timeout-factor=3",
                          "--opt-admit-workers-fraction=0.25",
                          "--allow-no-dcheck"};
    Flags flags = Flags::Parse(6, argv, 2);
    FuzzConfig config;
    bool ok = FuzzConfig::Load(flags, &config, &error, /*for_run=*/false);
    const std::string dump = config.ToJson(FuzzConfig::Detail::kFull);
    ok =
        ok && config.crash_rerun_timeout_factor == 3 &&
        CrashRerunTimeout(config) == 1500 * 1000 &&
        config.opt_admit_workers_fraction == 0.25 && config.allow_no_dcheck &&
        dump.find("\"crash_rerun_timeout_factor\":3") != std::string::npos &&
        dump.find("\"opt_admit_workers_fraction\":0.25") != std::string::npos &&
        dump.find("\"allow_no_dcheck\":true") != std::string::npos;
    const char* low[] = {
        "redline", "fuzz",
        R"(--config={"crash_rerun_timeout_factor": 0.5, "coverage": {"opt_admit_workers_fraction": 2}})"};
    flags = Flags::Parse(3, low, 2);
    FuzzConfig clamped;
    const bool low_ok =
        FuzzConfig::Load(flags, &clamped, &error, /*for_run=*/false);
    FuzzConfig defaults;
    checker->Check(
        "retention: crash rerun timeout factor and optcov admission load and "
        "dump",
        ok && low_ok && clamped.crash_rerun_timeout_factor == 1.0 &&
            clamped.opt_admit_workers_fraction == 1.0 &&
            defaults.crash_rerun_timeout_factor == 2.0 &&
            defaults.opt_admit_workers_fraction == 0.0 &&
            !defaults.allow_no_dcheck,
        error);
  }
  {
    // The timeout of an instrumented run: `explore.timeout_factor` times
    // the target timeout. The default factor is 4.
    const char* argv[] = {"redline", "fuzz", "--timeout-ms=500",
                          "--allow-no-dcheck"};
    Flags flags = Flags::Parse(4, argv, 2);
    FuzzConfig config;
    bool ok = FuzzConfig::Load(flags, &config, &error, /*for_run=*/false);
    TunableRegistry& registry = TunableRegistry::Instance();
    const Tunable* factor = registry.Find("explore.timeout_factor");
    ok = ok && factor != nullptr && factor->is_default() && (*factor)() == 4 &&
         InstrumentedRunTimeout(config) == 2000 * 1000;
    std::string tunable_error;
    ok = registry.Set("explore.timeout_factor", 1.5, &tunable_error) && ok &&
         InstrumentedRunTimeout(config) == 750 * 1000;
    ok = registry.Set("explore.timeout_factor", 1, &tunable_error) && ok &&
         InstrumentedRunTimeout(config) == config.target.timeout_us;
    const bool rejects =
        !registry.Set("explore.timeout_factor", 0.5, &tunable_error) &&
        !registry.Set("explore.timeout_factor", 16.5, &tunable_error);
    registry.Reset();
    checker->Check(
        "explore: the timeout of an instrumented run is "
        "`explore.timeout_factor` times the target timeout",
        ok && rejects && InstrumentedRunTimeout(config) == 2000 * 1000,
        ok && rejects ? "" : error + " " + tunable_error);
  }
  checker->Check(
      "retention: the startup crash probes use the d8 builtin",
      CrashProbeScript(true, 2) == "fuzzilli('FUZZILLI_CRASH', 2)" &&
          CrashProbeScript(false, 1) == "redline('REDLINE_CRASH', 1)",
      "");
  checker->Check(
      "retention: the crash probes stop for the modes that filter crashes",
      FiltersCrashProbes({"--fuzzing", "--hole-fuzzing"}) &&
          FiltersCrashProbes({"--hole_fuzzing"}) &&
          FiltersCrashProbes({"--sandbox-fuzzing"}) &&
          FiltersCrashProbes({"--sandbox-trap-fuzzing"}) &&
          FiltersCrashProbes({"--maglev", "--sandbox-testing"}) &&
          !FiltersCrashProbes({}) &&
          !FiltersCrashProbes({"--fuzzing", "--maglev"}) &&
          !FiltersCrashProbes({"--hole-fuzzing-extra"}),
      "");
  {
    // A corpus file header with line terminators in the lineage, and the
    // flags of the program. The file parses again, the flags come back as
    // program flags through SanitizeTestFlags, and they are not imported
    // flags.
    Lineage lineage;
    lineage.origin = "import";
    lineage.detail =
        "a\nb\rc\xE2\x80\xA8"
        "d\xE2\x80\xA9"
        "e";
    std::shared_ptr<Program> program =
        Program::Parse("let x = 1;\nx + 1;\n", lineage, &error);
    std::string header, file;
    ImportInfo info;
    std::unique_ptr<fast::Tree> tree;
    if (program != nullptr) {
      program->flags = {"--maglev", "--expose-gc",
                        "--no-lazy-feedback-allocation"};
      header = CorpusFileHeader(*program);
      file = header + program->source();
      tree =
          ImportSource(file, "/tmp/000001.js", ImportOptions(), &info, &error);
    }
    const bool one_line_each =
        std::count(header.begin(), header.end(), '\n') == 2 &&
        header.find('\r') == std::string::npos &&
        header.find("\xE2\x80\xA8") == std::string::npos &&
        header.find("\xE2\x80\xA9") == std::string::npos &&
        header.find("a\\nb\\rc\\u2028d\\u2029e") != std::string::npos;
    checker->Check(
        "retention: a corpus file header round-trips with its flags",
        program != nullptr && one_line_each && tree != nullptr &&
            info.flags.empty() && ParseFlagsLines(file).empty() &&
            info.program_flags ==
                std::vector<std::string>{"--maglev",
                                         "--no-lazy-feedback-allocation"} &&
            info.rejected_flags == std::vector<std::string>{"--expose-gc"} &&
            header.find(std::string(kRedlineFlagsPrefix) +
                        " --maglev --expose-gc") != std::string::npos,
        header + error);
  }
}

// Offline: wildcard expansion in ListFiles and Test262 frontmatter in
// ImportSource.
void RunImportChecks(Checker* checker) {
  char dir_template[] = "/tmp/redline-import.XXXXXX";
  if (mkdtemp(dir_template) == nullptr) {
    checker->Check("import: create a temporary directory", false,
                   strerror(errno));
    return;
  }
  const std::string dir = dir_template;
  std::string error;
  const bool dirs_ok =
      MakeDirectories(dir + "/test/test262/data/harness", &error) &&
      MakeDirectories(dir + "/test/test262/data/test/harness", &error) &&
      MakeDirectories(dir + "/test/test262/data/test/sub1/deep", &error) &&
      MakeDirectories(dir + "/test/test262/data/test/sub2", &error);
  const bool files_ok =
      dirs_ok &&
      WriteStringToFile(dir + "/test/test262/data/harness/helper.js",
                        "function helperValue() { return 42; }\n", &error) &&
      WriteStringToFile(dir + "/test/test262/data/test/harness/helper.js",
                        "throw new Error('wrong harness directory');\n",
                        &error) &&
      WriteStringToFile(dir + "/test/test262/data/harness/sta.js",
                        "throw new Error('sta.js should not be inlined');\n",
                        &error) &&
      WriteStringToFile(
          dir + "/test/test262/data/test/sub1/deep/keep_a.js",
          "/*---\nincludes: [helper.js, sta.js, assert.js]\n---*/\n"
          "assert.sameValue(helperValue(), 42);\n",
          &error) &&
      WriteStringToFile(
          dir + "/test/test262/data/test/sub1/deep/dep_FIXTURE.js",
          "var fixture = 1;\n", &error) &&
      WriteStringToFile(dir + "/test/test262/data/test/sub2/keep_b.js",
                        "var b = 2;\n", &error);

  const std::vector<std::string> all_js =
      ListFiles(dir + "/test/test262/data/test/**/*.js", ".js");
  const std::vector<std::string> sub1_js =
      ListFiles(dir + "/test/test262/data/test/sub1/**/keep_*.js", ".js");
  const std::vector<std::string> sub2_zero_depth =
      ListFiles(dir + "/test/test262/data/test/sub2/**/keep_*.js", ".js");
  const std::vector<std::string> non_recursive =
      ListFiles(dir + "/test/test262/data/test/sub1/*.js", ".js");
  const std::vector<std::string> prefix_glob =
      ListFiles(dir + "/test/test262/data/test/sub**/*.js", ".js");
  const std::vector<std::string> expected_all = {
      dir + "/test/test262/data/test/harness/helper.js",
      dir + "/test/test262/data/test/sub1/deep/keep_a.js",
      dir + "/test/test262/data/test/sub2/keep_b.js",
  };
  const std::vector<std::string> expected_sub = {
      dir + "/test/test262/data/test/sub1/deep/keep_a.js",
      dir + "/test/test262/data/test/sub2/keep_b.js",
  };
  const std::vector<std::string> expected_sub1 = {
      dir + "/test/test262/data/test/sub1/deep/keep_a.js",
  };
  const std::vector<std::string> expected_sub2 = {
      dir + "/test/test262/data/test/sub2/keep_b.js",
  };
  checker->Check(
      "import: ListFiles expands wildcards and skips *_FIXTURE.js",
      files_ok && all_js == expected_all && sub1_js == expected_sub1 &&
          sub2_zero_depth == expected_sub2 && non_recursive.empty() &&
          prefix_glob == expected_sub,
      StringPrintf("all=%zu, sub1=%zu, sub2=%zu, non_rec=%zu, prefix=%zu",
                   all_js.size(), sub1_js.size(), sub2_zero_depth.size(),
                   non_recursive.size(), prefix_glob.size()));

  ImportOptions options;
  options.root = dir;
  ImportInfo info;
  std::string src;
  bool read_ok = ReadFileToString(expected_sub1[0], &src, &error);
  std::unique_ptr<fast::Tree> tree =
      read_ok ? ImportSource(src, expected_sub1[0], options, &info, &error)
              : nullptr;
  std::string printed = tree ? fast::Print(tree->root()) : "";
  checker->Check(
      "import: ImportSource inlines Test262 includes and skips prelude files",
      tree != nullptr && info.inlined.size() == 1 && info.dropped.empty() &&
          printed.find("helperValue") != std::string::npos &&
          printed.find("sta.js") == std::string::npos,
      error);

  ImportInfo prose_info;
  std::string prose_err;
  std::unique_ptr<fast::Tree> prose_tree = ImportSource(
      "/*---\ninfo: |\n  negative: not a YAML key\n  flags: [module]\n"
      "includes:\n  - helper.js\n  - sta.js\n---*/\nvar ok = helperValue();\n",
      dir + "/test/test262/data/test/sub2/prose.js", options, &prose_info,
      &prose_err);
  std::string prose_printed = prose_tree ? fast::Print(prose_tree->root()) : "";
  checker->Check(
      "import: ImportSource ignores indented prose and parses block includes",
      prose_tree != nullptr && prose_info.inlined.size() == 1 &&
          prose_printed.find("helperValue") != std::string::npos,
      prose_err);

  ImportInfo neg_info, mod_info, mod_block_info, gen_info;
  std::string neg_err, mod_err, mod_block_err, gen_err;
  std::unique_ptr<fast::Tree> neg_tree = ImportSource(
      "/*---\nnegative:\n  phase: runtime\n  type: TypeError\n---*/\nx();\n",
      dir + "/neg.js", options, &neg_info, &neg_err);
  std::unique_ptr<fast::Tree> mod_tree =
      ImportSource("/*---\nflags: [module]\n---*/\nvar m = 1;\n",
                   dir + "/mod.js", options, &mod_info, &mod_err);
  std::unique_ptr<fast::Tree> mod_block_tree =
      ImportSource("/*---\nflags:\n  - module\n---*/\nvar m = 1;\n",
                   dir + "/mod2.js", options, &mod_block_info, &mod_block_err);
  std::unique_ptr<fast::Tree> gen_tree = ImportSource(
      "var g = 1;\n", dir + "/RegExp/property-escapes/generated/prop.js",
      options, &gen_info, &gen_err);
  checker->Check(
      "import: ImportSource skips Test262 negative, module and generated files",
      neg_tree == nullptr && mod_tree == nullptr && mod_block_tree == nullptr &&
          gen_tree == nullptr,
      "");
  fast::ParseError prelude_err;
  fast::ParseOptions prelude_opts;
  prelude_opts.allow_natives_syntax = true;
  std::unique_ptr<fast::Tree> prelude_tree =
      fast::Parser::ParseScript(DefaultPrelude(), prelude_opts, &prelude_err);
  checker->Check("import: DefaultPrelude parses cleanly",
                 prelude_tree != nullptr, prelude_err.ToString());
  RemoveTree(dir);
}

// Target: a session with --import-all and a seed strategy that wants hit
// edges. The check of an import uses import_reruns, not the large
// min_reruns of the session. An edge that the first run of a new target
// reaches only once can cause more reruns. Publish runs no program again
// for the hit edges.
void RunImportAllCheck(Checker* checker, const TargetOptions& target) {
  const std::vector<std::string> programs = {
      "var a = [1, 2, 3]; a.push(4); a.sort();\n",
      "var o = {x: 1}; o.y = 2; Object.keys(o);\n",
      "var s = 'abc'; s.split('').reverse().join('');\n",
  };
  const CrashSession session =
      RunCrashSession(target,
                      {"--import-all", "--min-reruns=20", "--scheduler=rarity",
                       "--stage-log=check"},
                      programs);
  std::string events, error = session.error;
  if (error.empty()) {
    ReadFileToString(session.dir + "/out/events.jsonl", &events, &error);
  }
  int added = 0, import_checks = 0, cheap_checks = 0;
  std::string reruns;
  int64_t hit_edge_executions = -1;
  size_t start = 0;
  while (start < events.size()) {
    size_t end = events.find('\n', start);
    if (end == std::string::npos) {
      end = events.size();
    }
    JsonValue event;
    std::string parse_error;
    if (JsonValue::Parse(std::string_view(events).substr(start, end - start),
                         &event, &parse_error) &&
        event.is_object()) {
      const std::string& kind = event.Get("event").as_string();
      if (kind == "import" && event.Get("result").as_string() == "added") {
        added++;
      } else if (kind == "check" &&
                 event.Get("origin").as_string() == "import") {
        import_checks++;
        const int64_t count = event.GetInt("reruns", -1);
        reruns += StringPrintf("%s%lld", reruns.empty() ? "" : " ",
                               static_cast<long long>(count));
        if (count >= 1 && count < 20 && event.GetBool("survived", false)) {
          cheap_checks++;
        }
      } else if (kind == "stats") {
        hit_edge_executions = event.GetInt("hit_edge_executions", -1);
      }
    }
    start = end + 1;
  }
  checker->Check(
      "import: --import-all uses import_reruns, and Publish runs no program "
      "again for the hit edges",
      error.empty() && added == static_cast<int>(programs.size()) &&
          import_checks == added && cheap_checks == added &&
          hit_edge_executions == 0,
      StringPrintf("%d added, %d import checks, %d with fewer than 20 reruns "
                   "(%s), %lld hit edge runs, stop \"%s\"%s%s",
                   added, import_checks, cheap_checks, reruns.c_str(),
                   static_cast<long long>(hit_edge_executions),
                   session.stop.c_str(), error.empty() ? "" : ": ",
                   error.c_str()));
  if (!session.dir.empty()) {
    RemoveTree(session.dir);
  }
}

// Offline: exception line extraction, AST statement salvage and local type
// propagation.
void RunSalvageChecks(Checker* checker) {
  // 1. ExtractExceptionLines on stdout, stderr, stack traces, prelude offset
  // and parse-time SyntaxError.
  {
    Execution simple;
    simple.outcome = Outcome::kFailed;
    simple.stdout_output =
        "fuzzcode.js:12: TypeError: Cannot read properties of null\n"
        "null.x;\n"
        "     ^\n"
        "TypeError: Cannot read properties of null\n"
        "    at fuzzcode.js:12:6\n";
    ExceptionLines l1 = ExtractExceptionLines(simple, 0);
    checker->Check(
        "salvage: extracts top-level TypeError line from stdout",
        l1.innermost == 12 && l1.outermost == 12,
        StringPrintf("inner=%d outer=%d", l1.innermost, l1.outermost));

    Execution multi;
    multi.outcome = Outcome::kFailed;
    multi.stdout_output =
        "TypeError: earlier printed stack\n"
        "    at fuzzcode.js:99:1\n"
        "fuzzcode.js:110: TypeError: boom\n"
        "  null.x;\n"
        "       ^\n"
        "TypeError: boom\n"
        "    at g (fuzzcode.js:110:8)\n"
        "    at f (fuzzcode.js:113:3)\n"
        "    at fuzzcode.js:118:1\n";
    ExceptionLines l2 = ExtractExceptionLines(multi, 108);
    checker->Check(
        "salvage: extracts innermost and outermost lines with prelude offset",
        l2.innermost == 2 && l2.outermost == 10,
        StringPrintf("inner=%d outer=%d", l2.innermost, l2.outermost));

    Execution prelude_frame;
    prelude_frame.outcome = Outcome::kFailed;
    prelude_frame.stdout_output =
        "fuzzcode.js:64: TypeError: helper failed\n"
        "    at isInterpreted (fuzzcode.js:64:30)\n"
        "    at fuzzcode.js:115:1\n";
    ExceptionLines l3 = ExtractExceptionLines(prelude_frame, 108);
    checker->Check(
        "salvage: skips frames inside the prelude",
        l3.innermost == 7 && l3.outermost == 7,
        StringPrintf("inner=%d outer=%d", l3.innermost, l3.outermost));

    Execution stderr_fallback;
    stderr_fallback.outcome = Outcome::kFailed;
    stderr_fallback.stderr_output = "fuzzcode.js:9: 42\nthrow 42;\n^\n";
    ExceptionLines l4 = ExtractExceptionLines(stderr_fallback, 4);
    checker->Check(
        "salvage: falls back to stderr and header-only throws",
        l4.innermost == 5 && l4.outermost == 5,
        StringPrintf("inner=%d outer=%d", l4.innermost, l4.outermost));

    Execution parse_err;
    parse_err.outcome = Outcome::kFailed;
    parse_err.stdout_output =
        "fuzzcode.js:3: SyntaxError: Unexpected token ';'\n"
        "let = ;\n"
        "      ^\n"
        "SyntaxError: Unexpected token ';'\n";
    ExceptionLines l5 = ExtractExceptionLines(parse_err, 0);
    checker->Check(
        "salvage: skips unrecoverable parse-time SyntaxError", l5.empty(),
        StringPrintf("inner=%d outer=%d", l5.innermost, l5.outermost));

    Execution runtime_syntax;
    runtime_syntax.outcome = Outcome::kFailed;
    runtime_syntax.stdout_output =
        "fuzzcode.js:4: SyntaxError: Invalid regular expression\n"
        "new RegExp('[');\n"
        "^\n"
        "SyntaxError: Invalid regular expression\n"
        "    at new RegExp (<anonymous>)\n"
        "    at fuzzcode.js:4:1\n";
    ExceptionLines l6 = ExtractExceptionLines(runtime_syntax, 0);
    checker->Check(
        "salvage: keeps runtime SyntaxError with stack frames",
        l6.innermost == 4 && l6.outermost == 4,
        StringPrintf("inner=%d outer=%d", l6.innermost, l6.outermost));

    Execution crlf;
    crlf.outcome = Outcome::kFailed;
    crlf.stdout_output =
        "fuzzcode.js:12: TypeError: boom\r\n"
        "null.x;\r\n"
        "     ^\r\n"
        "TypeError: boom\r\n"
        "    at g (fuzzcode.js:12:6)\r\n"
        "    at fuzzcode.js:15:1\r\n";
    ExceptionLines l7 = ExtractExceptionLines(crlf, 2);
    checker->Check(
        "salvage: handles CRLF line endings in exception output",
        l7.innermost == 10 && l7.outermost == 13,
        StringPrintf("inner=%d outer=%d", l7.innermost, l7.outermost));

    Execution succeeded_with_text = simple;
    succeeded_with_text.outcome = Outcome::kSucceeded;
    Execution crashed_with_text = simple;
    crashed_with_text.outcome = Outcome::kCrashed;
    Execution timed_out_with_text = simple;
    timed_out_with_text.outcome = Outcome::kTimedOut;
    checker->Check(
        "salvage: ignores non-kFailed executions in ExtractExceptionLines",
        ExtractExceptionLines(succeeded_with_text, 0).empty() &&
            ExtractExceptionLines(crashed_with_text, 0).empty() &&
            ExtractExceptionLines(timed_out_with_text, 0).empty(),
        "");

    checker->Check(
        "salvage: PreludeLineCount handles empty and trailing newline",
        PreludeLineCount("") == 0 && PreludeLineCount("var a = 1;") == 1 &&
            PreludeLineCount("var a = 1;\n") == 2,
        StringPrintf("empty=%d one=%d trailing=%d", PreludeLineCount(""),
                     PreludeLineCount("var a = 1;"),
                     PreludeLineCount("var a = 1;\n")));
  }

  // 2. NodeSpan line-to-statement lookup (innermost and top-level).
  {
    const char* src =
        "function boom() {\n"
        "  let x = 1;\n"
        "  null.y;\n"
        "}\n"
        "if (true) {\n"
        "  boom();\n"
        "}\n";
    fast::ParseError err;
    std::unique_ptr<fast::Tree> tree =
        fast::Parser::ParseScript(src, fast::ParseOptions(), &err);
    std::vector<fast::NodeSpan> spans;
    std::string printed =
        tree != nullptr
            ? fast::Print(tree->root(), fast::PrintOptions(), &spans)
            : "";
    StatementLocation inner =
        FindCoveringStatement(tree.get(), printed, spans, 3, false);
    StatementLocation top =
        FindCoveringStatement(tree.get(), printed, spans, 6, true);
    StatementLocation nested_call =
        FindCoveringStatement(tree.get(), printed, spans, 6, false);
    const bool ok = inner.valid() &&
                    inner.statement()->Is<fast::ExpressionStatement>() &&
                    inner.owner != tree->root() && top.valid() &&
                    top.statement()->Is<fast::IfStatement>() &&
                    top.owner == tree->root() && nested_call.valid() &&
                    nested_call.statement()->Is<fast::ExpressionStatement>() &&
                    nested_call.owner != tree->root();
    checker->Check("salvage: NodeSpan maps line to innermost and top-level", ok,
                   err.message);
  }

  // 3. Lexical declaration splitting and skipping unsafe statements.
  {
    auto parse = [](const char* code) {
      fast::ParseError err;
      return fast::Parser::ParseScript(code, fast::ParseOptions(), &err);
    };
    std::unique_ptr<fast::Tree> const_tree =
        parse("const x = null.a;\nlet y = x;\n");
    const bool split_const = SalvageException(const_tree.get(), {1, 1}, false);
    const std::string printed_const =
        const_tree != nullptr ? fast::Print(const_tree->root()) : "";
    fast::ParseError parse_err;
    const bool const_parses =
        fast::Parser::ParseScript(printed_const, fast::ParseOptions(),
                                  &parse_err) != nullptr;
    checker->Check(
        "salvage: splits const declaration into let and guarded assignment",
        split_const && const_parses &&
            printed_const.find("let x;\ntry {\n  x = null.a;\n} catch {}") !=
                std::string::npos,
        printed_const);

    std::unique_ptr<fast::Tree> multi_tree = parse("let a = 1, b = null.z;\n");
    const bool split_multi = SalvageException(multi_tree.get(), {1, 1}, false);
    const std::string printed_multi =
        multi_tree != nullptr ? fast::Print(multi_tree->root()) : "";
    checker->Check(
        "salvage: splits multi-declarator let declaration safely",
        split_multi &&
            printed_multi.find(
                "let a, b;\ntry {\n  a = 1;\n  b = null.z;\n} catch {}") !=
                std::string::npos,
        printed_multi);

    std::unique_ptr<fast::Tree> var_tree =
        parse("var x = null.a;\nlet y = x;\n");
    const bool wrapped_var =
        var_tree != nullptr && SalvageException(var_tree.get(), {1, 1}, false);
    const std::string printed_var =
        var_tree != nullptr ? fast::Print(var_tree->root()) : "";
    const bool var_parses =
        fast::Parser::ParseScript(printed_var, fast::ParseOptions(),
                                  &parse_err) != nullptr;
    checker->Check(
        "salvage: wraps var declaration directly in try/catch",
        wrapped_var && var_parses &&
            printed_var.find("try {\n  var x = null.a;\n} catch {}") !=
                std::string::npos,
        printed_var);

    std::unique_ptr<fast::Tree> try_tree = parse("try {\n  a();\n} catch {}\n");
    const bool skipped_try =
        try_tree != nullptr && !SalvageException(try_tree.get(), {1, 1}, false);
    std::unique_ptr<fast::Tree> fn_tree = parse("function f() {}\n");
    const bool skipped_fn =
        fn_tree != nullptr && !SalvageException(fn_tree.get(), {1, 1}, false);
    std::unique_ptr<fast::Tree> cls_tree = parse("class C extends null {}\n");
    const bool skipped_cls =
        cls_tree != nullptr && !SalvageException(cls_tree.get(), {1, 1}, false);
    std::unique_ptr<fast::Tree> destruct_tree = parse("const {a} = null;\n");
    const bool skipped_destruct =
        destruct_tree != nullptr &&
        !SalvageException(destruct_tree.get(), {1, 1}, false);
    checker->Check(
        "salvage: skips try, function, class and destructuring declarations",
        skipped_try && skipped_fn && skipped_cls && skipped_destruct, "");

    std::unique_ptr<fast::Tree> using_tree =
        parse("{\n  using r = null;\n  let y = r;\n}\n");
    bool skipped_using = false;
    if (using_tree != nullptr) {
      std::vector<fast::NodeSpan> using_spans;
      std::string printed_using =
          fast::Print(using_tree->root(), fast::PrintOptions(), &using_spans);
      StatementLocation using_loc = FindCoveringStatement(
          using_tree.get(), printed_using, using_spans, 2, false);
      skipped_using = using_loc.valid() &&
                      using_loc.statement()->Is<fast::VariableDeclaration>() &&
                      !WrapStatementForSalvage(using_tree.get(), using_loc);
    }
    std::unique_ptr<fast::Tree> directive_tree = parse("\"use strict\";\n");
    const bool skipped_directive =
        directive_tree != nullptr &&
        !SalvageException(directive_tree.get(), {1, 1}, false);
    std::unique_ptr<fast::Tree> nested_label_fn_tree =
        parse("L1: L2: function f() {}\n");
    const bool skipped_nested_label_fn =
        nested_label_fn_tree != nullptr &&
        !SalvageException(nested_label_fn_tree.get(), {1, 1}, false);
    std::unique_ptr<fast::Tree> labeled_ret_tree =
        parse("function f() {\n  L: return null.x;\n}\nf();\n");
    bool skipped_labeled_ret = false;
    bool labeled_ret_fell_back = false;
    std::string printed_labeled_ret;
    if (labeled_ret_tree != nullptr) {
      std::vector<fast::NodeSpan> ret_spans;
      std::string printed_ret = fast::Print(labeled_ret_tree->root(),
                                            fast::PrintOptions(), &ret_spans);
      StatementLocation ret_loc = FindCoveringStatement(
          labeled_ret_tree.get(), printed_ret, ret_spans, 2, false);
      skipped_labeled_ret =
          ret_loc.valid() &&
          ret_loc.statement()->Is<fast::LabeledStatement>() &&
          !WrapStatementForSalvage(labeled_ret_tree.get(), ret_loc);
      labeled_ret_fell_back =
          SalvageException(labeled_ret_tree.get(), {2, 4}, false);
      printed_labeled_ret = fast::Print(labeled_ret_tree->root());
    }
    checker->Check(
        "salvage: skips using, directives and unsafe labeled statements",
        skipped_using && skipped_directive && skipped_nested_label_fn &&
            skipped_labeled_ret && labeled_ret_fell_back &&
            printed_labeled_ret.find("try {\n  f();\n} catch {}") !=
                std::string::npos,
        StringPrintf("using=%d dir=%d lfn=%d lret=%d fb=%d", skipped_using,
                     skipped_directive, skipped_nested_label_fn,
                     skipped_labeled_ret, labeled_ret_fell_back));

    std::unique_ptr<fast::Tree> labeled_ok_tree = parse("L: null.x;\n");
    const bool wrapped_labeled =
        labeled_ok_tree != nullptr &&
        SalvageException(labeled_ok_tree.get(), {1, 1}, false);
    const std::string printed_labeled =
        labeled_ok_tree != nullptr ? fast::Print(labeled_ok_tree->root()) : "";
    const bool labeled_parses =
        fast::Parser::ParseScript(printed_labeled, fast::ParseOptions(),
                                  &parse_err) != nullptr;
    checker->Check(
        "salvage: wraps safe labeled statement outside the label",
        wrapped_labeled && labeled_parses &&
            printed_labeled.find("try {\n  L: null.x;\n} catch {}") !=
                std::string::npos,
        printed_labeled);

    std::unique_ptr<fast::Tree> fallback_tree =
        parse("function f() {\n  return null.x;\n}\nf();\n");
    const bool fell_back = fallback_tree != nullptr &&
                           SalvageException(fallback_tree.get(), {2, 4}, false);
    const std::string printed_fb =
        fallback_tree != nullptr ? fast::Print(fallback_tree->root()) : "";
    checker->Check(
        "salvage: falls back from ReturnStatement to outermost call statement",
        fell_back &&
            printed_fb.find("try {\n  f();\n} catch {}") != std::string::npos,
        printed_fb);
  }

  // 4. Local type propagation in Builder::GuessType.
  {
    const char* code =
        "let ta;\n"
        "try {\n"
        "  ta = new Int32Array(4);\n"
        "} catch {}\n"
        "let alias = ta;\n"
        "let len = alias.length;\n"
        "let sum = len + 1;\n"
        "let isArr = Array.isArray(alias);\n"
        "async function af() { return 1; }\n"
        "let p = af();\n"
        "function makeArr() { return [1, 2]; }\n"
        "let arr = makeArr();\n"
        "const makeStr = () => 'hi';\n"
        "let str = makeStr();\n"
        "function recFn() { return recFn(); }\n"
        "let rec = recFn();\n"
        "let conflict;\n"
        "conflict = 1;\n"
        "conflict = 's';\n"
        "function fn(n = 5, ...rest) {\n"
        "  for (let k in alias) {\n"
        "    return k;\n"
        "  }\n"
        "}\n"
        "let cycA = cycB;\n"
        "let cycB = cycA;\n";
    fast::ParseError err;
    std::unique_ptr<fast::Tree> tree =
        fast::Parser::ParseScript(code, fast::ParseOptions(), &err);
    if (tree == nullptr) {
      checker->Check(
          "builder: GuessType propagates guarded assignments and local types",
          false, "parse failed: " + err.ToString());
    } else {
      Random rng(1);
      Builder builder(tree.get(), &rng);
      auto type_of = [&](std::string_view name) {
        for (fast::Binding* b : builder.scopes().bindings()) {
          if (b->name == name) {
            return builder.GuessType(b);
          }
        }
        return JsType::kUnknown;
      };
      const bool ok = type_of("ta") == JsType::kTypedArray &&
                      type_of("alias") == JsType::kTypedArray &&
                      type_of("len") == JsType::kNumber &&
                      type_of("sum") == JsType::kNumber &&
                      type_of("isArr") == JsType::kBoolean &&
                      type_of("p") == JsType::kPromise &&
                      type_of("arr") == JsType::kArray &&
                      type_of("str") == JsType::kString &&
                      type_of("rec") == JsType::kUnknown &&
                      type_of("conflict") == JsType::kUnknown &&
                      type_of("n") == JsType::kNumber &&
                      type_of("rest") == JsType::kArray &&
                      type_of("k") == JsType::kString &&
                      type_of("cycA") == JsType::kUnknown &&
                      type_of("cycB") == JsType::kUnknown;
      checker->Check(
          "builder: GuessType propagates guarded assignments and local types",
          ok,
          StringPrintf("ta=%s alias=%s len=%s sum=%s isArr=%s p=%s arr=%s "
                       "str=%s rec=%s conflict=%s n=%s rest=%s k=%s",
                       JsTypeName(type_of("ta")), JsTypeName(type_of("alias")),
                       JsTypeName(type_of("len")), JsTypeName(type_of("sum")),
                       JsTypeName(type_of("isArr")), JsTypeName(type_of("p")),
                       JsTypeName(type_of("arr")), JsTypeName(type_of("str")),
                       JsTypeName(type_of("rec")),
                       JsTypeName(type_of("conflict")),
                       JsTypeName(type_of("n")), JsTypeName(type_of("rest")),
                       JsTypeName(type_of("k"))));
    }
  }
}

// Online: end-to-end exception salvage against d8 behind DefaultPrelude.
void RunSalvageTargetCheck(Checker* checker, ScriptRunner* runner) {
  const std::string prelude(DefaultPrelude());
  const int prelude_lines = PreludeLineCount(prelude);
  auto run_behind_prelude = [&](const std::string& src,
                                ScriptRunner::Result* out) {
    std::string full = prelude + "\n" + src;
    return RunOrFail(checker, runner, "salvage target", full, out);
  };

  // Case 1: throwing const declaration splits and keeps downstream code alive.
  {
    const char* src =
        "const v = null.foo;\n"
        "print('after:' + typeof v);\n";
    fast::ParseError err;
    std::unique_ptr<fast::Tree> tree =
        fast::Parser::ParseScript(src, fast::ParseOptions(), &err);
    if (tree == nullptr) {
      checker->Check(
          "salvage: recovers throwing const declaration end-to-end on d8",
          false, "parse failed: " + err.ToString());
    } else {
      std::string initial = fast::Print(tree->root());
      ScriptRunner::Result first, second;
      if (run_behind_prelude(initial, &first)) {
        ExceptionLines lines =
            ExtractExceptionLines(first.execution, prelude_lines);
        const bool wrapped = SalvageException(tree.get(), lines, false);
        const std::string salvaged = fast::Print(tree->root());
        const bool ran_second =
            wrapped && run_behind_prelude(salvaged, &second);
        const bool ok = first.execution.outcome == Outcome::kFailed &&
                        lines.innermost == 1 && lines.outermost == 1 &&
                        wrapped && ran_second && second.execution.succeeded() &&
                        second.execution.stdout_output.find(
                            "after:undefined") != std::string::npos;
        checker->Check(
            "salvage: recovers throwing const declaration end-to-end on d8", ok,
            StringPrintf("first=%s lines=%d/%d second=%s",
                         OutcomeName(first.execution.outcome), lines.innermost,
                         lines.outermost,
                         OutcomeName(second.execution.outcome)));
      }
    }
  }

  // Case 2: nested throw recovers in both innermost and outermost modes.
  {
    const char* src =
        "function boom() {\n"
        "  null.bar();\n"
        "}\n"
        "boom();\n"
        "print('recovered');\n";
    for (bool top_level : {false, true}) {
      const char* check_name =
          top_level
              ? "salvage: recovers nested throw via outermost frame on d8"
              : "salvage: recovers nested throw via innermost frame on d8";
      fast::ParseError err;
      std::unique_ptr<fast::Tree> tree =
          fast::Parser::ParseScript(src, fast::ParseOptions(), &err);
      if (tree == nullptr) {
        checker->Check(check_name, false, "parse failed: " + err.ToString());
        continue;
      }
      std::string initial = fast::Print(tree->root());
      ScriptRunner::Result first, second;
      if (run_behind_prelude(initial, &first)) {
        ExceptionLines lines =
            ExtractExceptionLines(first.execution, prelude_lines);
        const bool wrapped = SalvageException(tree.get(), lines, top_level);
        const std::string salvaged = fast::Print(tree->root());
        const bool ran_second =
            wrapped && run_behind_prelude(salvaged, &second);
        const bool ok = first.execution.outcome == Outcome::kFailed &&
                        lines.innermost == 2 && lines.outermost == 4 &&
                        wrapped && ran_second && second.execution.succeeded() &&
                        second.execution.stdout_output.find("recovered") !=
                            std::string::npos;
        checker->Check(check_name, ok,
                       StringPrintf("first=%s lines=%d/%d second=%s",
                                    OutcomeName(first.execution.outcome),
                                    lines.innermost, lines.outermost,
                                    OutcomeName(second.execution.outcome)));
      }
    }
  }
}

// Offline: the on-demand prelude (engine/prelude.h). The checks cover the
// statements that a program gets and the search for names in its text. They
// also cover the fallback rules and the preludes that Build() does not split.
void RunOnDemandPreludeChecks(Checker* checker) {
  const std::string prelude(DefaultPrelude());
  std::string error;
  const std::unique_ptr<OnDemandPrelude> on_demand =
      OnDemandPrelude::Build(prelude, /*fallback_rules=*/true, &error);
  if (!checker->Check("prelude: Build splits the default prelude",
                      on_demand != nullptr, error)) {
    return;
  }
  // Pin the split of the default prelude. A change of the default prelude
  // makes this check fail. After such a change, run the
  // whole-against-on-demand comparison on a corpus again.
  {
    const std::string trimmed = prelude.substr(prelude.find_first_not_of('\n'));
    const std::string padded = " \n" + trimmed + " \n";
    std::string trimmed_error;
    std::string padded_error;
    const std::unique_ptr<OnDemandPrelude> from_bundle = OnDemandPrelude::Build(
        trimmed, /*fallback_rules=*/true, &trimmed_error);
    const std::unique_ptr<OnDemandPrelude> from_padded =
        OnDemandPrelude::Build(padded, /*fallback_rules=*/true, &padded_error);
    checker->Check(
        "prelude: the default prelude splits into 79 statements with 2 in "
        "front of each program",
        on_demand->statements() == 79 && on_demand->always_kept() == 2 &&
            trimmed != prelude && from_bundle != nullptr &&
            from_bundle->statements() == 79 &&
            from_bundle->always_kept() == 2 && from_padded != nullptr &&
            from_padded->statements() == 79 && from_padded->always_kept() == 2,
        StringPrintf(
            "%zu statements, %zu always kept%s%s%s%s", on_demand->statements(),
            on_demand->always_kept(),
            trimmed_error.empty() ? "" : ", bundle: ", trimmed_error.c_str(),
            padded_error.empty() ? "" : ", padded: ", padded_error.c_str()));
  }
  auto text_for = [&](std::string_view program) {
    return on_demand->Text(on_demand->Select(program));
  };
  auto has = [](const std::string& text, std::string_view needle) {
    return text.find(needle) != std::string::npos;
  };

  // 1. The statements for a program.
  {
    const OnDemandPrelude::Selection none =
        on_demand->Select("let v = 1;\nprint(v);\n");
    const std::string text = on_demand->Text(none);
    checker->Check(
        "prelude: a program without a prelude name gets only the statements "
        "that declare no name, and their names",
        none.fallback == PreludeFallback::kNone &&
            none.count == on_demand->always_kept() &&
            none.count < on_demand->statements() &&
            has(text, "var __redline_noop =") && has(text, "d8.file.execute") &&
            !has(text, "assert") && !has(text, "Test262Error") &&
            !has(text, "promiseTestChain") && !has(text, "$262") &&
            none.bytes == text.size() && none.lines == PreludeLineCount(text),
        StringPrintf("%zu of %zu statements, %zu bytes, %d lines", none.count,
                     on_demand->statements(), none.bytes, none.lines));

    const std::string throws = text_for("assertThrows(() => { null.x; });\n");
    checker->Check(
        "prelude: a name gets its statement and the statements that it uses, "
        "in the order of the prelude",
        has(throws, "var assertThrows =") &&
            has(throws, "var __redline_call") &&
            throws.find("var __redline_call") <
                throws.find("var assertThrows =") &&
            !has(throws, "var assertEquals ") &&
            !has(throws, "var assertThrowsEquals"),
        StringPrintf("%zu bytes", throws.size()));

    const std::string assert_text = text_for("assert.sameValue(1, 1);\n");
    const std::string compare = text_for("compareArray([1], [1]);\n");
    checker->Check(
        "prelude: a statement that extends a function of the prelude belongs "
        "to the name of the function",
        has(assert_text, "var assert = ") &&
            has(assert_text, "assert.sameValue = ") &&
            has(assert_text, "assert.compareArray = compareArray;") &&
            has(assert_text, "var compareArray = ") &&
            has(assert_text, "compareArray.format = prettyPrinted;") &&
            has(assert_text, "var prettyPrinted = ") &&
            has(assert_text, "var formatIdentityFreeValue = ") &&
            has(assert_text, "var isPrimitive = ") &&
            has(assert_text, "var __redline_call = ") &&
            !has(assert_text, "Test262Error") &&
            !has(assert_text, "var assertEquals ") &&
            has(compare, "var compareArray = ") &&
            has(compare, "compareArray.isSameValue = Object.is;") &&
            !has(compare, "var assert = ") && !has(compare, "assert."),
        StringPrintf("assert %zu bytes, compareArray %zu bytes",
                     assert_text.size(), compare.size()));

    const std::string realm = text_for("$262.createRealm();\n");
    const std::string error_text = text_for("throw new Test262Error('x');\n");
    const std::string chain = text_for("promiseTestChain.then(() => {});\n");
    checker->Check(
        "prelude: `$262`, `promiseTestChain` and `Test262Error` get their "
        "statements",
        has(realm, "var $262 = ") && has(realm, "var __redline_realm = ") &&
            has(realm, "var __redline_call = ") &&
            has(chain, "var promiseTestChain = ") && !has(chain, "var $262") &&
            has(error_text, "var Test262Error = ") &&
            has(error_text, "Test262Error.prototype.toString = ") &&
            has(error_text, "Test262Error.thrower = __redline_noop;") &&
            !has(error_text, "var $262"),
        StringPrintf("$262 %zu bytes, Test262Error %zu bytes", realm.size(),
                     error_text.size()));
  }

  // 2. The search for names in the text.
  {
    auto gets_fail = [&](std::string_view program) {
      return has(text_for(program), "var fail = ");
    };
    const bool in_string = gets_fail("f(\"fail\");\n");
    const bool in_comment = gets_fail("// fail\nlet v = 1;\n");
    const bool identifier_escape = gets_fail("f\\u0061il();\n");
    const bool braces_escape = gets_fail("f(\"\\u{66}ail()\");\n");
    const bool hex_escape = gets_fail("f(\"\\x66ail()\");\n");
    const bool octal_escape = gets_fail("f(\"\\146ail()\");\n");
    const bool after_new_line = gets_fail("f(\"g();\\nfail()\");\n");
    const bool continued_line = gets_fail("f(\"fa\\\nil()\");\n");
    const bool same_character = gets_fail("f(\"\\fail \\quit\");\n");
    const bool other_names =
        gets_fail("failed(); xfail(); fail2(); f(\"\\x66ailed\");\n");
    // A backslash at the end of a line comment does not continue the line.
    // The next line is code.
    auto gets_equals = [&](std::string_view program) {
      return has(text_for(program), "var assertEquals = ");
    };
    const bool after_comment =
        gets_equals("// C:\\temp\\\nassertEquals(1, 2);\n") &&
        gets_equals("// C:\\temp\\\r\nassertEquals(1, 2);\r\n") &&
        gets_equals("<!-- foo\\\nassertEquals(1, 2);\n") &&
        gets_equals("// x\\\n// y\\\nassertEquals(1, 2);\n") &&
        on_demand->Select("// x\\\neval('1');\n").fallback ==
            PreludeFallback::kEval;
    checker->Check(
        "prelude: the search finds a name in a string and in a comment, and "
        "decodes escape sequences",
        in_string && in_comment && identifier_escape && braces_escape &&
            hex_escape && octal_escape && after_new_line && continued_line &&
            !same_character && !other_names && after_comment &&
            has(text_for("f(\"\\fail \\quit\");\n"), "var quit = "),
        StringPrintf("string %d, comment %d, \\u %d, \\u{} %d, \\x %d, octal "
                     "%d, after \\n %d, continued line %d, \\f %d, other "
                     "names %d, after a comment %d",
                     in_string, in_comment, identifier_escape, braces_escape,
                     hex_escape, octal_escape, after_new_line, continued_line,
                     same_character, other_names, after_comment));
  }

  // 3. The fallback rules.
  {
    auto rule = [&](std::string_view program) {
      return on_demand->Select(program).fallback;
    };
    const OnDemandPrelude::Selection whole = on_demand->Select("eval('1');\n");
    const bool whole_text = whole.fallback == PreludeFallback::kEval &&
                            on_demand->Text(whole) == prelude &&
                            whole.count == on_demand->statements() &&
                            whole.bytes == prelude.size() &&
                            whole.lines == PreludeLineCount(prelude);
    const bool tokens =
        rule("new Function('return 1')();\n") == PreludeFallback::kFunction &&
        rule("globalThis.v = 1;\n") == PreludeFallback::kGlobalThis &&
        rule("Realm.create();\n") == PreludeFallback::kRealm &&
        rule("f(\"ev\\x61l\");\n") == PreludeFallback::kEval;
    const bool this_value =
        rule("var g = this;\n") == PreludeFallback::kThis &&
        rule("this[k] = 1;\n") == PreludeFallback::kThis &&
        rule("for (var k in this) {}\n") == PreludeFallback::kThis &&
        rule("Object.keys(this);\n") == PreludeFallback::kThis &&
        rule("f(\"return this\");\n") == PreludeFallback::kThis &&
        rule("this?.[k];\n") == PreludeFallback::kThis &&
        rule("this ?.5 : 1;\n") == PreludeFallback::kThis;
    const bool this_property =
        rule("this.v = 1;\nfunction C() { this\n  .w = this.v; }\n") ==
            PreludeFallback::kNone &&
        rule("(this?.constructor || Object); this ?. v; this?.#p;\n") ==
            PreludeFallback::kNone &&
        rule("let thisValue = 1; evaluate(); Functions();\n") ==
            PreludeFallback::kNone;
    std::string no_rules_error;
    const std::unique_ptr<OnDemandPrelude> no_rules = OnDemandPrelude::Build(
        prelude, /*fallback_rules=*/false, &no_rules_error);
    const bool rules_off =
        no_rules != nullptr &&
        no_rules->Select("eval('1'); var g = this;\n").fallback ==
            PreludeFallback::kNone &&
        no_rules->Select("eval('1'); var g = this;\n").count ==
            no_rules->always_kept();
    // The text of the prelude has each name of the prelude.
    const bool all_names = no_rules != nullptr &&
                           no_rules->Text(no_rules->Select(prelude)) == prelude;
    checker->Check(
        "prelude: the fallback rules give the whole prelude to a program "
        "with `eval`, `Function`, `globalThis`, `Realm` or `this` as a value",
        whole_text && tokens && this_value && this_property && rules_off &&
            all_names,
        StringPrintf("whole text %d, tokens %d, this as a value %d, "
                     "this.name %d, rules off %d, all names %d",
                     whole_text, tokens, this_value, this_property, rules_off,
                     all_names));
  }

  // 4. Each selection parses as the prelude of ScriptTree, and
  // ScriptWithPrelude builds the expected scripts.
  {
    bool trees = true;
    for (const char* program : {"let v = 1;\n", "\"use strict\";\nlet v = 1;\n",
                                "assertEquals(1, 2);\nfail();\n",
                                "assert.throws(TypeError, () => {});\n",
                                "$262.gc();\n", "eval('1');\n"}) {
      const OnDemandPrelude::Selection selection = on_demand->Select(program);
      std::string parse_error;
      const std::unique_ptr<ScriptTree> script =
          ScriptTree::Parse(on_demand->Text(selection), program, &parse_error);
      trees = trees && script != nullptr &&
              script->prelude_statements() == selection.count;
    }
    const std::string strict = "\"use strict\";\nlet v = 1;\n";
    OnDemandPrelude::Selection strict_selection;
    const std::string with_part =
        ScriptWithPrelude(prelude, on_demand.get(), strict, &strict_selection);
    const bool scripts =
        strict_selection.count == on_demand->always_kept() &&
        with_part == on_demand->Text(strict_selection) + "\n" + strict &&
        ScriptWithPrelude(prelude, nullptr, strict) ==
            prelude + "\n" + strict &&
        ScriptWithPrelude("", nullptr, strict) == strict;
    checker->Check(
        "prelude: each selection parses in front of its program and "
        "ScriptWithPrelude puts the selected part",
        trees && scripts,
        StringPrintf("trees %d, scripts %d, count %zu", trees, scripts,
                     strict_selection.count));
  }

  // 5. Each prelude other than the default prelude fails Build() and stays
  // whole for each program.
  {
    const std::vector<std::string> custom_preludes = {
        "var a = 1;\nvar b = 2;\n",
        "var a = 1;\nvar b = (a = 5);\n",
        "function install() { Array.prototype.last = function() { return "
        "this[this.length - 1]; }; return true; }\nvar installed = "
        "install();\n",
        "class C { static { globalThis.inst = new this(); } }\n"
        "C.prototype.hello = function() { return 42; };\n",
        "var C = class { static { globalThis.inst = new this(); } };\n"
        "C.prototype.hello = function() { return 42; };\n",
        "var C = class Inner { static { globalThis.inst = new Inner(); } };\n"
        "C.prototype.hello = function() { return 42; };\n",
        "function make() { globalThis.getA = function() { return "
        "globalThis.a; }; return 1; }\nvar a = make();\nvar a = 42;\n",
        "var o = { set a(v) { globalThis.ran = v; } };\no.a = 42;\n",
        "var proto = { set a(v) { globalThis.ran = v; } };\n"
        "var o = { __proto__: proto };\no.a = 42;\n",
        "class C { static set a(v) { globalThis.ran = v; } }\nC.a = 42;\n",
        "class C { set a(v) { globalThis.ran = v; } }\n"
        "C.prototype.a = 42;\n",
        "async function af() {}\naf.prototype.x = 1;\n",
        "var af = async function() {};\naf.prototype.x = 1;\n",
        "'not a directive';\nvar a = 1;\n",
        "var d = { [Symbol.dispose]() {} };\nusing x = d;\n",
        "'use strict';\ng = 5;\n",
        "Object.defineProperty(globalThis, 'x', { set(v) { globalThis.ran = "
        "v; } });\nx = 42;\n",
        "Object.defineProperty(Object.prototype, 'a', { set(v) { "
        "globalThis.ran = v; } });\nvar o = {};\no.a = 42;\n",
        prelude + "var extra = 1;\n",
        prelude + "// extra\n",
        "var a = ;\n",
        "// nothing\n",
        "",
    };
    TunableRegistry& registry = TunableRegistry::Instance();
    std::string tunable_error;
    const LogLevel saved_level = GetLogLevel();
    SetLogLevel(LogLevel::kError);
    size_t refused = 0;
    std::string first_failed;
    const std::string program = "print(1);\n";
    bool set_mode = true;
    for (int mode : {1, 2}) {
      set_mode =
          registry.Set("prelude.on_demand", mode, &tunable_error) && set_mode;
      for (const std::string& custom : custom_preludes) {
        std::string why;
        const std::unique_ptr<OnDemandPrelude> built =
            OnDemandPrelude::Build(custom, /*fallback_rules=*/mode == 1, &why);
        const std::unique_ptr<OnDemandPrelude> made =
            MakeOnDemandPrelude(custom);
        const std::string script =
            ScriptWithPrelude(custom, made.get(), program);
        const std::string expected =
            custom.empty() ? program : custom + "\n" + program;
        if (built == nullptr && !why.empty() && made == nullptr &&
            script == expected) {
          refused++;
        } else if (first_failed.empty()) {
          first_failed = custom.substr(0, 40);
        }
      }
    }
    SetLogLevel(saved_level);
    registry.Reset();
    const size_t total_checks = custom_preludes.size() * 2;
    checker->Check(
        "prelude: Build and MakeOnDemandPrelude refuse each custom prelude "
        "and keep it whole",
        set_mode && refused == total_checks,
        StringPrintf("%zu of %zu refused%s%s", refused, total_checks,
                     first_failed.empty() ? "" : ", first failed: ",
                     first_failed.c_str()));
  }

  // The time of Select() for one program. A session calls it for each
  // execution.
  {
    std::string program;
    for (int i = 0; i < 100; i++) {
      program += StringPrintf(
          "function f%d(a, b) { return a.x + b[%d] * this.y; }\n"
          "let v%d = f%d({x: %d}, [1, 2, 3]);\n",
          i, i % 3, i, i, i);
    }
    const std::string with_names = program + "assertEquals(v1, v2);\n";
    const int rounds = 2000;
    size_t statements = 0;
    const Stopwatch clock;
    for (int i = 0; i < rounds; i++) {
      statements += on_demand->Select(i % 2 == 0 ? program : with_names).count;
    }
    checker->Info(
        "prelude: time of Select",
        StringPrintf("%.2f us for a program of %zu bytes (%zu statements in "
                     "%d calls)",
                     static_cast<double>(clock.ElapsedUs()) / rounds,
                     program.size(), statements, rounds));
  }

  // 6. The text of a helper in front of a program, and the tunable.
  {
    std::string helper_error;
    const std::unique_ptr<OnDemandPrelude> with_helper =
        OnDemandPrelude::Build(prelude, /*fallback_rules=*/true, &helper_error);
    const std::string helper =
        "const helper = (function() { return Function.prototype; })();\n"
        "// quit\n";
    bool helpers = with_helper != nullptr;
    if (helpers) {
      with_helper->AddHelper(helper);
      const OnDemandPrelude::Selection plain =
          with_helper->Select(helper + "let v = 1;\n");
      helpers = plain.fallback == PreludeFallback::kNone &&
                plain.count == with_helper->always_kept() + 1 &&
                has(with_helper->Text(plain), "var quit = ") &&
                with_helper->Select(helper + "eval('1');\n").fallback ==
                    PreludeFallback::kEval &&
                with_helper->Select("let v = 1;\n" + helper).fallback ==
                    PreludeFallback::kFunction;
    }
    TunableRegistry& registry = TunableRegistry::Instance();
    const Tunable* tunable = registry.Find("prelude.on_demand");
    std::string tunable_error;
    bool tunables = tunable != nullptr && tunable->is_default() &&
                    (*tunable)() == 0 &&
                    MakeOnDemandPrelude(prelude) == nullptr;
    // The runtimes of the runtime-assisted mutators, the runtimes that
    // MakeOnDemandPrelude() registered, and the runtimes that get the whole
    // prelude without the registration.
    int runtimes = 0;
    int registered = 0;
    int with_rule = 0;
    if (tunables) {
      std::string bound_error;
      const bool rejects_bounds =
          !registry.Set("prelude.on_demand", -1, &bound_error) &&
          !registry.Set("prelude.on_demand", 3, &bound_error);
      tunables = rejects_bounds &&
                 registry.Set("prelude.on_demand", 2, &tunable_error);
      const std::unique_ptr<OnDemandPrelude> mode2 =
          MakeOnDemandPrelude(prelude);
      tunables =
          tunables && mode2 != nullptr &&
          mode2->Select("eval('1');\n").fallback == PreludeFallback::kNone &&
          MakeOnDemandPrelude("") == nullptr;
      tunables =
          registry.Set("prelude.on_demand", 1, &tunable_error) && tunables;
      const std::unique_ptr<OnDemandPrelude> mode1 =
          MakeOnDemandPrelude(prelude);
      tunables =
          tunables && mode1 != nullptr &&
          mode1->Select("eval('1');\n").fallback == PreludeFallback::kEval &&
          mode1->fallback_rules() && !mode2->fallback_rules();
      const std::unique_ptr<MutatorSet> mutators = MutatorSet::CreateDefault();
      for (const std::unique_ptr<Mutator>& mutator : mutators->all()) {
        if (!mutator->runtime_assisted() || mode1 == nullptr) {
          continue;
        }
        const std::string instrumented =
            std::string(static_cast<const RuntimeAssistedMutator&>(*mutator)
                            .runtime()) +
            "let v = 1;\n";
        runtimes++;
        if (mode1->Select(instrumented).fallback == PreludeFallback::kNone) {
          registered++;
        }
        if (on_demand->Select(instrumented).fallback !=
            PreludeFallback::kNone) {
          with_rule++;
        }
      }
      registry.Reset();
      tunables = tunables && (*tunable)() == 0;
    }
    checker->Check(
        "prelude: the fallback rules skip a helper in front of the program, "
        "and `prelude.on_demand` is off by default",
        helpers && tunables,
        StringPrintf("helpers %d, tunables %d%s%s", helpers, tunables,
                     tunable_error.empty() ? "" : ": ", tunable_error.c_str()));
    checker->Check(
        "prelude: MakeOnDemandPrelude registers the runtime of each "
        "runtime-assisted mutator",
        runtimes > 0 && registered == runtimes && with_rule > 0,
        StringPrintf("%d runtimes, %d registered, %d with a fallback rule "
                     "without the registration",
                     runtimes, registered, with_rule));
  }
}

// Online: a program behind its part of the prelude gives the same result as
// behind the whole prelude. The line of an exception maps to the program.
void RunOnDemandPreludeTargetCheck(Checker* checker, ScriptRunner* runner) {
  const std::string prelude(DefaultPrelude());
  std::string error;
  const std::unique_ptr<OnDemandPrelude> on_demand =
      OnDemandPrelude::Build(prelude, /*fallback_rules=*/true, &error);
  if (on_demand == nullptr) {
    checker->Check("prelude: on-demand scripts run like whole scripts on d8",
                   false, error);
    return;
  }
  int same = 0;
  int total = 0;
  std::string differs;
  for (const char* program :
       {"print(typeof assertEquals, typeof fail, typeof $262);\n",
        "assertEquals(1, 2);\nassertThrows(() => { null.x; });\n"
        "assertThrows('null.x');\nprint('ran');\n",
        "print(assert.sameValue(1, 1), assert.compareArray([1], [1]));\n"
        "assert.throws(TypeError, () => { null.x; });\n",
        "print(String(new Test262Error('m')), typeof Test262Error.thrower);\n",
        "print(typeof $262.global, typeof $262.createRealm().evalScript);\n",
        "\"use strict\";\nundeclared = 1;\nprint(typeof undeclared);\n",
        "load('x.js');\nquit();\nprint(typeof d8.file.execute('x.js'));\n",
        "print(isNeverOptimize(), prettyPrinted(1), deepEquals(1, 1));\n",
        "let unknown = null;\nunknown.x;\n"}) {
    const std::string whole = ScriptWithPrelude(prelude, nullptr, program);
    const std::string part =
        ScriptWithPrelude(prelude, on_demand.get(), program);
    ScriptRunner::Result first, second;
    if (!RunOrFail(checker, runner, "on-demand prelude target", whole,
                   &first) ||
        !RunOrFail(checker, runner, "on-demand prelude target", part,
                   &second)) {
      return;
    }
    total++;
    if (first.execution.outcome == second.execution.outcome &&
        first.execution.ExceptionSignature() ==
            second.execution.ExceptionSignature() &&
        (first.execution.outcome != Outcome::kSucceeded ||
         first.execution.stdout_output == second.execution.stdout_output) &&
        part.size() < whole.size()) {
      same++;
    } else if (differs.empty()) {
      differs = program;
    }
  }
  checker->Check(
      "prelude: on-demand scripts run like whole scripts on d8", same == total,
      StringPrintf(
          "%d of %d programs%s%s", same, total,
          differs.empty() ? "" : ", first difference: ", differs.c_str()));

  const std::string throws = "let a = 1;\nnull.x;\n";
  OnDemandPrelude::Selection selection;
  const std::string script =
      ScriptWithPrelude(prelude, on_demand.get(), throws, &selection);
  ScriptRunner::Result result;
  if (RunOrFail(checker, runner, "on-demand prelude target", script, &result)) {
    const ExceptionLines lines =
        ExtractExceptionLines(result.execution, selection.lines);
    checker->Check(
        "prelude: the line of an exception maps to the program behind an "
        "on-demand prelude",
        result.execution.outcome == Outcome::kFailed &&
            selection.lines < PreludeLineCount(prelude) &&
            lines.innermost == 2 && lines.outermost == 2,
        StringPrintf("%s, %d prelude lines, lines %d/%d",
                     OutcomeName(result.execution.outcome), selection.lines,
                     lines.innermost, lines.outermost));
  }
}

// Offline: the `// Flags:` lines of imported tests and the denylist of the
// js_fuzzer of V8. Also the d8 trials, the flag pools and the flag blacklist.
// Also the second start of a target process without its random flags.
void RunTestFlagChecks(Checker* checker) {
  auto contains = [](const std::vector<std::string>& list, const char* flag) {
    return std::find(list.begin(), list.end(), flag) != list.end();
  };

  const std::vector<std::string> lines = ParseFlagsLines(
      "// Copyright\n// Flags: --turbofan --no-lazy_feedback_allocation\n"
      "var x = 1;  //   Flags: --maglev\n//Flags: --not-a-flags-line\n");
  checker->Check(
      "test flags: ParseFlagsLines reads all `// Flags:` lines",
      lines == std::vector<std::string>{"--turbofan",
                                        "--no-lazy_feedback_allocation",
                                        "--maglev"},
      StringPrintf("%zu flags", lines.size()));

  ImportOptions options;
  ImportInfo info;
  std::string error;
  std::unique_ptr<fast::Tree> tree = ImportSource(
      "// Flags: --allow-natives-syntax --expose_gc --turbofan --harmony-foo\n"
      "// Flags: --js-staging --stack-size=100 --trace-turbo foo.mjs\n"
      "// Flags: --no-lazy_feedback_allocation --random-seed=4 --verify-heap\n"
      "// Flags: --experimental-wasm-foo --no-turbofan --print-bytecode\n"
      "var a = 1;\n",
      "/tmp/flags.js", options, &info, &error);
  checker->Check(
      "test flags: ImportSource keeps the allowed flags of `// Flags:`",
      tree != nullptr &&
          info.flags == std::vector<std::string>{"--turbofan", "--js-staging",
                                                 "--no-lazy-feedback-"
                                                 "allocation"} &&
          info.rejected_flags.size() == 10,
      StringPrintf("%zu flags, %zu rejected", info.flags.size(),
                   info.rejected_flags.size()));

  const bool denylist_ok =
      IsAllowedTestFlag("--harmony-shipping") &&
      !IsAllowedTestFlag("--harmony-temporal") &&
      IsAllowedTestFlag("--js-shipping") &&
      !IsAllowedTestFlag("--js-float16") &&
      !IsAllowedTestFlag("--icu-data-file=x") &&
      !IsAllowedTestFlag("--sandbox-fuzzing") &&
      !IsAllowedTestFlag("--no-allow-natives-syntax") &&
      !IsAllowedTestFlag("--noexpose-gc") && !IsAllowedTestFlag("--") &&
      !IsAllowedTestFlag("--single-threaded") && !IsAllowedTestFlag("--help") &&
      !IsAllowedTestFlag("--perf-prof") && !IsAllowedTestFlag("--prof") &&
      !IsAllowedTestFlag("--log-all") && !IsAllowedTestFlag("--gdbjit") &&
      !IsAllowedTestFlag("--abort-on-contradictory-flags") &&
      IsAllowedTestFlag("--maglev") && IsAllowedTestFlag("--no-turbofan") &&
      IsAllowedTestFlag("--harmony");
  checker->Check("test flags: the denylist of exceptions.js and Redline",
                 denylist_ok, "");

  std::vector<std::string> contradictory = {"--jitless", "--maglev",
                                            "--no-jitless", "--stress-maglev"};
  SanitizeD8Flags(&contradictory);
  std::vector<std::string> fixed_wins = {"--jitless", "--turbofan"};
  SanitizeD8Flags(&fixed_wins, {"--maglev"});
  checker->Check(
      "test flags: SanitizeD8Flags removes negations and contradictions",
      contradictory == std::vector<std::string>{"--jitless"} &&
          fixed_wins == std::vector<std::string>{"--turbofan"},
      StringPrintf("%zu, %zu flags", contradictory.size(), fixed_wins.size()));

  const std::vector<std::string> merged = MergeProgramFlags(
      {"--a", "--b=1", "--c", "--d"}, {"--no-a", "--b=2", "--e", "--f", "--g"});
  checker->Check(
      "test flags: MergeProgramFlags keeps 6 flags with unique names",
      merged == std::vector<std::string>{"--a", "--b=1", "--c", "--d", "--e",
                                         "--f"} &&
          SharesD8Flag({"--x=1"}, {"--y", "--x=2"}) &&
          !SharesD8Flag({"--x"}, {"--no-x"}),
      StringPrintf("%zu flags", merged.size()));

  std::vector<D8Trial> trials;
  size_t dropped = 0;
  std::string trial_err;
  const bool trials_ok = ParseD8Trials(
      R"([{"app_args": "--trial-a", "app_name": "d8", "probability": 1,
           "contradicts": ["--trial-b"]},
          {"app_args": "--trial-b", "app_name": "d8", "probability": 1,
           "contradicts": ["--trial-a"]},
          {"app_args": "--experimental-x", "app_name": "d8", "probability": 1},
          {"app_args": "--trial-x", "app_name": "chrome", "probability": 1},
          {"app_args": "--trial-c --trial-d", "app_name": "d8",
           "probability": 0.5}])",
      &trials, &trial_err, &dropped);
  std::vector<D8Trial> bad_trials;
  std::string bad_err;
  const bool bad_rejected =
      !ParseD8Trials("{\"app_args\": 1}", &bad_trials, &bad_err);
  Random trial_rng(17);
  int one_of_ab = 0;
  int cd = 0;
  int fixed_b_ok = 0;
  for (int i = 0; i < 200; i++) {
    std::vector<std::string> drawn;
    AddD8Trials(trial_rng, trials, {}, &drawn);
    if (contains(drawn, "--trial-a") != contains(drawn, "--trial-b")) {
      one_of_ab++;
    }
    if (contains(drawn, "--trial-c") && contains(drawn, "--trial-d")) {
      cd++;
    }
    std::vector<std::string> with_fixed;
    AddD8Trials(trial_rng, trials, {"--trial-b"}, &with_fixed);
    if (!contains(with_fixed, "--trial-a") &&
        !contains(with_fixed, "--trial-b")) {
      fixed_b_ok++;
    }
  }
  checker->Check(
      "test flags: d8 trials load, filter and honor `contradicts`",
      trials_ok && trials.size() == 3 && dropped == 1 && bad_rejected &&
          one_of_ab == 200 && cd > 60 && cd < 140 && fixed_b_ok == 200,
      StringPrintf("%zu trials, %zu dropped, a xor b %d, c and d %d, fixed %d",
                   trials.size(), dropped, one_of_ab, cd, fixed_b_ok) +
          trial_err);

  FlagSetPool pool(2);
  pool.Record({"--pool-a"}, 1.0);
  pool.Record({"--pool-b"}, 2.0);
  pool.Record({"--pool-c"}, 3.0);
  pool.Record({"--pool-c"}, 0.5);
  pool.Record({}, 9.0);
  pool.Record({"--pool-d"}, 0.0);
  Random pool_rng(3);
  std::vector<std::string> best;
  int best_c = 0;
  int seen_b = 0;
  for (int i = 0; i < 100; i++) {
    if (pool.Pick(pool_rng, 32, &best) &&
        best == std::vector<std::string>{"--pool-c"}) {
      best_c++;
    }
    if (pool.Pick(pool_rng, 1, &best) &&
        best == std::vector<std::string>{"--pool-b"}) {
      seen_b++;
    }
  }
  FlagSetPool empty_pool(4);
  checker->Check(
      "test flags: FlagSetPool keeps the best sets",
      pool.size() == 2 && best_c == 100 && seen_b > 20 && seen_b < 80 &&
          !empty_pool.Pick(pool_rng, 1, &best),
      StringPrintf("size %zu, best %d, b %d", pool.size(), best_c, seen_b));

  D8FlagBlacklist single;
  single.RecordSpawn({"--bad-flag=1"}, false);
  std::vector<std::string> filtered = {"--bad-flag=2", "--ok-flag"};
  single.Filter(&filtered);
  D8FlagBlacklist early;
  early.RecordSpawn({"--x", "--y"}, false);
  D8FlagBlacklist isolated;
  D8FlagBlacklist repeated;
  for (int i = 0; i < 16; i++) {
    isolated.RecordSpawn({"--good-a", "--good-b"}, true);
    repeated.RecordSpawn({"--good-a"}, true);
  }
  isolated.RecordSpawn({"--good-a", "--new-flag", "--good-b"}, false);
  repeated.RecordSpawn({"--good-a", "--p", "--q"}, false);
  const bool repeated_first = repeated.Blocked().empty();
  repeated.RecordSpawn({"--good-a", "--p", "--r"}, false);
  D8FlagBlacklist boundary;
  for (int i = 0; i < 3; i++) {
    boundary.RecordSpawn({"--good-a"}, true);
  }
  boundary.RecordSpawn({"--good-a", "--late"}, false);
  const bool boundary_first = boundary.Blocked().empty();
  boundary.RecordSpawn({"--good-a"}, true);
  boundary.RecordSpawn({"--good-a", "--later"}, false);
  D8FlagBlacklist no_starts;
  no_starts.RecordSpawn({"--bad", "--c1"}, false);
  no_starts.RecordSpawn({"--bad", "--c2"}, false);
  D8FlagBlacklist tie;
  for (int i = 0; i < 16; i++) {
    tie.RecordSpawn({"--good-a"}, true);
  }
  tie.RecordSpawn({"--good-a", "--s", "--t"}, false);
  tie.RecordSpawn({"--good-a", "--s", "--t"}, false);
  checker->Check(
      "test flags: D8FlagBlacklist blocks the flags that stop d8",
      single.IsBlocked("--bad-flag") &&
          filtered == std::vector<std::string>{"--ok-flag"} &&
          early.Blocked().empty() &&
          isolated.Blocked() == std::vector<std::string>{"--new-flag"} &&
          repeated_first &&
          repeated.Blocked() == std::vector<std::string>{"--p"} &&
          repeated.failed_spawns() == 2 && tie.Blocked().empty() &&
          no_starts.Blocked() == std::vector<std::string>{"--bad"} &&
          boundary_first &&
          boundary.Blocked() == std::vector<std::string>{"--later"},
      "");

  // The flag pools of normal mode and the blacklist in DrawD8Flags.
  TunableRegistry& tunable_reg = TunableRegistry::Instance();
  std::string tunable_err;
  const bool set_ok =
      tunable_reg.Set("flags.imported_chance", 1.0, &tunable_err) &&
      tunable_reg.Set("flags.target_keep_chance", 1.0, &tunable_err);
  FuzzConfig cfg;
  cfg.randomize_d8_flags = true;
  Fuzzer fuzzer(std::move(cfg));
  fuzzer.RecordImportedFlags({"--imported-sentinel", "--blocked-sentinel"});
  fuzzer.RecordSpawn({"--blocked-sentinel"}, false);
  fuzzer.RecordProductiveFlags({"--productive-a"}, 10, 1000);
  const size_t productive_first = fuzzer.productive_flag_sets();
  fuzzer.RecordProductiveFlags({"--productive-b"}, 100, 1000);
  fuzzer.RecordProductiveFlags({"--productive-c"}, 0, 1000);
  Random draw_rng(11);
  int imported = 0;
  int blocked = 0;
  for (int i = 0; i < 50; i++) {
    const std::vector<std::string> drawn = fuzzer.DrawD8Flags(draw_rng);
    imported += contains(drawn, "--imported-sentinel") ? 1 : 0;
    blocked += contains(drawn, "--blocked-sentinel") ? 1 : 0;
  }
  tunable_reg.Set("flags.imported_chance", 0.0, &tunable_err);
  tunable_reg.Set("flags.affinity_chance", 1.0, &tunable_err);
  int productive = 0;
  for (int i = 0; i < 50; i++) {
    productive +=
        contains(fuzzer.DrawD8Flags(draw_rng), "--productive-b") ? 1 : 0;
  }
  tunable_reg.Reset();
  checker->Check(
      "test flags: DrawD8Flags uses the imported and productive flag sets",
      set_ok && imported == 50 && blocked == 0 && productive_first == 0 &&
          fuzzer.productive_flag_sets() == 1 && productive == 50,
      StringPrintf("imported %d, blocked %d, productive sets %zu then %zu, "
                   "productive %d",
                   imported, blocked, productive_first,
                   fuzzer.productive_flag_sets(), productive));

  // A fake target: /bin/bash does the REPRL handshake and answers each
  // execution with the status 0. Use /bin/bash instead of /bin/sh because
  // dash rejects multi-digit file descriptor redirections (>&101, <&100).
  // An unknown option stops /bin/bash before the handshake. The file
  // `fail_file` also stops it.
  const std::string fail_file =
      StringPrintf("/tmp/redline-spawn-fail.%d", static_cast<int>(getpid()));
  unlink(fail_file.c_str());
  ReprlChild::Options reprl;
  reprl.executable = "/bin/bash";
  reprl.args = {"-c",
                "[ -e \"$FAIL_FILE\" ] && exit 3; "
                "printf HELO >&101; head -c 4 <&100 >/dev/null; "
                "while head -c 12 <&100 >/dev/null; do "
                "printf '\\000\\000\\000\\000' >&101; done"};
  reprl.capture_stdout = false;
  reprl.capture_stderr = false;
  reprl.env = {{"PATH", "/usr/bin:/bin"}, {"FAIL_FILE", fail_file}};
  reprl.startup_timeout_us = 10 * 1000 * 1000;
  std::vector<std::string> next_args = {"--bad-sh-option"};
  std::vector<std::pair<std::vector<std::string>, bool>> results;
  reprl.random_args = [&next_args] { return next_args; };
  reprl.spawn_result = [&results](const std::vector<std::string>& args,
                                  bool started) {
    results.emplace_back(args, started);
  };
  std::string reprl_err;
  std::unique_ptr<ReprlChild> child =
      ReprlChild::Create(std::move(reprl), &reprl_err);
  Execution execution;
  const bool first_ok =
      child != nullptr &&
      child->Execute("1", 1000 * 1000, &execution, &reprl_err) &&
      execution.succeeded() && child->random_args().empty();
  bool second_ok = false;
  if (first_ok) {
    next_args = {"-e"};
    child->Kill();
    second_ok = child->Execute("1", 1000 * 1000, &execution, &reprl_err) &&
                execution.succeeded() &&
                child->random_args() == std::vector<std::string>{"-e"};
  }
  checker->Check(
      "test flags: a target that stops on its random flags starts again "
      "without them",
      first_ok && second_ok && results.size() == 2 &&
          results[0].first == std::vector<std::string>{"--bad-sh-option"} &&
          !results[0].second && results[1].second,
      reprl_err);

  // A pinned child keeps its flags when a new start fails. The failure is
  // not a flag failure.
  bool pinned_ok = false;
  if (second_ok) {
    child->Pin(true);
    child->Kill();
    std::string write_err;
    const bool failed_start =
        WriteStringToFile(fail_file, "", &write_err) &&
        !child->Execute("1", 1000 * 1000, &execution, &reprl_err);
    unlink(fail_file.c_str());
    pinned_ok = failed_start &&
                child->random_args() == std::vector<std::string>{"-e"} &&
                child->Execute("1", 1000 * 1000, &execution, &reprl_err) &&
                execution.succeeded() &&
                child->random_args() == std::vector<std::string>{"-e"} &&
                results.size() == 2;
  }
  checker->Check(
      "test flags: a pinned target keeps its random flags after a failed start",
      pinned_ok, StringPrintf("%zu spawn results", results.size()));

  // A script above `ReprlChild::kMaxDataSize` fails before spawning a child.
  {
    ReprlChild::Options oversized_opt;
    oversized_opt.executable = "/bin/sh";
    oversized_opt.args = {"-c", "exit 99"};
    oversized_opt.capture_stdout = false;
    oversized_opt.capture_stderr = false;
    std::string oversized_err;
    std::unique_ptr<ReprlChild> oversized_child =
        ReprlChild::Create(std::move(oversized_opt), &oversized_err);
    Execution oversized_exec;
    std::string huge_script(ReprlChild::kMaxDataSize + 1, ' ');
    const bool rejected =
        oversized_child != nullptr &&
        !oversized_child->Execute(huge_script, 1000 * 1000, &oversized_exec,
                                  &oversized_err) &&
        oversized_err.find("Script too large") != std::string::npos &&
        oversized_child->total_spawns() == 0;
    checker->Check("reprl: oversized script fails without spawning a child",
                   rejected, oversized_err);
  }
}

// Tests the compact donor window, the limits of Fuse and Combine and the
// partner cache. `syntax` is nullptr if V8 did not initialize.
void RunMaterialCompactChecks(Checker* checker, SyntaxChecker* syntax) {
  auto v8_accepts = [&](const std::string& source, std::string* message) {
    return syntax != nullptr && syntax->Accepts(source, message);
  };

  // 7. `ExtractCompactWindow` keeps large donors below `max_donor_nodes`.
  {
    std::string large_src = "function helper_used(v) { return v + 1; }\n";
    for (int i = 0; i < 160; i++) {
      large_src += StringPrintf("let unused_%d = %d + %d;\n", i, i, i + 1);
    }
    large_src += "let out_val = helper_used(41);\n";
    std::string iife_src = "(function() { \"use strict\";\n";
    for (int i = 0; i < 150; i++) {
      iife_src += StringPrintf("let inner_%d = %d + 1;\n", i, i);
    }
    iife_src += "return inner_0; })();\n";

    std::string parse_err;
    ProgramPtr large_prog = Program::Parse(large_src, Lineage{}, &parse_err);
    ProgramPtr iife_prog = Program::Parse(iife_src, Lineage{}, &parse_err);
    Random window_rng(17);
    std::unique_ptr<fast::Tree> copy1 =
        large_prog != nullptr ? large_prog->CopyTree() : nullptr;
    std::unique_ptr<fast::Tree> copy2 =
        iife_prog != nullptr ? iife_prog->CopyTree() : nullptr;
    const bool ok1 =
        copy1 != nullptr &&
        ExtractCompactWindow(copy1.get(), MaterialMaxDonorNodes(), window_rng);
    const bool ok2 =
        copy2 != nullptr &&
        ExtractCompactWindow(copy2.get(), MaterialMaxDonorNodes(), window_rng);
    const size_t n1 = copy1 != nullptr ? fast::CountNodes(copy1->root()) : 0;
    const size_t n2 = copy2 != nullptr ? fast::CountNodes(copy2->root()) : 0;
    std::string printed1 = ok1 ? fast::Print(copy1->root()) : "";
    std::string printed2 = ok2 ? fast::Print(copy2->root()) : "";
    fast::ParseError re1, re2;
    std::string msg1, msg2;
    const bool valid1 = ok1 && n1 <= MaterialMaxDonorNodes() &&
                        fast::Parser::ParseScript(
                            printed1, fast::ParseOptions(), &re1) != nullptr &&
                        v8_accepts(printed1, &msg1);
    const bool valid2 = ok2 && n2 <= MaterialMaxDonorNodes() &&
                        fast::Parser::ParseScript(
                            printed2, fast::ParseOptions(), &re2) != nullptr &&
                        v8_accepts(printed2, &msg2);
    checker->Check(
        "material: ExtractCompactWindow extracts a valid bounded window",
        large_prog != nullptr && large_prog->node_count() > 600 &&
            iife_prog != nullptr && iife_prog->node_count() > 600 && valid1 &&
            valid2,
        StringPrintf("n1=%zu valid1=%d (%s) n2=%zu valid2=%d (%s)", n1, valid1,
                     msg1.c_str(), n2, valid2, msg2.c_str()));

    ProgramPtr small_prog =
        Program::Parse("let ok = 1;\n", Lineage{}, &parse_err);
    ProgramPtr cycle_prog = Program::Parse(
        "function cyc_a() { return cyc_b(); }\n"
        "function cyc_b() { return cyc_a(); }\n"
        "let big_a = cyc_a();\n"
        "let extra_0 = 1;\nlet extra_1 = 2;\nlet extra_2 = 3;\n",
        Lineage{}, &parse_err);
    ProgramPtr dir_only =
        Program::Parse("\"use strict\";\n", Lineage{}, &parse_err);
    auto small_copy = small_prog->CopyTree();
    auto cycle_copy = cycle_prog->CopyTree();
    auto dir_copy = dir_only->CopyTree();
    const bool small_kept =
        ExtractCompactWindow(small_copy.get(), 300, window_rng) &&
        fast::CountNodes(small_copy->root()) == small_prog->node_count();
    const bool cycle_ok =
        ExtractCompactWindow(cycle_copy.get(), 30, window_rng) &&
        fast::CountNodes(cycle_copy->root()) <= 30;
    const bool dir_empty = !ExtractCompactWindow(dir_copy.get(), 5, window_rng);
    checker->Check(
        "material: ExtractCompactWindow handles small, cyclic and "
        "directive-only trees",
        small_kept && cycle_ok && dir_empty,
        StringPrintf("small=%d cycle=%d dir_empty=%d", small_kept, cycle_ok,
                     dir_empty));
  }

  // 8. `FusePrograms` respects `max_fuse_nodes` and emits cross-donor glue.
  {
    std::string big_a = "function alpha_fn(x) { return x + 1; }\n";
    std::string big_b = "class BetaCls { compute(y) { return y * 2; } }\n";
    for (int i = 0; i < 180; i++) {
      big_a += StringPrintf("let fill_a_%d = %d + 2;\n", i, i);
      big_b += StringPrintf("let fill_b_%d = %d + 3;\n", i, i);
    }
    big_a += "let res_a = alpha_fn(5);\n";
    big_b += "let res_b = new BetaCls();\n";
    std::string parse_err;
    ProgramPtr pa = Program::Parse(big_a, Lineage{}, &parse_err);
    ProgramPtr pb = Program::Parse(big_b, Lineage{}, &parse_err);
    std::shared_ptr<Program> mut_pb =
        Program::Parse(big_b, Lineage{}, &parse_err);
    if (mut_pb != nullptr) {
      mut_pb->flags = {"--maglev"};
    }
    Random fuse_rng(23);
    ProgramPtr big_fused = FusePrograms({pa, pb}, "cap_check", &fuse_rng);
    std::string v8_big_msg;
    const bool cap_ok = big_fused != nullptr &&
                        big_fused->node_count() <= MaterialMaxFuseNodes() &&
                        v8_accepts(big_fused->source(), &v8_big_msg);

    // Lexical splits expand nodes by more than 2x. Verify that the first
    // donor obeys `max_fuse_nodes` and excluded donors drop their flags.
    std::string split_decls;
    for (int i = 0; i < 32; i++) {
      split_decls += StringPrintf("let [a_%d, b_%d, c_%d] = x;\n", i, i, i);
    }
    std::string multi_decl = "let m0 = 0";
    for (int i = 1; i < 96; i++) {
      multi_decl += StringPrintf(", m%d = %d", i, i);
    }
    multi_decl += ";\n";
    ProgramPtr split_prog = Program::Parse(split_decls, Lineage{}, &parse_err);
    ProgramPtr multi_prog = Program::Parse(multi_decl, Lineage{}, &parse_err);
    ProgramPtr split_fused =
        FusePrograms({split_prog, mut_pb}, "split_cap", &fuse_rng);
    ProgramPtr multi_fused = FusePrograms({multi_prog}, "multi_cap", &fuse_rng);
    std::string split_v8_msg, multi_v8_msg;
    const bool split_ok = split_prog != nullptr &&
                          split_prog->node_count() <= 300 &&
                          split_fused != nullptr &&
                          split_fused->node_count() <= MaterialMaxFuseNodes() &&
                          v8_accepts(split_fused->source(), &split_v8_msg);
    const bool multi_ok = multi_prog != nullptr &&
                          multi_prog->node_count() <= 300 &&
                          multi_fused != nullptr &&
                          multi_fused->node_count() <= MaterialMaxFuseNodes() &&
                          v8_accepts(multi_fused->source(), &multi_v8_msg);

    TunableRegistry& tun_reg = TunableRegistry::Instance();
    std::string tun_err;
    tun_reg.Set("material.max_fuse_nodes", 200, &tun_err);
    std::string tight_a;
    for (int i = 0; i < 45; i++) {
      tight_a += StringPrintf("let ta_%d = %d;\n", i, i);
    }
    std::shared_ptr<Program> tight_pa =
        Program::Parse(tight_a, Lineage{}, &parse_err);
    if (tight_pa != nullptr) {
      tight_pa->flags = {"--turbofan"};
    }
    ProgramPtr tight_fused =
        FusePrograms({tight_pa, mut_pb}, "tight_cap", &fuse_rng);
    const bool tight_ok =
        tight_fused != nullptr && tight_fused->node_count() <= 200 &&
        tight_fused->flags == std::vector<std::string>{"--turbofan"};
    tun_reg.Reset();

    ProgramPtr ga = Program::Parse("const donor_alpha = x => x + 1;\n",
                                   Lineage{}, &parse_err);
    ProgramPtr gb = Program::Parse(
        "function donor_beta(y) { return y * 2; }\n"
        "let val_beta = {tag: 7};\n",
        Lineage{}, &parse_err);
    Random glue_rng(31);
    ProgramPtr glue_fused = FusePrograms({ga, gb}, "glue_check", &glue_rng);
    bool has_cross_try = false;
    if (glue_fused != nullptr) {
      for (const fast::Statement* s : glue_fused->tree().root()->body) {
        if (!s->Is<fast::TryStatement>()) {
          continue;
        }
        bool sees_a = false;
        bool sees_b = false;
        fast::Walk(const_cast<fast::Statement*>(s), [&](fast::Node* n) {
          if (auto* id = n->As<fast::Identifier>()) {
            if (id->name == "donor_alpha") {
              sees_a = true;
            }
            if (id->name == "donor_beta" || id->name == "val_beta") {
              sees_b = true;
            }
          }
          return true;
        });
        if (sees_a && sees_b) {
          has_cross_try = true;
          break;
        }
      }
    }
    std::string v8_glue_msg;
    const bool glue_ok = glue_fused != nullptr && has_cross_try &&
                         v8_accepts(glue_fused->source(), &v8_glue_msg);
    checker->Check(
        "material: FusePrograms caps AST nodes and emits cross-donor glue",
        cap_ok && split_ok && multi_ok && tight_ok && glue_ok,
        !cap_ok ? v8_big_msg
                : (!split_ok
                       ? split_v8_msg
                       : (!multi_ok
                              ? multi_v8_msg
                              : (!tight_ok ? "tight max_fuse_nodes failed"
                                           : (has_cross_try
                                                  ? v8_glue_msg
                                                  : (glue_fused != nullptr
                                                         ? glue_fused->source()
                                                         : parse_err))))));
  }

  // 9. `Combine` caps oversized partner pasting and `Splice` caches partners.
  {
    std::string parse_err;
    ProgramPtr parent =
        Program::Parse("function host(v) { return v + 1; }\nlet h = host(2);\n",
                       Lineage{}, &parse_err);
    std::string partner_src = "function pfn(x) { return x * 2; }\n";
    for (int i = 0; i < 160; i++) {
      partner_src += StringPrintf("let pvar_%d = %d + 1;\n", i, i);
    }
    partner_src += "let pres = pfn(3);\n";
    ProgramPtr partner = Program::Parse(partner_src, Lineage{}, &parse_err);
    auto mutators = MutatorSet::CreateDefault();
    auto generators = GeneratorSet::CreateDefault();
    Mutator* combine = mutators->Find("Combine");
    Mutator* splice = mutators->Find("Splice");
    Random mut_rng(99);
    std::string detail;
    std::unique_ptr<fast::Tree> combined =
        (parent != nullptr && partner != nullptr && combine != nullptr)
            ? ApplyMutation(*combine, *parent, partner, mut_rng,
                            Environment::Default(), *generators, 1, &detail)
            : nullptr;
    std::unique_ptr<fast::Tree> spliced =
        (parent != nullptr && partner != nullptr && splice != nullptr)
            ? ApplyMutation(*splice, *parent, partner, mut_rng,
                            Environment::Default(), *generators, 7, &detail)
            : nullptr;
    const size_t comb_nodes =
        combined != nullptr ? fast::CountNodes(combined->root()) : 0;
    const size_t comb_limit = MaterialMaxCombineNodes() +
                              (parent != nullptr ? parent->node_count() : 0) +
                              16;
    std::string comb_msg, spl_msg;
    const bool comb_ok = combined != nullptr && comb_nodes <= comb_limit &&
                         v8_accepts(fast::Print(combined->root()), &comb_msg);
    const bool spl_ok = spliced != nullptr &&
                        v8_accepts(fast::Print(spliced->root()), &spl_msg);
    bool cache_ok = false;
    if (parent != nullptr && partner != nullptr && splice != nullptr) {
      auto parent_copy = parent->CopyTree();
      Random cache_rng(13);
      Builder b(parent_copy.get(), &cache_rng);
      MutationContext ctx;
      ctx.partner = partner;
      ctx.env = &Environment::Default();
      ctx.generators = generators.get();
      splice->Mutate(b, ctx);
      const void* first_cache = ctx.partner_cache.get();
      splice->Mutate(b, ctx);
      const bool reused =
          first_cache != nullptr && ctx.partner_cache.get() == first_cache;
      ctx.partner = parent;
      splice->Mutate(b, ctx);
      const bool invalidated = ctx.partner_cache != nullptr &&
                               ctx.partner_cache.get() != first_cache;
      cache_ok = reused && invalidated;
    }
    checker->Check(
        "material: Combine caps large partners and multi-round Splice succeeds",
        partner != nullptr && partner->node_count() > 600 && comb_ok &&
            spl_ok && cache_ok,
        StringPrintf("comb=%zu/%zu (%s) spl=%s (%zu nodes) cache=%d",
                     comb_nodes, comb_limit, comb_msg.c_str(), spl_msg.c_str(),
                     spliced != nullptr ? fast::CountNodes(spliced->root()) : 0,
                     cache_ok));
  }
}

void RunMaterialChecks(Checker* checker) {
  std::string v8_err;
  // V8 initializes only once per process. Use the default fuzz flags so that
  // later checks that need `--allow-natives-syntax` still work.
  const bool v8_ok = InitializeV8(FuzzConfig().v8_flags, &v8_err);
  checker->Check("material: initialize V8 syntax checker", v8_ok, v8_err);
  std::unique_ptr<SyntaxChecker> syntax;
  if (v8_ok) {
    syntax = std::make_unique<SyntaxChecker>();
  }
  auto v8_accepts = [&](const std::string& source, std::string* message) {
    return syntax != nullptr && syntax->Accepts(source, message);
  };

  // 1. Repeatable `--material` flags and JSON config roundtrip.
  {
    const char* argv[] = {"redline", "fuzz", "--material=/tmp/m1",
                          "--material=/tmp/m2,/tmp/m3", "--material-max=42"};
    Flags flags = Flags::Parse(5, argv, 2);
    FuzzConfig config;
    std::string error;
    const bool loaded =
        FuzzConfig::Load(flags, &config, &error, /*for_run=*/false);
    const std::vector<std::string> expected_paths = {"/tmp/m1", "/tmp/m2",
                                                     "/tmp/m3"};
    JsonValue json;
    FuzzConfig reloaded;
    const bool dumped_ok = loaded &&
                           JsonValue::Parse(config.ToJson(), &json, &error) &&
                           FuzzConfig::LoadJsonForTest(json, &reloaded, &error);
    checker->Check(
        "material: repeatable --material flags and config keys roundtrip",
        dumped_ok && config.material_paths == expected_paths &&
            config.material_max == 42 &&
            reloaded.material_paths == expected_paths &&
            reloaded.material_max == 42,
        error);
    TunableRegistry::Instance().Reset();
  }

  // Repeated flags: the last value wins for scalar getters, `ToString` keeps
  // all values.
  {
    const char* argv[] = {"redline",  "fuzz", "--name=a",
                          "--name=b", "--on", "--no-on"};
    Flags flags = Flags::Parse(6, argv, 2);
    const std::string rendered = flags.ToString();
    checker->Check(
        "material: repeated flags keep the last value and render all values",
        flags.GetString("name") == "b" && !flags.GetBool("on", true) &&
            rendered == "--name=a --name=b --on=true --on=false",
        rendered);
  }

  // 2. Loading, ClusterFuzz filters, `128` KB cap, and deterministic reservoir.
  char dir_template[] = "/tmp/redline-material.XXXXXX";
  if (mkdtemp(dir_template) == nullptr) {
    checker->Check("material: create a temporary directory", false,
                   strerror(errno));
    return;
  }
  const std::string dir = dir_template;
  std::string err;
  const bool dirs_ok =
      MakeDirectories(dir + "/bank/sqlite", &err) &&
      MakeDirectories(dir + "/bank/embenchen", &err) &&
      MakeDirectories(dir + "/bank/poppler", &err) &&
      MakeDirectories(dir + "/bank/test262/safe", &err) &&
      MakeDirectories(dir + "/bank/test262/early-errors", &err) &&
      MakeDirectories(dir + "/bank/v8/test/inspector", &err) &&
      MakeDirectories(dir + "/bank/v8/test/mjsunit/tools", &err) &&
      MakeDirectories(dir + "/bank/v8/test/mjsunit/d8/cwd", &err) &&
      MakeDirectories(dir + "/bank/v8/test/message", &err) &&
      MakeDirectories(dir + "/bank/mozilla/data/e4x", &err) &&
      MakeDirectories(dir + "/bank/root/test", &err) &&
      MakeDirectories(dir + "/reservoir", &err);

  const std::string ok_prefix = "var ok = 1;\n";
  const std::string big_prefix = "var big = 1;\n";
  const std::string pad_ok(MaterialBank::kMaxFileBytes - 1 - ok_prefix.size(),
                           ' ');
  const std::string pad_big(MaterialBank::kMaxFileBytes - big_prefix.size(),
                            ' ');
  bool files_ok = dirs_ok;
  auto write_file = [&](const std::string& rel, const std::string& body) {
    files_ok = files_ok && WriteStringToFile(dir + rel, body, &err);
  };
  // 5 kept files (including a safe `test262` file outside skipped directories).
  write_file("/bank/keep.js", "var mat_keep = 1;\n");
  write_file("/bank/size_ok.js", ok_prefix + pad_ok);
  write_file("/bank/root/test/helper.js",
             "function helperFromRoot() { return 7; }\n");
  write_file("/bank/load_user.js",
             "load(\"test/helper.js\");\nhelperFromRoot();\n");
  write_file("/bank/good_flags.js",
             "// Flags: --harmony-shipping --js-staging "
             "--allow-natives-syntax\nvar flagged = 1;\n");
  write_file("/bank/test262/safe/a.js", "var t262_safe = 1;\n");
  // 27 skipped entries (names, directories, size cap, flags, module, empty).
  write_file("/bank/d8-os.js", "var s = 1;\n");
  write_file("/bank/unicode-test.js", "var s = 1;\n");
  write_file("/bank/modules_a.js", "var s = 1;\n");
  write_file("/bank/regexp-property-a.js", "var s = 1;\n");
  write_file("/bank/foozzie-a.js", "var s = 1;\n");
  write_file("/bank/numops-fuzz-part1.js", "var s = 1;\n");
  write_file("/bank/d8-finalization-registry-quit.js", "var s = 1;\n");
  write_file("/bank/regress-444805.js", "var s = 1;\n");
  write_file("/bank/sqlite/a.js", "var s = 1;\n");
  write_file("/bank/embenchen/a.js", "var s = 1;\n");
  write_file("/bank/poppler/a.js", "var s = 1;\n");
  write_file("/bank/test262/early-errors/b.js", "var t262_early = 1;\n");
  write_file("/bank/v8/test/inspector/a.js", "var s = 1;\n");
  write_file("/bank/v8/test/mjsunit/tools/a.js", "var s = 1;\n");
  write_file("/bank/v8/test/mjsunit/d8/cwd/a.js", "var s = 1;\n");
  write_file("/bank/v8/test/message/a.js", "var s = 1;\n");
  write_file("/bank/mozilla/data/e4x/a.js", "var s = 1;\n");
  write_file("/bank/too_big.js", "var big = 1;\n" + pad_big);
  write_file("/bank/flag_sandbox.js",
             "// Flags: --sandbox-testing\nvar s = 1;\n");
  write_file("/bank/flag_fuzzing.js",
             "// Flags: --sandbox-fuzzing\nvar s = 1;\n");
  write_file("/bank/flag_trigger.js",
             "// Flags: --expose-trigger-failure\nvar s = 1;\n");
  write_file("/bank/flag_harmony.js",
             "// Flags: --harmony-weak-refs\nvar s = 1;\n");
  write_file("/bank/flag_js.js", "// Flags: --js-decorators\nvar s = 1;\n");
  write_file("/bank/module_header.js", "// MODULE\nvar s = 1;\n");
  write_file("/bank/flag_mjs.js", "// Flags: foo.mjs\nvar s = 1;\n");
  write_file("/bank/flag_module.js", "// Flags: --module\nvar s = 1;\n");
  write_file("/bank/empty_body.js", "// Only a comment.\n");
  // 1 parse error.
  write_file("/bank/syntax_error.js", "function ( {}\n");

  MaterialBank bank;
  MaterialBank::Options opts;
  opts.paths = {dir + "/bank/keep.js",
                dir + "/bank/size_ok.js",
                dir + "/bank/load_user.js",
                dir + "/bank/good_flags.js",
                dir + "/bank/d8-os.js",
                dir + "/bank/unicode-test.js",
                dir + "/bank/modules_a.js",
                dir + "/bank/regexp-property-a.js",
                dir + "/bank/foozzie-a.js",
                dir + "/bank/numops-fuzz-part1.js",
                dir + "/bank/d8-finalization-registry-quit.js",
                dir + "/bank/regress-444805.js",
                dir + "/bank/sqlite",
                dir + "/bank/embenchen",
                dir + "/bank/poppler",
                dir + "/bank/test262",
                dir + "/bank/v8/test/inspector",
                dir + "/bank/v8/test/mjsunit/tools",
                dir + "/bank/v8/test/mjsunit/d8/cwd",
                dir + "/bank/v8/test/message",
                dir + "/bank/mozilla/data/e4x",
                dir + "/bank/too_big.js",
                dir + "/bank/flag_sandbox.js",
                dir + "/bank/flag_fuzzing.js",
                dir + "/bank/flag_trigger.js",
                dir + "/bank/flag_harmony.js",
                dir + "/bank/flag_js.js",
                dir + "/bank/module_header.js",
                dir + "/bank/flag_mjs.js",
                dir + "/bank/flag_module.js",
                dir + "/bank/empty_body.js",
                dir + "/bank/syntax_error.js"};
  opts.import_root = dir + "/bank/root";
  opts.max_programs = 100;
  opts.seed = 42;
  const MaterialBank::LoadStats stats = bank.Load(opts);
  bool inlined_helper = false;
  bool loaded_safe_t262 = false;
  bool loaded_early_t262 = false;
  for (const ProgramPtr& p : bank.programs()) {
    if (p->source().find("helperFromRoot") != std::string::npos &&
        p->source().find("return 7") != std::string::npos) {
      inlined_helper = true;
    }
    if (p->source().find("t262_safe") != std::string::npos) {
      loaded_safe_t262 = true;
    }
    if (p->source().find("t262_early") != std::string::npos) {
      loaded_early_t262 = true;
    }
  }
  Random pick_rng(7);
  ProgramPtr picked = bank.Pick(pick_rng);
  checker->Check(
      "material: MaterialBank applies filters, size cap, flags, and inlining",
      files_ok && stats.scanned == 33 && stats.loaded == 5 &&
          stats.skipped == 27 && stats.errors == 1 && bank.size() == 5 &&
          inlined_helper && loaded_safe_t262 && !loaded_early_t262 &&
          picked != nullptr,
      StringPrintf("scanned=%zu loaded=%zu skipped=%zu errors=%zu",
                   stats.scanned, stats.loaded, stats.skipped, stats.errors));

  // Deterministic reservoir sampling cap.
  for (int i = 0; i < 10; i++) {
    write_file(StringPrintf("/reservoir/f%02d.js", i),
               StringPrintf("let item_%d = %d;\n", i, i));
  }
  MaterialBank res_a, res_b, res_c;
  MaterialBank::Options res_opts;
  res_opts.paths = {dir + "/reservoir"};
  res_opts.max_programs = 4;
  res_opts.seed = 1337;
  res_a.Load(res_opts);
  res_b.Load(res_opts);
  res_opts.seed = 9999;
  res_c.Load(res_opts);
  bool match_ab = res_a.size() == 4 && res_b.size() == 4;
  if (match_ab) {
    for (size_t i = 0; i < 4; i++) {
      if (res_a.programs()[i]->source() != res_b.programs()[i]->source()) {
        match_ab = false;
      }
    }
  }
  bool diff_ac = res_c.size() == 4;
  if (diff_ac) {
    bool same = true;
    for (size_t i = 0; i < 4; i++) {
      if (res_a.programs()[i]->source() != res_c.programs()[i]->source()) {
        same = false;
      }
    }
    diff_ac = !same;
  }
  checker->Check("material: reservoir sampling caps and is deterministic",
                 match_ab && diff_ac, "");

  // 3. IIFE unwrapping of safe and unsafe IIFEs.
  {
    std::string safe_src =
        "outerRef();\n"
        "(function() { function innerFn() { return 1; } let x = innerFn(); "
        "function outerRef() {} })();\n"
        "(() => { const x = 2; })();\n";
    fast::ParseError perr;
    std::unique_ptr<fast::Tree> safe_tree =
        fast::Parser::ParseScript(safe_src, fast::ParseOptions(), &perr);
    const bool unwrapped =
        safe_tree != nullptr && UnwrapTopLevelIIFEs(safe_tree.get());
    bool no_top_call = safe_tree != nullptr;
    if (safe_tree != nullptr) {
      for (const fast::Statement* s : safe_tree->root()->body) {
        if (auto* es = s->As<fast::ExpressionStatement>()) {
          if (es->expression != nullptr &&
              es->expression->Is<fast::CallExpression>() &&
              es->expression->Cast<fast::CallExpression>()->callee != nullptr &&
              (es->expression->Cast<fast::CallExpression>()
                   ->callee->Is<fast::FunctionExpression>() ||
               es->expression->Cast<fast::CallExpression>()
                   ->callee->Is<fast::ArrowFunctionExpression>())) {
            no_top_call = false;
          }
        }
      }
    }
    const std::string printed =
        safe_tree != nullptr ? fast::Print(safe_tree->root()) : "";
    std::string v8_msg;
    const bool v8_valid = safe_tree != nullptr && v8_accepts(printed, &v8_msg);
    const bool fast_valid =
        fast::Parser::ParseScript(printed, fast::ParseOptions(), &perr) !=
        nullptr;
    checker->Check("material: UnwrapTopLevelIIFEs unwraps safe IIFEs cleanly",
                   unwrapped && no_top_call && v8_valid && fast_valid,
                   v8_valid ? perr.ToString() : v8_msg);

    const char* unsafe_cases[] = {
        "(function() { return 1; })();",
        "(function() { arguments[0]; })();",
        "(function() { this.x = 1; })();",
        "(() => { this.x = 1; })();",
        "(function(a) { let x = a; })(1);",
        "(async function() { let x = 1; })();",
        "(function*() { yield 1; })();",
        "(function self() { self; })();",
        "(function() { \"use strict\"; let z = 1; })();",
        "(function() { using u = null; })();",
        "(function() { class C { [new.target] = 1; } })();",
    };
    bool all_kept = true;
    std::string failed_case;
    for (const char* src : unsafe_cases) {
      fast::ParseError uerr;
      std::unique_ptr<fast::Tree> utree =
          fast::Parser::ParseScript(src, fast::ParseOptions(), &uerr);
      if (utree == nullptr || UnwrapTopLevelIIFEs(utree.get())) {
        all_kept = false;
        failed_case = src;
        break;
      }
    }
    checker->Check(
        "material: UnwrapTopLevelIIFEs keeps return, arguments, this, param, "
        "using and computed new.target IIFEs intact",
        all_kept, failed_case);
  }

  // 4. `FusePrograms` merges 2 or 3 colliding donors safely.
  {
    std::string parse_err;
    ProgramPtr d1 = Program::Parse(
        "(function() { function helper(v) { return v + 1; } "
        "let x = helper(10); const k = 1; class Box { v() { return x; } } "
        "throw new Error(\"boom\"); })();\n",
        Lineage{}, &parse_err);
    ProgramPtr d2 = Program::Parse(
        "function helper(z) { return z * 2; }\n"
        "let x = helper(5);\nconst k = 2;\nclass Box {}\n"
        "const {a, b: c} = {a: 3, b: 4};\n"
        "var ok_var = x + k + a + c;\n",
        Lineage{}, &parse_err);
    ProgramPtr d3 = Program::Parse(
        "(() => { let x = 99; const k = 3; function helper() { return x; } "
        "helper(); })();\n",
        Lineage{}, &parse_err);

    checker->Check("material: FusePrograms returns nullptr on empty donors",
                   FusePrograms({}, "") == nullptr, "");

    ProgramPtr fused2 = FusePrograms({d1, d2}, "2 donors");
    ProgramPtr fused3 = FusePrograms({d1, d2, d3}, "3 donors");
    bool has_top_fn = false;
    bool has_top_class = false;
    bool has_top_let = false;
    bool has_try = false;
    if (fused3 != nullptr) {
      for (const fast::Statement* s : fused3->tree().root()->body) {
        if (s->Is<fast::FunctionDeclaration>()) {
          has_top_fn = true;
        } else if (s->Is<fast::ClassDeclaration>()) {
          has_top_class = true;
        } else if (s->Is<fast::VariableDeclaration>() &&
                   s->Cast<fast::VariableDeclaration>()->declaration_kind ==
                       fast::VariableKind::kLet) {
          has_top_let = true;
        } else if (s->Is<fast::TryStatement>()) {
          has_try = true;
        }
      }
    }
    fast::ParseError reerr2, reerr3;
    std::string v8_msg2, v8_msg3;
    const bool ok2 =
        fused2 != nullptr && fused2->lineage().origin == "Fuse" &&
        fast::Parser::ParseScript(fused2->source(), fast::ParseOptions(),
                                  &reerr2) != nullptr &&
        v8_accepts(fused2->source(), &v8_msg2);
    const bool ok3 =
        fused3 != nullptr && fused3->lineage().origin == "Fuse" &&
        fused3->lineage().detail == "3 donors" && has_top_fn && has_top_class &&
        has_top_let && has_try &&
        fast::Parser::ParseScript(fused3->source(), fast::ParseOptions(),
                                  &reerr3) != nullptr &&
        v8_accepts(fused3->source(), &v8_msg3);
    checker->Check(
        "material: FusePrograms merges 2 and 3 donors with valid syntax",
        ok2 && ok3,
        !ok2 ? (v8_msg2.empty() ? reerr2.ToString() : v8_msg2)
             : (v8_msg3.empty() ? reerr3.ToString() : v8_msg3));
  }

  // 5. `FusePrograms` keeps `using` in a block and runs class definitions that
  // can throw inside `try {} catch {}`.
  {
    std::string parse_err;
    ProgramPtr using_donor =
        Program::Parse("(function() { using u = null; let w = 1; })();\n",
                       Lineage{}, &parse_err);
    ProgramPtr class_donor = Program::Parse(
        "class Base {}\n"
        "class Derived extends Base { static s = missing(); }\n"
        "class Keyed { [Symbol.iterator]() {} }\n"
        "class Plain { m() { return Plain; } }\n"
        "new Derived();\n",
        Lineage{}, &parse_err);
    ProgramPtr empty_donor = Program::Parse(";\n", Lineage{}, &parse_err);
    ProgramPtr fused = FusePrograms({using_donor, class_donor}, "isolation");
    std::vector<std::string_view> top_classes;
    bool has_top_using = false;
    if (fused != nullptr) {
      for (const fast::Statement* s : fused->tree().root()->body) {
        if (auto* cls = s->As<fast::ClassDeclaration>()) {
          top_classes.push_back(cls->name->name);
        } else if (auto* vd = s->As<fast::VariableDeclaration>()) {
          has_top_using =
              has_top_using ||
              vd->declaration_kind == fast::VariableKind::kUsing ||
              vd->declaration_kind == fast::VariableKind::kAwaitUsing;
        }
      }
    }
    std::string v8_msg;
    const bool valid = fused != nullptr && v8_accepts(fused->source(), &v8_msg);
    const std::vector<std::string_view> expected_classes = {"Base", "Plain"};
    const bool ok = valid && !has_top_using &&
                    top_classes == expected_classes &&
                    FusePrograms({empty_donor}, "empty") == nullptr;
    std::string detail;
    if (!ok) {
      detail = fused != nullptr ? fused->source() + v8_msg : parse_err;
    }
    checker->Check(
        "material: FusePrograms isolates using and throwing class definitions",
        ok, detail);
  }

  // 6. `MaterialBank` keeps the allowed `// Flags:` of a file in
  // `Program::flags`. `FusePrograms` merges the flags of its donors.
  {
    files_ok = files_ok && MakeDirectories(dir + "/flags", &err);
    write_file("/flags/flagged.js",
               "// Flags: --turbofan --no-lazy_feedback_allocation\n"
               "// Flags: --allow-natives-syntax --print-bytecode\n"
               "var material_flagged = 1;\n");
    write_file("/flags/plain.js", "var material_plain = 1;\n");
    MaterialBank flag_bank;
    MaterialBank::Options flag_opts;
    flag_opts.paths = {dir + "/flags"};
    flag_opts.max_programs = 10;
    flag_opts.seed = 42;
    flag_bank.Load(flag_opts);
    std::vector<std::string> flagged_flags;
    std::vector<std::string> plain_flags = {"--not-loaded"};
    for (const ProgramPtr& p : flag_bank.programs()) {
      if (p->source().find("material_flagged") != std::string::npos) {
        flagged_flags = p->flags;
      } else if (p->source().find("material_plain") != std::string::npos) {
        plain_flags = p->flags;
      }
    }
    checker->Check(
        "material: MaterialBank keeps the allowed `// Flags:` in "
        "Program::flags",
        files_ok && flag_bank.size() == 2 &&
            flagged_flags ==
                std::vector<std::string>{"--turbofan",
                                         "--no-lazy-feedback-allocation"} &&
            plain_flags.empty(),
        StringPrintf("%zu programs, %zu and %zu flags", flag_bank.size(),
                     flagged_flags.size(), plain_flags.size()));

    std::string parse_err;
    std::shared_ptr<Program> donor_a =
        Program::Parse("var fuse_a = 1;\n", Lineage{}, &parse_err);
    std::shared_ptr<Program> donor_b =
        Program::Parse("var fuse_b = 2;\n", Lineage{}, &parse_err);
    std::shared_ptr<Program> donor_c =
        Program::Parse("var fuse_c = 3;\n", Lineage{}, &parse_err);
    ProgramPtr fused;
    ProgramPtr fused_plain;
    if (donor_a != nullptr && donor_b != nullptr && donor_c != nullptr) {
      donor_a->flags = {"--turbofan", "--interrupt-budget=1000"};
      donor_b->flags = {"--no-turbofan", "--interrupt-budget=50", "--maglev"};
      fused = FusePrograms({donor_a, donor_b, donor_c}, "flags");
      fused_plain = FusePrograms({donor_c}, "plain");
    }
    const std::vector<std::string> expected_flags = {
        "--turbofan", "--interrupt-budget=1000", "--maglev"};
    checker->Check("material: FusePrograms merges the flags of its donors",
                   fused != nullptr && fused->flags == expected_flags &&
                       fused_plain != nullptr && fused_plain->flags.empty(),
                   fused == nullptr
                       ? parse_err
                       : StringPrintf("%zu flags", fused->flags.size()));
  }

  RunMaterialCompactChecks(checker, syntax.get());

  // 10. `RecordMaterialFlags` fills free pool slots without evicting seeds.
  {
    files_ok = files_ok && MakeDirectories(dir + "/pool_flags", &err);
    write_file("/pool_flags/m1.js", "// Flags: --mat-flag-one\nlet m1 = 1;\n");
    write_file("/pool_flags/m2.js", "// Flags: --mat-flag-two\nlet m2 = 2;\n");
    MaterialBank pool_bank;
    MaterialBank::Options pool_opts;
    pool_opts.paths = {dir + "/pool_flags"};
    pool_opts.max_programs = 10;
    pool_opts.seed = 7;
    pool_bank.Load(pool_opts);

    FuzzConfig cfg_partial;
    cfg_partial.randomize_d8_flags = true;
    Fuzzer fuzzer_partial(std::move(cfg_partial));
    fuzzer_partial.RecordImportedFlags({"--seed-flag-0"});
    fuzzer_partial.RecordMaterialFlags(pool_bank);
    const bool added_to_pool = fuzzer_partial.imported_flag_sets() == 3;

    FuzzConfig cfg_full;
    cfg_full.randomize_d8_flags = true;
    Fuzzer fuzzer_full(std::move(cfg_full));
    for (int i = 0; i < 2048; i++) {
      fuzzer_full.RecordImportedFlags({StringPrintf("--seed-slot-%d", i)});
    }
    fuzzer_full.RecordMaterialFlags(pool_bank);
    TunableRegistry& reg = TunableRegistry::Instance();
    std::string reg_err;
    reg.Set("flags.imported_chance", 1.0, &reg_err);
    Random draw_rng(5);
    bool saw_material = false;
    for (int i = 0; i < 20; i++) {
      for (const std::string& f : fuzzer_full.DrawD8Flags(draw_rng)) {
        if (f == "--mat-flag-one" || f == "--mat-flag-two") {
          saw_material = true;
        }
      }
    }
    reg.Reset();
    checker->Check(
        "material: RecordMaterialFlags fills free slots without evicting seeds",
        files_ok && added_to_pool && fuzzer_full.imported_flag_sets() == 2048 &&
            !saw_material,
        StringPrintf("partial=%zu full=%zu saw_mat=%d",
                     fuzzer_partial.imported_flag_sets(),
                     fuzzer_full.imported_flag_sets(), saw_material));
  }

  RemoveTree(dir);
}

int RunSelftest(const Flags& flags) {
  Checker checker;
  std::string error;

  // Offline checks first: they need no target.
  RunCrashSignatureChecks(&checker);
  RunReprlBundleChecks(&checker);
  RunCoverageHeaderChecks(&checker);
  RunFuzzilliCoverageChecks(&checker);
  RunFuzzilliOptimizedCoverageChecks(&checker);
  RunRetentionChecks(&checker);
  RunD8FlagProfileChecks(&checker);
  RunFuzzilliFlagProfileChecks(&checker);
  RunCoverageCounterChecks(&checker);
  RunCoverageBitsChecks(&checker);
  RunCrashSlotChecks(&checker);
  RunCrashMinimizerChecks(&checker);
  RunCommitCostChecks(&checker);
  RunCoalesceChecks(&checker);
  RunFlagMinimizerChecks(&checker);
  RunImportChecks(&checker);
  RunSalvageChecks(&checker);
  RunOnDemandPreludeChecks(&checker);
  RunTestFlagChecks(&checker);
  RunMaterialChecks(&checker);
  RunAmplifiedTestChecks(&checker);
  RunBundleStepChecks(&checker);
  RunNodeKindChecks(&checker);
  RunPresetChecks(&checker);
  std::string testdata = flags.GetString("testdata", DefaultTestdataDir());
  if (testdata.empty()) {
    checker.Check("selection: locate experimental/redline/testdata", false,
                  "not found next to the binary; pass --testdata=DIR");
  } else {
    RunSelectionChecks(&checker, testdata,
                       flags.GetBool("write-golden", false));
  }
  if (flags.GetBool("offline", false)) {
    printf("selftest: %d passed, %d failed\n", checker.passed(),
           checker.failed());
    return checker.failed() == 0 ? kExitOk : kExitFailure;
  }

  TargetOptions target;
  if (!TargetOptions::FromFlags(flags, &target, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitUsage;
  }
  // The respawn behaviour is part of what is tested; use a small value.
  target.max_execs_before_respawn =
      static_cast<int>(flags.GetInt("max-execs-before-respawn", 50));
  int speed_runs = static_cast<int>(flags.GetInt("speed-runs", 200));

  printf("target: %s\n", target.CommandLine().c_str());

  std::unique_ptr<ScriptRunner> runner =
      ScriptRunner::Create(target, 0, &error);
  if (!checker.Check("create runner", runner != nullptr, error)) {
    return kExitFailure;
  }
  if (!checker.Check("REPRL handshake and coverage initialization",
                     runner->Initialize(&error), error)) {
    return kExitFailure;
  }
  checker.Info("instrumented edges",
               StringPrintf("%u", runner->coverage().num_edges()));
  const bool fuzzilli_target = runner->coverage().is_fuzzilli();
  {
    const CoverageMap& coverage = runner->coverage();
    if (fuzzilli_target) {
      checker.Info("coverage header",
                   StringPrintf("fuzzilli layout (%u edges, no region table or "
                                "build id)",
                                coverage.num_edges()));
    } else {
      const cov::Header& header = coverage.header();
      std::string regions;
      for (uint32_t i = 0; i < header.num_regions; i++) {
        static const char* const kKinds[] = {"?", "code", "builtins",
                                             "optimized"};
        const cov::Region& region = header.regions[i];
        regions += StringPrintf(" %s %u..%u",
                                kKinds[region.kind <= 3 ? region.kind : 0],
                                region.first, region.first + region.count - 1);
      }
      checker.Info("coverage header",
                   StringPrintf("%s, %u bit(s) per edge,%s, build id %s",
                                coverage.mode_name(), header.bits_per_edge,
                                regions.c_str(), coverage.build_id().c_str()));
    }
  }

  ScriptRunner::Result result;

  // Basic outcomes.
  if (RunOrFail(&checker, runner.get(), "empty script", "", &result)) {
    checker.Check("empty script succeeds", result.execution.succeeded(),
                  Describe(result.execution));
  }
  {
    std::string prelude_script(DefaultPrelude());
    prelude_script +=
        "\n(function() { 'use strict';\n"
        "  if (!isPrimitive(1) || isPrimitive({})) throw 'isPrimitive';\n"
        "  if (!(Test262Error('x') instanceof Test262Error)) throw 'err';\n"
        "  if (assert._formatIdentityFreeValue(1) !== '1') throw 'fmt';\n"
        "  assert._toString(Object.create(null));\n"
        "  compareArray.format(Object.create(null));\n"
        "  if ($262.evalScript('1 + 1') !== 2) throw 'evalScript';\n"
        "  if ($262.createRealm().evalScript('2 + 2') !== 4) throw 'realm';\n"
        "})();\n";
    if (RunOrFail(&checker, runner.get(), "default prelude", prelude_script,
                  &result)) {
      checker.Check("default prelude Test262 helpers run without throwing",
                    result.execution.succeeded(), Describe(result.execution));
    }
  }
  if (RunOrFail(&checker, runner.get(), "uncaught exception",
                "throw new Error('expected');", &result)) {
    checker.Check("uncaught exception is reported as failed",
                  result.execution.outcome == Outcome::kFailed,
                  Describe(result.execution));
  }
  if (RunOrFail(&checker, runner.get(), "syntax error", "let let = ;",
                &result)) {
    checker.Check("syntax error is reported as failed",
                  result.execution.outcome == Outcome::kFailed,
                  Describe(result.execution));
  }
  RunSalvageTargetCheck(&checker, runner.get());
  RunOnDemandPreludeTargetCheck(&checker, runner.get());

  // Output channels.
  {
    const char* print_script =
        fuzzilli_target ? "fuzzilli('FUZZILLI_PRINT', 'hello fuzzout');"
                        : "redline('REDLINE_PRINT', 'hello fuzzout');";
    const char* print_label = fuzzilli_target
                                  ? "fuzzilli('FUZZILLI_PRINT') is captured"
                                  : "redline('REDLINE_PRINT') is captured";
    if (RunOrFail(&checker, runner.get(), "fuzzout", print_script, &result)) {
      checker.Check(print_label,
                    result.execution.succeeded() &&
                        result.execution.fuzzout.find("hello fuzzout") !=
                            std::string::npos,
                    Describe(result.execution) + ", fuzzout=\"" +
                        result.execution.fuzzout + "\"");
    }
  }
  // Environment introspection over the fuzzout channel.
  if (RunOrFail(&checker, runner.get(), "introspection",
                std::string(Environment::IntrospectionScript()), &result)) {
    std::string env_error;
    std::unique_ptr<Environment> env =
        result.execution.succeeded()
            ? Environment::WithDiscovered(result.execution.fuzzout, &env_error)
            : nullptr;
    checker.Check(
        "environment introspection works on the target",
        env != nullptr && !env->discovered().empty(),
        env != nullptr
            ? StringPrintf("%zu globals", env->discovered().size())
            : (env_error.empty() ? Describe(result.execution) : env_error));
  }
  if (RunOrFail(&checker, runner.get(), "stdout",
                "print('hello stdout'); console.log('hello console');",
                &result)) {
    bool ok = result.execution.succeeded() &&
              result.execution.stdout_output.find("hello stdout") !=
                  std::string::npos &&
              result.execution.stdout_output.find("hello console") !=
                  std::string::npos;
    checker.Check("stdout is captured", ok,
                  Describe(result.execution) + ", stdout=\"" +
                      result.execution.stdout_output + "\"");
  }
  if (RunOrFail(&checker, runner.get(), "stderr", "console.error('to stderr')",
                &result)) {
    checker.Check(
        "stderr is captured",
        result.execution.stderr_output.find("to stderr") != std::string::npos,
        "stderr=\"" + result.execution.stderr_output + "\"");
  }
  if (RunOrFail(&checker, runner.get(), "output isolation", "print('second')",
                &result)) {
    checker.Check(
        "output of earlier executions is not leaked",
        result.execution.stdout_output.find("hello") == std::string::npos &&
            result.execution.fuzzout.empty(),
        "stdout=\"" + result.execution.stdout_output + "\"");
  }

  // Oversized script fails at once without killing or respawning the target.
  {
    const pid_t pid_before = runner->child().pid();
    const uint64_t spawns_before = runner->child().total_spawns();
    const uint64_t failures_before = runner->stats().infrastructure_failures;
    std::string huge_script(ReprlChild::kMaxDataSize + 1, ' ');
    Execution oversized_exec;
    std::string oversized_err;
    const bool rejected = !runner->RunWithoutEvaluation(
        huge_script, target.timeout_us, &oversized_exec, &oversized_err);
    const bool healthy =
        runner->Run("1 + 1", &result, &error) && result.execution.succeeded();
    checker.Check(
        "runner: oversized script fails without killing or respawning target",
        rejected &&
            oversized_err.find("Script too large") != std::string::npos &&
            runner->child().pid() == pid_before &&
            runner->child().total_spawns() == spawns_before &&
            runner->stats().infrastructure_failures == failures_before + 1 &&
            healthy,
        oversized_err);
  }

  // Timeouts.
  {
    uint64_t timeout_us = static_cast<uint64_t>(300) * 1000;
    Stopwatch stopwatch;
    if (runner->Run("while (true) {}", timeout_us, &result, &error)) {
      uint64_t elapsed = stopwatch.ElapsedUs();
      checker.Check("infinite loop times out",
                    result.execution.outcome == Outcome::kTimedOut &&
                        elapsed >= timeout_us && elapsed < timeout_us * 20,
                    Describe(result.execution) +
                        StringPrintf(", wall %.1f ms",
                                     static_cast<double>(elapsed) / 1000.0));
    } else {
      checker.Check("infinite loop times out", false, error);
    }
    if (RunOrFail(&checker, runner.get(), "after timeout", "1 + 1", &result)) {
      checker.Check("target is respawned after a timeout",
                    result.execution.succeeded(), Describe(result.execution));
    }
  }

  // Crashes.
  struct CrashCase {
    const char* description;
    int kind;
    bool required;
  };
  const CrashCase crash_cases[] = {
      {"IMMEDIATE_CRASH", 0, true},    {"CHECK failure", 1, true},
      {"DCHECK failure", 2, false},    {"wild write", 3, true},
      {"DEBUG build check", 8, false}, {"sandbox violation", 9, false},
  };
  for (const CrashCase& c : crash_cases) {
    const char* crash_fn = fuzzilli_target ? "fuzzilli('FUZZILLI_CRASH'"
                                           : "redline('REDLINE_CRASH'";
    std::string script = StringPrintf("%s, %d)", crash_fn, c.kind);
    std::string name =
        StringPrintf("%s, %d) [%s]", crash_fn, c.kind, c.description);
    if (!RunOrFail(&checker, runner.get(), name.c_str(), script, &result)) {
      continue;
    }
    bool crashed = result.execution.crashed();
    if (c.required) {
      checker.Check(name.c_str(), crashed, Describe(result.execution));
    } else {
      checker.Info(name.c_str(), (crashed ? "crashes" : "does not crash") +
                                     std::string(" -- ") +
                                     Describe(result.execution));
    }
    if (RunOrFail(&checker, runner.get(), "after crash", "1 + 1", &result)) {
      checker.Check("target is respawned after a crash",
                    result.execution.succeeded(), Describe(result.execution));
    }
  }

  // Coverage.
  const char* kProgramA =
      "function f(a) { let s = 0; for (let i = 0; i < a.length; i++) s += "
      "a[i]; return s; }\n"
      "let arr = [1, 2.5, 'x', {}, null];\n"
      "for (let j = 0; j < 100; j++) f(arr);\n"
      "class C { #p = 1; get p() { return this.#p; } }\n"
      "new C().p;\n";
  const char* kProgramB =
      "let m = new Map(); for (let i = 0; i < 50; i++) m.set(i, 'v' + i);\n"
      "let s = new Set([1, 2, 3]); [...m.keys()].filter(x => s.has(x));\n"
      "JSON.parse(JSON.stringify({a: [1, {b: 2}], c: 'd'}));\n"
      "/a(b+)c/.exec('xxabbbcxx'); new Date(0).toISOString();\n";
  std::vector<uint32_t> edges_a;
  if (RunOrFail(&checker, runner.get(), "coverage A", kProgramA, &result)) {
    edges_a = result.new_edges;
    checker.Check("first execution of a program finds new edges",
                  result.execution.succeeded() && !result.new_edges.empty(),
                  StringPrintf("%zu new edges, %u hit", result.new_edges.size(),
                               result.edges_hit));
  }
  if (RunOrFail(&checker, runner.get(), "coverage A again", kProgramA,
                &result)) {
    checker.Check(
        "re-running the same program finds (almost) no new edges",
        result.new_edges.size() * 100 < std::max<size_t>(edges_a.size(), 1),
        StringPrintf("%zu new edges on second run vs %zu on first",
                     result.new_edges.size(), edges_a.size()));
  }
  if (RunOrFail(&checker, runner.get(), "coverage B", kProgramB, &result)) {
    checker.Check("a different program finds new edges",
                  result.execution.succeeded() && !result.new_edges.empty(),
                  StringPrintf("%zu new edges", result.new_edges.size()));
  }
  if (RunOrFail(&checker, runner.get(), "coverage of failing program",
                "let q = []; q.push(1); throw 1;", &result)) {
    checker.Check("failed executions do not advance coverage",
                  result.execution.outcome == Outcome::kFailed &&
                      result.new_edges.empty() && result.edges_hit > 0,
                  StringPrintf("%zu new edges, %u hit", result.new_edges.size(),
                               result.edges_hit));
  }

  // Determinism of edges across runs. V8 is not deterministic at the edge
  // level: GC timing, tiering decisions and concurrent compiler threads make
  // 5-15% of the edges of any script flaky (even `let x = 1 + 2;` shows ~6%).
  // This is the reason for the multi-run determinism check in the fuzzing
  // loop; here we only sanity-check that the stable core is large (a broken
  // coverage map would show up as near-random bitmaps) and report the rate.
  {
    struct Probe {
      const char* name;
      const char* script;
    };
    const Probe kProbes[] = {{"trivial script", "let x = 1 + 2;"},
                             {"program A", kProgramA}};
    for (const Probe& probe : kProbes) {
      std::vector<uint32_t> all_runs;
      std::set<uint32_t> any_run;
      bool ok = true;
      for (int i = 0; i < 5; i++) {
        Execution execution;
        if (!runner->RunWithoutEvaluation(probe.script, target.timeout_us,
                                          &execution, &error)) {
          ok = false;
          break;
        }
        std::vector<uint32_t> current = runner->coverage().CurrentEdges();
        if (i == 0) {
          all_runs = current;
        } else {
          std::vector<uint32_t> intersection;
          std::set_intersection(all_runs.begin(), all_runs.end(),
                                current.begin(), current.end(),
                                std::back_inserter(intersection));
          all_runs.swap(intersection);
        }
        any_run.insert(current.begin(), current.end());
      }
      double stable_pct = any_run.empty()
                              ? 0.0
                              : 100.0 * static_cast<double>(all_runs.size()) /
                                    static_cast<double>(any_run.size());
      checker.Info(
          StringPrintf("edge nondeterminism of %s", probe.name).c_str(),
          StringPrintf("%zu edges in every one of 5 runs, %zu in at "
                       "least one run (%.1f%% stable)",
                       all_runs.size(), any_run.size(), stable_pct));
      checker.Check(
          StringPrintf("stable edge core of %s is >= 75%% of the union",
                       probe.name)
              .c_str(),
          ok && !all_runs.empty() && stable_pct >= 75.0,
          StringPrintf("%.1f%%", stable_pct));
    }
  }

  // Respawning.
  {
    uint64_t spawns_before = runner->child().total_spawns();
    int runs = target.max_execs_before_respawn * 2 + 1;
    bool ok = true;
    for (int i = 0; i < runs; i++) {
      if (!runner->Run("", &result, &error) || !result.execution.succeeded()) {
        ok = false;
        break;
      }
    }
    uint64_t spawns = runner->child().total_spawns() - spawns_before;
    checker.Check("target is respawned every max_execs_before_respawn runs",
                  ok && spawns >= 2,
                  StringPrintf("%llu spawns in %d runs",
                               static_cast<unsigned long long>(spawns), runs));
  }

  // Random flags (randomize_d8_flags): a new target process gets new
  // flags, except while the flags are pinned.
  {
    TargetOptions random_target = target;
    int draws = 0;
    random_target.random_args = [&draws] {
      draws++;
      // Two harmless flags, so that the check can see which draw a target
      // process got.
      return std::vector<std::string>{draws % 2 == 1 ? "--stress-ic"
                                                     : "--no-stress-ic"};
    };
    std::unique_ptr<ScriptRunner> random_runner =
        ScriptRunner::Create(random_target, 1, &error);
    bool ok = random_runner != nullptr && random_runner->Initialize(&error);
    std::vector<std::string> first, second, pinned, released;
    auto respawn = [&](std::vector<std::string>* args) {
      random_runner->child().Kill();
      ok = ok && random_runner->Run("", &result, &error) &&
           result.execution.succeeded();
      *args = random_runner->child().CurrentArgs();
    };
    if (ok) {
      first = random_runner->child().CurrentArgs();
      respawn(&second);
      random_runner->child().Pin(true);
      respawn(&pinned);
      random_runner->child().Pin(false);
      respawn(&released);
    }
    auto starts_with = [](const std::vector<std::string>& args,
                          const char* flag) {
      return !args.empty() && args.front() == flag;
    };
    checker.Check(
        "random flags: a new target gets new flags, a pinned one keeps them",
        ok && starts_with(first, "--stress-ic") &&
            starts_with(second, "--no-stress-ic") && pinned == second &&
            starts_with(released, "--stress-ic") && draws == 3,
        ok ? StringPrintf("%d draws", draws) : error);
    // A respawn while pinned keeps the flags. Then Pin(false) kills the
    // target, because its flags ran max_execs_before_respawn runs, and the
    // next target draws new flags.
    bool respawned = false;
    bool killed = false;
    std::vector<std::string> kept, renewed;
    if (ok) {
      random_runner->child().Pin(true);
      uint64_t spawns_before = random_runner->child().total_spawns();
      for (int i = 0; ok && i < random_target.max_execs_before_respawn; i++) {
        ok = random_runner->Run("", &result, &error) &&
             result.execution.succeeded();
      }
      respawned = random_runner->child().total_spawns() > spawns_before;
      kept = random_runner->child().CurrentArgs();
      random_runner->child().Pin(false);
      killed = !random_runner->child().is_running();
      ok = ok && random_runner->Run("", &result, &error) &&
           result.execution.succeeded();
      renewed = random_runner->child().CurrentArgs();
    }
    checker.Check("random flags: unpinning replaces a target with old flags",
                  ok && respawned && kept == released && killed &&
                      starts_with(renewed, "--no-stress-ic") && draws == 4,
                  ok ? StringPrintf("%d draws", draws) : error);
  }

  // Speed (informational).
  if (speed_runs > 0) {
    Stopwatch stopwatch;
    bool ok = true;
    for (int i = 0; i < speed_runs; i++) {
      if (!runner->Run("let x = 1 + 1;", &result, &error)) {
        ok = false;
        break;
      }
    }
    double seconds = stopwatch.ElapsedSeconds();
    checker.Info(
        "execution speed",
        ok ? StringPrintf("%.0f execs/s for a trivial script (%d runs)",
                          static_cast<double>(speed_runs) / seconds, speed_runs)
           : "failed: " + error);
  }

  // The session of this check has a worker 0. The coverage map of worker 0
  // has the same name as the map of `runner`, so stop `runner` first.
  runner.reset();
  RunCrashReproCheck(&checker, target, fuzzilli_target);
  RunCrashOnDemandCheck(&checker, target, fuzzilli_target);
  RunCrashFallbackCheck(&checker, target, fuzzilli_target);
  RunCrashFlagsCheck(&checker, target, fuzzilli_target);
  RunCrashAmplifiedCheck(&checker, target, fuzzilli_target);
  RunCrashBundleCheck(&checker, target, fuzzilli_target);
  RunImportAllCheck(&checker, target);

  printf("selftest: %d passed, %d failed\n", checker.passed(),
         checker.failed());
  return checker.failed() == 0 ? kExitOk : kExitFailure;
}

}  // namespace

const Command kSelftestCommand = {
    "selftest",
    "Check that the execution layer works against a target",
    "usage: redline selftest --d8=PATH [options] | --offline [options]\n"
    "\n"
    "Runs a series of checks against the target: REPRL handshake, outcome\n"
    "classification (success, exception, timeout, crash), output capture,\n"
    "coverage evaluation, edge determinism and respawning. A short session\n"
    "then checks the crash minimizer. Prints PASS/FAIL per check and exits\n"
    "with 1 if any check failed.\n"
    "\n"
    "Before the target checks it runs the offline selection checks: the\n"
    "default seed, partner, mutator and generator strategies must reproduce\n"
    "the picks recorded in experimental/redline/testdata/selection/*.trace.\n"
    "\n"
    "options:\n"
    "  --speed-runs=N     executions for the speed measurement (200)\n"
    "  --offline          only the checks that need no target (--d8 not "
    "needed)\n"
    "  --testdata=DIR     experimental/redline/testdata (default: found next "
    "to the binary)\n"
    "  --write-golden     rewrite the selection traces instead of comparing "
    "them\n"
    "                     (only for an intended change of the default picks)\n",
    RunSelftest,
};

}  // namespace redline
