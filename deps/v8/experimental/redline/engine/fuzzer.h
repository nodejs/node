// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_FUZZER_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_FUZZER_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "experimental/redline/base/json.h"
#include "experimental/redline/core/program.h"
#include "experimental/redline/core/swarm.h"
#include "experimental/redline/corpus/corpus.h"
#include "experimental/redline/corpus/feature_weights.h"
#include "experimental/redline/corpus/material.h"
#include "experimental/redline/engine/amplifier.h"
#include "experimental/redline/engine/config.h"
#include "experimental/redline/engine/crash.h"
#include "experimental/redline/engine/d8_flags.h"
#include "experimental/redline/engine/prelude.h"
#include "experimental/redline/engine/shared_coverage.h"
#include "experimental/redline/engine/stages.h"
#include "experimental/redline/generators/generator.h"
#include "experimental/redline/minimizer/minimizer.h"
#include "experimental/redline/mutators/mutator.h"

namespace redline {

class CoverageMap;
class Environment;
class Worker;

// Session-wide counters. Everything a worker does is accounted here (and,
// per component, in the ComponentStats of mutators/generators/reducers).
struct FuzzerStats {
  std::atomic<uint64_t> executions{0};       // all target executions
  std::atomic<uint64_t> fuzz_executions{0};  // ... of fresh mutation results
  std::atomic<uint64_t> succeeded{0};        // outcome of fuzz executions
  std::atomic<uint64_t> failed{0};
  std::atomic<uint64_t> timed_out{0};
  std::atomic<uint64_t> crashed{0};
  std::atomic<uint64_t> invalid{0};     // rejected by the V8 parser
  std::atomic<uint64_t> noop{0};        // mutator found nothing to do
  std::atomic<uint64_t> candidates{0};  // executions with new edges
  std::atomic<uint64_t> flaky{0};       // candidates that failed determinism
  std::atomic<uint64_t> corpus_added{0};
  std::atomic<uint64_t> imports_total{0};
  std::atomic<uint64_t> imports_added{0};
  std::atomic<uint64_t> imports_failed{0};  // parse error / failed / timed out
  std::atomic<uint64_t> imports_redundant{0};  // ran fine, no new edges
  std::atomic<uint64_t> unique_crashes{0};  // crashes reaching new crash edges
  std::atomic<uint64_t> duplicate_crashes{0};
  // Crashes of minimizer probes and determinism reruns (--harvest-crashes).
  std::atomic<uint64_t> harvested_crashes{0};
  std::atomic<uint64_t> crash_signatures{0};    // distinct signatures so far
  std::atomic<uint64_t> crashes_reproduced{0};  // reproduced in a fresh target
  std::atomic<uint64_t> crashes_state_dependent{0};  // only with history
  std::atomic<uint64_t> crashes_amplified{0};  // reproduced by the amplify step
  std::atomic<uint64_t> amplify_processes{
      0};  // plain d8 runs of the amplify step
  // State-dependent crashes whose NAME.bundle.js crashed in a plain d8 run.
  std::atomic<uint64_t> crashes_bundled{0};
  std::atomic<uint64_t> bundle_processes{0};  // plain d8 runs of the bundles
  // Plain d8 runs that searched the d8 flags of NAME.repro.js.
  std::atomic<uint64_t> flag_processes{0};
  // Plain d8 runs of the minimizers and of the rate checks of NAME.repro.js
  // and of the bundles.
  std::atomic<uint64_t> crash_min_processes{0};
  std::atomic<uint64_t> crash_replay_executions{0};  // spent on history replays
  std::atomic<uint64_t> minimizer_tests{0};
  std::atomic<uint64_t> minimizer_time_us{0};
  std::atomic<uint64_t> minimizer_nodes_removed{0};
  std::atomic<uint64_t> minimizer_rejected{0};  // result failed verification
  std::atomic<uint64_t> rerun_executions{0};
  // Crash reruns that timed out (with the timeout of CrashRerunTimeout).
  std::atomic<uint64_t> crash_rerun_timeouts{0};
  // Rounds of a mutation chain that ran again after a no-op mutator.
  std::atomic<uint64_t> noop_retries{0};
  // Extra runs of a published program for Program::hit_edges. Usually 0:
  // the check and the minimization record the edges of their runs.
  std::atomic<uint64_t> hit_edge_executions{0};
  std::atomic<uint64_t> explore_executions{0};  // instrumented runs
  std::atomic<uint64_t> explore_applied{0};     // ... that produced a mutation
  std::atomic<uint64_t> explore_timeouts{0};
  std::atomic<uint64_t> explore_exec_time_us{0};  // target time of these runs
  std::atomic<uint64_t> salvage_attempted{0};  // reruns after wrapping a throw
  std::atomic<uint64_t> salvage_succeeded{0};  // salvage reruns that succeeded
  // Failed mutants without new edges whose throw was in the last top-level
  // statement. They got no salvage rerun.
  std::atomic<uint64_t> salvage_skipped_tail{0};
  // Checks that converged after clean_min_reruns reruns.
  std::atomic<uint64_t> clean_checks{0};
  // Checked candidates that merged into the pending commit of their chain,
  // and checked candidates that replaced it (ChainCommits::Offer).
  std::atomic<uint64_t> coalesce_merges{0};
  std::atomic<uint64_t> coalesce_breaks{0};
  // Mutation chains focused on one function body.
  std::atomic<uint64_t> focus_chains{0};
  std::atomic<uint64_t> exec_time_us{0};
  std::atomic<uint64_t> fuzz_nodes{0};    // AST nodes of executed mutants
  std::atomic<uint64_t> corpus_nodes{0};  // AST nodes of added corpus programs
  std::atomic<uint64_t> mutation_time_us{0};
  std::atomic<uint64_t> infrastructure_failures{0};
  std::atomic<uint64_t> swarm_epochs{0};  // swarm masks drawn by the workers
  // With the tunable `prelude.on_demand` (engine/prelude.h): the executions
  // behind a part of the prelude, and the prelude statements of their
  // scripts. For each PreludeFallback, the executions that got the whole
  // prelude because of this rule.
  std::atomic<uint64_t> prelude_scripts{0};
  std::atomic<uint64_t> prelude_statements{0};
  std::atomic<uint64_t> prelude_fallbacks[kNumPreludeFallbacks] = {};
};

// The timeout of a minimizer probe for a program that runs in
// exec_time_us (FuzzConfig::probe_timeout_*).
uint64_t MinimizerProbeTimeout(const FuzzConfig& config, uint64_t exec_time_us);
// The timeout of the next minimizer probe after the minimizer kept a
// reduction whose probe ran in accepted_exec_us. The result is between
// probe_timeout_min_ms and first_timeout_us
// (FuzzConfig::probe_timeout_tighten_*).
uint64_t TightenedProbeTimeout(const FuzzConfig& config,
                               uint64_t first_timeout_us,
                               uint64_t accepted_exec_us);

// The timeout of a crash rerun: crash_rerun_timeout_factor times the target
// timeout. A crash that occurs near the timeout then reproduces.
uint64_t CrashRerunTimeout(const FuzzConfig& config);

// The timeout of the instrumented run of a runtime-assisted mutator: the
// tunable `explore.timeout_factor` times the target timeout. The default
// factor is 4, as in Fuzzilli.
uint64_t InstrumentedRunTimeout(const FuzzConfig& config);

// The number of workers that admit a program for new optimized code
// features alone (coverage.opt_admit_workers_fraction of `jobs`, rounded
// up). The other workers only weigh these features.
int OptimizedAdmitWorkers(double fraction, int jobs);

// The script that makes the target crash on purpose: kind 1 is a failed
// CHECK, kind 2 a failed DCHECK (the `fuzzilli` or `redline` builtin of d8).
std::string CrashProbeScript(bool fuzzilli_target, int kind);
// True if `args` has a d8 flag with which a failed CHECK or DCHECK does not
// crash: --hole-fuzzing, --sandbox-fuzzing, --sandbox-trap-fuzzing or
// --sandbox-testing. These modes report only memory corruption, thus the crash
// probes of Fuzzer::CheckTargetCrashes do not apply.
bool FiltersCrashProbes(const std::vector<std::string>& args);

// `text` with the line terminators of JavaScript (\n, \r, U+2028, U+2029)
// written as escape sequences, so that it fits in one `//` comment.
std::string EscapeCommentLine(std::string_view text);
// The comment lines at the top of a saved corpus file: the lineage, and the
// d8 flags of the program after kRedlineFlagsPrefix (engine/importer.h).
std::string CorpusFileHeader(const Program& program);

// The candidates of the swarm mask draw (core/swarm.h): the weights of the
// strategies, the core generators and the structural mutators. Returns false
// if a name of SwarmCoreGenerators() or SwarmStructuralMutators() does not
// exist.
bool BuildSwarmCandidates(const GeneratorSet& generators,
                          const MutatorSet& mutators,
                          SwarmCandidates* swarm_generators,
                          SwarmCandidates* swarm_mutators, std::string* error);

// Collects Program::hit_edges from the runs that the commit of a program
// does: the edges that all recorded runs of the published program reached.
// The edges of the minimized program come from the last kept probe and
// from the verification run. These two runs replace the runs of the
// original program. The minimizer removes empty statements after the last
// probe, thus the verification run of the exact program limits the set.
class HitEdges {
 public:
  // Records a run of the original program: the first run or a rerun.
  void RecordRun(std::vector<uint32_t> edges);
  // Records a probe that the minimizer kept. It replaces the earlier probe.
  void RecordProbe(std::vector<uint32_t> edges);
  // Records the verification run of the minimized program. Then the last
  // probe and this run replace all runs of the original program.
  void RecordVerifiedRun(std::vector<uint32_t> edges);
  void Clear();
  // The number of runs that the edges come from.
  int runs() const { return runs_; }
  // The sorted edges that all recorded runs reached.
  const std::vector<uint32_t>& edges() const { return edges_; }
  std::vector<uint32_t> Take();

 private:
  // Keeps the edges that `edges` also contains, or takes `edges` if no
  // run is in the record.
  void Intersect(std::vector<uint32_t> edges);

  std::vector<uint32_t> edges_;
  int runs_ = 0;
  std::vector<uint32_t> probe_;
  bool has_probe_ = false;
};

// --- determinism check ----------------------------------------------------

// The rules of the determinism check (stage check). See FuzzConfig.
struct CheckRules {
  int min_reruns = 5;
  int clean_min_reruns = 2;
  int min_stable_reruns = 1;
  int max_reruns = 50;
  int flaky_threshold = 50;
  // Keep a program without new edges (import_all).
  bool keep_without_edges = false;
};

// One rerun of a candidate, as the determinism check sees it.
struct CheckRun {
  bool succeeded = false;
  // The candidate edges that the run did not reach (sorted).
  std::vector<uint32_t> missing;
  // The run reached all edges of the pending commit of the chain.
  bool pending_held = true;
};

// The result of RunDeterminismCheck.
struct CheckResult {
  std::vector<uint32_t> edges;  // the candidate edges that all runs reached
  int reruns = 0;
  size_t dropped = 0;  // edges that a rerun did not reach
  bool deterministic = true;
  bool converged = false;
  bool survived = false;
  // Converged after clean_min_reruns, because no edge was flaky.
  bool clean = false;
  // The first run and all reruns reached all edges of the pending commit.
  bool pending_held = true;
};

// Runs the determinism check of the `claimed` edges of a candidate. `rerun`
// runs the candidate once for the edges that are still in the set. It
// returns false on an infrastructure failure. `pending_held` tells if the
// first run reached the edges of the pending commit.
//
// The check runs the candidate min_reruns times. If no edge of `claimed`
// failed a check before (SharedCoverage::AnyFlaky) and no rerun lost an
// edge, clean_min_reruns runs are sufficient. An edge that a successful
// rerun did not reach goes to SharedCoverage::RecordFlaky, and then back to
// `shared` if it is below the threshold. The check can also drop the
// candidate because a rerun did not succeed or because the edges did not
// settle. Then its other edges go back to `shared` without a flaky count.
// The check changes only
// edges of `claimed`: an edge of the pending commit that a run did not reach
// stays claimed and is not flaky.
CheckResult RunDeterminismCheck(
    SharedCoverage& shared, const CheckRules& rules,
    std::vector<uint32_t> claimed, bool pending_held,
    const std::function<bool(const std::vector<uint32_t>& edges,
                             CheckRun* run)>& rerun);

// --- coalesced commit -----------------------------------------------------

// The credit of one candidate in a commit: the component statistics that
// get its executions and its edges, and the seed that gets the result.
struct CommitCredit {
  ComponentStats* stats = nullptr;  // nullptr: no component gets credit
  std::vector<Generator*> generators;
  ProgramPtr seed;              // nullptr: no Corpus::OnResult
  std::vector<uint32_t> edges;  // the new edges of this candidate
};

// A candidate that passed the determinism check. The fields keep the data
// of the check runs, because later runs change the coverage map.
struct CheckedCandidate {
  std::shared_ptr<Program> program;
  std::vector<uint32_t> edges;  // the minimization property
  HitEdges hit_edges;
  uint32_t edges_hit = 0;
  uint64_t exec_time_us = 0;
  int reruns = 0;
  size_t dropped = 0;
  bool minimize = true;
  CommitCredit credit;
};

// The checked candidates of a mutation chain that wait for one
// minimization. `candidate` is the last candidate and has the union of the
// edges. `credits` has one entry for each candidate.
struct PendingCommit {
  CheckedCandidate candidate;
  std::vector<CommitCredit> credits;
  int merges = 0;
};

// The limits of a merge (FuzzConfig::max_coalesce_*).
struct CoalesceLimits {
  int max_merges = 3;
  size_t max_edges = 2048;
  size_t max_nodes = 2000;
};

// Splits `executions` between candidates in proportion to their new edges
// `edges`. The parts add up to `executions`. If no candidate has edges, the
// parts are almost equal.
std::vector<uint64_t> SplitByEdges(uint64_t executions,
                                   const std::vector<size_t>& edges);

// Holds the pending commit of one mutation chain. Offer() merges a new
// checked candidate into it or replaces it. The destructor finalizes the
// pending commit. Thus each exit of the chain (its end, a crash, a stop
// request, an infrastructure failure) publishes the claimed edges.
class ChainCommits {
 public:
  using Finalizer = std::function<void(PendingCommit)>;
  enum class Offered { kStarted, kMerged, kReplaced };

  ChainCommits(CoalesceLimits limits, Finalizer finalize)
      : limits_(limits), finalize_(std::move(finalize)) {}
  ~ChainCommits();
  ChainCommits(const ChainCommits&) = delete;
  ChainCommits& operator=(const ChainCommits&) = delete;

  bool has_pending() const { return pending_ != nullptr; }
  // The edges of the pending commit, or nullptr.
  const std::vector<uint32_t>* pending_edges() const {
    return pending_ != nullptr ? &pending_->candidate.edges : nullptr;
  }
  // Whether `candidate` can merge into the pending commit. There must be a
  // pending commit, and all runs of the candidate reached its edges
  // (`pending_held`). The merge must also stay in the limits.
  bool CanMerge(const CheckedCandidate& candidate, bool pending_held) const;
  // Takes `candidate`. If CanMerge(), the candidate merges. Otherwise the
  // pending commit (if any) is finalized first, and the candidate becomes
  // the pending commit.
  Offered Offer(CheckedCandidate candidate, bool pending_held);
  // Finalizes the pending commit, if there is one.
  void Flush();

 private:
  CoalesceLimits limits_;
  Finalizer finalize_;
  std::unique_ptr<PendingCommit> pending_;
};

// The coordinator of one fuzzing session: owns the configuration, the
// shared state (corpus, coverage, components, event log) and the worker
// threads. Each worker owns a target process and mutates/executes
// independently; the only shared mutable state is the corpus (mutex), the
// coverage bits (atomics) and the counters.
class Fuzzer {
 public:
  // Most AST nodes that a prefix result or mutant can hold before printing.
  static constexpr size_t kMaxMutantNodes = 100000;

  explicit Fuzzer(FuzzConfig config);
  ~Fuzzer();
  Fuzzer(const Fuzzer&) = delete;
  Fuzzer& operator=(const Fuzzer&) = delete;

  // Creates the output directory, the event log and the components, and
  // initializes the in-process V8. Returns false with a message on failure.
  bool Initialize(std::string* error);
  // Runs the session until a stop condition (duration, max_execs, stop
  // request) is met. Returns a process exit code.
  int Run();
  // Asks all workers to stop after their current step (signal-safe).
  void RequestStop() { stop_.store(true, std::memory_order_release); }
  bool stop_requested() const { return stop_.load(std::memory_order_acquire); }

  const FuzzConfig& config() const { return config_; }
  const FuzzerStats& stats() const { return stats_; }
  Corpus& corpus() { return *corpus_; }
  const MaterialBank& material() const { return *material_; }
  Component& fuse_component() { return fuse_component_; }
  SharedCoverage& coverage() { return coverage_; }
  MutatorSet& mutators() { return *mutators_; }
  GeneratorSet& generators() { return *generators_; }
  Minimizer& minimizer() { return *minimizer_; }
  // Minimizes with the configured reducer order (stages.minimize.reducers).
  MinimizationResult Minimize(fast::Tree* tree,
                              const MinimizationOracle& oracle,
                              SyntaxChecker* checker,
                              const MinimizerOptions& options) const;
  // Minimizes a crash with the reducer order of crash_minimizer.reducers.
  MinimizationResult MinimizeCrash(fast::Tree* tree,
                                   const MinimizationOracle& oracle,
                                   SyntaxChecker* checker,
                                   const MinimizerOptions& options) const;

  // One line summarizing the session so far.
  std::string StatusLine() const;
  // The per-component tables (mutators, reducers, scheduler, generators).
  std::string ComponentTables(bool include_generators) const;

  // Target-guided `d8` flag state for change-based fuzzing.
  void SetTargetsForTest(std::vector<std::pair<uint32_t, double>> targets) {
    targets_ = std::move(targets);
  }
  void RecordTargetFlags(const std::vector<std::string>& args, double score);
  void RecordTargetChild(bool hit);
  // The flag affinity of normal mode with randomize_d8_flags. An imported
  // test gives its `// Flags:` set. A target process gives its random flags,
  // the new edges that the workers found with them and its executions.
  void RecordImportedFlags(const std::vector<std::string>& flags,
                           double score = 1.0);
  void RecordMaterialFlags(const MaterialBank& bank);
  void RecordProductiveFlags(const std::vector<std::string>& flags,
                             uint64_t new_edges, uint64_t executions);
  // Records a start of a target process with the random flags `args`
  // (D8FlagBlacklist).
  void RecordSpawn(const std::vector<std::string>& args, bool started);
  const D8FlagBlacklist& flag_blacklist() const { return flag_blacklist_; }
  size_t imported_flag_sets() const { return imported_flags_.size(); }
  size_t productive_flag_sets() const { return productive_flags_.size(); }
  // The flags for a new target process: a random draw, the d8 trials, and a
  // set of the flag pools. DrawD8Flags removes the blocked flags.
  std::vector<std::string> DrawD8Flags(Random& rng);
  uint64_t target_children_completed() const {
    return target_children_completed_.load(std::memory_order_acquire);
  }
  uint64_t target_children_hit() const {
    return target_children_hit_.load(std::memory_order_acquire);
  }
  // For the selftest: one crash of `signature` through the crash
  // bookkeeping (RegisterCrash, then RecordCrashOutcome). The minimization
  // succeeds if the crash gets a minimize slot and `minimization_succeeds`
  // is true. Returns true if the crash got a minimize slot.
  bool RecordCrashForTest(uint64_t source_hash, const CrashSignature& signature,
                          bool minimization_succeeds);

 private:
  friend class Worker;

  void LogEvent(const JsonObject& event);
  void WriteStatsEvent();
  bool ShouldStop() const;
  void CollectImports();
  void FinishImportWorker();
  // Builds the feature weights after the workers started (coverage counters
  // only). `targets_code_edges` is the code edge count of the targets file.
  bool InitializeFeatureWeights(uint32_t targets_code_edges,
                                std::string* error);
  // Makes the target fail a CHECK and a DCHECK on purpose, like the startup
  // tests of Fuzzilli. Returns false if the CHECK does not crash the target,
  // or if the DCHECK does not crash it without allow_no_dcheck.
  bool CheckTargetCrashes(std::string* error);
  bool SaveCorpusProgram(const Program& program);
  // Saves the programs of a stage that matched its capture rule (see
  // engine/stages.h) under out_dir/captures/<stage>/. Returns false once
  // the rule's cap is reached.
  bool WriteCapture(Stage stage, CaptureTrigger trigger,
                    std::string_view mutator, const Program* parent,
                    const std::string& child, const JsonObject& details,
                    int worker);
  // Records a fatal problem and stops the session.
  void Fail(const std::string& message);

  FuzzConfig config_;
  FuzzerStats stats_;
  StageTable stages_;                                // per-stage counters
  std::atomic<uint64_t> captures_[kNumStages] = {};  // files written per stage
  std::atomic<bool> stop_{false};
  std::atomic<bool> failed_{false};
  std::string failure_;
  std::mutex failure_mutex_;

  SharedCoverage coverage_;
  // Coverage counters and change-based fuzzing (exec/coverage.h): the targets
  // of --targets, the weight table that the rarity strategy reads, and the
  // feature ranges for the statistics. Empty without coverage counters.
  std::vector<std::pair<uint32_t, double>> targets_;
  FeatureWeights::EdgeRanges focus_;
  std::unique_ptr<FeatureWeights> feature_weights_;
  // Change-based fuzzing with a focus: removes the features outside the focus
  // from the new features of a program. They stay claimed, so they do not
  // count again. Returns false if no feature is left (the program is not kept).
  // With `admit_optimized` (the coverage map of a worker of
  // OptimizedAdmitWorkers), the optimized code features also stay.
  bool KeepFocused(std::vector<uint32_t>* claimed,
                   const CoverageMap* admit_optimized = nullptr);
  // The workers with an id below this admit programs for new optimized code
  // features alone (OptimizedAdmitWorkers).
  int optimized_admit_workers_ = 0;
  // Programs and features that KeepFocused() removed.
  std::atomic<uint64_t> unfocused_programs_{0};
  std::atomic<uint64_t> unfocused_features_{0};
  struct CoverageGeometry {
    bool counters = false;
    uint32_t optimized_first = 0;
    uint32_t optimized_count = 0;
    uint32_t bucket_first = 0;
    uint32_t features = 0;
    uint32_t watched = 0;
  } geometry_;
  // " | focus x/y | target x/y | opt n | buckets n" for the status line.
  // Empty without coverage counters and without optimized code features.
  // AddCoverageStats() adds the same numbers to an event.
  std::string CoverageStatus() const;
  void AddCoverageStats(JsonObject* event) const;
  // Number of focus edges that the session reached.
  uint32_t FocusSeen() const;
  // The parts of DrawD8Flags for normal mode and for change-based fuzzing.
  // `fresh` is the random draw with the d8 trials.
  std::vector<std::string> DrawNormalModeFlags(Random& rng,
                                               std::vector<std::string> fresh);
  std::vector<std::string> DrawTargetFlags(Random& rng,
                                           std::vector<std::string> fresh);
  // Target-guided d8 flag pool for change-based fuzzing with
  // randomize_d8_flags.
  FlagSetPool target_flags_{64};
  std::atomic<uint64_t> target_children_completed_{0};
  std::atomic<uint64_t> target_children_hit_{0};
  // The flag pools of normal mode with randomize_d8_flags. One pool has the
  // `// Flags:` sets of the imported tests. The other pool has the random
  // flag sets that found more new edges than the mean.
  FlagSetPool imported_flags_{2048};
  FlagSetPool productive_flags_{64};
  std::mutex productive_mutex_;
  double productive_mean_ = 0;  // moving mean of the new edges per process
  uint64_t productive_processes_ = 0;
  D8FlagBlacklist flag_blacklist_;
  // Corpus programs with Program::flags.
  std::atomic<uint64_t> flagged_programs_{0};
  // The draws of DrawD8Flags, the draws that started from a pool, and the
  // flag names drawn in the session.
  std::atomic<uint64_t> flag_draws_{0};
  std::atomic<uint64_t> imported_flag_draws_{0};
  std::atomic<uint64_t> affinity_flag_draws_{0};
  std::mutex drawn_names_mutex_;
  std::set<std::string, std::less<>> drawn_names_;
  std::unique_ptr<Corpus> corpus_;
  std::unique_ptr<MaterialBank> material_;
  MaterialBank::LoadStats material_stats_;
  Component fuse_component_{"Fuse"};
  std::unique_ptr<MutatorSet> mutators_;
  std::unique_ptr<GeneratorSet> generators_;
  // The prelude as a list of statements, if the tunable `prelude.on_demand`
  // is on (engine/prelude.h). Else nullptr: each program gets the whole
  // prelude. Set in Initialize(), read-only after it.
  std::unique_ptr<OnDemandPrelude> on_demand_prelude_;
  std::unique_ptr<Minimizer> minimizer_;
  // The configured reducer order (stages.minimize.reducers); empty: the
  // default order.
  std::vector<Reducer*> minimizer_reducers_;
  // The reducer order of the crash minimizer (crash_minimizer.reducers, or
  // Minimizer::CrashOrder()).
  std::vector<Reducer*> crash_reducers_;
  const Environment* env_ = nullptr;
  std::unique_ptr<Environment> discovered_env_;  // owns env_ when introspected
  Mutator* codegen_mutator_ = nullptr;
  Mutator* prefix_mutator_ = nullptr;
  // Swarm testing (selection.swarm, core/swarm.h): the settings and the
  // candidates of the mask draw. Set in Initialize(), read-only after it.
  SwarmSettings swarm_settings_;
  SwarmCandidates swarm_generators_;
  SwarmCandidates swarm_mutators_;
  ProgramPtr empty_program_;
  JsonlWriter events_;
  uint64_t start_ms_ = 0;

  // Import work queue, drained by the workers before they start fuzzing.
  std::vector<std::string> import_files_;
  std::atomic<size_t> next_import_{0};
  std::atomic<int> workers_importing_{0};
  std::atomic<int> import_workers_pending_{0};

  // Crash bookkeeping, by signature.
  struct SignatureInfo {
    std::string text;
    int count = 0;       // crashes seen with this signature
    int saved = 0;       // ... whose program was written to disk
    int minimized = 0;   // ... that were minimized
    int reproduced = 0;  // ... that reproduced in a fresh target
    int amplify = 0;     // ... that ran the amplify step
    std::string first_path;
    double first_seen_sec = 0;
  };
  // What ProcessCrash should do with a crash, decided under crash_mutex_.
  struct CrashDecision {
    bool new_source = false;  // this exact program was not seen crashing before
    bool save = false;        // write the files
    bool minimize = false;    // spend minimization budget on it
    int signature_count = 0;  // including this crash
  };
  CrashDecision RegisterCrash(uint64_t source_hash,
                              const CrashSignature& signature);
  // True if the amplify step may run for this signature now (reserves it).
  bool ClaimAmplify(const CrashSignature& signature);
  // Records the result of a crash that RegisterCrash decided. Call it once
  // for each call of RegisterCrash, with the same signature and decision.
  void RecordCrashOutcome(const CrashSignature& signature,
                          const CrashDecision& decision, bool minimized,
                          bool reproduced, const std::string& path);
  // Rewrites DIR/crashes/signatures.txt.
  void WriteSignatureSummary();
  std::mutex crash_mutex_;
  std::unordered_set<uint64_t> crash_hashes_;
  std::map<uint64_t, SignatureInfo> crash_signatures_;
  std::unique_ptr<AmplifyStrategy> amplifier_;  // the amplify step (5.8)

  std::vector<std::unique_ptr<Worker>> workers_;
};

// True if `tree` has more than `max_nodes` AST nodes.
bool ExceedsMutantNodeLimit(const fast::Tree& tree,
                            size_t max_nodes = Fuzzer::kMaxMutantNodes);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_FUZZER_H_
