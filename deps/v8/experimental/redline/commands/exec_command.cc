// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "redline exec": runs one or more JavaScript files through the target and
// reports outcome, timing and coverage for each. The simplest way to check
// what the fuzzer sees for a given program.

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <set>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/json.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/exec/runner.h"

namespace redline {

namespace {

constexpr const char* kLog = "exec";

void PrintOutput(const char* label, const std::string& text) {
  if (text.empty()) {
    return;
  }
  printf("---- %s (%zu bytes) ----\n%s%s", label, text.size(), text.c_str(),
         text.back() == '\n' ? "" : "\n");
}

int RunExec(const Flags& flags) {
  TargetOptions target;
  std::string error;
  if (!TargetOptions::FromFlags(flags, &target, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitUsage;
  }
  int repeat = static_cast<int>(flags.GetInt("repeat", 1));
  bool print_output = flags.GetBool("print-output", false);
  bool print_edges = flags.GetBool("print-edges", false);
  std::string events_path = flags.GetString("events");

  if (flags.positional().empty()) {
    REDLINE_ERROR(kLog, "No JavaScript files given");
    return kExitUsage;
  }

  JsonlWriter events;
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
  REDLINE_LOG(kLog, "%u edges instrumented", runner->coverage().num_edges());

  int exit_code = kExitOk;
  for (const std::string& path : flags.positional()) {
    std::string script;
    if (!ReadFileToString(path, &script, &error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      exit_code = kExitFailure;
      continue;
    }
    // For --repeat we track which edges were hit in every run vs. in any
    // run, which is what the fuzzer's determinism check looks at.
    std::vector<uint32_t> edges_in_all_runs;
    std::set<uint32_t> edges_in_any_run;
    for (int run = 0; run < repeat; run++) {
      ScriptRunner::Result result;
      if (!runner->Run(script, &result, &error)) {
        REDLINE_ERROR(kLog, "%s: execution failed: %s", path.c_str(),
                      error.c_str());
        exit_code = kExitFailure;
        break;
      }
      const Execution& execution = result.execution;
      std::vector<uint32_t> current = runner->coverage().CurrentEdges();
      if (run == 0) {
        edges_in_all_runs = current;
      } else {
        std::vector<uint32_t> intersection;
        std::set_intersection(edges_in_all_runs.begin(),
                              edges_in_all_runs.end(), current.begin(),
                              current.end(), std::back_inserter(intersection));
        edges_in_all_runs.swap(intersection);
      }
      edges_in_any_run.insert(current.begin(), current.end());

      printf("%s%s: %s", path.c_str(),
             repeat > 1 ? StringPrintf(" [run %d]", run + 1).c_str() : "",
             OutcomeName(execution.outcome));
      if (execution.outcome == Outcome::kFailed) {
        printf(" (exit code %d)", execution.exit_code);
      } else if (execution.outcome == Outcome::kCrashed) {
        printf(" (signal %d)", execution.signal);
      }
      printf(" in %.1f ms, %u edges hit, %zu new, %u found total (%.3f%%)\n",
             static_cast<double>(execution.exec_time_us) / 1000.0,
             result.edges_hit, result.new_edges.size(),
             runner->coverage().found_edges(),
             runner->coverage().coverage_fraction() * 100.0);
      if (print_output) {
        PrintOutput("stdout", execution.stdout_output);
        PrintOutput("stderr", execution.stderr_output);
        PrintOutput("fuzzout", execution.fuzzout);
      }
      if (print_edges && !result.new_edges.empty()) {
        printf("new edges:");
        for (uint32_t edge : result.new_edges) {
          printf(" %u", edge);
        }
        printf("\n");
      }
      if (events.is_open()) {
        JsonObject event;
        event.Add("event", "exec")
            .Add("file", path)
            .Add("run", run)
            .Add("outcome", OutcomeName(execution.outcome))
            .Add("exit_code", execution.exit_code)
            .Add("signal", execution.signal)
            .Add("exec_us", execution.exec_time_us)
            .Add("edges_hit", result.edges_hit)
            .Add("new_edges", static_cast<uint64_t>(result.new_edges.size()))
            .Add("found_edges", runner->coverage().found_edges());
        if (print_edges) {
          event.AddArray("new_edge_list", result.new_edges);
        }
        events.Write(event);
      }
    }
    if (repeat > 1) {
      printf(
          "%s: %zu edges hit in every run, %zu in at least one run "
          "(%zu non-deterministic)\n",
          path.c_str(), edges_in_all_runs.size(), edges_in_any_run.size(),
          edges_in_any_run.size() - edges_in_all_runs.size());
    }
  }

  const ScriptRunner::Stats& stats = runner->stats();
  REDLINE_LOG(kLog,
              "%llu executions: %llu succeeded, %llu failed, %llu crashed, "
              "%llu timed out; %llu spawns",
              static_cast<unsigned long long>(stats.executions),
              static_cast<unsigned long long>(stats.succeeded),
              static_cast<unsigned long long>(stats.failed),
              static_cast<unsigned long long>(stats.crashed),
              static_cast<unsigned long long>(stats.timed_out),
              static_cast<unsigned long long>(runner->child().total_spawns()));
  return exit_code;
}

}  // namespace

const Command kExecCommand = {
    "exec",
    "Run JavaScript files through the target and report coverage",
    "usage: redline exec --d8=PATH [options] FILE.js...\n"
    "\n"
    "Runs each file (in order, in the same target process, like the fuzzer\n"
    "would) and prints outcome, execution time and the number of new edges.\n"
    "\n"
    "options:\n"
    "  --repeat=N         run each file N times and report edge stability\n"
    "  --print-output     dump the target's stdout, stderr and fuzzout\n"
    "  --print-edges      list the indices of newly found edges\n"
    "  --events=FILE      write one JSON object per execution to FILE\n",
    RunExec,
};

}  // namespace redline
