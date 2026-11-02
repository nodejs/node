// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_FUZZER_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_FUZZER_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "experimental/redline/base/json.h"
#include "experimental/redline/core/program.h"
#include "experimental/redline/corpus/corpus.h"
#include "experimental/redline/engine/amplifier.h"
#include "experimental/redline/engine/config.h"
#include "experimental/redline/engine/crash.h"
#include "experimental/redline/engine/shared_coverage.h"
#include "experimental/redline/engine/stages.h"
#include "experimental/redline/generators/generator.h"
#include "experimental/redline/minimizer/minimizer.h"
#include "experimental/redline/mutators/mutator.h"

namespace redline {

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
  std::atomic<uint64_t> crash_signatures{0};    // distinct signatures so far
  std::atomic<uint64_t> crashes_reproduced{0};  // reproduced in a fresh target
  std::atomic<uint64_t> crashes_state_dependent{0};  // only with history
  std::atomic<uint64_t> crashes_amplified{0};  // reproduced by the amplify step
  std::atomic<uint64_t> amplify_processes{
      0};  // plain d8 runs of the amplify step
  std::atomic<uint64_t> crash_replay_executions{0};  // spent on history replays
  std::atomic<uint64_t> minimizer_tests{0};
  std::atomic<uint64_t> minimizer_time_us{0};
  std::atomic<uint64_t> minimizer_nodes_removed{0};
  std::atomic<uint64_t> minimizer_rejected{0};  // result failed verification
  std::atomic<uint64_t> rerun_executions{0};
  std::atomic<uint64_t> explore_executions{0};  // instrumented runs
  std::atomic<uint64_t> explore_applied{0};     // ... that produced a mutation
  std::atomic<uint64_t> explore_timeouts{0};
  std::atomic<uint64_t> exec_time_us{0};
  std::atomic<uint64_t> fuzz_nodes{0};    // AST nodes of executed mutants
  std::atomic<uint64_t> corpus_nodes{0};  // AST nodes of added corpus programs
  std::atomic<uint64_t> mutation_time_us{0};
  std::atomic<uint64_t> infrastructure_failures{0};
};

// The coordinator of one fuzzing session: owns the configuration, the
// shared state (corpus, coverage, components, event log) and the worker
// threads. Each worker owns a target process and mutates/executes
// independently; the only shared mutable state is the corpus (mutex), the
// coverage bits (atomics) and the counters.
class Fuzzer {
 public:
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
  SharedCoverage& coverage() { return coverage_; }
  MutatorSet& mutators() { return *mutators_; }
  GeneratorSet& generators() { return *generators_; }
  Minimizer& minimizer() { return *minimizer_; }
  // Minimizes with the configured reducer order (stages.minimize.reducers).
  MinimizationResult Minimize(fast::Tree* tree,
                              const MinimizationOracle& oracle,
                              SyntaxChecker* checker,
                              const MinimizerOptions& options) const;

  // One line summarizing the session so far.
  std::string StatusLine() const;
  // The per-component tables (mutators, reducers, scheduler, generators).
  std::string ComponentTables(bool include_generators) const;

 private:
  friend class Worker;

  void LogEvent(const JsonObject& event);
  void WriteStatsEvent();
  bool ShouldStop() const;
  void CollectImports();
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
  std::unique_ptr<Corpus> corpus_;
  std::unique_ptr<MutatorSet> mutators_;
  std::unique_ptr<GeneratorSet> generators_;
  std::unique_ptr<Minimizer> minimizer_;
  // The configured reducer order (stages.minimize.reducers); empty: the
  // default order.
  std::vector<Reducer*> minimizer_reducers_;
  const Environment* env_ = nullptr;
  std::unique_ptr<Environment> discovered_env_;  // owns env_ when introspected
  Mutator* codegen_mutator_ = nullptr;
  Mutator* prefix_mutator_ = nullptr;
  ProgramPtr empty_program_;
  JsonlWriter events_;
  uint64_t start_ms_ = 0;

  // Import work queue, drained by the workers before they start fuzzing.
  std::vector<std::string> import_files_;
  std::atomic<size_t> next_import_{0};
  std::atomic<int> workers_importing_{0};

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
  void RecordCrashOutcome(const CrashSignature& signature, bool saved,
                          bool minimized, bool reproduced,
                          const std::string& path);
  // Rewrites DIR/crashes/signatures.txt.
  void WriteSignatureSummary();
  std::mutex crash_mutex_;
  std::unordered_set<uint64_t> crash_hashes_;
  std::map<uint64_t, SignatureInfo> crash_signatures_;
  std::unique_ptr<AmplifyStrategy> amplifier_;  // the amplify step (5.8)

  std::vector<std::unique_ptr<Worker>> workers_;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_FUZZER_H_
