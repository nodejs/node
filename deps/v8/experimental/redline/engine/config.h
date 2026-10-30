// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_CONFIG_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_CONFIG_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "experimental/redline/core/selection.h"
#include "experimental/redline/engine/stages.h"
#include "experimental/redline/exec/target_options.h"
#include "experimental/redline/minimizer/minimizer.h"

namespace redline {

class Flags;

// Everything a fuzzing session is parameterized by. Four layers, later
// ones win: built-in defaults, the preset (--preset), the JSON given with
// --config, and the command line. The effective configuration is written
// to the event log so that a run can be reproduced.
//
// JSON keys mirror the field names below (snake_case). The selection
// strategies are configured under "selection" (see SelectionSettings); the
// older top-level keys "scheduler", "mutators" and "generators" are still
// accepted as shorthands for them.
struct FuzzConfig {
  // --- target -------------------------------------------------------------
  TargetOptions target;
  std::string prelude;         // JS prepended to every execution (not mutated)
  std::string prelude_source;  // "default", "none" or the file it came from
  // Flags for the in-process V8 (syntax oracle). Must accept everything the
  // target accepts: natives syntax and staged language features.
  std::string v8_flags = "--allow-natives-syntax --js-staging --js-decorators";
  // Parse every mutant and every minimizer probe with the in-process V8
  // before executing it, so that syntactically invalid programs never
  // reach the target and are counted as `invalid` per mutator. Off by
  // default: the mutators produce 0.02% invalid programs and the minimizer
  // 0.04% invalid probes, while the parse costs as much worker CPU as the
  // mutation itself. Without it, an invalid program fails in the target
  // (a SyntaxError at parse time, as cheap as the check) and is counted
  // as `invalid` from the target's error message.
  bool syntax_check = false;

  // --- session ------------------------------------------------------------
  int jobs = 1;
  uint64_t seed = 0;  // 0: derived from the clock
  std::string out_dir = "redline-out";
  std::vector<std::string> imports;  // .js files or directories
  std::string import_root;  // base for load("...") paths (default: cwd)
  double duration_sec = 0;  // 0: until interrupted
  uint64_t max_execs = 0;   // 0: unlimited
  double stats_interval_sec = 30;
  bool component_stats = true;  // per-mutator/generator tables with the stats
  bool save_corpus = true;
  bool quiet = false;  // no periodic stats on stderr
  // Enumerate the target's globals at startup
  // (Environment::IntrospectionScript)
  // ...
  bool introspect = true;
  // ... giving the script this long; it walks every builtin.
  double introspect_timeout_ms = 20000;

  // --- policy -------------------------------------------------------------
  // Mutations applied in a row to one parent, each executed (Fuzzilli's
  // numConsecutiveMutations); the chain continues from the child only if
  // it ran successfully.
  int consecutive_mutations = 5;
  // Sub-mutations per mutator application. 0: drawn uniformly from
  // 1..Mutator::max_simultaneous() (7 for in-place and splice mutators, 3
  // for code generation, 1 for runtime-assisted and whole-program ones),
  // like Fuzzilli's BaseInstructionMutator. N > 0: exactly N.
  int mutation_rounds = 0;
  // Determinism check (Fuzzilli's processMaybeInteresting): a candidate is
  // re-executed at least min_reruns times and until the set of its new
  // edges that survived every re-execution did not change for
  // min_stable_reruns re-executions in a row; edges that disappear are
  // dropped. A candidate that has not converged after max_reruns is
  // discarded.
  int min_reruns = 5;
  int min_stable_reruns = 1;
  int max_reruns = 50;
  // Edges that failed the determinism check this many times are treated as
  // seen forever (they only cost re-runs). They are not counted as found.
  int flaky_threshold = 50;
  // Prepend fresh value declarations to the parent of every mutation chain
  // (prefix stage): between prefix_min_values and
  // prefix_min_values + prefix_extra_values of them (Fuzzilli: 10-15).
  bool value_prefix = true;
  int prefix_min_values = 10;
  int prefix_extra_values = 5;
  bool minimize = true;
  // Minimizer probes run with a timeout derived from the execution time of
  // the program being reduced:
  //   clamp(probe_timeout_factor * exec_time + probe_timeout_extra_ms,
  //         probe_timeout_min_ms, timeout_ms)
  // Generous enough for JIT-tier variance, short enough that a probe which
  // turned a terminating loop into an endless one is cheap.
  double probe_timeout_factor = 5;
  double probe_timeout_extra_ms = 20;
  double probe_timeout_min_ms = 50;
  // A minimized program must still qualify (reach its edges, or crash the
  // same way) in one of this many re-executions; otherwise the unminimized
  // program is kept. More than one because the target's own
  // non-determinism (concurrent GC, compilation) occasionally hides an edge.
  int verify_attempts = 3;
  bool minimize_crashes = true;
  bool minimize_imports = false;
  // Keep imported programs that run successfully even without new edges.
  bool import_all = false;
  // Programs generated from scratch when there is nothing to import.
  int initial_programs = 32;
  // The "init" block of the configuration (DESIGN.md 5.1) sets the import
  // fields above. The "accept" block names the Accept criterion (5.5);
  // `new-edges` is the only one today.
  std::string accept_strategy = "new-edges";
  // The "amplify" block (DESIGN.md 5.8): the strategy that searches extra
  // d8 flags for a crash that does not reproduce in a fresh target, and its
  // params as JSON text (engine/amplifier.h). "none" turns the step off.
  std::string amplify_strategy = "flags";
  std::string amplify_params = "{}";
  MinimizerOptions minimizer;
  // The minimizer's reducers, in the order they run (stages.minimize.reducers).
  // Empty: the default order (Minimizer::DefaultOrder).
  std::vector<std::string> minimizer_reducers;
  // Which strategy picks seeds, partners, mutators and generators, and
  // with which parameters (core/selection.h). JSON:
  //   "selection": {"mutators": {"strategy": "weighted",
  //                              "params": {"weights": {"Splice": 4, "Reorder":
  //                              0}}}}
  // Shorthands: "scheduler": NAME (seeds), "mutators"/"generators":
  // {"Name": weight} and the --scheduler/--mutator-weights/
  // --generator-weights/--disable-* flags. Validated when loaded: an
  // unknown strategy, parameter or weight name is an error.
  struct SelectionSettings {
    SelectionConfig seeds;
    SelectionConfig partners;
    SelectionConfig mutators;
    SelectionConfig generators;
  };
  SelectionSettings selection;

  // --- stages -------------------------------------------------------------
  // The log/capture/stats switches of every stage (engine/stages.h). The
  // stage parameters are the policy fields above; the "stages" block of
  // the configuration groups them by stage:
  //   "stages": {
  //     "prefix":   {"enabled": true, "min_values": 10, "extra_values": 5},
  //     "mutate":   {"consecutive": 5, "rounds": 0, "log": "events",
  //                  "capture": {"on": ["timeout"], "mutators": ["Splice"],
  //                  "max": 50}},
  //     "check":    {"min_reruns": 5, "min_stable_reruns": 1, "max_reruns": 50,
  //     "flaky_threshold": 50}, "minimize": {"enabled": true, "max_tests": 300,
  //     ..., "probe_timeout_factor": 5,
  //                  "verify_attempts": 3, "capture": {"on": ["rejected"]}},
  //     "publish":  {"save_corpus": true}
  //   }
  // Flags: --capture=STAGE:TRIGGER[,..][:MUTATOR[,..]][:MAX] (several,
  // separated by ';'), --stage-log=STAGE[,STAGE..].
  StageOptions stages[kNumStages];
  StageOptions& stage(Stage s) { return stages[static_cast<int>(s)]; }
  const StageOptions& stage(Stage s) const {
    return stages[static_cast<int>(s)];
  }

  // --- crashes ------------------------------------------------------------
  // Re-executions of a crashing program in a fresh target. The crash counts
  // as reproducible if any of them crashes again (with any signature) ...
  int crash_reruns = 3;
  // ... and the re-executions stop early once this many crashed again:
  // that is deterministic enough.
  int crash_reruns_stop_after = 2;
  // Crash files saved per signature; later crashes with the same signature
  // are only counted. The first `minimize_crashes_per_signature` of them
  // are minimized (if they reproduce).
  int max_crashes_per_signature = 5;
  int minimize_crashes_per_signature = 2;
  // For crashes that do not reproduce in a fresh target: up to this many of
  // the programs the same target executed before the crash are replayed in
  // front of it, to find out whether the crash depends on state left behind
  // by earlier programs (0 disables the replay) ...
  int crash_history = 100;
  // ... spending at most this long per crash.
  double crash_history_budget_sec = 90;
  // Budget for minimizing crashes (they are rare and worth more tests than
  // corpus programs).
  MinimizerOptions crash_minimizer{/*max_tests=*/1000,
                                   /*max_time_ms=*/120000,
                                   /*max_rounds=*/4,
                                   /*target_time_ms=*/30000,
                                   /*min_tests=*/200};

  // Applies --config (a file, or inline JSON starting with '{') then the
  // command line flags. `for_run` is false only for --dump-config: then no
  // target is needed and no clock seed is drawn.
  static bool Load(const Flags& flags, FuzzConfig* config, std::string* error,
                   bool for_run = true);
  // Builds the configured selectors once to report an unknown strategy,
  // parameter or weight name (called by Load).
  static bool ValidateSelection(const SelectionSettings& selection,
                                std::string* error);
  // Reports a reducer name in stages.minimize.reducers that does not exist.
  static bool ValidateReducers(const std::vector<std::string>& names,
                               std::string* error);
  // The JSON part of Load() alone (no flags, no prelude, no validation),
  // for the selftest.
  static bool LoadJsonForTest(const JsonValue& json, FuzzConfig* config,
                              std::string* error);
  // The effective configuration as JSON (one object). The compact form
  // (the `start` event) lists only the tunables that were changed and the
  // selection parameters that were given; the full form (--dump-config)
  // lists every tunable and every parameter of the chosen strategies, so
  // it documents all the defaults and can be edited and passed back with
  // --config.
  enum class Detail { kCompact, kFull };
  std::string ToJson(Detail detail = Detail::kCompact) const;
  // Documentation of the config file, for `redline help fuzz`.
  static std::string JsonHelp();
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_CONFIG_H_
