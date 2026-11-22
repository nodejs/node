// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "redline replay": executes a directory of JavaScript files through the
// target exactly like the fuzzer would import a corpus, and reports the
// coverage reached. This is the Phase 0 acceptance tool: for the same
// corpus and the same d8 the edge counts must match Fuzzilli's.

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <random>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/json.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/exec/runner.h"

namespace redline {

namespace {

constexpr const char* kLog = "replay";

struct ReplayTotals {
  uint64_t files = 0;
  uint64_t unreadable = 0;
  uint64_t succeeded = 0;
  uint64_t failed = 0;
  uint64_t crashed = 0;
  uint64_t timed_out = 0;
  uint64_t with_new_edges = 0;
  uint64_t new_edges = 0;
  uint64_t nondeterministic_edges = 0;
  uint64_t determinism_runs = 0;
};

// Re-runs `script` `attempts` times and drops the edges of `new_edges` that
// are not reached by every run from the coverage state. Returns the number
// of edges dropped.
uint64_t CheckDeterminism(ScriptRunner* runner, const std::string& script,
                          std::vector<uint32_t>* new_edges, int attempts,
                          std::string* error) {
  std::vector<uint32_t> flaky;
  for (int i = 0; i < attempts && !new_edges->empty(); i++) {
    Execution execution;
    if (!runner->RunWithoutEvaluation(script, runner->options().timeout_us,
                                      &execution, error)) {
      break;
    }
    std::vector<uint32_t> missing;
    if (!execution.succeeded()) {
      // A program that does not reliably succeed does not reliably cover
      // anything either.
      missing = *new_edges;
    } else {
      missing = runner->coverage().MissingEdges(*new_edges);
    }
    if (missing.empty()) {
      continue;
    }
    std::vector<uint32_t> kept;
    std::set_difference(new_edges->begin(), new_edges->end(), missing.begin(),
                        missing.end(), std::back_inserter(kept));
    new_edges->swap(kept);
    flaky.insert(flaky.end(), missing.begin(), missing.end());
  }
  if (!flaky.empty()) {
    runner->coverage().ForgetEdges(flaky);
  }
  return flaky.size();
}

int RunReplay(const Flags& flags) {
  TargetOptions target;
  std::string error;
  if (!TargetOptions::FromFlags(flags, &target, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitUsage;
  }
  std::vector<std::string> inputs = flags.GetList("corpus");
  for (const std::string& p : flags.positional()) {
    inputs.push_back(p);
  }
  if (inputs.empty()) {
    REDLINE_ERROR(kLog, "No corpus given: pass --corpus=DIR or directories");
    return kExitUsage;
  }
  std::string suffix = flags.GetString("suffix", ".js");
  int64_t max_files = flags.GetInt("max-files", 0);
  bool shuffle = flags.GetBool("shuffle", false);
  uint64_t seed = static_cast<uint64_t>(flags.GetInt("seed", 1));
  int determinism = static_cast<int>(flags.GetInt("determinism", 0));
  std::string events_path = flags.GetString("events");
  std::string dump_edges_path = flags.GetString("dump-edges");
  int64_t progress_every = flags.GetInt("progress-every", 500);

  std::vector<std::string> files;
  for (const std::string& input : inputs) {
    std::vector<std::string> listed = ListFiles(input, suffix);
    if (listed.empty()) {
      REDLINE_WARN(kLog, "No *%s files found in %s", suffix.c_str(),
                   input.c_str());
    }
    files.insert(files.end(), listed.begin(), listed.end());
  }
  if (shuffle) {
    std::mt19937_64 rng(seed);
    std::shuffle(files.begin(), files.end(), rng);
  }
  if (max_files > 0 && static_cast<int64_t>(files.size()) > max_files) {
    files.resize(static_cast<size_t>(max_files));
  }
  if (files.empty()) {
    REDLINE_ERROR(kLog, "Nothing to replay");
    return kExitFailure;
  }

  JsonlWriter events;
  std::vector<uint32_t> all_covered_edges;
  if (!events_path.empty() && !events.Open(events_path, /*truncate=*/true)) {
    return kExitFailure;
  }

  std::unique_ptr<ScriptRunner> runner =
      ScriptRunner::Create(target, 0, &error);
  if (!runner || !runner->Initialize(&error)) {
    REDLINE_ERROR(kLog, "Cannot start target: %s", error.c_str());
    return kExitFailure;
  }
  REDLINE_LOG(kLog, "Target: %s", target.CommandLine().c_str());
  REDLINE_LOG(kLog, "%u edges instrumented; replaying %zu files",
              runner->coverage().num_edges(), files.size());
  if (events.is_open()) {
    JsonObject header;
    header.Add("event", "start")
        .Add("command", "replay")
        .Add("target", target.CommandLine())
        .Add("num_edges", runner->coverage().num_edges())
        .Add("files", static_cast<uint64_t>(files.size()))
        .Add("timeout_us", target.timeout_us)
        .Add("determinism", determinism)
        .Add("flags", flags.ToString())
        .Add("wall_ms", WallClockNowMs());
    events.Write(header);
  }

  Stopwatch stopwatch;
  ReplayTotals totals;
  for (size_t i = 0; i < files.size(); i++) {
    const std::string& path = files[i];
    totals.files++;
    std::string script;
    if (!ReadFileToString(path, &script, &error)) {
      REDLINE_WARN(kLog, "%s", error.c_str());
      totals.unreadable++;
      continue;
    }
    ScriptRunner::Result result;
    if (!runner->Run(script, &result, &error)) {
      REDLINE_ERROR(kLog, "%s: %s", path.c_str(), error.c_str());
      return kExitFailure;
    }
    const Execution& execution = result.execution;
    switch (execution.outcome) {
      case Outcome::kSucceeded:
        totals.succeeded++;
        break;
      case Outcome::kFailed:
        totals.failed++;
        break;
      case Outcome::kCrashed:
        totals.crashed++;
        REDLINE_WARN(kLog, "%s crashed the target (signal %d)", path.c_str(),
                     execution.signal);
        break;
      case Outcome::kTimedOut:
        totals.timed_out++;
        break;
    }
    uint64_t flaky = 0;
    if (!result.new_edges.empty() && determinism > 0) {
      flaky = CheckDeterminism(runner.get(), script, &result.new_edges,
                               determinism, &error);
      totals.determinism_runs += static_cast<uint64_t>(determinism);
      totals.nondeterministic_edges += flaky;
    }

    if (!result.new_edges.empty()) {
      all_covered_edges.insert(all_covered_edges.end(),
                               result.new_edges.begin(),
                               result.new_edges.end());
      totals.with_new_edges++;
      totals.new_edges += result.new_edges.size();
    }
    if (events.is_open()) {
      JsonObject event;
      event.Add("event", "file")
          .Add("index", static_cast<uint64_t>(i))
          .Add("file", path)
          .Add("outcome", OutcomeName(execution.outcome))
          .Add("exit_code", execution.exit_code)
          .Add("signal", execution.signal)
          .Add("exec_us", execution.exec_time_us)
          .Add("edges_hit", result.edges_hit)
          .Add("new_edges", static_cast<uint64_t>(result.new_edges.size()))
          .Add("flaky_edges", flaky)
          .Add("found_edges", runner->coverage().found_edges());
      events.Write(event);
    }
    if (progress_every > 0 &&
        (i + 1) % static_cast<size_t>(progress_every) == 0) {
      REDLINE_LOG(kLog, "%zu/%zu files, %u edges (%.3f%%), %.1f execs/s", i + 1,
                  files.size(), runner->coverage().found_edges(),
                  runner->coverage().coverage_fraction() * 100.0,
                  static_cast<double>(runner->stats().executions) /
                      std::max(stopwatch.ElapsedSeconds(), 1e-9));
    }
  }

  double wall_s = stopwatch.ElapsedSeconds();
  const ScriptRunner::Stats& stats = runner->stats();
  const CoverageMap& coverage = runner->coverage();
  printf(
      "replay: %llu files (%llu unreadable): %llu succeeded, %llu failed, "
      "%llu crashed, %llu timed out\n"
      "replay: %llu files found new edges; %llu edges found of %u "
      "(%.4f%%)%s\n"
      "replay: %llu executions in %.1f s (%.1f execs/s, %.1f ms avg target "
      "time), %llu target spawns, %llu infrastructure failures\n",
      static_cast<unsigned long long>(totals.files),
      static_cast<unsigned long long>(totals.unreadable),
      static_cast<unsigned long long>(totals.succeeded),
      static_cast<unsigned long long>(totals.failed),
      static_cast<unsigned long long>(totals.crashed),
      static_cast<unsigned long long>(totals.timed_out),
      static_cast<unsigned long long>(totals.with_new_edges),
      static_cast<unsigned long long>(coverage.found_edges()),
      coverage.num_edges(), coverage.coverage_fraction() * 100.0,
      determinism > 0
          ? StringPrintf(
                " after dropping %llu non-deterministic edges",
                static_cast<unsigned long long>(totals.nondeterministic_edges))
                .c_str()
          : "",
      static_cast<unsigned long long>(stats.executions), wall_s,
      static_cast<double>(stats.executions) / std::max(wall_s, 1e-9),
      stats.executions == 0
          ? 0.0
          : static_cast<double>(stats.total_exec_time_us) /
                static_cast<double>(stats.executions) / 1000.0,
      static_cast<unsigned long long>(runner->child().total_spawns()),
      static_cast<unsigned long long>(stats.infrastructure_failures));

  if (events.is_open()) {
    JsonObject summary;
    summary.Add("event", "summary")
        .Add("files", totals.files)
        .Add("unreadable", totals.unreadable)
        .Add("succeeded", totals.succeeded)
        .Add("failed", totals.failed)
        .Add("crashed", totals.crashed)
        .Add("timed_out", totals.timed_out)
        .Add("files_with_new_edges", totals.with_new_edges)
        .Add("found_edges", coverage.found_edges())
        .Add("num_edges", coverage.num_edges())
        .Add("coverage", coverage.coverage_fraction())
        .Add("nondeterministic_edges", totals.nondeterministic_edges)
        .Add("executions", stats.executions)
        .Add("spawns", runner->child().total_spawns())
        .Add("infrastructure_failures", stats.infrastructure_failures)
        .Add("total_exec_us", stats.total_exec_time_us)
        .Add("wall_s", wall_s);
    events.Write(summary);
    events.Flush();
  }

  if (!dump_edges_path.empty()) {
    std::sort(all_covered_edges.begin(), all_covered_edges.end());
    FILE* f = fopen(dump_edges_path.c_str(), "w");
    if (f) {
      for (uint32_t e : all_covered_edges) {
        fprintf(f, "%u\n", e);
      }
      fclose(f);
    }
  }
  return kExitOk;
}

}  // namespace

const Command kReplayCommand = {
    "replay",
    "Execute a corpus directory and report the coverage it reaches",
    "usage: redline replay --d8=PATH --corpus=DIR[,DIR...] [options]\n"
    "\n"
    "Runs every *.js file below the given directories (sorted by path)\n"
    "through one target process, tracking new edges exactly like the\n"
    "fuzzer does when importing a corpus, and prints a summary.\n"
    "\n"
    "options:\n"
    "  --suffix=.js         file name suffix to replay\n"
    "  --max-files=N        stop after N files\n"
    "  --shuffle --seed=N   randomize the file order\n"
    "  --determinism=N      re-run programs that found new edges N times and\n"
    "                       drop edges that are not reached every time\n"
    "  --events=FILE        write one JSON object per file to FILE\n"
    "  --dump-edges=FILE    write the union of all covered edge indices to "
    "FILE\n"
    "  --progress-every=N   log progress every N files (default 500)\n",
    RunReplay,
};

}  // namespace redline
