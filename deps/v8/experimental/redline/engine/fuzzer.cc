// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/fuzzer.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iterator>
#include <thread>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/base/v8_runtime.h"
#include "experimental/redline/core/environment.h"
#include "experimental/redline/engine/d8_flags.h"
#include "experimental/redline/engine/flag_minimizer.h"
#include "experimental/redline/engine/importer.h"
#include "experimental/redline/engine/prelude.h"
#include "experimental/redline/exec/process_run.h"
#include "experimental/redline/exec/runner.h"
#include "experimental/redline/fast/parser.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/mutators/explore.h"

namespace redline {

namespace {

constexpr char kLog[] = "fuzz";
constexpr size_t kMaxSavedOutput = static_cast<size_t>(64) * 1024;

// Keeps the random d8 flags of a target while it lives: a new target
// process gets the flags of the last one. The reruns, the minimization and
// the crash triage of a program run with the flags that ran it.
class FlagPin {
 public:
  explicit FlagPin(ReprlChild& child) : child_(child), was_(child.pinned()) {
    child_.Pin(true);
  }
  ~FlagPin() { child_.Pin(was_); }
  FlagPin(const FlagPin&) = delete;
  FlagPin& operator=(const FlagPin&) = delete;

 private:
  ReprlChild& child_;
  bool was_;
};

// Runs a function when the scope ends (all exits of a loop iteration).
class ScopeExit {
 public:
  explicit ScopeExit(std::function<void()> fn) : fn_(std::move(fn)) {}
  ~ScopeExit() { fn_(); }
  ScopeExit(const ScopeExit&) = delete;
  ScopeExit& operator=(const ScopeExit&) = delete;

 private:
  std::function<void()> fn_;
};

// {"strategy": NAME, "params": {...}} of a selector as built, for the
// `start` event.
template <typename T>
std::string EffectiveSelector(const Selector<T>& selector) {
  JsonObject o;
  o.Add("strategy", selector.name());
  o.AddRaw("params", selector.Params().ToString());
  return o.ToString();
}

std::string FormatDuration(uint64_t seconds) {
  return StringPrintf("%02llu:%02llu:%02llu",
                      static_cast<unsigned long long>(seconds / 3600),
                      static_cast<unsigned long long>((seconds / 60) % 60),
                      static_cast<unsigned long long>(seconds % 60));
}

double Percent(uint64_t part, uint64_t total) {
  return total == 0
             ? 0.0
             : 100.0 * static_cast<double>(part) / static_cast<double>(total);
}

std::string Truncate(const std::string& text, size_t limit) {
  if (text.size() <= limit) {
    return text;
  }
  return text.substr(0, limit) + "\n... (truncated)\n";
}

// `path` as an absolute path without symbolic links, or `path` if it does
// not exist.
std::string AbsolutePath(const std::string& path) {
  char* resolved = realpath(path.c_str(), nullptr);
  if (resolved == nullptr) {
    return path;
  }
  std::string result(resolved);
  free(resolved);
  return result;
}

}  // namespace

// ---------------------------------------------------------------------------
// Worker: one thread, one target process.

class Worker {
 public:
  Worker(Fuzzer* fuzzer, int id)
      : fuzzer_(fuzzer),
        id_(id),
        rng_(fuzzer->config().seed * 0x9E3779B97F4A7C15ull +
             static_cast<uint64_t>(id) + 1),
        flags_rng_((fuzzer->config().seed * 0x9E3779B97F4A7C15ull +
                    static_cast<uint64_t>(id) + 1) ^
                   kFlagsSeedSalt) {}

  int id() const { return id_; }

  // Starts the target synchronously so that configuration problems are
  // reported before any thread runs.
  bool Start(std::string* error) {
    TargetOptions target = fuzzer_->config().target;
    if (config().randomize_d8_flags) {
      // Each new target process gets new random flags (DESIGN.md 5.4).
      target.random_args = [this] {
        if (!fuzzer_->targets_.empty() && child_execs_ > 0) {
          fuzzer_->RecordTargetChild(child_target_hits_ > 0);
        }
        child_target_buckets_.assign(fuzzer_->targets_.size(), 0);
        child_target_hits_ = 0;
        child_execs_ = 0;
        return fuzzer_->DrawD8Flags(flags_rng_);
      };
    }
    runner_ = ScriptRunner::Create(target, id_, error);
    if (runner_ == nullptr) {
      return false;
    }
    if (!runner_->Initialize(error)) {
      return false;
    }
    const CoverageMap& coverage = runner_->coverage();
    return fuzzer_->coverage_.Initialize(
        coverage.num_edges(), coverage.bitmap_size(), error,
        coverage.counted_edges(), coverage.real_edges());
  }

  const CoverageMap& coverage() const { return runner_->coverage(); }

  void Launch() {
    thread_ = std::thread([this] { Run(); });
  }

  // Runs the environment introspection script in this worker's target and
  // returns what it printed on fuzzout. Called before Launch().
  bool Introspect(std::string* json, std::string* error) {
    Execution execution;
    const uint64_t timeout_us =
        static_cast<uint64_t>(config().introspect_timeout_ms * 1000.0);
    if (!runner_->RunWithoutEvaluation(Environment::IntrospectionScript(),
                                       timeout_us, &execution, error)) {
      return false;
    }
    if (!execution.succeeded()) {
      *error = StringPrintf("introspection script %s: %s",
                            OutcomeName(execution.outcome),
                            FirstLines(execution.stderr_output).c_str());
      return false;
    }
    *json = execution.fuzzout;
    return true;
  }
  void Join() {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

 private:
  const FuzzConfig& config() const { return fuzzer_->config(); }
  FuzzerStats& stats() { return fuzzer_->stats_; }
  bool stopping() const { return fuzzer_->stop_requested(); }
  // The syntax checker for minimizer probes: only with syntax_check on
  // (nullptr makes the minimizer skip the in-process parse).
  SyntaxChecker* probe_checker() const {
    return config().syntax_check ? checker_.get() : nullptr;
  }

  void Run() {
    checker_ = std::make_unique<SyntaxChecker>();
    ImportPhase();
    while (!stopping()) {
      FuzzOne();
    }
    if (config().randomize_d8_flags && !fuzzer_->targets_.empty() &&
        child_execs_ > 0) {
      fuzzer_->RecordTargetChild(child_target_hits_ > 0);
      child_execs_ = 0;
    }
    // Let the target go now; the main thread only joins.
    runner_.reset();
  }

  // --- execution --------------------------------------------------------

  // The script that the target runs for `source`: the prelude, then the
  // program.
  std::string ScriptFor(const std::string& source) const {
    const std::string& prelude = config().prelude;
    if (prelude.empty()) {
      return source;
    }
    std::string script;
    script.reserve(prelude.size() + 1 + source.size());
    script.append(prelude).append("\n").append(source);
    return script;
  }

  // Runs `source` behind the prelude without touching per-runner coverage
  // bookkeeping. Returns false on an infrastructure failure.
  bool Execute(const std::string& source, Execution* execution,
               uint64_t timeout_us = 0) {
    const bool ok = config().prelude.empty()
                        ? RunScript(source, execution, timeout_us)
                        : RunScript(ScriptFor(source), execution, timeout_us);
    if (ok) {
      RememberExecution(source);
    }
    return ok;
  }

  // Runs `script` as it is, without the prelude. The crash minimizer uses it
  // for the tree of the prelude and the program. The history holds only
  // programs for Execute(), so it starts again after the script.
  bool ExecuteScript(const std::string& script, Execution* execution,
                     uint64_t timeout_us = 0) {
    const bool ok = RunScript(script, execution, timeout_us);
    if (ok) {
      ForgetHistory();
    }
    return ok;
  }

  // Runs `script` in the target and counts the execution. Returns false on
  // an infrastructure failure.
  bool RunScript(const std::string& script, Execution* execution,
                 uint64_t timeout_us) {
    if (timeout_us == 0) {
      timeout_us = config().target.timeout_us;
    }
    std::string error;
    if (!runner_->RunWithoutEvaluation(script, timeout_us, execution, &error)) {
      stats().infrastructure_failures++;
      ForgetHistory();
      if (++consecutive_failures_ >= 10) {
        fuzzer_->Fail(StringPrintf("worker %d: target keeps failing: %s", id_,
                                   error.c_str()));
      } else {
        REDLINE_WARN(kLog, "worker %d: execution failed: %s", id_,
                     error.c_str());
      }
      return false;
    }
    consecutive_failures_ = 0;
    executions_++;
    child_execs_++;
    stats().executions++;
    stats().exec_time_us += execution->exec_time_us;
    if (execution->succeeded()) {
      ObserveTargetCoverage();
    }
    return true;
  }

  // The number of target features in `edges` (0 outside change-based fuzzing).
  static uint64_t TargetFeatures(const std::vector<uint32_t>& edges) {
    const FeatureWeights* weights = FeatureWeights::Active();
    if (weights == nullptr) {
      return 0;
    }
    return static_cast<uint64_t>(
        std::count_if(edges.begin(), edges.end(),
                      [&](uint32_t edge) { return weights->IsTarget(edge); }));
  }

  // Tracks per-child target bucket coverage in change-based fuzzing and records
  // flag combinations that reach target edges or higher hit-count buckets.
  void ObserveTargetCoverage() {
    if (fuzzer_->targets_.empty() || !config().randomize_d8_flags) {
      return;
    }
    if (child_target_buckets_.size() != fuzzer_->targets_.size()) {
      child_target_buckets_.assign(fuzzer_->targets_.size(), 0);
    }
    bool hit_any = false;
    bool new_bucket = false;
    for (size_t i = 0; i < fuzzer_->targets_.size(); i++) {
      uint32_t count = runner_->coverage().HitCount(fuzzer_->targets_[i].first);
      if (count == 0) {
        continue;
      }
      hit_any = true;
      uint8_t bit =
          static_cast<uint8_t>(1u << CoverageMap::Bucket(static_cast<uint8_t>(
                                   std::min<uint32_t>(count, 255))));
      if ((child_target_buckets_[i] & bit) == 0) {
        child_target_buckets_[i] |= bit;
        new_bucket = true;
      }
    }
    if (!hit_any) {
      return;
    }
    child_target_hits_++;
    if (!new_bucket) {
      return;
    }
    double score = 0;
    for (size_t i = 0; i < fuzzer_->targets_.size(); i++) {
      uint8_t bits = child_target_buckets_[i];
      if (bits != 0) {
        score += fuzzer_->targets_[i].second *
                 (4.0 + static_cast<double>(std::popcount(bits)) +
                  static_cast<double>(std::bit_width(bits)));
      }
    }
    fuzzer_->RecordTargetFlags(runner_->child().random_args(), score);
  }

  // Respawns an unpinned child in change-based fuzzing when it has run
  // target_probe_execs executions without reaching a single target edge.
  void MaybeRespawnForTargets() {
    if (fuzzer_->targets_.empty() || !config().randomize_d8_flags) {
      return;
    }
    if (runner_->child().pinned() || child_target_hits_ > 0) {
      return;
    }
    const int probe = TargetProbeExecs();
    if (probe > 0 && runner_->child().execs_since_draw() >= probe) {
      runner_->child().Kill();
    }
  }

  // The programs the current target process executed, oldest first (bounded
  // by crash_history entries and kHistoryBytes). A fresh process starts an
  // empty history; the entry for the execution that killed a process stays
  // at the end until the next execution.
  void RememberExecution(const std::string& source) {
    if (config().crash_history <= 0) {
      return;
    }
    if (runner_->child().execs_since_spawn() <= 1) {
      ForgetHistory();
    }
    history_.push_back(source);
    history_bytes_ += source.size();
    while (history_.size() > static_cast<size_t>(config().crash_history) ||
           (history_bytes_ > kHistoryBytes && history_.size() > 1)) {
      history_bytes_ -= history_.front().size();
      history_.pop_front();
    }
  }
  void ForgetHistory() {
    history_.clear();
    history_bytes_ = 0;
  }
  static constexpr size_t kHistoryBytes = static_cast<size_t>(8) * 1024 * 1024;

  // Timeout for minimizer probes, derived from the execution time of the
  // program being reduced (stages.minimize.probe_timeout_*): generous
  // enough for JIT-tier variance, short enough that a probe which runs
  // forever is cheap.
  uint64_t ProbeTimeout(uint64_t exec_time_us) const {
    const FuzzConfig& c = config();
    auto probe = static_cast<uint64_t>(c.probe_timeout_factor *
                                           static_cast<double>(exec_time_us) +
                                       c.probe_timeout_extra_ms * 1000.0);
    probe =
        std::max(probe, static_cast<uint64_t>(c.probe_timeout_min_ms * 1000.0));
    return std::min(probe, c.target.timeout_us);
  }

  // Bookkeeping for a mutant that is not valid JavaScript: per-mutator and
  // global counters, the verbose event, the capture and the mutant event.
  // Without the in-process syntax check the verdict comes from the target
  // (Execution::IsParseError()).
  void CountInvalid(Mutator& mutator, const std::string& detail,
                    uint32_t ancestor_id, const Program* parent,
                    const std::string& source, const std::string& message) {
    mutator.stats().invalid++;
    stats().invalid++;
    if (GetLogLevel() == LogLevel::kVerbose) {
      JsonObject event;
      event.Add("event", "invalid")
          .Add("mutator", mutator.name())
          .Add("detail", detail);
      event.Add("error", message).Add("parent", ancestor_id).Add("worker", id_);
      fuzzer_->LogEvent(event);
    }
    JsonObject why;
    why.Add("error", message);
    Capture(Stage::kMutate, CaptureTrigger::kInvalid, mutator.name(), parent,
            source, why);
    LogMutant(mutator, detail, ancestor_id, "invalid", nullptr, 0);
  }

  // Feedback for the mutator strategy (core/selection.h). "interesting"
  // is only known after the determinism check, so it is reported as false
  // here; the strategy can read it later from the mutator's stats.
  void ReportToSelector(Mutator& mutator, const Execution& execution) {
    SelectionOutcome outcome;
    outcome.time_us = execution.exec_time_us;
    switch (execution.outcome) {
      case Outcome::kSucceeded:
        outcome.kind = SelectionOutcome::Kind::kSucceeded;
        break;
      case Outcome::kFailed:
        outcome.kind = SelectionOutcome::Kind::kFailed;
        break;
      case Outcome::kTimedOut:
        outcome.kind = SelectionOutcome::Kind::kTimedOut;
        break;
      case Outcome::kCrashed:
        outcome.kind = SelectionOutcome::Kind::kCrashed;
        break;
    }
    fuzzer_->mutators().selector().OnOutcome(mutator, outcome);
  }

  static void Account(ComponentStats& stats, const Execution& execution) {
    stats.executed++;
    stats.time_us += execution.exec_time_us;
    switch (execution.outcome) {
      case Outcome::kSucceeded:
        stats.succeeded++;
        break;
      case Outcome::kFailed:
        stats.failed++;
        break;
      case Outcome::kTimedOut:
        stats.timed_out++;
        break;
      case Outcome::kCrashed:
        stats.crashed++;
        break;
    }
  }

  void AccountFuzzExecution(const Execution& execution) {
    stats().fuzz_executions++;
    switch (execution.outcome) {
      case Outcome::kSucceeded:
        stats().succeeded++;
        break;
      case Outcome::kFailed:
        stats().failed++;
        break;
      case Outcome::kTimedOut:
        stats().timed_out++;
        break;
      case Outcome::kCrashed:
        stats().crashed++;
        break;
    }
  }

  // --- import -----------------------------------------------------------

  void ImportPhase() {
    fuzzer_->workers_importing_++;
    ImportOptions options;
    options.root = config().import_root;
    while (!stopping()) {
      size_t index = fuzzer_->next_import_.fetch_add(1);
      if (index >= fuzzer_->import_files_.size()) {
        break;
      }
      ImportFile(fuzzer_->import_files_[index], options);
    }
    fuzzer_->workers_importing_--;
  }

  void ImportFile(const std::string& path, const ImportOptions& options) {
    MaybeRespawnForTargets();
    stats().imports_total++;
    JsonObject event;
    event.Add("event", "import").Add("file", path).Add("worker", id_);
    std::string source, error;
    if (!ReadFileToString(path, &source, &error)) {
      stats().imports_failed++;
      fuzzer_->LogEvent(event.Add("result", "unreadable").Add("error", error));
      return;
    }
    ImportInfo info;
    std::unique_ptr<fast::Tree> tree =
        ImportSource(source, path, options, &info, &error);
    if (tree == nullptr) {
      stats().imports_failed++;
      fuzzer_->LogEvent(event.Add("result", "parse_error").Add("error", error));
      return;
    }
    if (!info.inlined.empty()) {
      event.AddArray("inlined", info.inlined);
    }
    if (!info.dropped.empty()) {
      event.AddArray("dropped_loads", info.dropped);
    }
    Lineage lineage;
    lineage.origin = "import";
    lineage.detail = path;
    lineage.worker = id_;
    std::shared_ptr<Program> program =
        Program::FromTree(std::move(tree), lineage);
    std::string message;
    if (!checker_->Accepts(program->source(), &message)) {
      stats().imports_failed++;
      fuzzer_->LogEvent(
          event.Add("result", "syntax_error").Add("error", message));
      return;
    }
    Execution execution;
    if (!Execute(program->source(), &execution)) {
      stats().imports_failed++;
      fuzzer_->LogEvent(event.Add("result", "infrastructure_failure"));
      return;
    }
    event.Add("exec_time_ms",
              static_cast<double>(execution.exec_time_us) / 1000.0);
    if (execution.crashed()) {
      stats().imports_failed++;
      fuzzer_->LogEvent(event.Add("result", "crashed"));
      ProcessCrash(program->source(), *program, execution, lineage);
      return;
    }
    if (!execution.succeeded()) {
      stats().imports_failed++;
      event.Add("result", execution.timed_out() ? "timed_out" : "failed");
      event.Add("stderr", FirstLines(execution.stderr_output.empty()
                                         ? execution.stdout_output
                                         : execution.stderr_output));
      fuzzer_->LogEvent(event);
      return;
    }
    std::vector<uint32_t> claimed =
        fuzzer_->coverage_.ClaimNewEdges(runner_->coverage().bitmap());
    fuzzer_->KeepFocused(&claimed);
    if (claimed.empty() && !config().import_all) {
      stats().imports_redundant++;
      fuzzer_->LogEvent(event.Add("result", "redundant"));
      return;
    }
    ProgramPtr added = Commit(program, std::move(claimed), execution,
                              config().minimize_imports,
                              /*mutator=*/nullptr);
    if (added == nullptr) {
      stats().imports_failed++;
      fuzzer_->LogEvent(event.Add("result", "flaky"));
      return;
    }
    stats().imports_added++;
    fuzzer_->LogEvent(
        event.Add("result", "added")
            .Add("id", added->id)
            .Add("edges", static_cast<uint64_t>(added->edges.size())));
  }

  static std::string FirstLines(const std::string& text) {
    size_t end = 0;
    for (int lines = 0; lines < 3 && end < text.size(); lines++) {
      size_t nl = text.find('\n', end);
      end = nl == std::string::npos ? text.size() : nl + 1;
    }
    std::string out = text.substr(0, std::min<size_t>(end, 400));
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
      out.pop_back();
    }
    return out;
  }

  // --- fuzzing ----------------------------------------------------------

  void FuzzOne() {
    MaybeRespawnForTargets();
    Corpus& corpus = fuzzer_->corpus();
    Stopwatch pick_time;
    ProgramPtr parent = corpus.Pick(rng_);
    bool from_empty = false;
    if (parent == nullptr ||
        (corpus.size() < static_cast<size_t>(config().initial_programs) &&
         rng_.Coin())) {
      parent = fuzzer_->empty_program_;
      from_empty = true;
    }
    {
      StageStats& st = stage(Stage::kPick);
      st.invocations++;
      st.time_us += pick_time.ElapsedUs();
      if (!from_empty) {
        st.successes++;
      }
      if (config().stage(Stage::kPick).events) {
        JsonObject event;
        event.Add("event", "pick")
            .Add("worker", id_)
            .Add("program", from_empty ? 0u : parent->id);
        fuzzer_->LogEvent(event);
      }
    }
    // The corpus program the chain of consecutive mutations started from
    // (the scheduler's feedback and the lineage refer to it), and the
    // number of mutations applied since.
    ProgramPtr ancestor = parent;
    int chain = 0;
    const Environment& env = *fuzzer_->env_;
    MutatorSet& mutators = fuzzer_->mutators();
    GeneratorSet& generators = fuzzer_->generators();

    // Fresh values at the top of the parent give every mutation of the chain
    // something new to work with (Fuzzilli's buildPrefix()). The prefix is
    // not a mutation of its own: it is only executed as part of the first
    // child, and the minimizer drops whatever ends up unused. The empty
    // program gets a prefix too, unless `prefix_from_scratch` is false.
    if ((!from_empty || config().prefix_from_scratch) &&
        config().value_prefix && fuzzer_->prefix_mutator_ != nullptr) {
      Stopwatch prefix_time;
      std::string detail;
      std::unique_ptr<fast::Tree> prefixed =
          ApplyMutation(*fuzzer_->prefix_mutator_, *parent, nullptr, rng_, env,
                        generators, 1, &detail);
      StageStats& st = stage(Stage::kPrefix);
      st.invocations++;
      st.time_us += prefix_time.ElapsedUs();
      if (prefixed != nullptr) {
        st.successes++;
        parent = Program::FromTree(std::move(prefixed), parent->lineage());
      } else {
        Capture(Stage::kPrefix, CaptureTrigger::kNoop, "Prefix", parent.get(),
                "", JsonObject());
      }
      if (config().stage(Stage::kPrefix).events) {
        JsonObject event;
        event.Add("event", "prefix")
            .Add("worker", id_)
            .Add("program", ancestor->id);
        event.Add("applied", prefixed != nullptr).Add("detail", detail);
        fuzzer_->LogEvent(event);
      }
    }

    // A chain from the empty program uses only CodeGen for its first
    // `scratch_codegen_links` links that run without an error.
    int scratch_links = from_empty ? config().scratch_codegen_links : 0;
    for (int round = 0; round < config().consecutive_mutations && !stopping();
         round++) {
      Mutator* mutator = scratch_links > 0
                             ? fuzzer_->codegen_mutator_
                             : mutators.Pick(rng_, corpus.size() >= 2);
      if (mutator == nullptr) {
        return;
      }
      ProgramPtr partner;
      if (mutator->needs_partner()) {
        partner = corpus.PickPartner(*parent, rng_);
        if (partner == nullptr) {
          continue;
        }
      }
      Stopwatch mutation_time;
      Stopwatch
          stage_time;  // the whole mutate stage: mutate, check syntax, execute
      StageStats& mutate_stage = stage(Stage::kMutate);
      mutate_stage.invocations++;
      std::string detail;
      std::vector<ExploreProbe> probes;
      // Sub-mutations per application: a fixed number if configured,
      // otherwise 1..max_simultaneous() like Fuzzilli's
      // BaseInstructionMutator (Int.random(in: 1...maxSimultaneousMutations)).
      int rounds = config().mutation_rounds;
      if (rounds <= 0) {
        rounds = 1 + static_cast<int>(rng_.Index(static_cast<size_t>(
                         std::max(1, mutator->max_simultaneous()))));
      }
      BeginGeneratorUsage();
      const uint64_t spent_before = executions_;
      std::unique_ptr<fast::Tree> tree =
          ApplyMutation(*mutator, *parent, partner, rng_, env, generators,
                        rounds, &detail, &probes);
      // The generators this mutation used share the credit for its result.
      const std::vector<Generator*> used_generators = GeneratorsUsed();
      // Every target execution of this step (the mutant, instrumented
      // probe runs, determinism reruns, minimizer runs, crash triage)
      // counts as spent by the mutator and by the generators it used.
      const ScopeExit credit_spent([&] {
        const uint64_t spent = executions_ - spent_before;
        mutator->stats().spent += spent;
        for (Generator* g : used_generators) {
          g->stats().spent += spent;
        }
      });
      stats().mutation_time_us += mutation_time.ElapsedUs();
      if (tree == nullptr) {
        stats().noop++;
        mutate_stage.time_us += stage_time.ElapsedUs();
        Capture(Stage::kMutate, CaptureTrigger::kNoop, mutator->name(),
                parent.get(), "", JsonObject());
        LogMutant(*mutator, detail, ancestor->id, "noop", nullptr, 0);
        continue;
      }
      if (mutator->runtime_assisted()) {
        bool crashed = false;
        if (!CompleteRuntimeAssisted(*mutator, tree.get(), probes, &detail,
                                     &crashed)) {
          mutate_stage.time_us += stage_time.ElapsedUs();
          if (crashed) {
            return;
          }
          stats().noop++;
          Capture(Stage::kMutate, CaptureTrigger::kNoop, mutator->name(),
                  parent.get(), "", JsonObject());
          LogMutant(*mutator, detail, ancestor->id, "noop", nullptr, 0);
          continue;
        }
      }
      std::string source = fast::Print(tree->root());
      std::string message;
      if (config().syntax_check && !checker_->Accepts(source, &message)) {
        CountInvalid(*mutator, detail, ancestor->id, parent.get(), source,
                     message);
        mutate_stage.time_us += stage_time.ElapsedUs();
        continue;
      }
      Execution execution;
      if (!Execute(source, &execution)) {
        if (fuzzer_->failed_.load()) {
          return;
        }
        continue;
      }
      if (!config().syntax_check && execution.IsParseError()) {
        // The target did the parsing: same verdict, no in-process V8.
        CountInvalid(*mutator, detail, ancestor->id, parent.get(), source,
                     execution.ExceptionSignature());
        mutate_stage.time_us += stage_time.ElapsedUs();
        continue;
      }
      Account(mutator->stats(), execution);
      for (Generator* g : used_generators) {
        Account(g->stats(), execution);
      }
      AccountFuzzExecution(execution);
      ReportToSelector(*mutator, execution);
      stats().fuzz_nodes += fast::CountNodes(tree->root());
      mutate_stage.executions++;
      mutate_stage.exec_time_us += execution.exec_time_us;
      mutate_stage.time_us += stage_time.ElapsedUs();
      if (execution.outcome == Outcome::kTimedOut) {
        mutate_stage.timeouts++;
        Capture(Stage::kMutate, CaptureTrigger::kTimeout, mutator->name(),
                parent.get(), source, JsonObject());
      } else if (execution.outcome == Outcome::kFailed) {
        Capture(Stage::kMutate, CaptureTrigger::kFailed, mutator->name(),
                parent.get(), source, JsonObject());
      } else if (execution.outcome == Outcome::kCrashed) {
        Capture(Stage::kMutate, CaptureTrigger::kCrash, mutator->name(),
                parent.get(), source, JsonObject());
      }

      Lineage lineage;
      lineage.origin = mutator->name();
      lineage.detail = detail;
      lineage.parent_hash = parent->hash();
      lineage.parent_id = ancestor->id;
      lineage.chain = chain + 1;
      lineage.partner_hash = partner != nullptr ? partner->hash() : 0;
      lineage.generation = parent->lineage().generation + 1;
      lineage.worker = id_;

      switch (execution.outcome) {
        case Outcome::kSucceeded: {
          std::vector<uint32_t> claimed =
              fuzzer_->coverage_.ClaimNewEdges(runner_->coverage().bitmap());
          fuzzer_->KeepFocused(&claimed);
          std::shared_ptr<Program> child =
              Program::FromTree(std::move(tree), lineage, source);
          ProgramPtr added;
          size_t new_edges = claimed.size();
          if (!claimed.empty()) {
            stats().candidates++;
            mutate_stage.successes++;
            JsonObject why;
            why.Add("new_edges", static_cast<uint64_t>(new_edges));
            Capture(Stage::kMutate, CaptureTrigger::kInteresting,
                    mutator->name(), parent.get(), source, why);
            added = Commit(child, std::move(claimed), execution,
                           config().minimize, mutator);
            if (added != nullptr) {
              // Each generator the mutant used gets the full credit.
              const uint64_t targets = TargetFeatures(added->edges);
              for (Generator* g : used_generators) {
                g->stats().interesting++;
                g->stats().new_edges += added->edges.size();
                g->stats().new_targets += targets;
              }
            }
          }
          LogMutant(*mutator, detail, ancestor->id,
                    OutcomeName(execution.outcome), &execution, new_edges);
          if (ancestor->id != 0) {
            corpus.OnResult(*ancestor, added != nullptr);
          }
          // The chain continues from the child as it ran, not from its
          // minimized copy. Minimization can remove material that the next
          // mutations can use (DESIGN.md 4.2). Fuzzilli's MutationEngine also
          // minimizes on the side. The lineage of the next links has the id
          // of the published program.
          parent = child;
          if (added != nullptr) {
            ancestor = added;
            chain = 0;
          } else {
            chain++;
          }
          if (scratch_links > 0) {
            scratch_links--;
          }
          break;
        }
        case Outcome::kCrashed: {
          LogMutant(*mutator, detail, ancestor->id,
                    OutcomeName(execution.outcome), &execution, 0);
          std::shared_ptr<Program> child =
              Program::FromTree(std::move(tree), lineage, source);
          ProcessCrash(source, *child, execution, lineage);
          return;
        }
        case Outcome::kFailed:
        case Outcome::kTimedOut:
          LogMutant(*mutator, detail, ancestor->id,
                    OutcomeName(execution.outcome), &execution, 0);
          if (ancestor->id != 0) {
            corpus.OnResult(*ancestor, false);
          }
          break;
      }
    }
  }

  // --- stages: statistics, events and captures ---------------------------

  StageStats& stage(Stage s) { return fuzzer_->stages_[s]; }

  // One "mutant" event per executed (or dropped) mutation result when the
  // mutate stage logs events.
  void LogMutant(const Mutator& mutator, const std::string& detail,
                 uint32_t parent_id, const char* outcome,
                 const Execution* execution, size_t new_edges) {
    if (!config().stage(Stage::kMutate).events) {
      return;
    }
    JsonObject event;
    event.Add("event", "mutant")
        .Add("worker", id_)
        .Add("origin", mutator.name())
        .Add("detail", detail);
    event.Add("parent", parent_id).Add("outcome", outcome);
    if (execution != nullptr) {
      event.Add("exec_ms",
                static_cast<double>(execution->exec_time_us) / 1000.0);
    }
    event.Add("new_edges", static_cast<uint64_t>(new_edges));
    fuzzer_->LogEvent(event);
  }

  // Saves `parent` (if any) and `child` (if not empty) under
  // OUT/captures/<stage>/ when the stage's capture rule matches `trigger`
  // for `mutator`, within the rule's cap. `details` go to the JSON sidecar.
  void Capture(Stage s, CaptureTrigger trigger, std::string_view mutator,
               const Program* parent, const std::string& child,
               const JsonObject& details) {
    const CaptureRule& rule = config().stage(s).capture;
    if (!rule.enabled() || !rule.Matches(trigger, mutator)) {
      return;
    }
    fuzzer_->WriteCapture(s, trigger, mutator, parent, child, details, id_);
  }

  // Second half of a runtime-assisted mutation (see mutators/explore.h):
  // runs the instrumented tree with the runtime helper prepended and turns
  // the target's answers into ordinary code in `tree`. Returns false if
  // nothing came of it (the caller treats the mutation as a no-op).
  bool CompleteRuntimeAssisted(Mutator& base, fast::Tree* tree,
                               const std::vector<ExploreProbe>& probes,
                               std::string* detail, bool* crashed) {
    *crashed = false;
    if (probes.empty()) {
      return false;
    }
    auto& mutator = static_cast<RuntimeAssistedMutator&>(base);
    std::string instrumented =
        std::string(mutator.runtime()) + fast::Print(tree->root());
    std::string message;
    if (config().syntax_check && !checker_->Accepts(instrumented, &message)) {
      mutator.stats().invalid++;
      return false;
    }
    Execution execution;
    if (!Execute(instrumented, &execution)) {
      return false;
    }
    stats().explore_executions++;
    stage(Stage::kMutate).executions++;
    stage(Stage::kMutate).exec_time_us += execution.exec_time_us;
    if (execution.crashed()) {
      // Crashes of the instrumented program count like any other crash; the
      // saved program includes the runtime so that it reproduces.
      *crashed = true;
      Account(mutator.stats(), execution);
      AccountFuzzExecution(execution);
      ReportToSelector(mutator, execution);
      Lineage lineage;
      lineage.origin = mutator.name();
      lineage.detail = "instrumented program";
      lineage.worker = id_;
      std::shared_ptr<Program> program =
          Program::Parse(instrumented, lineage, &message);
      if (program != nullptr) {
        ProcessCrash(instrumented, *program, execution, lineage);
      }
      return false;
    }
    if (execution.timed_out()) {
      stats().explore_timeouts++;
      stage(Stage::kMutate).timeouts++;
    }
    if (execution.fuzzout.empty()) {
      return false;
    }
    Stopwatch mutation_time;
    Builder builder(tree, &rng_);
    MutationContext ctx;
    ctx.env = fuzzer_->env_;
    ctx.generators = &fuzzer_->generators();
    int applied =
        mutator.Apply(builder, probes, execution.fuzzout, ctx, detail);
    stats().mutation_time_us += mutation_time.ElapsedUs();
    if (applied > 0) {
      stats().explore_applied++;
    }
    return applied > 0;
  }

  // Determinism check, minimization and publication of a program that
  // reached `claimed` new edges. Returns the published program, or nullptr
  // if the program was dropped (edges released or suppressed as flaky).
  ProgramPtr Commit(std::shared_ptr<Program> program,
                    std::vector<uint32_t> claimed,
                    const Execution& first_execution, bool minimize,
                    Mutator* mutator) {
    FlagPin pin(runner_->child());
    SharedCoverage& shared = fuzzer_->coverage_;
    const bool claimed_was_empty = claimed.empty();
    std::vector<uint32_t> edges = std::move(claimed);
    std::vector<uint32_t> release;
    auto drop = [&](const std::vector<uint32_t>& dropped) {
      for (uint32_t edge : dropped) {
        if (!shared.RecordFlaky(edge, config().flaky_threshold)) {
          release.push_back(edge);
        }
      }
    };

    // 1. Determinism (Fuzzilli's processMaybeInteresting): every edge that
    //    is kept must be reached by all of the re-executions; edges that
    //    disappear are dropped from the set. There are at least min_reruns
    //    re-executions, and the loop ends once the set survived
    //    min_stable_reruns re-runs unchanged. A candidate that still loses
    //    edges after max_reruns re-executions has not converged and is
    //    dropped.
    int stable = 0;
    int reruns = 0;
    size_t dropped = 0;
    bool deterministic = true;
    bool converged = false;
    bool rerun_timeout = false;
    uint64_t exec_time_us = first_execution.exec_time_us;
    uint32_t edges_hit = runner_->coverage().CountCurrentEdges();
    Stopwatch check_time;
    StageStats& check_stage = stage(Stage::kCheck);
    check_stage.invocations++;
    // (A program imported with --import-all may have no new edges at all;
    // it is still re-run so that only reliable programs are published.)
    while (deterministic && (!edges.empty() || claimed_was_empty)) {
      if (stable >= config().min_stable_reruns &&
          reruns >= config().min_reruns) {
        converged = true;
        break;
      }
      if (reruns >= config().max_reruns) {
        break;
      }
      Execution execution;
      if (!Execute(program->source(), &execution)) {
        deterministic = false;
        break;
      }
      reruns++;
      stats().rerun_executions++;
      check_stage.executions++;
      check_stage.exec_time_us += execution.exec_time_us;
      if (execution.outcome == Outcome::kTimedOut) {
        check_stage.timeouts++;
        rerun_timeout = true;
      }
      if (execution.crashed()) {
        HarvestCrash(program->source(), execution, program->lineage(),
                     "determinism rerun");
      }
      if (!execution.succeeded()) {
        deterministic = false;
        break;
      }
      exec_time_us = std::min(exec_time_us, execution.exec_time_us);
      std::vector<uint32_t> missing = runner_->coverage().MissingEdges(edges);
      if (missing.empty()) {
        stable++;
        continue;
      }
      stable = 0;
      dropped += missing.size();
      drop(missing);
      std::vector<uint32_t> kept;
      std::set_difference(edges.begin(), edges.end(), missing.begin(),
                          missing.end(), std::back_inserter(kept));
      edges.swap(kept);
    }
    // A program without new edges is only published for --import-all; it
    // still had to run reliably.
    const bool keep_without_edges =
        edges.empty() && claimed_was_empty && config().import_all;
    const bool survived =
        deterministic && converged && (!edges.empty() || keep_without_edges);
    check_stage.time_us += check_time.ElapsedUs();
    if (survived) {
      check_stage.successes++;
    }
    if (config().stage(Stage::kCheck).events) {
      JsonObject event;
      event.Add("event", "check")
          .Add("worker", id_)
          .Add("origin", program->lineage().origin);
      event.Add("reruns", reruns)
          .Add("dropped_edges", static_cast<uint64_t>(dropped));
      event.Add("kept_edges", static_cast<uint64_t>(edges.size()))
          .Add("survived", survived);
      event.Add("deterministic_outcome", deterministic)
          .Add("converged", converged);
      fuzzer_->LogEvent(event);
    }
    if (!survived || rerun_timeout) {
      JsonObject why;
      why.Add("reruns", reruns)
          .Add("dropped_edges", static_cast<uint64_t>(dropped));
      why.Add("deterministic_outcome", deterministic)
          .Add("converged", converged);
      Capture(Stage::kCheck,
              rerun_timeout ? CaptureTrigger::kTimeout : CaptureTrigger::kFlaky,
              program->lineage().origin, nullptr, program->source(), why);
    }
    if (!survived) {
      drop(edges);
      shared.Release(release);
      stats().flaky++;
      JsonObject event;
      event.Add("event", "flaky").Add("origin", program->lineage().origin);
      event.Add("deterministic_outcome", deterministic).Add("worker", id_);
      event.Add("reruns", reruns)
          .Add("dropped_edges", static_cast<uint64_t>(dropped));
      fuzzer_->LogEvent(event);
      return nullptr;
    }
    shared.Release(release);

    // 2. Minimization against the surviving edges.
    MinimizationResult minimization;
    bool minimized = false;
    if (minimize && !edges.empty() && !stopping()) {
      Stopwatch minimize_time;
      StageStats& minimize_stage = stage(Stage::kMinimize);
      const size_t nodes_before = program->node_count();
      minimize_stage.invocations++;
      std::unique_ptr<fast::Tree> work = program->CopyTree();
      // A reduction can turn a terminating loop into an infinite one; such
      // probes must not pay the full target timeout (plus a child respawn).
      const uint64_t probe_timeout_us = ProbeTimeout(exec_time_us);
      // The property that the minimizer keeps (see MinimizationProperty).
      bool fragile_only = false;
      const std::vector<uint32_t> property =
          MinimizationProperty(edges, &fragile_only);
      MinimizationOracle oracle = [&](const std::string& candidate) {
        if (stopping()) {
          return false;
        }
        Execution execution;
        if (!Execute(candidate, &execution, probe_timeout_us)) {
          return false;
        }
        minimize_stage.executions++;
        minimize_stage.exec_time_us += execution.exec_time_us;
        if (execution.outcome == Outcome::kTimedOut) {
          minimize_stage.timeouts++;
          Capture(Stage::kMinimize, CaptureTrigger::kTimeout,
                  program->lineage().origin, program.get(), candidate,
                  JsonObject());
        }
        if (execution.crashed()) {
          HarvestCrash(candidate, execution, program->lineage(),
                       "minimizer probe");
          return false;
        }
        return execution.succeeded() &&
               runner_->coverage().AllEdgesSet(property);
      };
      MinimizerOptions budget = config().minimizer.ForProgram(exec_time_us);
      if (fragile_only) {
        budget.max_tests = std::min(budget.max_tests, kFragileMinimizerTests);
      }
      minimization =
          fuzzer_->Minimize(work.get(), oracle, probe_checker(), budget);
      stats().minimizer_tests += static_cast<uint64_t>(minimization.tests);
      stats().minimizer_time_us += minimization.time_us;
      if (minimization.accepted > 0) {
        // Guard against a flaky oracle: the result must still qualify. The
        // verification itself is retried a few times because target-inherent
        // non-determinism (concurrent GC and compilation) occasionally hides
        // an edge that is reached deterministically otherwise.
        std::shared_ptr<Program> candidate =
            Program::FromTree(std::move(work), program->lineage());
        for (int attempt = 0; attempt < config().verify_attempts && !stopping();
             attempt++) {
          Execution execution;
          if (!Execute(candidate->source(), &execution)) {
            break;
          }
          stats().rerun_executions++;
          if (!execution.succeeded()) {
            break;
          }
          if (!runner_->coverage().AllEdgesSet(property)) {
            continue;
          }
          stats().minimizer_nodes_removed += static_cast<uint64_t>(
              program->node_count() - candidate->node_count());
          program =
              candidate;  // a copy: `candidate` is reported below if rejected
          exec_time_us = execution.exec_time_us;
          edges_hit = runner_->coverage().CountCurrentEdges();
          minimized = true;
          break;
        }
        if (!minimized) {
          stats().minimizer_rejected++;
          JsonObject why;
          why.Add("tests", minimization.tests);
          Capture(Stage::kMinimize, CaptureTrigger::kRejected,
                  program->lineage().origin, program.get(), candidate->source(),
                  why);
        }
      }
      minimize_stage.time_us += minimize_time.ElapsedUs();
      if (minimized) {
        minimize_stage.successes++;
      }
      if (config().stage(Stage::kMinimize).events) {
        JsonObject event;
        event.Add("event", "minimize")
            .Add("worker", id_)
            .Add("origin", program->lineage().origin);
        event.Add("tests", minimization.tests)
            .Add("accepted", minimization.accepted);
        event.Add("nodes_before", static_cast<uint64_t>(nodes_before));
        event.Add("nodes_after", static_cast<uint64_t>(program->node_count()));
        event.Add("minimized", minimized)
            .Add("time_ms", static_cast<double>(minimization.time_us) / 1000.0);
        fuzzer_->LogEvent(event);
      }
    }

    // 3. Publish.
    Stopwatch publish_time;
    StageStats& publish_stage = stage(Stage::kPublish);
    publish_stage.invocations++;
    program->edges = edges;
    program->edges_hit = edges_hit;
    program->exec_time_us = exec_time_us;
    if (fuzzer_->corpus().WantsHitEdges() && !stopping()) {
      // Every edge the final program reaches in two runs (Fuzzilli's
      // RarityCorpus does the same two executions).
      std::vector<uint32_t> hit;
      for (int run = 0; run < 2; run++) {
        Execution execution;
        if (!Execute(program->source(), &execution) || !execution.succeeded()) {
          hit.clear();
          break;
        }
        stats().rerun_executions++;
        std::vector<uint32_t> current = runner_->coverage().CurrentEdges();
        std::sort(current.begin(), current.end());
        if (run == 0) {
          hit.swap(current);
          continue;
        }
        std::vector<uint32_t> both;
        std::set_intersection(hit.begin(), hit.end(), current.begin(),
                              current.end(), std::back_inserter(both));
        hit.swap(both);
      }
      program->hit_edges = std::move(hit);
    }
    ProgramPtr added = fuzzer_->corpus().Add(program);
    stats().corpus_added++;
    stats().corpus_nodes += program->node_count();
    if (mutator != nullptr) {
      mutator->stats().interesting++;
      mutator->stats().new_edges += added->edges.size();
      mutator->stats().new_targets += TargetFeatures(added->edges);
    }
    if (config().save_corpus) {
      fuzzer_->SaveCorpusProgram(*added);
    }
    publish_stage.successes++;
    publish_stage.time_us += publish_time.ElapsedUs();
    JsonObject event;
    event.Add("event", "corpus_add").Add("id", added->id);
    event.Add("edges", static_cast<uint64_t>(edges.size()))
        .Add("edges_hit", edges_hit);
    event.Add("origin", added->lineage().origin)
        .Add("detail", added->lineage().detail);
    event.Add("parent", added->lineage().parent_id)
        .Add("chain", added->lineage().chain);
    event.Add("generation", added->lineage().generation);
    event.Add("nodes", static_cast<uint64_t>(added->node_count()));
    event.Add("reruns", reruns)
        .Add("dropped_edges", static_cast<uint64_t>(dropped));
    event.Add("exec_time_ms", static_cast<double>(exec_time_us) / 1000.0);
    event.Add("worker", id_);
    if (config().randomize_d8_flags) {
      event.Add("d8_random_args", RandomArgsText());
    }
    if (minimize) {
      event.Add("minimized", minimized).Add("min_tests", minimization.tests);
      event.Add("min_nodes_before",
                static_cast<uint64_t>(minimization.nodes_before));
      event.Add("min_nodes_after",
                static_cast<uint64_t>(minimization.nodes_after));
      event.Add("min_time_ms",
                static_cast<double>(minimization.time_us) / 1000.0);
    }
    event.Add("coverage", shared.coverage_fraction());
    fuzzer_->LogEvent(event);
    return added;
  }

  // --- crashes ----------------------------------------------------------

  // Replays the last `count` programs of `history` in a fresh target, then
  // `source`. Returns true if `source` then crashes (its execution is left
  // in *execution). A history program that itself crashes or hangs resets
  // the state being rebuilt, so the replay cannot answer the question then.
  bool ReplayCrash(const std::vector<std::string>& history, size_t count,
                   const std::string& source, Execution* execution) {
    runner_->child().Kill();
    for (size_t i = history.size() - count; i < history.size(); i++) {
      Execution e;
      if (stopping() || !Execute(history[i], &e)) {
        return false;
      }
      stats().crash_replay_executions++;
      if (e.crashed() || e.timed_out()) {
        return false;
      }
    }
    if (stopping() || !Execute(source, execution)) {
      return false;
    }
    stats().crash_replay_executions++;
    return execution->crashed();
  }

  // The path of the files of a saved crash, without the extension.
  std::string CrashBase(int signal, uint64_t hash) const {
    return StringPrintf("%s/crashes/crash_sig%d_%016llx",
                        config().out_dir.c_str(), signal,
                        static_cast<unsigned long long>(hash));
  }

  // The target of this worker as it runs now, with its random flags.
  TargetOptions CurrentTarget() const {
    TargetOptions target = config().target;
    target.args = runner_->child().CurrentArgs();
    target.random_args = nullptr;
    return target;
  }

  // The random flags of the current target, separated by spaces.
  std::string RandomArgsText() const {
    std::string text;
    for (const std::string& arg : runner_->child().random_args()) {
      text += (text.empty() ? "" : " ") + arg;
    }
    return text;
  }

  // Everything learned about one crash, for the report and the event log.
  struct CrashInfo {
    CrashSignature signature;
    int child_age = 0;             // executions of the target before the crash
    int reruns = 0;                // fresh-target re-executions
    int reproduced = 0;            // ... that crashed again
    int same_signature = 0;        // ... with the same signature
    bool replayed = false;         // the history replay was attempted
    bool state_dependent = false;  // crashes only behind (part of) the history
    size_t history_needed = 0;     // shortest history suffix that reproduces it
    size_t history_available = 0;
    uint64_t replay_executions = 0;
    bool minimized = false;  // NAME.js has the minimized program
    // The minimization. Its node counts are those of the program alone, as
    // in NAME.js (also when the minimizer reduced the tree of the prelude
    // and the program). They do not change if the check rejected the result.
    MinimizationResult minimization;
    // What the minimizer reduced: "prelude and program" (one tree), or
    // "program alone" and the reason. Empty if the minimizer did not run.
    std::string min_tree;
    // How a test of an amplified crash runs: "1 of K plain d8 runs with the
    // amplified flags". Empty if the tests run in the target of the worker.
    std::string min_oracle;
    // For the tree of the prelude and the program: its top-level prelude
    // statements and its nodes, before and after the minimization. For an
    // amplified crash, the prelude of the tree can be empty.
    bool prelude_in_tree = false;
    size_t prelude_statements_before = 0;
    size_t prelude_statements_after = 0;
    size_t script_nodes_before = 0;
    size_t script_nodes_after = 0;
    // The minimized script, which reproduces the crash without the prelude
    // (the text of NAME.repro.js after its header). Empty if there is none.
    std::string repro;
    // The program part of `repro` also crashed with the signature behind
    // the full prelude. Then NAME.js has this program.
    bool program_verified = false;
    std::string repro_path;  // empty if Redline did not write NAME.repro.js
    size_t repro_bytes = 0;
    // The d8 flags of NAME.repro.js: all flags of the test, or the flags
    // that the flag minimization kept. `repro_flags_note` tells which. The
    // flags of the test are the flags of the target, or the amplified flags.
    std::vector<std::string> repro_flags;
    std::string repro_flags_note;
    int repro_flag_tests = 0;      // flag lists that the search tried
    int repro_flag_processes = 0;  // plain d8 processes of the search
    // The rate check of NAME.repro.js (CheckReproRate). The fields are the
    // plain d8 runs, the runs that crashed with the signature, and the
    // result in words (empty: no check).
    int repro_rate_runs = 0;
    int repro_rate_hits = 0;
    std::string repro_rate_note;
    bool amplify_ran = false;  // the amplify step ran (5.8)
    AmplifyResult amplify;
    // The one-file reproducer of a state-dependent crash (NAME.bundle.js).
    std::string bundle_path;  // empty if Redline did not write it
    std::string bundle_note;  // why there is no bundle, or why the runs stopped
    int bundle_runs = 0;      // plain d8 runs of the bundle
    int bundle_hits = 0;      // ... that crashed with the same signature
    // The minimization of the bundle: the result in one line, or why it did
    // not give a smaller bundle. Empty if it did not run.
    std::string bundle_min_note;
    // NAME.bundle.min.js, the minimized bundle (empty if there is none), and
    // its plain d8 runs and the runs that crashed with the signature.
    std::string bundle_min_path;
    int bundle_min_runs = 0;
    int bundle_min_hits = 0;
    bool reproducible() const { return reproduced > 0; }
  };

  // The amplify lines of the crash report (read by the uploader).
  // `amplified_program` names the file of the changed program, if any.
  static std::string AmplifyReportLines(const CrashInfo& info,
                                        const std::string& amplified_program) {
    if (!info.amplify_ran) {
      return "amplified: skipped\n";
    }
    return StringPrintf(
        "amplified: %s\namplified_args: %s\namplified_rate: "
        "%d/%d\namplify_processes: %d\n"
        "amplify_time_sec: %.0f\namplify_note: %s\n%s",
        info.amplify.amplified ? "yes" : "no",
        info.amplify.amplified ? info.amplify.args.c_str() : "",
        info.amplify.hits, info.amplify.processes, info.amplify.total_processes,
        info.amplify.time_sec, info.amplify.note.c_str(),
        amplified_program.empty()
            ? ""
            : ("amplified_program: " + amplified_program + "\n").c_str());
  }

  // The bundle lines of the crash report (read by the uploader).
  // `bundle_rate` gives the plain d8 runs of NAME.bundle.js that crashed
  // with the same signature.
  static std::string BundleReportLines(const CrashInfo& info) {
    if (!info.state_dependent) {
      return "bundle: skipped\n";
    }
    if (info.bundle_path.empty()) {
      return "bundle: no\nbundle_note: " + info.bundle_note + "\n";
    }
    std::string lines =
        StringPrintf("bundle: %s\nbundle_rate: %d/%d\nbundle_note: %s\n",
                     BaseName(info.bundle_path).c_str(), info.bundle_hits,
                     info.bundle_runs, info.bundle_note.c_str());
    if (!info.bundle_min_path.empty()) {
      lines += "bundle_min: " + BaseName(info.bundle_min_path) + "\n";
    }
    if (info.bundle_min_runs > 0) {
      lines += StringPrintf("bundle_min_rate: %d/%d\n", info.bundle_min_hits,
                            info.bundle_min_runs);
    }
    if (!info.bundle_min_note.empty()) {
      lines += "bundle_min_note: " + info.bundle_min_note + "\n";
    }
    return lines;
  }

  // The minimization lines of the crash report (read by the uploader).
  // `repro` names NAME.repro.js, the minimized script that reproduces the
  // crash without the prelude.
  static std::string MinimizationReportLines(const CrashInfo& info) {
    std::string lines = StringPrintf(
        "min_time_ms: %.0f\nmin_budget_exhausted: %s\nmin_stop: %s\n",
        static_cast<double>(info.minimization.time_us) / 1000.0,
        info.minimization.budget_exhausted ? "yes" : "no",
        MinimizationStopName(info.minimization.stop));
    if (!info.min_tree.empty()) {
      lines += "min_tree: " + info.min_tree + "\n";
    }
    if (!info.min_oracle.empty()) {
      lines += "min_oracle: " + info.min_oracle + "\n";
    }
    if (info.prelude_in_tree) {
      lines += StringPrintf(
          "min_prelude_statements: %zu -> %zu\nmin_script_nodes: %zu -> %zu\n",
          info.prelude_statements_before, info.prelude_statements_after,
          info.script_nodes_before, info.script_nodes_after);
    }
    if (!info.repro_path.empty()) {
      // Only the program of a reproducible crash runs behind the full
      // prelude again.
      if (info.reproducible()) {
        lines += StringPrintf("min_program_verified: %s\n",
                              info.program_verified ? "yes" : "no");
      }
      lines += StringPrintf(
          "repro: %s\nrepro_bytes: %zu\nrepro_flags: %s\nrepro_flags_note: "
          "%s\n",
          BaseName(info.repro_path).c_str(), info.repro_bytes,
          JoinArgs(info.repro_flags).c_str(), info.repro_flags_note.c_str());
    }
    if (!info.repro_rate_note.empty()) {
      lines += StringPrintf("repro_rate: %d/%d\nrepro_rate_note: %s\n",
                            info.repro_rate_hits, info.repro_rate_runs,
                            info.repro_rate_note.c_str());
    }
    return lines;
  }

  // `args`, separated by spaces.
  static std::string JoinArgs(const std::vector<std::string>& args) {
    std::string text;
    for (const std::string& arg : args) {
      text += (text.empty() ? "" : " ") + arg;
    }
    return text;
  }

  // The text of NAME.repro.js at `path`: the two header lines with `flags`
  // in the d8 command, then the minimized script.
  std::string ReproText(const CrashInfo& info,
                        const std::vector<std::string>& flags,
                        const std::string& path) const {
    std::string signature = info.signature.text;
    std::replace(signature.begin(), signature.end(), '\n', ' ');
    TargetOptions target = CurrentTarget();
    target.executable = AbsolutePath(target.executable);
    target.args = flags;
    return StringPrintf("// signature: %s\n// run: %s %s/%s\n",
                        signature.c_str(), target.CommandLine().c_str(),
                        AbsolutePath(config().out_dir + "/crashes").c_str(),
                        BaseName(path).c_str()) +
           info.repro;
  }

  // Writes BASE.repro.js for a minimized crash: two comment lines (the
  // signature and the d8 command that runs the file), then the minimized
  // script. The script has the prelude statements that the crash needs, so
  // d8 runs the file without the prelude. The d8 command has the flags of
  // `repro_flags` and absolute paths, so it works in each directory.
  void WriteRepro(const std::string& base, CrashInfo* info) {
    if (info->repro.empty()) {
      return;
    }
    const std::string path = base + ".repro.js";
    const std::string text = ReproText(*info, info->repro_flags, path);
    std::string error;
    if (!WriteStringToFile(path, text, &error)) {
      REDLINE_WARN(kLog, "%s", error.c_str());
      return;
    }
    info->repro_path = path;
    info->repro_bytes = text.size();
  }

  // The time that a plain d8 process needs to start and to stop, in addition
  // to the time of its scripts (the bundle runs and the flag search).
  static constexpr uint64_t kPlainStartupUs = 10'000'000;

  // The header of NAME.bundle.js at `path`, for a bundle of `programs`
  // history programs and the crash program.
  std::string BundleHeader(size_t programs, const std::string& path,
                           const CrashInfo& info) const {
    std::string signature = info.signature.text;
    std::replace(signature.begin(), signature.end(), '\n', ' ');
    return StringPrintf(
        "// redline state-dependent crash: %zu program(s), then the crash "
        "program\n// signature: %s\n// run: %s --bundle-reprl %s\n",
        programs, signature.c_str(), CurrentTarget().CommandLine().c_str(),
        BaseName(path).c_str());
  }

  // Writes BASE.bundle.js for a state-dependent crash: one part for each
  // program of `history`, then one part for `source`. `d8 --bundle-reprl`
  // runs each part in the same way as the REPRL loop runs one program. Thus
  // ClusterFuzz can reproduce the crash from this one file. Then the bundle
  // runs in up to crash_bundle_runs plain d8 processes, and the runs that
  // crash with the signature of the crash are counted.
  void WriteReprlBundle(const std::vector<std::string>& history,
                        const std::string& source, const std::string& base,
                        CrashInfo* info) {
    // A NAME.bundle.min.js of an earlier session in the same directory must
    // not stay next to the new NAME.bundle.js.
    std::remove((base + ".bundle.min.js").c_str());
    std::vector<std::string> scripts;
    scripts.reserve(history.size() + 1);
    for (const std::string& program : history) {
      scripts.push_back(ScriptFor(program));
    }
    scripts.push_back(ScriptFor(source));
    const TargetOptions target = CurrentTarget();
    const std::string path = base + ".bundle.js";
    std::string bundle;
    if (!MakeReprlBundle(BundleHeader(history.size(), path, *info), scripts,
                         &bundle)) {
      info->bundle_note = "a program contains the bundle marker";
      return;
    }
    std::string error;
    if (!WriteStringToFile(path, bundle, &error)) {
      info->bundle_note = error;
      return;
    }
    info->bundle_path = path;
    std::vector<std::string> args = target.args;
    args.push_back("--bundle-reprl");
    args.push_back(path);
    const auto env = target.PlainEnvironment();
    const uint64_t timeout_us =
        config().target.timeout_us * scripts.size() + kPlainStartupUs;
    const uint64_t budget_us =
        static_cast<uint64_t>(config().crash_history_budget_sec * 1e6);
    const Stopwatch clock;
    while (info->bundle_runs < config().crash_bundle_runs && !stopping()) {
      if (clock.ElapsedUs() > budget_us) {
        info->bundle_note = "time budget used up";
        break;
      }
      const ProcessResult run =
          RunProcess(target.executable, args, env, timeout_us);
      if (!run.started) {
        info->bundle_note = "cannot start d8: " + run.error;
        break;
      }
      info->bundle_runs++;
      if (!run.timed_out &&
          CrashSignature::FromOutput(run.signal, run.exit_code,
                                     run.stderr_text) == info->signature) {
        info->bundle_hits++;
      }
    }
    stats().bundle_processes += static_cast<uint64_t>(info->bundle_runs);
    if (info->bundle_hits > 0) {
      stats().crashes_bundled++;
    }
  }

  // The test budget of a minimization that keeps only fragile features.
  static constexpr int kFragileMinimizerTests = 60;

  // The features that the minimizer must keep. By default, all new features
  // of the program. With --minimize-code-edges, only the new code edges, the
  // new builtins edges and the target features. Optimized code features and
  // hit count buckets change with most edits of a program. A minimization
  // that keeps them spends its budget and removes little. If the program has
  // none of the stable features, the property is all its new features,
  // `*fragile_only` is true, and the budget is small.
  std::vector<uint32_t> MinimizationProperty(const std::vector<uint32_t>& edges,
                                             bool* fragile_only) const {
    *fragile_only = false;
    const FeatureWeights* weights = fuzzer_->feature_weights_.get();
    const CoverageMap& coverage = runner_->coverage();
    if (!config().minimize_code_edges) {
      return edges;
    }
    std::vector<uint32_t> robust;
    for (uint32_t feature : edges) {
      const CoverageMap::FeatureKind kind = coverage.KindOf(feature);
      if (kind == CoverageMap::FeatureKind::kCode ||
          kind == CoverageMap::FeatureKind::kBuiltins ||
          (weights != nullptr && weights->IsTarget(feature))) {
        robust.push_back(feature);
      }
    }
    if (robust.empty()) {
      *fragile_only = true;
      return edges;
    }
    return robust;
  }

  // With --harvest-crashes, a crash of a minimizer probe or of a
  // determinism rerun counts like a crash of the mutate stage. These stages
  // run most of the executions, and their programs are close relatives of
  // the programs that reached new coverage.
  void HarvestCrash(const std::string& source, const Execution& execution,
                    const Lineage& parent, const char* detail) {
    if (!config().harvest_crashes) {
      return;
    }
    Lineage lineage = parent;
    lineage.detail = detail;
    std::string error;
    std::shared_ptr<Program> program = Program::Parse(source, lineage, &error);
    if (program == nullptr) {
      return;
    }
    stats().harvested_crashes++;
    ProcessCrash(source, *program, execution, lineage);
  }

  // Minimizes a crash that reproduces alone, against its signature
  // (DESIGN.md 5.6). The minimizer reduces one tree of the prelude and the
  // program, which prints to a script that is equivalent to the script of
  // the target. Thus it also removes the prelude statements that the crash
  // does not need. The minimizer reduces the program alone, behind the
  // prelude, in four cases. The prelude is empty. The prelude or the program
  // does not parse. The prelude continues into the program. The printed tree
  // does not crash with the signature. Sets the minimization fields of
  // `info`. Returns the program for NAME.js.
  std::string MinimizeReproducibleCrash(const std::string& source,
                                        const Program& program,
                                        uint64_t exec_time_us,
                                        CrashInfo* info) {
    const CrashSignature& wanted = info->signature;
    auto same_crash = [&](const Execution& execution) {
      return execution.crashed() &&
             CrashSignature::FromExecution(execution) == wanted;
    };
    // A text of the one tree runs as it is. A program runs behind the
    // prelude.
    auto run = [&](const std::string& text, bool is_script,
                   Execution* execution, uint64_t timeout_us) {
      return is_script ? ExecuteScript(text, execution, timeout_us)
                       : Execute(text, execution, timeout_us);
    };
    // True if the text crashes with the signature in one of
    // verify_attempts runs. The target is not deterministic.
    bool target_failed = false;
    auto verified = [&](const std::string& text, bool is_script) {
      for (int attempt = 0; attempt < config().verify_attempts && !stopping();
           attempt++) {
        Execution e;
        if (!run(text, is_script, &e, 0)) {
          target_failed = true;
          return false;
        }
        stats().rerun_executions++;
        if (same_crash(e)) {
          return true;
        }
      }
      return false;
    };
    std::unique_ptr<ScriptTree> script;
    info->min_tree = "program alone: the prelude is empty";
    if (!config().prelude.empty()) {
      std::string error;
      script = ScriptTree::Parse(config().prelude, source, &error);
      if (script == nullptr) {
        // The report has one line for the reason.
        std::replace(error.begin(), error.end(), '\n', ' ');
        info->min_tree = "program alone: " + error;
      } else if (!verified(fast::Print(script->tree()->root()),
                           /*is_script=*/true)) {
        // A stop request or a target that failed also gives false.
        info->min_tree = stopping() ? "program alone: the session stopped"
                         : target_failed
                             ? "program alone: the target did not run the "
                               "printed script of the prelude and the program"
                             : "program alone: the printed script of the "
                               "prelude and the program did not crash with the "
                               "signature";
        script.reset();
      } else {
        info->min_tree = "prelude and program";
      }
    }
    std::unique_ptr<fast::Tree> program_tree;
    fast::Tree* work;
    size_t program_nodes_before = 0;
    if (script != nullptr) {
      work = script->tree();
      program_nodes_before = script->ProgramNodes();
      info->prelude_in_tree = true;
      info->prelude_statements_before = script->prelude_statements();
    } else {
      if (!config().prelude.empty()) {
        REDLINE_LOG(kLog, "worker %d: the crash minimizer reduces the %s", id_,
                    info->min_tree.c_str());
      }
      program_tree = program.CopyTree();
      work = program_tree.get();
    }
    const bool whole_script = script != nullptr;
    const uint64_t probe_timeout_us = ProbeTimeout(exec_time_us);
    MinimizationOracle oracle = [&](const std::string& candidate) {
      if (stopping()) {
        return false;
      }
      Execution e;
      return run(candidate, whole_script, &e, probe_timeout_us) &&
             same_crash(e);
    };
    info->minimization = fuzzer_->MinimizeCrash(
        work, oracle, probe_checker(),
        config().crash_minimizer.ForProgram(exec_time_us));
    if (stopping()) {
      // The oracle refused every test after the stop request.
      info->minimization.stop = MinimizationStop::kStopped;
    }
    stats().minimizer_tests += static_cast<uint64_t>(info->minimization.tests);
    stats().minimizer_time_us += info->minimization.time_us;
    // Keep the node counts of the program alone, as in NAME.js. Until the
    // check below accepts the result, the counts after are the counts before.
    if (script != nullptr) {
      info->script_nodes_before = info->minimization.nodes_before;
      info->script_nodes_after = info->script_nodes_before;
      info->minimization.nodes_before = program_nodes_before;
      info->prelude_statements_after = info->prelude_statements_before;
    }
    const size_t nodes_after = script != nullptr
                                   ? script->ProgramNodes()
                                   : info->minimization.nodes_after;
    info->minimization.nodes_after = info->minimization.nodes_before;
    if (info->minimization.accepted == 0) {
      return source;
    }
    const std::string candidate = fast::Print(work->root());
    if (!verified(candidate, whole_script)) {
      stats().minimizer_rejected++;
      return source;
    }
    if (script == nullptr) {
      info->minimization.nodes_after = nodes_after;
      info->minimized = true;
      return candidate;
    }
    info->script_nodes_after = fast::CountNodes(work->root());
    info->prelude_statements_after = script->PreludeStatementsNow();
    info->repro = candidate;
    // MinimizeReproFlags can remove flags later.
    info->repro_flags = CurrentTarget().args;
    info->repro_flags_note = "all flags of the test, no search";
    // NAME.js keeps the program alone, which runs behind the full prelude.
    const std::string reduced = script->PrintProgram();
    info->program_verified = verified(reduced, /*is_script=*/false);
    if (!info->program_verified) {
      return source;
    }
    info->minimization.nodes_after = nodes_after;
    info->minimized = true;
    return reduced;
  }

  // How plain d8 processes test the script of NAME.repro.js.
  struct PlainTest {
    std::vector<std::string> flags;  // the flags before the flag search
    int runs = 1;                    // the processes of one test
    int needed = 1;                  // ... that must crash with the signature
    uint64_t timeout_us = 0;         // the time limit of one process
  };

  // The time limit of one plain d8 process of an amplified crash, if the
  // amplify step did not give one.
  static constexpr uint64_t kAmplifiedRunTimeoutUs = 30'000'000;

  // The plain test of NAME.repro.js. For an amplified crash: the amplified
  // flags (AmplifiedFlags), AmplifiedRuns() processes, and one of them must
  // crash. For a crash that reproduces alone: the flags of the target,
  // crash_flag_runs processes, and all of them must crash.
  PlainTest ReproTest(const CrashInfo& info) const {
    PlainTest test;
    if (!info.reproducible() && info.amplify.amplified) {
      test.flags = AmplifiedFlags(CurrentTarget().args, info.amplify.args);
      test.runs = AmplifiedRuns(info.amplify.hits, info.amplify.processes);
      test.needed = 1;
      test.timeout_us = info.amplify.run_timeout_us > 0
                            ? info.amplify.run_timeout_us
                            : kAmplifiedRunTimeoutUs;
      return test;
    }
    test.flags = CurrentTarget().args;
    test.runs = config().crash_flag_runs;
    test.needed = test.runs;
    test.timeout_us = config().target.timeout_us + kPlainStartupUs;
    return test;
  }

  // Removes the d8 flags that NAME.repro.js at BASE.repro.js does not need
  // (DESIGN.md 5.6). A list of flags passes if the script passes the plain
  // test of ReproTest(). The test file has the text of NAME.repro.js, so
  // that the lines of the script are the same. If the script does not pass
  // with all flags of the test, NAME.repro.js keeps all flags.
  void MinimizeReproFlags(const std::string& base, CrashInfo* info) {
    if (!FlagSearchOn()) {
      info->repro_flags_note = "all flags of the test, the search is off";
      return;
    }
    const PlainTest test = ReproTest(*info);
    // The search removes single words. A flag with its value in the next
    // word would lose its value.
    for (const std::string& arg : test.flags) {
      if (!arg.starts_with("-")) {
        info->repro_flags_note =
            "all flags of the test: a flag has its value in a separate word";
        return;
      }
    }
    // The suffix is not .js, so that `replay` of the crashes directory does
    // not run this temporary file.
    const std::string path = base + ".repro.tmp";
    std::string error;
    if (!WriteStringToFile(
            path, ReproText(*info, test.flags, base + ".repro.js"), &error)) {
      std::remove(path.c_str());
      info->repro_flags_note = "all flags of the test: " + error;
      return;
    }
    const Stopwatch clock;
    PlainCrashRunner runner(CurrentTarget(), info->signature, test.timeout_us);
    runner.set_stop([this] { return stopping(); });
    auto crashes = [&](const std::vector<std::string>& flags) {
      std::vector<std::string> args = flags;
      args.push_back(path);
      return runner.Crashes(args, test.runs, test.needed);
    };
    // The check with all flags of the test counts as the first test.
    if (!crashes(test.flags)) {
      info->repro_flag_tests = 1;
      info->repro_flags_note =
          !runner.error().empty()
              ? "all flags of the test: cannot start d8: " + runner.error()
          : stopping()
              ? "all flags of the test, the session stopped"
              : StringPrintf(
                    "all flags of the test: %d of %d plain d8 runs "
                    "crashed with the signature, %d needed",
                    runner.last_hits(), runner.last_runs(), test.needed);
    } else {
      // KeepReproFlag keeps --allow-natives-syntax for a % call, the flags
      // that make the run more deterministic, and --stress-runs. Other
      // flags that the script needs to parse stay too, because the script
      // cannot crash without them.
      const bool natives = HasNativeCall(info->repro);
      const uint64_t budget_us =
          static_cast<uint64_t>(config().crash_flag_budget_sec * 1e6);
      const FlagMinimization result = MinimizeFlags(
          test.flags,
          [natives](const std::string& flag) {
            return KeepReproFlag(flag, natives);
          },
          crashes,
          [&] {
            return stopping() || !runner.error().empty() ||
                   runner.processes() + test.runs >
                       config().crash_flag_processes ||
                   clock.ElapsedUs() > budget_us;
          });
      info->repro_flags = result.flags;
      info->repro_flag_tests = result.tests + 1;
      const char* end = !result.budget_exhausted  ? ""
                        : stopping()              ? ", the session stopped"
                        : !runner.error().empty() ? ", cannot start d8"
                                                  : ", budget used up";
      info->repro_flags_note =
          StringPrintf("%zu -> %zu flags in %d tests, %d processes, %.0f s%s",
                       test.flags.size(), result.flags.size(),
                       info->repro_flag_tests, runner.processes(),
                       static_cast<double>(clock.ElapsedUs()) / 1e6, end);
    }
    info->repro_flag_processes = runner.processes();
    stats().flag_processes += static_cast<uint64_t>(runner.processes());
    std::remove(path.c_str());
  }

  // Minimizes an amplified crash (DESIGN.md 5.6). Such a crash does not
  // reproduce alone. It reproduces in plain d8 processes with the flags of
  // the amplify step. The minimizer reduces one tree of the prelude and the
  // program. The program is the program of the amplify step, if the step
  // changed it, or else `source`. A test runs the printed tree in the plain
  // processes of ReproTest(), and passes if one of them crashes with the
  // signature. crash_amplified_budget_sec limits only the tests of the
  // minimizer. The time limit starts after the start check, and the last
  // check runs after the time limit. The result goes to NAME.repro.js only.
  // NAME.js keeps the original program.
  void MinimizeAmplifiedCrash(uint64_t hash, const std::string& source,
                              const Program& program, CrashInfo* info) {
    // NAME.js keeps the original program, so the counts of the program do
    // not change.
    info->minimization.nodes_before = program.node_count();
    info->minimization.nodes_after = program.node_count();
    const uint64_t budget_us =
        static_cast<uint64_t>(config().crash_amplified_budget_sec * 1e6);
    if (budget_us == 0) {
      info->min_tree = "not minimized: crash_amplified_budget_sec is 0";
      return;
    }
    // AmplifiedFlags removes single words. A flag with its value in the next
    // word would lose its value.
    for (const std::string& arg : CurrentTarget().args) {
      if (!arg.starts_with("-")) {
        info->min_tree =
            "not minimized: a flag has its value in a separate word";
        return;
      }
    }
    const std::string& text =
        info->amplify.program.empty() ? source : info->amplify.program;
    std::string error;
    std::unique_ptr<ScriptTree> script =
        ScriptTree::Parse(config().prelude, text, &error);
    if (script == nullptr) {
      // The report has one line for the reason.
      std::replace(error.begin(), error.end(), '\n', ' ');
      info->min_tree = "not minimized: " + error;
      return;
    }
    // The time of the tests of the minimizer. Restart() below starts it
    // after the start check. Thus a slow start check, for example on a
    // loaded machine, does not use up the time of the minimizer.
    Stopwatch tests_clock;
    const PlainTest test = ReproTest(*info);
    info->min_oracle = StringPrintf(
        "1 of %d plain d8 runs with the amplified flags", test.runs);
    // The suffix is not .js, so that `replay` of the crashes directory does
    // not run this temporary file.
    const std::string path = StringPrintf(
        "%s/crashes/amplified_%016llx.tmp", config().out_dir.c_str(),
        static_cast<unsigned long long>(hash));
    PlainCrashRunner runner(CurrentTarget(), info->signature, test.timeout_us);
    runner.set_stop([this] { return stopping(); });
    std::string write_error;
    // One test, without the time limit.
    auto passes = [&](const std::string& candidate) {
      write_error.clear();
      if (stopping()) {
        return false;
      }
      if (!WriteStringToFile(path, candidate, &write_error)) {
        std::remove(path.c_str());
        return false;
      }
      std::vector<std::string> args = test.flags;
      args.push_back(path);
      return runner.Crashes(args, test.runs, test.needed);
    };
    // The tests of the minimizer stop at the time limit.
    MinimizationOracle oracle = [&](const std::string& candidate) {
      return tests_clock.ElapsedUs() <= budget_us && passes(candidate);
    };
    // True if the text passes the test in one of verify_attempts tries. The
    // start check and the last check do not stop at the time limit. Thus a
    // minimization that used up its time can still give a result.
    auto verified = [&](const std::string& candidate) {
      for (int attempt = 0; attempt < config().verify_attempts && !stopping();
           attempt++) {
        if (passes(candidate)) {
          return true;
        }
      }
      return false;
    };
    // Why a check failed: a stop, a test file that Redline cannot write, the
    // time limit, or no crash.
    auto reason = [&]() -> std::string {
      if (stopping()) {
        return "the session stopped";
      }
      if (!write_error.empty()) {
        return "cannot write the test file: " + write_error;
      }
      if (!runner.error().empty()) {
        return "cannot start d8: " + runner.error();
      }
      return "";
    };
    if (!verified(fast::Print(script->tree()->root()))) {
      const std::string why = reason();
      info->min_tree =
          "not minimized: " +
          (why.empty() ? "the printed script did not crash with the amplified "
                         "flags"
                       : why);
    } else {
      info->min_tree = config().prelude.empty() ? "program, without a prelude"
                                                : "prelude and program";
      info->prelude_in_tree = true;
      info->prelude_statements_before = script->prelude_statements();
      MinimizerOptions options = config().crash_minimizer;
      options.max_time_ms = std::max<uint64_t>(1, budget_us / 1000);
      tests_clock.Restart();
      info->minimization = fuzzer_->MinimizeCrash(script->tree(), oracle,
                                                  probe_checker(), options);
      if (stopping()) {
        // The oracle refused every test after the stop request.
        info->minimization.stop = MinimizationStop::kStopped;
      }
      stats().minimizer_tests +=
          static_cast<uint64_t>(info->minimization.tests);
      stats().minimizer_time_us += info->minimization.time_us;
      // Until the check below accepts the result, the counts after are the
      // counts before.
      info->script_nodes_before = info->minimization.nodes_before;
      info->script_nodes_after = info->script_nodes_before;
      info->prelude_statements_after = info->prelude_statements_before;
      info->minimization.nodes_before = program.node_count();
      info->minimization.nodes_after = program.node_count();
      if (info->minimization.accepted > 0) {
        const std::string candidate = fast::Print(script->tree()->root());
        if (verified(candidate)) {
          info->script_nodes_after = fast::CountNodes(script->tree()->root());
          info->prelude_statements_after = script->PreludeStatementsNow();
          info->repro = candidate;
          // MinimizeReproFlags can remove flags later.
          info->repro_flags = test.flags;
          info->repro_flags_note = "all flags of the test, no search";
        } else {
          stats().minimizer_rejected++;
          const std::string why = reason();
          info->min_tree += ", the last check failed";
          if (!why.empty()) {
            info->min_tree += ": " + why;
          }
        }
      }
    }
    stats().crash_min_processes += static_cast<uint64_t>(runner.processes());
    std::remove(path.c_str());
  }

  // True if crash_flag_processes and crash_flag_budget_sec turn on the
  // flag search of NAME.repro.js.
  bool FlagSearchOn() const {
    return config().crash_flag_processes > 0 &&
           config().crash_flag_budget_sec > 0;
  }

  // The processes of the rate check of a crash that reproduces alone. If
  // the flag search is on and crash_flag_runs is larger, the check uses
  // crash_flag_runs.
  static constexpr int kReproRateRuns = 5;

  // The rate check of NAME.repro.js (DESIGN.md 5.6). A test of the
  // minimizer or of the flag search can accept a script that crashes less
  // often than the original crash. Thus Redline runs the text of
  // NAME.repro.js with repro_flags in plain d8 processes, without an early
  // stop. For an amplified crash, the processes and the bar come from the
  // amplify step. For a crash that reproduces alone, all processes must
  // crash: max(kReproRateRuns, crash_flag_runs) processes if the flag search
  // is on, else kReproRateRuns. If the count is complete and below the bar,
  // and the flag search removed flags, Redline also counts all flags of the
  // test. DecideAmplifiedRate or DecideReproducibleRate decides with these
  // counts.
  void CheckReproRate(const std::string& base, CrashInfo* info) {
    const PlainTest test = ReproTest(*info);
    const bool amplified = !info->reproducible();
    int planned = kReproRateRuns;
    if (amplified) {
      planned = std::max(1, info->amplify.planned_processes);
    } else if (FlagSearchOn()) {
      planned = std::max(kReproRateRuns, test.runs);
    }
    const int bar =
        amplified ? std::clamp(info->amplify.min_hits, 1, planned) : planned;
    // The suffix is not .js, so that `replay` of the crashes directory does
    // not run this temporary file.
    const std::string path = base + ".rate.tmp";
    PlainCrashRunner runner(CurrentTarget(), info->signature, test.timeout_us);
    runner.set_stop([this] { return stopping(); });
    // Why a count stopped early: a stop, a test file that Redline cannot
    // write, or a d8 that did not start.
    std::string stop_cause = "the check stopped";
    // The count of the script with `flags`. A write error gives no runs.
    auto count = [&](const std::vector<std::string>& flags) {
      RateCount result;
      std::string error;
      if (!WriteStringToFile(path, ReproText(*info, flags, base + ".repro.js"),
                             &error)) {
        std::remove(path.c_str());
        stop_cause = "cannot write the test file: " + error;
        return result;
      }
      std::vector<std::string> args = flags;
      args.push_back(path);
      result.hits = runner.Count(args, planned);
      result.runs = runner.last_runs();
      if (stopping()) {
        stop_cause = "the session stopped";
      } else if (!runner.error().empty()) {
        stop_cause = "cannot start d8: " + runner.error();
      }
      return result;
    };
    const RateCount kept = count(info->repro_flags);
    RateCount all;
    const bool count_all = kept.runs == planned && kept.hits < bar &&
                           info->repro_flags != test.flags;
    if (count_all) {
      all = count(test.flags);
    }
    const RateCount* all_count = count_all ? &all : nullptr;
    const RateDecision decision =
        amplified
            ? DecideAmplifiedRate(bar, planned, kept, all_count, stop_cause)
            : DecideReproducibleRate(planned, kept, all_count, stop_cause);
    if (decision.all_flags && info->repro_flags != test.flags) {
      info->repro_flags = test.flags;
      info->repro_flags_note += ", the rate check restored all flags";
    }
    if (!decision.keep) {
      info->repro.clear();
      // No NAME.repro.js, so the report gives no results of its tree.
      info->script_nodes_after = info->script_nodes_before;
      info->prelude_statements_after = info->prelude_statements_before;
    }
    info->repro_rate_runs = decision.rate.runs;
    info->repro_rate_hits = decision.rate.hits;
    info->repro_rate_note = decision.note;
    stats().crash_min_processes += static_cast<uint64_t>(runner.processes());
    std::remove(path.c_str());
  }

  // Minimizes NAME.bundle.js of a state-dependent crash (DESIGN.md 5.6). A
  // test writes a candidate bundle to a file and runs it with --bundle-reprl
  // in k plain d8 processes. The test passes if one of them crashes with the
  // signature. k comes from the bundle runs, as for an amplified crash
  // (AmplifiedRuns). MinimizeBundleParts runs the steps, and the tests stop
  // at crash_bundle_min_budget_sec. The time limit starts after the start
  // check. The start check, the last check and the rate check stop only at a
  // stop request. The result goes to
  // NAME.bundle.min.js only if it crashes in at least bundle_hits of
  // bundle_runs plain d8 runs. NAME.bundle.js stays the original bundle.
  void MinimizeBundle(const std::vector<std::string>& history,
                      const std::string& source, CrashInfo* info) {
    const uint64_t budget_us =
        static_cast<uint64_t>(config().crash_bundle_min_budget_sec * 1e6);
    if (budget_us == 0) {
      info->bundle_min_note = "not minimized: crash_bundle_min_budget_sec is 0";
      return;
    }
    const Stopwatch clock;
    // One part for each history program, then the crash part. Each part is
    // a script: the prelude and the program.
    std::vector<std::string> parts;
    for (const std::string& program : history) {
      parts.push_back(ScriptFor(program));
    }
    parts.push_back(ScriptFor(source));
    const std::vector<std::string> parts_before = parts;
    std::string bundle_before;
    if (!MakeReprlBundle(BundleHeader(history.size(), info->bundle_path, *info),
                         parts, &bundle_before)) {
      info->bundle_min_note =
          "not minimized: a part contains the bundle marker";
      return;
    }
    const TargetOptions target = CurrentTarget();
    const int runs = AmplifiedRuns(info->bundle_hits, info->bundle_runs);
    // NAME.bundle.min.js, and the temporary file of the tests. The suffix is
    // not .js, so that `replay` of the crashes directory does not run it.
    const std::string min_path =
        info->bundle_path.substr(0, info->bundle_path.size() - 3) + ".min.js";
    const std::string path = min_path + ".tmp";
    // Only the tests of the steps stop at the time limit. The start check,
    // the last check and the rate check stop only at a stop request. The time
    // of the steps starts after the start check (Restart below).
    Stopwatch steps_clock;
    bool steps = false;
    bool budget_used_up = false;
    auto exhausted = [&] {
      return stopping() || (steps && steps_clock.ElapsedUs() > budget_us);
    };
    PlainCrashRunner runner(
        target, info->signature,
        config().target.timeout_us * parts.size() + kPlainStartupUs);
    runner.set_stop(exhausted);
    int tests = 0;
    std::string write_error;
    // Writes `candidate` with `header` to `file`. False on an error.
    auto write = [&](const std::vector<std::string>& candidate,
                     const std::string& header, const std::string& file) {
      std::string bundle;
      write_error.clear();
      if (!MakeReprlBundle(header, candidate, &bundle)) {
        write_error = "a part contains the bundle marker";
        return false;
      }
      if (!WriteStringToFile(file, bundle, &write_error)) {
        std::remove(file.c_str());
        return false;
      }
      return true;
    };
    auto crashes = [&](const std::vector<std::string>& candidate) {
      if (exhausted() || !write(candidate, "", path)) {
        return false;
      }
      tests++;
      std::vector<std::string> args = target.args;
      args.push_back("--bundle-reprl");
      args.push_back(path);
      return runner.Crashes(args, runs, 1);
    };
    // True if the bundle passes the test in one of verify_attempts tries.
    auto verified = [&](const std::vector<std::string>& candidate) {
      for (int attempt = 0; attempt < config().verify_attempts && !stopping();
           attempt++) {
        if (crashes(candidate)) {
          return true;
        }
      }
      return false;
    };
    // Reduces part `i` with the crash reducers. The part is a script, so the
    // tests use its printed tree as it is. A reduced history part can crash
    // itself. Then the bundle still crashes with the signature.
    auto reduce = [&](std::vector<std::string>* all, size_t i) {
      std::unique_ptr<fast::Tree> tree =
          fast::Parser::ParseScript((*all)[i], fast::ParseOptions(), nullptr);
      if (tree == nullptr) {
        return;
      }
      MinimizationOracle oracle = [&](const std::string& candidate) {
        std::vector<std::string> changed = *all;
        changed[i] = candidate;
        return crashes(changed);
      };
      MinimizerOptions options = config().crash_minimizer;
      options.max_time_ms = std::max<uint64_t>(
          1, (budget_us - std::min(budget_us, steps_clock.ElapsedUs())) / 1000);
      const MinimizationResult result =
          fuzzer_->MinimizeCrash(tree.get(), oracle, probe_checker(), options);
      if (result.accepted > 0) {
        (*all)[i] = fast::Print(tree->root());
      }
    };
    std::string note;
    if (!verified(parts)) {
      note = stopping() ? "not minimized: the session stopped"
             : !write_error.empty()
                 ? "not minimized: cannot write the test file: " + write_error
                 : "not minimized: the bundle did not crash again";
    } else {
      steps_clock.Restart();
      steps = true;
      parts = MinimizeBundleParts(parts, crashes, reduce, exhausted);
      steps = false;
      budget_used_up = !stopping() && steps_clock.ElapsedUs() > budget_us;
      std::string bundle_after;
      MakeReprlBundle(BundleHeader(parts.size() - 1, min_path, *info), parts,
                      &bundle_after);
      // Compare the parts without the headers, which have other file names
      // and program counts.
      std::string parts_after_text, parts_before_text;
      MakeReprlBundle("", parts, &parts_after_text);
      MakeReprlBundle("", parts_before, &parts_before_text);
      if (parts == parts_before ||
          parts_after_text.size() >= parts_before_text.size()) {
        note = "not minimized: no smaller bundle crashed";
      } else if (!verified(parts)) {
        note = stopping() ? "not minimized: the session stopped"
                          : "not minimized: the smaller bundle did not crash "
                            "again";
      } else if (!write(parts, BundleHeader(parts.size() - 1, min_path, *info),
                        path)) {
        note = "not minimized: cannot write the bundle: " + write_error;
      } else {
        // The rate check: the original bundle crashed in bundle_hits of
        // bundle_runs runs, and the minimized bundle must crash as often.
        std::vector<std::string> args = target.args;
        args.push_back("--bundle-reprl");
        args.push_back(path);
        info->bundle_min_hits = runner.Count(args, info->bundle_runs);
        info->bundle_min_runs = runner.last_runs();
        if (info->bundle_min_runs < info->bundle_runs) {
          note = StringPrintf(
              "not minimized: the rate check stopped after %d of %d runs",
              info->bundle_min_runs, info->bundle_runs);
        } else if (info->bundle_min_hits < info->bundle_hits) {
          note = StringPrintf(
              "not minimized: the smaller bundle crashed in %d of %d runs, "
              "less than NAME.bundle.js",
              info->bundle_min_hits, info->bundle_min_runs);
        } else if (std::rename(path.c_str(), min_path.c_str()) != 0) {
          note = "not minimized: cannot rename the bundle";
        } else {
          info->bundle_min_path = min_path;
          note = StringPrintf("%zu -> %zu parts, %zu -> %zu bytes",
                              parts_before.size(), parts.size(),
                              bundle_before.size(), bundle_after.size());
          if (parts.size() == 1) {
            note += ", the crash part crashes alone in plain d8";
          }
        }
      }
    }
    info->bundle_min_note =
        note + StringPrintf(
                   ", %d tests, %d processes, %.0f s, 1 of %d plain "
                   "d8 runs per test%s",
                   tests, runner.processes(),
                   static_cast<double>(clock.ElapsedUs()) / 1e6, runs,
                   budget_used_up ? ", budget used up" : "");
    stats().minimizer_tests += static_cast<uint64_t>(tests);
    stats().minimizer_time_us += clock.ElapsedUs();
    stats().crash_min_processes += static_cast<uint64_t>(runner.processes());
    std::remove(path.c_str());
  }

  void ProcessCrash(const std::string& source, const Program& program,
                    const Execution& execution, const Lineage& lineage) {
    FlagPin pin(runner_->child());
    SharedCoverage& shared = fuzzer_->coverage_;
    const bool unique = shared.ClaimNewCrashEdges(runner_->coverage().bitmap());
    const uint64_t hash = Program::HashSource(source);
    // The crash's edges, for a seed strategy that wants them (taken now:
    // the reruns below overwrite the coverage map).
    const std::vector<uint32_t> crash_edges =
        fuzzer_->corpus().WantsHitEdges() ? runner_->coverage().CurrentEdges()
                                          : std::vector<uint32_t>();
    CrashInfo info;
    info.signature = CrashSignature::FromExecution(execution);
    // Executions of this target before the crashing one, and those programs
    // (Execute() appended the crasher itself; drop it).
    info.child_age = std::max(0, runner_->child().execs_since_spawn() - 1);
    std::vector<std::string> history(history_.begin(), history_.end());
    if (!history.empty()) {
      history.pop_back();
    }
    info.history_available = history.size();
    const Fuzzer::CrashDecision decision =
        fuzzer_->RegisterCrash(hash, info.signature);
    if (unique) {
      stats().unique_crashes++;
    } else {
      stats().duplicate_crashes++;
    }
    REDLINE_LOG(kLog, "worker %d: crash #%d of signature \"%s\" (%s, %s)", id_,
                decision.signature_count, info.signature.text.c_str(),
                unique ? "new crash edges" : "known crash edges",
                decision.save ? "saving" : "only counted");

    // 1. Does it reproduce in a fresh target? The crash killed the target,
    //    so the first re-execution already runs in a new process.
    //    crash_reruns_stop_after crashes are deterministic enough to stop
    //    early.
    uint64_t exec_time_us = execution.exec_time_us;
    for (; info.reruns < config().crash_reruns && !stopping(); info.reruns++) {
      Execution again;
      if (!Execute(source, &again)) {
        break;
      }
      stats().rerun_executions++;
      exec_time_us = std::max(exec_time_us, again.exec_time_us);
      if (!again.crashed()) {
        continue;
      }
      info.reproduced++;
      if (CrashSignature::FromExecution(again) == info.signature) {
        info.same_signature++;
      }
      if (info.reproduced >= config().crash_reruns_stop_after) {
        info.reruns++;
        break;
      }
    }
    if (info.reproducible()) {
      stats().crashes_reproduced++;
      if (!crash_edges.empty()) {
        fuzzer_->corpus().RecordCrashEdges(crash_edges);
      }
    }

    // 2. If not: does it reproduce behind the programs the same target ran
    //    before it? If so, the shortest suffix of that history that still
    //    reproduces it is determined by binary search (every probe replays
    //    the suffix in a fresh target). Only worth doing for crashes that
    //    are saved, within the time budget.
    const uint64_t replay_start_us = MonotonicNowUs();
    const uint64_t replay_budget_us =
        static_cast<uint64_t>(config().crash_history_budget_sec * 1e6);
    auto over_budget = [&] {
      return MonotonicNowUs() - replay_start_us > replay_budget_us;
    };
    if (!info.reproducible() && decision.save && !history.empty() &&
        replay_budget_us > 0 && !stopping()) {
      info.replayed = true;
      const uint64_t replay_execs_before =
          stats().crash_replay_executions.load();
      Execution e;
      if (ReplayCrash(history, history.size(), source, &e)) {
        info.state_dependent = true;
        size_t low = 0;                // known not to reproduce (fresh target)
        size_t high = history.size();  // known to reproduce
        while (high - low > 1 && !over_budget() && !stopping()) {
          size_t mid = low + (high - low) / 2;
          if (ReplayCrash(history, mid, source, &e)) {
            high = mid;
          } else {
            low = mid;
          }
        }
        info.history_needed = high;
        history.erase(history.begin(),
                      history.end() - static_cast<ptrdiff_t>(high));
        stats().crashes_state_dependent++;
      }
      info.replay_executions =
          stats().crash_replay_executions.load() - replay_execs_before;
    }

    // 2a. A state-dependent crash also gets a one-file reproducer: a d8
    //     bundle of the needed history and the crash program. A plain d8
    //     with --bundle-reprl runs it like the REPRL loop (DESIGN.md 5.8).
    if (info.state_dependent && !stopping()) {
      WriteReprlBundle(history, source, CrashBase(execution.signal, hash),
                       &info);
    }

    // 2b. If it does not reproduce alone: the amplify step searches for a
    //     plain d8 command line (extra flags, --stress-runs) that
    //     reproduces the program alone, reliably (DESIGN.md 5.8). This
    //     also runs for state-dependent crashes: an earlier program may
    //     have switched on a feature (for example the CPU profiler) that
    //     a flag can switch on as well. Once per signature.
    if (!info.reproducible() && decision.save && !stopping() &&
        fuzzer_->ClaimAmplify(info.signature)) {
      AmplifyRequest request;
      request.target = CurrentTarget();
      request.program = source;
      request.prelude = config().prelude;
      request.script_path = StringPrintf("%s/crashes/amplify_%016llx.js",
                                         config().out_dir.c_str(),
                                         static_cast<unsigned long long>(hash));
      request.signature = info.signature;
      request.stderr_text = execution.stderr_output;
      request.exec_time_us = exec_time_us;
      request.stopping = [this] { return stopping(); };
      info.amplify = fuzzer_->amplifier_->Amplify(request, rng_);
      info.amplify_ran = true;
      std::remove(request.script_path.c_str());
      stats().amplify_processes +=
          static_cast<uint64_t>(info.amplify.total_processes);
      if (info.amplify.amplified) {
        stats().crashes_amplified++;
      }
      REDLINE_LOG(kLog, "worker %d: signature \"%s\" %s (%d/%d with \"%s\")",
                  id_, info.signature.text.c_str(),
                  info.amplify.amplified ? "amplified" : "not amplified",
                  info.amplify.hits, info.amplify.processes,
                  info.amplify.args.c_str());
    }

    // 3. Minimization, against the same signature so that the reduction
    //    cannot wander off into a different bug. Crashes are rare, so they
    //    get their own, larger budget and all reducers.
    std::string final_source = source;
    if (info.reproducible() && config().minimize_crashes && decision.minimize &&
        !stopping()) {
      final_source =
          MinimizeReproducibleCrash(source, program, exec_time_us, &info);
    } else if (!info.reproducible() && info.amplify.amplified &&
               config().minimize_crashes && decision.minimize && !stopping()) {
      MinimizeAmplifiedCrash(hash, source, program, &info);
    }
    // 3a. The d8 flags of NAME.repro.js: remove the flags that the crash
    //     does not need.
    if (!info.repro.empty() && !stopping()) {
      MinimizeReproFlags(CrashBase(execution.signal, hash), &info);
    }
    // 3b. The rate check of NAME.repro.js. After a stop request, the check
    //     does not run its processes. Then an amplified crash gets no
    //     NAME.repro.js, and the NAME.repro.js of a crash that reproduces
    //     alone gets all flags of the target.
    if (!info.repro.empty()) {
      CheckReproRate(CrashBase(execution.signal, hash), &info);
    }
    // 3c. A state-dependent crash without NAME.repro.js: minimize its
    //     bundle, if the bundle crashed in a plain d8 run.
    if (info.state_dependent && info.repro.empty() && info.bundle_hits > 0 &&
        config().minimize_crashes && decision.minimize && !stopping()) {
      MinimizeBundle(history, source, &info);
    }

    // 4. Store.
    std::string path;
    if (decision.save) {
      WriteRepro(CrashBase(execution.signal, hash), &info);
      path = SaveCrash(source, final_source, execution, lineage, info, unique,
                       hash, history);
    }
    // A minimization that gave only NAME.repro.js or a minimized bundle also
    // used the slot. Then signatures.txt counts the crash as minimized, while
    // its report says `minimized: no` (NAME.js has the original program).
    fuzzer_->RecordCrashOutcome(info.signature, decision,
                                info.minimized || !info.repro_path.empty() ||
                                    !info.bundle_min_path.empty(),
                                info.reproducible() || info.state_dependent,
                                path);

    JsonObject event;
    event.Add("event", "crash")
        .Add("hash",
             StringPrintf("%016llx", static_cast<unsigned long long>(hash)));
    event.Add("signature", info.signature.text);
    event.Add("signature_hash",
              StringPrintf("%016llx", static_cast<unsigned long long>(
                                          info.signature.hash)));
    event.Add("signature_count", decision.signature_count);
    event.Add("signal", execution.signal)
        .Add("unique_edges", unique)
        .Add("saved", decision.save);
    event.Add("reproducible", info.reproducible()).Add("reruns", info.reruns);
    event.Add("reproduced", info.reproduced)
        .Add("same_signature", info.same_signature);
    event.Add("child_age", info.child_age).Add("replayed", info.replayed);
    event.Add("state_dependent", info.state_dependent);
    event.Add("history_needed", static_cast<uint64_t>(info.history_needed));
    event.Add("history_available",
              static_cast<uint64_t>(info.history_available));
    event.Add("replay_executions", info.replay_executions);
    event.Add("minimized", info.minimized)
        .Add("min_tests", info.minimization.tests);
    event.Add("min_nodes_before",
              static_cast<uint64_t>(info.minimization.nodes_before));
    event.Add("min_nodes_after",
              static_cast<uint64_t>(info.minimization.nodes_after));
    event.Add("min_time_ms",
              static_cast<double>(info.minimization.time_us) / 1000.0);
    event.Add("min_budget_exhausted", info.minimization.budget_exhausted);
    event.Add("min_stop", MinimizationStopName(info.minimization.stop));
    if (!info.min_tree.empty()) {
      event.Add("min_tree", info.min_tree);
    }
    if (!info.min_oracle.empty()) {
      event.Add("min_oracle", info.min_oracle);
    }
    if (!info.repro_rate_note.empty()) {
      event.Add("repro_rate", StringPrintf("%d/%d", info.repro_rate_hits,
                                           info.repro_rate_runs));
      event.Add("repro_rate_note", info.repro_rate_note);
    }
    if (info.prelude_in_tree) {
      event.Add("min_prelude_statements_before",
                static_cast<uint64_t>(info.prelude_statements_before));
      event.Add("min_prelude_statements_after",
                static_cast<uint64_t>(info.prelude_statements_after));
      event.Add("min_script_nodes_before",
                static_cast<uint64_t>(info.script_nodes_before));
      event.Add("min_script_nodes_after",
                static_cast<uint64_t>(info.script_nodes_after));
    }
    if (!info.repro_path.empty()) {
      if (info.reproducible()) {
        event.Add("min_program_verified", info.program_verified);
      }
      event.Add("repro", info.repro_path)
          .Add("repro_bytes", static_cast<uint64_t>(info.repro_bytes));
      event.Add("repro_flags", JoinArgs(info.repro_flags))
          .Add("repro_flags_note", info.repro_flags_note);
      event.Add("repro_flag_tests", info.repro_flag_tests)
          .Add("repro_flag_processes", info.repro_flag_processes);
    }
    event.Add("amplify", info.amplify_ran
                             ? (info.amplify.amplified ? "yes" : "no")
                             : "skipped");
    if (info.state_dependent) {
      event.Add("bundle", info.bundle_path)
          .Add("bundle_runs", info.bundle_runs)
          .Add("bundle_hits", info.bundle_hits);
      if (!info.bundle_min_note.empty()) {
        event.Add("bundle_min", info.bundle_min_path)
            .Add("bundle_min_note", info.bundle_min_note);
        event.Add("bundle_min_rate", StringPrintf("%d/%d", info.bundle_min_hits,
                                                  info.bundle_min_runs));
      }
    }
    if (info.amplify_ran) {
      event.Add("amplified_args", info.amplify.args);
      event.Add("amplified_hits", info.amplify.hits)
          .Add("amplified_processes", info.amplify.processes);
      event.Add("amplify_processes", info.amplify.total_processes);
      event.Add("amplify_time_sec", info.amplify.time_sec)
          .Add("amplify_note", info.amplify.note);
    }
    event.Add("origin", lineage.origin)
        .Add("detail", lineage.detail)
        .Add("parent", lineage.parent_id);
    event.Add("path", path).Add("worker", id_);
    if (config().randomize_d8_flags) {
      event.Add("d8_random_args", RandomArgsText());
    }
    event.Add("stderr", FirstLines(execution.stderr_output));
    fuzzer_->LogEvent(event);
  }

  // Writes NAME.js (the minimized program, or the original), NAME.orig.js
  // (if minimized), NAME.txt (the report) and NAME.history/NNN.js (the
  // programs a state-dependent crash needs first). Returns the .js path.
  std::string SaveCrash(const std::string& source,
                        const std::string& final_source,
                        const Execution& execution, const Lineage& lineage,
                        const CrashInfo& info, bool unique, uint64_t hash,
                        const std::vector<std::string>& history) {
    std::string base = CrashBase(execution.signal, hash);
    std::string status;
    if (info.reproducible()) {
      status = StringPrintf("reproducible (%d/%d fresh-target runs crashed)",
                            info.reproduced, info.reruns);
    } else if (info.state_dependent) {
      status = StringPrintf(
          "STATE-DEPENDENT: does not crash alone, crashes after the %zu "
          "program(s) in "
          "%s.history/ ran in the same d8 (replay them in order first)",
          info.history_needed, base.substr(base.rfind('/') + 1).c_str());
      if (!info.bundle_path.empty()) {
        status += StringPrintf(
            ", or run %s with d8 --bundle-reprl (%d/%d plain d8 runs crashed)",
            BaseName(info.bundle_path).c_str(), info.bundle_hits,
            info.bundle_runs);
      }
    } else if (info.amplify.amplified) {
      status = StringPrintf(
          "AMPLIFIED: not reproduced in %d fresh-target runs, but %d/%d plain "
          "d8 processes crashed with the "
          "extra flags: %s",
          info.reruns, info.amplify.hits, info.amplify.processes,
          info.amplify.args.c_str());
    } else {
      status = StringPrintf(
          "NOT reproduced (%d fresh-target runs%s)", info.reruns,
          info.replayed ? ", nor behind the target's history" : "");
    }
    std::string header = StringPrintf(
        "// redline crash: signal %d, %s\n// signature: %s\n// %s, worker %d "
        "(target had run %d "
        "programs), origin %s (%s), parent #%u, generation %d\n// d8 %s\n",
        execution.signal, info.minimized ? "minimized" : "not minimized",
        info.signature.text.c_str(), status.c_str(), id_, info.child_age,
        lineage.origin.c_str(), lineage.detail.c_str(), lineage.parent_id,
        lineage.generation, CurrentTarget().CommandLine().c_str());
    std::string error;
    std::string path = base + ".js";
    if (!WriteStringToFile(path, header + final_source, &error)) {
      REDLINE_WARN(kLog, "%s", error.c_str());
    }
    if (info.minimized) {
      WriteStringToFile(base + ".orig.js", header + source, &error);
    }
    // The program the amplified flags reproduce, if the amplify step
    // changed it (wrapped in try/catch).
    std::string amplified_program;
    if (info.amplify.amplified && !info.amplify.program.empty()) {
      amplified_program = base.substr(base.rfind('/') + 1) + ".amplified.js";
      WriteStringToFile(base + ".amplified.js", header + info.amplify.program,
                        &error);
    }
    if (info.state_dependent) {
      std::string dir = base + ".history";
      if (MakeDirectories(dir, &error)) {
        for (size_t i = 0; i < history.size(); i++) {
          WriteStringToFile(StringPrintf("%s/%03zu.js", dir.c_str(), i),
                            history[i], &error);
        }
      } else {
        REDLINE_WARN(kLog, "%s", error.c_str());
      }
    }
    std::string report = StringPrintf(
        "signal: %d\nexit_code: %d\nsignature: %s\nunique_crash_edges: %s\n"
        "reproducible: %s\nfresh_target_runs: %d\nfresh_target_crashes: %d\n"
        "same_signature_crashes: %d\ntarget_age: %d executions before the "
        "crash\n"
        "history_replayed: %s\nstate_dependent: %s\nhistory_needed: %zu of "
        "%zu\n"
        "minimized: %s (%d tests, %zu -> %zu nodes)\n%sd8_random_args: %s\n"
        "%s%s\n--- stderr "
        "---\n%s\n--- stdout ---\n%s\n"
        "--- fuzzout ---\n%s\n",
        execution.signal, execution.exit_code, info.signature.text.c_str(),
        unique ? "yes" : "no", info.reproducible() ? "yes" : "no", info.reruns,
        info.reproduced, info.same_signature, info.child_age,
        info.replayed ? "yes" : "no", info.state_dependent ? "yes" : "no",
        info.history_needed, info.history_available,
        info.minimized ? "yes" : "no", info.minimization.tests,
        info.minimization.nodes_before, info.minimization.nodes_after,
        MinimizationReportLines(info).c_str(), RandomArgsText().c_str(),
        AmplifyReportLines(info, amplified_program).c_str(),
        BundleReportLines(info).c_str(),
        Truncate(execution.stderr_output, kMaxSavedOutput).c_str(),
        Truncate(execution.stdout_output, kMaxSavedOutput).c_str(),
        Truncate(execution.fuzzout, kMaxSavedOutput).c_str());
    WriteStringToFile(base + ".txt", report, &error);
    REDLINE_WARN(
        kLog, "CRASH [%s] (%s%s%s) saved to %s", info.signature.text.c_str(),
        unique ? "new crash edges" : "known crash edges",
        info.reproducible() ? ""
                            : (info.state_dependent ? ", state-dependent"
                                                    : ", not reproducible"),
        info.minimized ? ", minimized" : "", path.c_str());
    return path;
  }

  // Separates the random flags from the other random decisions: a change
  // of the flags does not change the programs.
  static constexpr uint64_t kFlagsSeedSalt = 0x5DEECE66Dull;

  Fuzzer* fuzzer_;
  int id_;
  Random rng_;
  Random flags_rng_;  // random d8 flags (randomize_d8_flags)
  std::unique_ptr<ScriptRunner> runner_;
  std::unique_ptr<SyntaxChecker> checker_;
  std::thread thread_;
  int consecutive_failures_ = 0;
  uint64_t executions_ = 0;  // target executions of this worker
  std::deque<std::string> history_;
  size_t history_bytes_ = 0;
  std::vector<uint8_t> child_target_buckets_;
  uint32_t child_target_hits_ = 0;
  uint32_t child_execs_ = 0;
};

// ---------------------------------------------------------------------------
// Fuzzer

Fuzzer::Fuzzer(FuzzConfig config) : config_(std::move(config)) {}

Fuzzer::~Fuzzer() {
  for (auto& worker : workers_) {
    worker->Join();
  }
  if (feature_weights_ != nullptr &&
      FeatureWeights::Active() == feature_weights_.get()) {
    FeatureWeights::SetActive(nullptr);
  }
}

bool Fuzzer::Initialize(std::string* error) {
  if (!MakeDirectories(config_.out_dir + "/crashes", error)) {
    return false;
  }
  if (config_.save_corpus &&
      !MakeDirectories(config_.out_dir + "/corpus", error)) {
    return false;
  }
  std::string events_path = config_.out_dir + "/events.jsonl";
  if (!events_.Open(events_path, /*truncate=*/false)) {
    *error = "cannot open " + events_path;
    return false;
  }
  if (!InitializeV8(config_.v8_flags, error)) {
    return false;
  }

  // The amplify step (DESIGN.md 5.8); FuzzConfig::Load validated it.
  {
    JsonValue amplify_params;
    if (!JsonValue::Parse(config_.amplify_params, &amplify_params, error)) {
      return false;
    }
    amplifier_ =
        CreateAmplifyStrategy(config_.amplify_strategy, amplify_params, error);
    if (amplifier_ == nullptr) {
      return false;
    }
  }

  // The selection strategies (core/selection.h). The configuration was
  // validated by FuzzConfig::Load, so errors here are unexpected.
  std::unique_ptr<ProgramSelector> seeds =
      SelectorRegistry<const Program>::Instance().Create(
          config_.selection.seeds, error);
  if (seeds == nullptr) {
    return false;
  }
  SelectionConfig partner_config = config_.selection.partners;
  if (partner_config.strategy.empty()) {
    partner_config.strategy = "uniform-partner";
  }
  std::unique_ptr<ProgramSelector> partners =
      SelectorRegistry<const Program>::Instance().Create(partner_config, error);
  if (partners == nullptr) {
    return false;
  }
  corpus_ = std::make_unique<Corpus>(std::move(seeds), std::move(partners));
  mutators_ = MutatorSet::CreateDefault();
  generators_ = GeneratorSet::CreateDefault();
  minimizer_ = std::make_unique<Minimizer>(config_.minimizer);
  for (const std::string& name : config_.minimizer_reducers) {
    minimizer_reducers_.push_back(
        minimizer_->Find(name));  // names validated by FuzzConfig::Load
  }
  if (config_.crash_minimizer_reducers.empty()) {
    for (std::string_view name : Minimizer::CrashOrder()) {
      crash_reducers_.push_back(minimizer_->Find(name));
    }
  } else {
    // FuzzConfig::Load checked the names.
    for (const std::string& name : config_.crash_minimizer_reducers) {
      crash_reducers_.push_back(minimizer_->Find(name));
    }
  }
  env_ = &Environment::Default();
  std::unique_ptr<Selector<Mutator>> mutator_selector =
      SelectorRegistry<Mutator>::Instance().Create(config_.selection.mutators,
                                                   error);
  if (mutator_selector == nullptr ||
      !mutators_->SetSelector(std::move(mutator_selector), error)) {
    return false;
  }
  std::unique_ptr<Selector<Generator>> generator_selector =
      SelectorRegistry<Generator>::Instance().Create(
          config_.selection.generators, error);
  if (generator_selector == nullptr ||
      !generators_->SetSelector(std::move(generator_selector), error)) {
    return false;
  }
  mutators_->ConfigurePrefix(config_.prefix_min_values,
                             config_.prefix_extra_values);
  codegen_mutator_ = mutators_->Find("CodeGen");
  if (codegen_mutator_ == nullptr) {
    *error = "the CodeGen mutator is required for bootstrapping";
    return false;
  }
  prefix_mutator_ = mutators_->Find("Prefix");
  Lineage empty;
  empty.origin = "empty";
  empty_program_ = Program::Parse("", empty, error);
  if (empty_program_ == nullptr) {
    return false;
  }

  CollectImports();

  // Coverage counters and change-based fuzzing: the targets become watched
  // edges.
  config_.target.watch_optimized = config_.watch_optimized;
  uint32_t targets_code_edges = 0;
  if (!config_.targets_file.empty()) {
    if (!ReadTargetsFile(config_.targets_file, &targets_, &focus_,
                         &targets_code_edges, error)) {
      return false;
    }
    if (!config_.focus) {
      focus_.clear();
    }
    config_.target.watch_edges.clear();
    for (const auto& target : targets_) {
      config_.target.watch_edges.push_back(target.first);
    }
  }

  for (int i = 0; i < config_.jobs; i++) {
    auto worker = std::make_unique<Worker>(this, i);
    if (!worker->Start(error)) {
      *error = StringPrintf("worker %d: %s", i, error->c_str());
      return false;
    }
    workers_.push_back(std::move(worker));
  }
  if (!config_.targets_file.empty() && workers_[0]->coverage().is_fuzzilli()) {
    *error =
        "--targets is not supported with a Fuzzilli target (v8_fuzzilli=true "
        "has no coverage counters or PC table); use a d8 built with "
        "v8_redline=true and "
        "sanitizer_coverage_flags=\"inline-8bit-counters,pc-table\"";
    return false;
  }

  if (!InitializeFeatureWeights(targets_code_edges, error)) {
    return false;
  }

  // Ask the target what it offers beyond the hand-written model.
  if (config_.introspect) {
    std::string json, problem;
    if (!workers_[0]->Introspect(&json, &problem)) {
      REDLINE_WARN(kLog,
                   "environment introspection failed (%s); using the built-in "
                   "model only",
                   problem.c_str());
    } else {
      discovered_env_ = Environment::WithDiscovered(json, &problem);
      if (discovered_env_ == nullptr) {
        REDLINE_WARN(kLog,
                     "environment introspection unusable (%s); using the "
                     "built-in model only",
                     problem.c_str());
      } else {
        env_ = discovered_env_.get();
        REDLINE_LOG(kLog,
                    "environment: %zu globals/namespaces with %zu members "
                    "discovered in the target",
                    env_->discovered().size(), env_->discovered_member_count());
      }
    }
  }
  return true;
}

bool Fuzzer::InitializeFeatureWeights(uint32_t targets_code_edges,
                                      std::string* error) {
  const CoverageMap& coverage = workers_[0]->coverage();
  uint32_t code_edges = 0;
  for (const CoverageMap::Region& region : coverage.regions()) {
    if (region.kind == CoverageMap::RegionKind::kCode) {
      code_edges += region.count;
    } else if (region.kind == CoverageMap::RegionKind::kOptimized) {
      geometry_.optimized_first = region.first;
      geometry_.optimized_count = region.count;
    }
  }
  // The weights and the admission rule of the optimized code features apply
  // in both layouts. Without coverage counters and without optimized code
  // features, all features keep the weight 1.
  if (!coverage.counters_mode() && geometry_.optimized_count == 0) {
    return true;
  }
  if (targets_code_edges != 0 && targets_code_edges != code_edges) {
    *error = StringPrintf(
        "%s was made for a d8 with %u code edges, but the target has %u. Run "
        "tools/change_targets.py again for this d8.",
        config_.targets_file.c_str(), targets_code_edges, code_edges);
    return false;
  }
  geometry_.counters = coverage.counters_mode();
  geometry_.bucket_first = coverage.bucket_base();
  geometry_.features = coverage.num_edges();
  geometry_.watched = static_cast<uint32_t>(coverage.watched().size());
  feature_weights_ = FeatureWeights::Build(coverage, config_.feature_weights,
                                           targets_, focus_);
  FeatureWeights::SetActive(feature_weights_.get());
  REDLINE_LOG(kLog,
              "coverage %s: %u edges (%u code), %u optimized code features, "
              "%u watched edges, %zu target edges, %u focus edges",
              geometry_.counters ? "counters" : "bits", coverage.real_edges(),
              code_edges, geometry_.optimized_count, geometry_.watched,
              feature_weights_->target_edges().size(),
              feature_weights_->focus_edges());
  return true;
}

bool Fuzzer::KeepFocused(std::vector<uint32_t>* claimed) {
  const FeatureWeights* weights = feature_weights_.get();
  if (claimed->empty() || weights == nullptr || !weights->filters_admission()) {
    return !claimed->empty();
  }
  const size_t before = claimed->size();
  claimed->erase(std::remove_if(claimed->begin(), claimed->end(),
                                [weights](uint32_t feature) {
                                  return !weights->InFocus(feature);
                                }),
                 claimed->end());
  unfocused_features_.fetch_add(before - claimed->size(),
                                std::memory_order_relaxed);
  if (claimed->empty()) {
    unfocused_programs_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  return true;
}

void Fuzzer::RecordTargetFlags(const std::vector<std::string>& args,
                               double score) {
  if (score <= 0) {
    return;
  }
  std::scoped_lock<std::mutex> lock(target_flags_mutex_);
  for (TargetFlagEntry& entry : target_flags_) {
    if (entry.args == args) {
      entry.score = std::max(entry.score, score);
      return;
    }
  }
  if (target_flags_.size() < 64) {
    target_flags_.push_back(TargetFlagEntry{args, score});
  } else {
    auto worst = std::min_element(
        target_flags_.begin(), target_flags_.end(),
        [](const TargetFlagEntry& a, const TargetFlagEntry& b) {
          return a.score < b.score;
        });
    if (score > worst->score) {
      *worst = TargetFlagEntry{args, score};
    }
  }
  has_target_flags_.store(true, std::memory_order_relaxed);
}

void Fuzzer::RecordTargetChild(bool hit) {
  if (hit) {
    target_children_hit_.fetch_add(1, std::memory_order_relaxed);
  }
  target_children_completed_.fetch_add(1, std::memory_order_release);
}

std::vector<std::string> Fuzzer::DrawD8Flags(Random& rng) {
  D8FlagProfile profile = D8FlagProfile::kMaglev;
  ParseD8FlagProfile(config_.d8_flag_profile, &profile);
  const std::vector<std::string>& fixed = config_.target.args;
  std::vector<std::string> fresh = RandomD8Flags(rng, profile, fixed);
  if (targets_.empty() || !has_target_flags_.load(std::memory_order_relaxed)) {
    return fresh;
  }
  // When most children reach target edges, `EffectiveTargetGuidedChance` lowers
  // the guided probability so that new children get fresh random flags.
  const uint64_t completed =
      target_children_completed_.load(std::memory_order_acquire);
  const uint64_t hit = target_children_hit_.load(std::memory_order_relaxed);
  const double effective_guided_chance =
      EffectiveTargetGuidedChance(completed, hit);
  if (!rng.Chance(effective_guided_chance)) {
    return fresh;
  }
  std::vector<std::string> base;
  {
    std::scoped_lock<std::mutex> lock(target_flags_mutex_);
    if (target_flags_.empty()) {
      return fresh;
    }
    size_t best = rng.Index(target_flags_.size());
    const int k = TargetTournament();
    for (int i = 1; i < k; i++) {
      size_t candidate = rng.Index(target_flags_.size());
      if (target_flags_[candidate].score > target_flags_[best].score) {
        best = candidate;
      }
    }
    base = target_flags_[best].args;
  }
  if (rng.Chance(TargetMutateChance())) {
    return CombineD8Flags(base, fresh, rng, fixed);
  }
  return base;
}

std::string Fuzzer::CoverageStatus() const {
  if (feature_weights_ == nullptr) {
    return "";
  }
  std::string out;
  if (feature_weights_ != nullptr && feature_weights_->has_focus()) {
    out += StringPrintf(" | focus %u/%u", FocusSeen(),
                        feature_weights_->focus_edges());
  }
  if (feature_weights_ != nullptr &&
      !feature_weights_->target_edges().empty()) {
    auto count = [this](const std::vector<uint32_t>& edges) {
      uint32_t found = 0;
      for (uint32_t edge : edges) {
        if (coverage_.IsSeen(edge)) {
          found++;
        }
      }
      return found;
    };
    out += StringPrintf(" | target %u/%zu (line %u/%zu)",
                        count(feature_weights_->target_edges()),
                        feature_weights_->target_edges().size(),
                        count(feature_weights_->line_target_edges()),
                        feature_weights_->line_target_edges().size());
  }
  if (geometry_.optimized_count != 0) {
    out += StringPrintf(" | opt %u",
                        coverage_.CountSeen(geometry_.optimized_first,
                                            geometry_.optimized_first +
                                                geometry_.optimized_count - 1));
  }
  if (geometry_.watched != 0) {
    out += StringPrintf(
        " | buckets %u",
        coverage_.CountSeen(geometry_.bucket_first, geometry_.features));
  }
  return out;
}

uint32_t Fuzzer::FocusSeen() const {
  uint32_t found = 0;
  for (const auto& [first, last] : feature_weights_->focus()) {
    found += coverage_.CountSeen(first, last);
  }
  return found;
}

void Fuzzer::AddCoverageStats(JsonObject* event) const {
  if (feature_weights_ == nullptr) {
    return;
  }
  if (feature_weights_->has_focus()) {
    event->Add("focus_found", FocusSeen())
        .Add("focus_total", feature_weights_->focus_edges());
    event->Add("unfocused_programs",
               unfocused_programs_.load(std::memory_order_relaxed));
  }
  if (geometry_.counters) {
    uint32_t found = 0;
    for (uint32_t edge : feature_weights_->target_edges()) {
      if (coverage_.IsSeen(edge)) {
        found++;
      }
    }
    event->Add("target_found", found)
        .Add("target_total",
             static_cast<uint64_t>(feature_weights_->target_edges().size()));
    if (!targets_.empty() && config_.randomize_d8_flags) {
      event->Add("target_children_completed", target_children_completed())
          .Add("target_children_hit", target_children_hit());
    }
  }
  if (geometry_.optimized_count != 0) {
    event->Add("opt_found",
               coverage_.CountSeen(
                   geometry_.optimized_first,
                   geometry_.optimized_first + geometry_.optimized_count - 1));
  }
  if (geometry_.counters) {
    event->Add("bucket_found",
               coverage_.CountSeen(geometry_.bucket_first, geometry_.features));
  }
}

void Fuzzer::CollectImports() {
  for (const std::string& path : config_.imports) {
    std::vector<std::string> files = ListFiles(path, ".js");
    if (files.empty()) {
      REDLINE_WARN(kLog, "--import=%s: no .js files found", path.c_str());
    }
    import_files_.insert(import_files_.end(), files.begin(), files.end());
  }
  // Spread big directories over the workers in a reproducible order that
  // does not put all the slow tests of one directory on one worker.
  Random rng(config_.seed);
  for (size_t i = import_files_.size(); i > 1; i--) {
    std::swap(import_files_[i - 1], import_files_[rng.Index(i)]);
  }
}

void Fuzzer::Fail(const std::string& message) {
  {
    std::scoped_lock<std::mutex> lock(failure_mutex_);
    if (failure_.empty()) {
      failure_ = message;
    }
  }
  failed_.store(true);
  RequestStop();
}

bool Fuzzer::ClaimAmplify(const CrashSignature& signature) {
  std::scoped_lock<std::mutex> lock(crash_mutex_);
  SignatureInfo& info = crash_signatures_[signature.hash];
  if (amplifier_ == nullptr || info.amplify >= amplifier_->per_signature()) {
    return false;
  }
  info.amplify++;
  return true;
}

Fuzzer::CrashDecision Fuzzer::RegisterCrash(uint64_t source_hash,
                                            const CrashSignature& signature) {
  std::scoped_lock<std::mutex> lock(crash_mutex_);
  CrashDecision decision;
  SignatureInfo& info = crash_signatures_[signature.hash];
  if (info.count == 0) {
    info.text = signature.text;
    info.first_seen_sec =
        static_cast<double>(WallClockNowMs() - start_ms_) / 1000.0;
    stats_.crash_signatures = crash_signatures_.size();
  }
  info.count++;
  decision.signature_count = info.count;
  decision.new_source = crash_hashes_.insert(source_hash).second;
  decision.save =
      decision.new_source && info.saved < config_.max_crashes_per_signature;
  decision.minimize =
      decision.save && info.minimized < config_.minimize_crashes_per_signature;
  // Reserve the slots now, so that concurrent crashes of the same signature
  // do not all get saved and minimized. RecordCrashOutcome gives back the
  // minimize slot of a crash that Redline did not minimize. A save slot
  // stays reserved.
  if (decision.save) {
    info.saved++;
  }
  if (decision.minimize) {
    info.minimized++;
  }
  return decision;
}

void Fuzzer::RecordCrashOutcome(const CrashSignature& signature,
                                const CrashDecision& decision, bool minimized,
                                bool reproduced, const std::string& path) {
  std::scoped_lock<std::mutex> lock(crash_mutex_);
  SignatureInfo& info = crash_signatures_[signature.hash];
  // RecordCrashOutcome gives back the minimize slot of this crash if
  // RegisterCrash reserved it and the minimization did not happen or failed.
  // Then the next crash of this signature can use it. A crash without a slot
  // gives back nothing.
  if (decision.minimize && !minimized && info.minimized > 0) {
    info.minimized--;
  }
  if (reproduced) {
    info.reproduced++;
  }
  if (decision.save && info.first_path.empty()) {
    info.first_path = path;
  }
  WriteSignatureSummary();
}

bool Fuzzer::RecordCrashForTest(uint64_t source_hash,
                                const CrashSignature& signature,
                                bool minimization_succeeds) {
  const CrashDecision decision = RegisterCrash(source_hash, signature);
  RecordCrashOutcome(signature, decision,
                     decision.minimize && minimization_succeeds,
                     /*reproduced=*/true, "");
  return decision.minimize;
}

// Called with crash_mutex_ held.
void Fuzzer::WriteSignatureSummary() {
  std::string text =
      "# signature | crashes | saved | minimized | reproduced | first seen (s) "
      "| first file\n";
  for (const auto& [hash, info] : crash_signatures_) {
    text +=
        StringPrintf("%s | %d | %d | %d | %d | %.0f | %s\n", info.text.c_str(),
                     info.count, info.saved, info.minimized, info.reproduced,
                     info.first_seen_sec, info.first_path.c_str());
  }
  std::string error;
  if (!WriteStringToFile(config_.out_dir + "/crashes/signatures.txt", text,
                         &error)) {
    REDLINE_WARN(kLog, "%s", error.c_str());
  }
}

bool Fuzzer::SaveCorpusProgram(const Program& program) {
  const Lineage& lineage = program.lineage();
  std::string header = StringPrintf(
      "// redline corpus #%u | origin %s | parent #%u +%d | generation %d | "
      "%zu new edges | "
      "%zu nodes | worker %d%s%s\n",
      program.id, lineage.origin.c_str(), lineage.parent_id, lineage.chain,
      lineage.generation, program.edges.size(), program.node_count(),
      lineage.worker, lineage.detail.empty() ? "" : " | ",
      lineage.detail.c_str());
  std::string path =
      StringPrintf("%s/corpus/%06u.js", config_.out_dir.c_str(), program.id);
  std::string error;
  if (!WriteStringToFile(path, header + program.source(), &error)) {
    REDLINE_WARN(kLog, "%s", error.c_str());
    return false;
  }
  return true;
}

void Fuzzer::LogEvent(const JsonObject& event) {
  // Every event carries the session time so logs of different runs can be
  // aligned.
  JsonObject stamped;
  stamped.Add("t", static_cast<double>(WallClockNowMs() - start_ms_) / 1000.0);
  std::string line = stamped.ToString();
  std::string body = event.ToString();
  // {"t":1.5} + {"event":...} -> {"t":1.5,"event":...}
  line.pop_back();
  line += ',';
  line += body.substr(1);
  events_.WriteLine(line);
}

bool Fuzzer::ShouldStop() const {
  if (stop_requested()) {
    return true;
  }
  if (config_.duration_sec > 0 &&
      static_cast<double>(WallClockNowMs() - start_ms_) / 1000.0 >=
          config_.duration_sec) {
    return true;
  }
  if (config_.max_execs > 0 && stats_.executions.load() >= config_.max_execs) {
    return true;
  }
  return false;
}

std::string Fuzzer::StatusLine() const {
  uint64_t elapsed_ms = WallClockNowMs() - start_ms_;
  double elapsed_s = std::max(0.001, static_cast<double>(elapsed_ms) / 1000.0);
  uint64_t execs = stats_.executions.load();
  uint64_t fuzz = stats_.fuzz_executions.load();
  uint64_t produced = fuzz + stats_.invalid.load();
  double exec_share = Percent(stats_.exec_time_us.load(),
                              static_cast<uint64_t>(elapsed_s * 1e6) *
                                  static_cast<uint64_t>(config_.jobs));
  std::string line = StringPrintf(
      "%s | execs %llu (%.0f/s) | cov %.3f%% (%u edges)%s | corpus %zu | "
      "ok %.1f%% fail %.1f%% timeout %.1f%% | invalid %.2f%% noop %llu | "
      "crashes %llu (%llu sigs, %llu repro) | cands %llu flaky %llu | min %llu "
      "tests (%.1f%% of execs, %.1f%% of "
      "time) | "
      "exec %.0f%% busy | avg exec %.1f ms | avg nodes %.0f (corpus %.0f)",
      FormatDuration(elapsed_ms / 1000).c_str(),
      static_cast<unsigned long long>(execs),
      static_cast<double>(execs) / elapsed_s,
      100.0 * coverage_.coverage_fraction(), coverage_.found_edges(),
      CoverageStatus().c_str(), corpus_->size(),
      Percent(stats_.succeeded.load(), fuzz),
      Percent(stats_.failed.load(), fuzz),
      Percent(stats_.timed_out.load(), fuzz),
      Percent(stats_.invalid.load(), produced),
      static_cast<unsigned long long>(stats_.noop.load()),
      static_cast<unsigned long long>(stats_.crashed.load()),
      static_cast<unsigned long long>(stats_.crash_signatures.load()),
      static_cast<unsigned long long>(stats_.crashes_reproduced.load()) +
          static_cast<unsigned long long>(
              stats_.crashes_state_dependent.load()),
      static_cast<unsigned long long>(stats_.candidates.load()),
      static_cast<unsigned long long>(stats_.flaky.load()),
      static_cast<unsigned long long>(stats_.minimizer_tests.load()),
      Percent(stats_.minimizer_tests.load(), execs),
      Percent(stats_.minimizer_time_us.load(),
              static_cast<uint64_t>(elapsed_s * 1e6) *
                  static_cast<uint64_t>(config_.jobs)),
      exec_share,
      execs == 0 ? 0.0
                 : static_cast<double>(stats_.exec_time_us.load()) / 1000.0 /
                       static_cast<double>(execs),
      fuzz == 0 ? 0.0
                : static_cast<double>(stats_.fuzz_nodes.load()) /
                      static_cast<double>(fuzz),
      stats_.corpus_added.load() == 0
          ? 0.0
          : static_cast<double>(stats_.corpus_nodes.load()) /
                static_cast<double>(stats_.corpus_added.load()));
  return line;
}

std::string Fuzzer::ComponentTables(bool include_generators) const {
  std::string out = FormatStatsTable("mutators", mutators_->Components());
  out += FormatStatsTable("reducers", minimizer_->Components());
  bool enabled[kNumStages];
  for (int i = 0; i < kNumStages; i++) {
    enabled[i] = config_.stages[i].stats;
  }
  out += stages_.Format(enabled);
  out += FormatStatsTable("seeds", {&corpus_->seed_stats()});
  if (include_generators) {
    out += FormatStatsTable("generators", generators_->Components());
  }
  return out;
}

void Fuzzer::WriteStatsEvent() {
  JsonObject event;
  event.Add("event", "stats");
  event.Add("executions", stats_.executions.load());
  event.Add("fuzz_executions", stats_.fuzz_executions.load());
  event.Add("succeeded", stats_.succeeded.load())
      .Add("failed", stats_.failed.load());
  event.Add("timed_out", stats_.timed_out.load())
      .Add("crashed", stats_.crashed.load());
  event.Add("invalid", stats_.invalid.load()).Add("noop", stats_.noop.load());
  event.Add("candidates", stats_.candidates.load())
      .Add("flaky", stats_.flaky.load());
  event.Add("corpus", static_cast<uint64_t>(corpus_->size()));
  event.Add("corpus_added", stats_.corpus_added.load());
  event.Add("coverage", coverage_.coverage_fraction());
  event.Add("found_edges", coverage_.found_edges())
      .Add("num_edges", coverage_.num_edges());
  event.Add("suppressed_flaky_edges", coverage_.suppressed_flaky_edges());
  AddCoverageStats(&event);
  event.Add("unique_crashes", stats_.unique_crashes.load());
  event.Add("duplicate_crashes", stats_.duplicate_crashes.load());
  event.Add("harvested_crashes", stats_.harvested_crashes.load());
  event.Add("crash_signatures", stats_.crash_signatures.load());
  event.Add("crashes_reproduced", stats_.crashes_reproduced.load());
  event.Add("crashes_state_dependent", stats_.crashes_state_dependent.load());
  event.Add("crashes_amplified", stats_.crashes_amplified.load());
  event.Add("amplify_processes", stats_.amplify_processes.load());
  event.Add("crashes_bundled", stats_.crashes_bundled.load());
  event.Add("bundle_processes", stats_.bundle_processes.load());
  event.Add("flag_processes", stats_.flag_processes.load());
  event.Add("crash_min_processes", stats_.crash_min_processes.load());
  event.Add("crash_replay_executions", stats_.crash_replay_executions.load());
  event.Add("imports_total", stats_.imports_total.load());
  event.Add("imports_added", stats_.imports_added.load());
  event.Add("imports_failed", stats_.imports_failed.load());
  event.Add("imports_redundant", stats_.imports_redundant.load());
  event.Add("minimizer_tests", stats_.minimizer_tests.load());
  event.Add("minimizer_time_us", stats_.minimizer_time_us.load());
  event.Add("minimizer_nodes_removed", stats_.minimizer_nodes_removed.load());
  event.Add("minimizer_rejected", stats_.minimizer_rejected.load());
  event.Add("rerun_executions", stats_.rerun_executions.load());
  event.Add("explore_executions", stats_.explore_executions.load());
  event.Add("explore_applied", stats_.explore_applied.load());
  event.Add("explore_timeouts", stats_.explore_timeouts.load());
  event.Add("exec_time_us", stats_.exec_time_us.load());
  event.Add("fuzz_nodes", stats_.fuzz_nodes.load());
  event.Add("corpus_nodes", stats_.corpus_nodes.load());
  event.Add("mutation_time_us", stats_.mutation_time_us.load());
  event.Add("infrastructure_failures", stats_.infrastructure_failures.load());
  event.AddRaw("stages", stages_.ToJson());
  LogEvent(event);
}

bool Fuzzer::WriteCapture(Stage stage, CaptureTrigger trigger,
                          std::string_view mutator, const Program* parent,
                          const std::string& child, const JsonObject& details,
                          int worker) {
  const CaptureRule& rule = config_.stage(stage).capture;
  uint64_t index = captures_[static_cast<int>(stage)].fetch_add(
      1, std::memory_order_relaxed);
  if (index >= static_cast<uint64_t>(rule.max)) {
    return false;
  }
  std::string dir =
      config_.out_dir + "/captures/" + std::string(StageName(stage));
  std::string error;
  if (!MakeDirectories(dir, &error)) {
    REDLINE_WARN(kLog, "capture: %s", error.c_str());
    return false;
  }
  std::string base = StringPrintf(
      "%s/%04llu-%.*s%s%.*s", dir.c_str(),
      static_cast<unsigned long long>(index),
      static_cast<int>(CaptureTriggerName(trigger).size()),
      CaptureTriggerName(trigger).data(), mutator.empty() ? "" : "-",
      static_cast<int>(mutator.size()), mutator.data());
  if (parent != nullptr) {
    WriteStringToFile(base + ".parent.js", parent->source(), &error);
  }
  if (!child.empty()) {
    WriteStringToFile(base + ".child.js", child, &error);
  }
  JsonObject sidecar;
  sidecar.Add("stage", StageName(stage))
      .Add("trigger", CaptureTriggerName(trigger));
  sidecar.Add("mutator", mutator).Add("worker", worker);
  sidecar.Add("elapsed_sec",
              static_cast<double>(WallClockNowMs() - start_ms_) / 1000.0);
  if (parent != nullptr) {
    sidecar.Add("parent_id", parent->id)
        .Add("parent_origin", parent->lineage().origin);
    sidecar.Add("parent_detail", parent->lineage().detail);
  }
  sidecar.AddRaw("details", details.ToString());
  WriteStringToFile(base + ".json", sidecar.ToString() + "\n", &error);
  if (index + 1 == static_cast<uint64_t>(rule.max)) {
    REDLINE_LOG(
        kLog, "capture: %s reached its cap of %d, no more files for this stage",
        dir.c_str(), rule.max);
  }
  return true;
}

MinimizationResult Fuzzer::Minimize(fast::Tree* tree,
                                    const MinimizationOracle& oracle,
                                    SyntaxChecker* checker,
                                    const MinimizerOptions& options) const {
  if (minimizer_reducers_.empty()) {
    return minimizer_->Minimize(tree, oracle, checker, options);
  }
  return minimizer_->MinimizeWith(minimizer_reducers_, tree, oracle, checker,
                                  options);
}

MinimizationResult Fuzzer::MinimizeCrash(
    fast::Tree* tree, const MinimizationOracle& oracle, SyntaxChecker* checker,
    const MinimizerOptions& options) const {
  return minimizer_->MinimizeWith(crash_reducers_, tree, oracle, checker,
                                  options);
}

int Fuzzer::Run() {
  start_ms_ = WallClockNowMs();
  {
    JsonObject event;
    event.Add("event", "start").AddRaw("config", config_.ToJson());
    // The strategies as built, with every parameter filled in.
    JsonObject selectors;
    selectors.AddRaw("seeds", EffectiveSelector(corpus_->seeds()));
    selectors.AddRaw("partners", EffectiveSelector(corpus_->partners()));
    selectors.AddRaw("mutators", EffectiveSelector(mutators_->selector()));
    selectors.AddRaw("generators", EffectiveSelector(generators_->selector()));
    event.AddRaw("selectors", selectors.ToString());
    event.Add("imports", static_cast<uint64_t>(import_files_.size()));
    event.Add("num_edges", coverage_.num_edges());
    event.Add("target_kind",
              TargetKindName(workers_[0]->coverage().target_kind()));
    event.Add("coverage_mode",
              workers_[0]->coverage().is_fuzzilli()
                  ? "fuzzilli"
                  : (geometry_.counters ? "counters" : "bits"));
    if (geometry_.counters) {
      event.Add("watched_edges", geometry_.watched)
          .Add("optimized_features", geometry_.optimized_count);
      event.Add(
          "target_edges",
          static_cast<uint64_t>(feature_weights_ != nullptr
                                    ? feature_weights_->target_edges().size()
                                    : 0));
      event.Add("focus_edges", feature_weights_ != nullptr
                                   ? feature_weights_->focus_edges()
                                   : 0u);
    } else if (geometry_.optimized_count != 0) {
      event.Add("optimized_features", geometry_.optimized_count);
    }
    LogEvent(event);
  }
  REDLINE_LOG(
      kLog,
      "session: %d worker(s), seed %llu, %u edges in target, %zu file(s) to "
      "import, "
      "seeds %.*s, partners %.*s, mutators %.*s, generators %.*s, output %s",
      config_.jobs, static_cast<unsigned long long>(config_.seed),
      coverage_.num_edges(), import_files_.size(),
      static_cast<int>(corpus_->seeds().name().size()),
      corpus_->seeds().name().data(),
      static_cast<int>(corpus_->partners().name().size()),
      corpus_->partners().name().data(),
      static_cast<int>(mutators_->selector().name().size()),
      mutators_->selector().name().data(),
      static_cast<int>(generators_->selector().name().size()),
      generators_->selector().name().data(), config_.out_dir.c_str());
  for (auto& worker : workers_) {
    worker->Launch();
  }

  uint64_t last_stats_ms = start_ms_;
  size_t last_corpus = 0;
  int intervals = 0;
  bool import_reported = import_files_.empty();
  while (!ShouldStop()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    uint64_t now = WallClockNowMs();
    if (!import_reported && workers_importing_.load() == 0 &&
        next_import_.load() >= import_files_.size()) {
      import_reported = true;
      REDLINE_LOG(
          kLog,
          "import done: %llu files, %llu added, %llu redundant, %llu failed; "
          "coverage %.3f%%, corpus %zu",
          static_cast<unsigned long long>(stats_.imports_total.load()),
          static_cast<unsigned long long>(stats_.imports_added.load()),
          static_cast<unsigned long long>(stats_.imports_redundant.load()),
          static_cast<unsigned long long>(stats_.imports_failed.load()),
          100.0 * coverage_.coverage_fraction(), corpus_->size());
      JsonObject event;
      event.Add("event", "import_done")
          .Add("total", stats_.imports_total.load());
      event.Add("added", stats_.imports_added.load());
      event.Add("redundant", stats_.imports_redundant.load());
      event.Add("failed", stats_.imports_failed.load());
      event.Add("coverage", coverage_.coverage_fraction());
      LogEvent(event);
    }
    if (config_.stats_interval_sec > 0 &&
        static_cast<double>(now - last_stats_ms) / 1000.0 >=
            config_.stats_interval_sec) {
      last_stats_ms = now;
      intervals++;
      WriteStatsEvent();
      if (!config_.quiet) {
        size_t corpus_size = corpus_->size();
        REDLINE_LOG(kLog, "%s (+%zu)", StatusLine().c_str(),
                    corpus_size - last_corpus);
        last_corpus = corpus_size;
        if (config_.component_stats) {
          fputs(ComponentTables(intervals % 10 == 0).c_str(), stderr);
        }
      }
    }
  }
  stop_.store(true);
  for (auto& worker : workers_) {
    worker->Join();
  }

  WriteStatsEvent();
  {
    JsonObject event;
    event.Add("event", "end")
        .Add("reason", failed_.load() ? "failure" : "stop");
    event.Add("elapsed_sec",
              static_cast<double>(WallClockNowMs() - start_ms_) / 1000.0);
    LogEvent(event);
  }
  events_.Flush();
  REDLINE_LOG(kLog, "final: %s", StatusLine().c_str());
  if (config_.component_stats) {
    fputs(ComponentTables(true).c_str(), stderr);
  }
  if (failed_.load()) {
    REDLINE_ERROR(kLog, "%s", failure_.c_str());
    return 1;
  }
  return 0;
}

}  // namespace redline
