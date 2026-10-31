// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/fuzzer.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <deque>
#include <functional>
#include <iterator>
#include <thread>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/base/v8_runtime.h"
#include "experimental/redline/core/environment.h"
#include "experimental/redline/engine/importer.h"
#include "experimental/redline/exec/runner.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/mutators/explore.h"

namespace redline {

namespace {

constexpr char kLog[] = "fuzz";
constexpr size_t kMaxSavedOutput = static_cast<size_t>(64) * 1024;

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

}  // namespace

// ---------------------------------------------------------------------------
// Worker: one thread, one target process.

class Worker {
 public:
  Worker(Fuzzer* fuzzer, int id)
      : fuzzer_(fuzzer),
        id_(id),
        rng_(fuzzer->config().seed * 0x9E3779B97F4A7C15ull +
             static_cast<uint64_t>(id) + 1) {}

  int id() const { return id_; }

  // Starts the target synchronously so that configuration problems are
  // reported before any thread runs.
  bool Start(std::string* error) {
    runner_ = ScriptRunner::Create(fuzzer_->config().target, id_, error);
    if (runner_ == nullptr) {
      return false;
    }
    if (!runner_->Initialize(error)) {
      return false;
    }
    const CoverageMap& coverage = runner_->coverage();
    return fuzzer_->coverage_.Initialize(coverage.num_edges(),
                                         coverage.bitmap_size(), error);
  }

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
    // Let the target go now; the main thread only joins.
    runner_.reset();
  }

  // --- execution --------------------------------------------------------

  // Runs `source` behind the prelude without touching per-runner coverage
  // bookkeeping. Returns false on an infrastructure failure.
  bool Execute(const std::string& source, Execution* execution,
               uint64_t timeout_us = 0) {
    if (timeout_us == 0) {
      timeout_us = config().target.timeout_us;
    }
    const std::string& prelude = config().prelude;
    std::string error;
    bool ok;
    if (prelude.empty()) {
      ok = runner_->RunWithoutEvaluation(source, timeout_us, execution, &error);
    } else {
      std::string script;
      script.reserve(prelude.size() + 1 + source.size());
      script.append(prelude).append("\n").append(source);
      ok = runner_->RunWithoutEvaluation(script, timeout_us, execution, &error);
    }
    if (!ok) {
      stats().infrastructure_failures++;
      history_.clear();
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
    stats().executions++;
    stats().exec_time_us += execution->exec_time_us;
    RememberExecution(source);
    return true;
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
      history_.clear();
      history_bytes_ = 0;
    }
    history_.push_back(source);
    history_bytes_ += source.size();
    while (history_.size() > static_cast<size_t>(config().crash_history) ||
           (history_bytes_ > kHistoryBytes && history_.size() > 1)) {
      history_bytes_ -= history_.front().size();
      history_.pop_front();
    }
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
    // child, and the minimizer drops whatever ends up unused.
    if (!from_empty && config().value_prefix &&
        fuzzer_->prefix_mutator_ != nullptr) {
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

    for (int round = 0; round < config().consecutive_mutations && !stopping();
         round++) {
      Mutator* mutator = from_empty ? fuzzer_->codegen_mutator_
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
              for (Generator* g : used_generators) {
                g->stats().interesting++;
                g->stats().new_edges += added->edges.size();
              }
            }
          }
          LogMutant(*mutator, detail, ancestor->id,
                    OutcomeName(execution.outcome), &execution, new_edges);
          if (ancestor->id != 0) {
            corpus.OnResult(*ancestor, added != nullptr);
          }
          // The chain continues from the child as executed, not from
          // its minimized form (Fuzzilli's MutationEngine does the
          // same: minimization happens on the side); the lineage of
          // what follows refers to the published program.
          parent = child;
          if (added != nullptr) {
            ancestor = added;
            chain = 0;
          } else {
            chain++;
          }
          from_empty = false;
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
        return execution.succeeded() && runner_->coverage().AllEdgesSet(edges);
      };
      const MinimizerOptions budget =
          config().minimizer.ForProgram(exec_time_us);
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
          if (!runner_->coverage().AllEdgesSet(edges)) {
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
    bool minimized = false;
    MinimizationResult minimization;
    bool amplify_ran = false;  // the amplify step ran (5.8)
    AmplifyResult amplify;
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

  void ProcessCrash(const std::string& source, const Program& program,
                    const Execution& execution, const Lineage& lineage) {
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

    // 2b. If it does not reproduce alone: the amplify step searches for a
    //     plain d8 command line (extra flags, --stress-runs) that
    //     reproduces the program alone, reliably (DESIGN.md 5.8). This
    //     also runs for state-dependent crashes: an earlier program may
    //     have switched on a feature (for example the CPU profiler) that
    //     a flag can switch on as well. Once per signature.
    if (!info.reproducible() && decision.save && !stopping() &&
        fuzzer_->ClaimAmplify(info.signature)) {
      AmplifyRequest request;
      request.target = config().target;
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
    //    get their own, larger budget.
    std::string final_source = source;
    if (info.reproducible() && config().minimize_crashes && decision.minimize &&
        !stopping()) {
      std::unique_ptr<fast::Tree> work = program.CopyTree();
      const uint64_t probe_timeout_us = ProbeTimeout(exec_time_us);
      const CrashSignature& wanted = info.signature;
      MinimizationOracle oracle = [&](const std::string& candidate) {
        if (stopping()) {
          return false;
        }
        Execution e;
        if (!Execute(candidate, &e, probe_timeout_us) || !e.crashed()) {
          return false;
        }
        return CrashSignature::FromExecution(e) == wanted;
      };
      const MinimizerOptions budget =
          config().crash_minimizer.ForProgram(exec_time_us);
      info.minimization =
          fuzzer_->Minimize(work.get(), oracle, probe_checker(), budget);
      stats().minimizer_tests += static_cast<uint64_t>(info.minimization.tests);
      stats().minimizer_time_us += info.minimization.time_us;
      if (info.minimization.accepted > 0) {
        std::string candidate = fast::Print(work->root());
        for (int attempt = 0; attempt < config().verify_attempts && !stopping();
             attempt++) {
          Execution e;
          if (!Execute(candidate, &e)) {
            break;
          }
          stats().rerun_executions++;
          if (!e.crashed() || CrashSignature::FromExecution(e) != wanted) {
            continue;
          }
          final_source = candidate;
          info.minimized = true;
          break;
        }
        if (!info.minimized) {
          stats().minimizer_rejected++;
        }
      }
    }

    // 4. Store.
    std::string path;
    if (decision.save) {
      path = SaveCrash(source, final_source, execution, lineage, info, unique,
                       hash, history);
    }
    fuzzer_->RecordCrashOutcome(info.signature, decision.save, info.minimized,
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
    event.Add("amplify", info.amplify_ran
                             ? (info.amplify.amplified ? "yes" : "no")
                             : "skipped");
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
    std::string base =
        StringPrintf("%s/crashes/crash_sig%d_%016llx", config().out_dir.c_str(),
                     execution.signal, static_cast<unsigned long long>(hash));
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
        lineage.generation, config().target.CommandLine().c_str());
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
        "minimized: %s (%d tests, %zu -> %zu nodes)\n%s\n--- stderr "
        "---\n%s\n--- stdout ---\n%s\n"
        "--- fuzzout ---\n%s\n",
        execution.signal, execution.exit_code, info.signature.text.c_str(),
        unique ? "yes" : "no", info.reproducible() ? "yes" : "no", info.reruns,
        info.reproduced, info.same_signature, info.child_age,
        info.replayed ? "yes" : "no", info.state_dependent ? "yes" : "no",
        info.history_needed, info.history_available,
        info.minimized ? "yes" : "no", info.minimization.tests,
        info.minimization.nodes_before, info.minimization.nodes_after,
        AmplifyReportLines(info, amplified_program).c_str(),
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

  Fuzzer* fuzzer_;
  int id_;
  Random rng_;
  std::unique_ptr<ScriptRunner> runner_;
  std::unique_ptr<SyntaxChecker> checker_;
  std::thread thread_;
  int consecutive_failures_ = 0;
  uint64_t executions_ = 0;  // target executions of this worker
  std::deque<std::string> history_;
  size_t history_bytes_ = 0;
};

// ---------------------------------------------------------------------------
// Fuzzer

Fuzzer::Fuzzer(FuzzConfig config) : config_(std::move(config)) {}

Fuzzer::~Fuzzer() {
  for (auto& worker : workers_) {
    worker->Join();
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

  for (int i = 0; i < config_.jobs; i++) {
    auto worker = std::make_unique<Worker>(this, i);
    if (!worker->Start(error)) {
      *error = StringPrintf("worker %d: %s", i, error->c_str());
      return false;
    }
    workers_.push_back(std::move(worker));
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
  // Reserve the slots now so that concurrent crashes of the same signature
  // do not all get saved/minimized; RecordCrashOutcome corrects the counts.
  if (decision.save) {
    info.saved++;
  }
  if (decision.minimize) {
    info.minimized++;
  }
  return decision;
}

void Fuzzer::RecordCrashOutcome(const CrashSignature& signature, bool saved,
                                bool minimized, bool reproduced,
                                const std::string& path) {
  std::scoped_lock<std::mutex> lock(crash_mutex_);
  SignatureInfo& info = crash_signatures_[signature.hash];
  // RegisterCrash reserved a save slot and a minimize slot; give back the
  // minimize slot if the minimization did not happen or failed, so the
  // next crash of this signature gets a chance.
  if (saved && !minimized && info.minimized > 0) {
    info.minimized--;
  }
  if (reproduced) {
    info.reproduced++;
  }
  if (saved && info.first_path.empty()) {
    info.first_path = path;
  }
  WriteSignatureSummary();
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
  return StringPrintf(
      "%s | execs %llu (%.0f/s) | cov %.3f%% (%u edges) | corpus %zu | "
      "ok %.1f%% fail %.1f%% timeout %.1f%% | invalid %.2f%% noop %llu | "
      "crashes %llu (%llu sigs, %llu repro) | cands %llu flaky %llu | min %llu "
      "tests (%.1f%% of execs, %.1f%% of "
      "time) | "
      "exec %.0f%% busy | avg exec %.1f ms | avg nodes %.0f (corpus %.0f)",
      FormatDuration(elapsed_ms / 1000).c_str(),
      static_cast<unsigned long long>(execs),
      static_cast<double>(execs) / elapsed_s,
      100.0 * coverage_.coverage_fraction(), coverage_.found_edges(),
      corpus_->size(), Percent(stats_.succeeded.load(), fuzz),
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
  event.Add("unique_crashes", stats_.unique_crashes.load());
  event.Add("duplicate_crashes", stats_.duplicate_crashes.load());
  event.Add("crash_signatures", stats_.crash_signatures.load());
  event.Add("crashes_reproduced", stats_.crashes_reproduced.load());
  event.Add("crashes_state_dependent", stats_.crashes_state_dependent.load());
  event.Add("crashes_amplified", stats_.crashes_amplified.load());
  event.Add("amplify_processes", stats_.amplify_processes.load());
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
