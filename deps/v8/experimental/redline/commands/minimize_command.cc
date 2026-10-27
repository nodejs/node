// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// redline minimize: run the minimizer (or single reducers) on a program
// against a chosen oracle, outside of a fuzzing session.

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/base/v8_runtime.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/core/component.h"
#include "experimental/redline/core/program.h"
#include "experimental/redline/engine/prelude.h"
#include "experimental/redline/exec/runner.h"
#include "experimental/redline/exec/target_options.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/minimizer/minimizer.h"

namespace redline {

namespace {

constexpr char kLog[] = "minimize";

const char kUsage[] =
    "usage: redline minimize --d8=PATH [options] FILE.js\n"
    "\n"
    "Minimizes FILE.js while an oracle keeps holding, and prints the result\n"
    "(or writes it with --out=FILE). Oracles:\n"
    "  --oracle=edges      (default) the edges the program reaches on its\n"
    "                      first run and that survive --reruns re-executions\n"
    "                      must still be reached, by a successful execution.\n"
    "                      With --baseline=FILE,... only the edges that none\n"
    "                      of the baseline programs reach count (this is what\n"
    "                      the fuzzer preserves: the edges new w.r.t. the\n"
    "                      parent)\n"
    "  --oracle=crash      the program must still crash\n"
    "  --oracle=fail       the program must still fail (uncaught exception)\n"
    "  --oracle=output     stdout of the program must stay identical\n"
    "\n"
    "options:\n"
    "  --baseline=FILE,... programs whose edges are subtracted (edges oracle)\n"
    "  --reducer=NAME,...  run only these reducers, in this order (see "
    "--list)\n"
    "  --max-tests=N       oracle executions (default 300)\n"
    "  --max-time-ms=N     wall-clock bound (default 30000)\n"
    "  --reruns=N          re-runs to settle the edge set (default 2)\n"
    "  --probe-timeout-ms=N  timeout for the reduced candidates (default:\n"
    "                      5x the program's run time + 20 ms, at least 50 ms;\n"
    "                      a reduction can create an endless loop)\n"
    "  --prelude=FILE|none JS prepended at execution time (default: the\n"
    "                      mjsunit shim)\n"
    "  --out=FILE          write the result to FILE instead of stdout\n"
    "  --list              list the reducers and exit\n"
    "  --v8-flags=\"...\"    flags for the in-process V8 syntax oracle\n"
    "\n";

int Run(const Flags& flags) {
  Minimizer minimizer;
  if (flags.GetBool("list", false)) {
    printf("reducers (default order):\n");
    for (const auto& reducer : minimizer.reducers()) {
      printf("  %-12s %s\n", reducer->name().c_str(),
             std::string(reducer->description()).c_str());
    }
    return kExitOk;
  }
  if (flags.positional().size() != 1) {
    fputs(kUsage, stderr);
    return kExitUsage;
  }
  const std::string& path = flags.positional()[0];
  std::string source, error;
  if (!ReadFileToString(path, &source, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }
  Lineage lineage;
  lineage.origin = "import";
  lineage.detail = path;
  std::shared_ptr<Program> program = Program::Parse(source, lineage, &error);
  if (program == nullptr) {
    REDLINE_ERROR(kLog, "%s: %s", path.c_str(), error.c_str());
    return kExitFailure;
  }

  std::vector<Reducer*> reducers;
  for (const std::string& name : flags.GetList("reducer")) {
    Reducer* reducer = minimizer.Find(name);
    if (reducer == nullptr) {
      REDLINE_ERROR(kLog, "unknown reducer '%s' (see --list)", name.c_str());
      return kExitUsage;
    }
    reducers.push_back(reducer);
  }
  if (reducers.empty()) {
    for (const auto& reducer : minimizer.reducers()) {
      reducers.push_back(reducer.get());
    }
  }
  MinimizerOptions options;
  options.max_tests =
      static_cast<int>(flags.GetInt("max-tests", options.max_tests));
  options.max_time_ms = static_cast<uint64_t>(
      flags.GetInt("max-time-ms", static_cast<int64_t>(options.max_time_ms)));
  Minimizer configured(options);
  // Use the reducers of `minimizer` (which carry the stats) with the
  // configured budget.
  std::string oracle_name = flags.GetString("oracle", "edges");
  std::string prelude;
  std::string prelude_flag = flags.GetString("prelude");
  if (prelude_flag.empty()) {
    prelude = std::string(DefaultPrelude());
  } else if (prelude_flag != "none" &&
             !ReadFileToString(prelude_flag, &prelude, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }

  if (!InitializeV8(flags.GetString(
                        "v8-flags",
                        "--allow-natives-syntax --js-staging --js-decorators"),
                    &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }
  SyntaxChecker checker;
  TargetOptions target;
  if (!TargetOptions::FromFlags(flags, &target, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitUsage;
  }
  std::unique_ptr<ScriptRunner> runner =
      ScriptRunner::Create(target, 0, &error);
  if (runner == nullptr || !runner->Initialize(&error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }
  auto execute = [&](const std::string& candidate, Execution* execution,
                     uint64_t timeout_us) {
    std::string script =
        prelude.empty() ? candidate : prelude + "\n" + candidate;
    std::string exec_error;
    if (!runner->RunWithoutEvaluation(script, timeout_us, execution,
                                      &exec_error)) {
      REDLINE_WARN(kLog, "execution failed: %s", exec_error.c_str());
      return false;
    }
    return true;
  };
  auto execute_full = [&](const std::string& candidate, Execution* execution) {
    return execute(candidate, execution, target.timeout_us);
  };

  // Establish the property on the original program.
  Execution first;
  if (!execute_full(program->source(), &first)) {
    return kExitFailure;
  }
  uint64_t probe_timeout_us =
      static_cast<uint64_t>(flags.GetInt("probe-timeout-ms", 0)) * 1000;
  if (probe_timeout_us == 0) {
    probe_timeout_us =
        std::max<uint64_t>(5 * first.exec_time_us + 20'000, 50'000);
  }
  probe_timeout_us = std::min(probe_timeout_us, target.timeout_us);

  std::vector<uint32_t> edges;
  std::string expected_output;
  MinimizationOracle oracle;
  if (oracle_name == "edges") {
    if (!first.succeeded()) {
      REDLINE_ERROR(kLog,
                    "the program does not run successfully (outcome %s); use "
                    "--oracle=crash/fail",
                    first.crashed()     ? "crashed"
                    : first.timed_out() ? "timed out"
                                        : "failed");
      return kExitFailure;
    }
    edges = runner->coverage().CurrentEdges();
    int reruns = static_cast<int>(flags.GetInt("reruns", 2));
    for (int i = 0; i < reruns; i++) {
      Execution again;
      if (!execute_full(program->source(), &again) || !again.succeeded()) {
        REDLINE_ERROR(
            kLog,
            "the program is not deterministic (re-run %d did not succeed)",
            i + 1);
        return kExitFailure;
      }
      std::vector<uint32_t> missing = runner->coverage().MissingEdges(edges);
      if (!missing.empty()) {
        std::vector<uint32_t> kept;
        std::set_difference(edges.begin(), edges.end(), missing.begin(),
                            missing.end(), std::back_inserter(kept));
        edges.swap(kept);
      }
    }
    size_t settled = edges.size();
    // Subtract everything the baseline programs reach in any of their runs.
    std::vector<std::string> baselines = flags.GetList("baseline");
    if (!baselines.empty()) {
      std::vector<uint8_t> seen(runner->coverage().num_edges() + 1, 0);
      for (const std::string& baseline_path : baselines) {
        std::string baseline;
        if (!ReadFileToString(baseline_path, &baseline, &error)) {
          REDLINE_ERROR(kLog, "%s", error.c_str());
          return kExitFailure;
        }
        for (int i = 0; i <= reruns; i++) {
          Execution run;
          if (!execute_full(baseline, &run)) {
            return kExitFailure;
          }
          for (uint32_t edge : runner->coverage().CurrentEdges()) {
            if (edge < seen.size()) {
              seen[edge] = 1;
            }
          }
        }
      }
      std::vector<uint32_t> remaining;
      for (uint32_t edge : edges) {
        if (edge >= seen.size() || !seen[edge]) {
          remaining.push_back(edge);
        }
      }
      edges.swap(remaining);
      if (edges.empty()) {
        REDLINE_ERROR(
            kLog,
            "the program reaches no edge beyond the %zu baseline program(s)",
            baselines.size());
        return kExitFailure;
      }
    }
    REDLINE_LOG(kLog,
                "preserving %zu edges (%zu settled over %d re-runs%s); probe "
                "timeout %llu ms",
                edges.size(), settled, reruns,
                baselines.empty() ? "" : ", minus the baseline",
                static_cast<unsigned long long>(probe_timeout_us / 1000));
    oracle = [&](const std::string& candidate) {
      Execution execution;
      return execute(candidate, &execution, probe_timeout_us) &&
             execution.succeeded() && runner->coverage().AllEdgesSet(edges);
    };
  } else if (oracle_name == "crash") {
    if (!first.crashed()) {
      REDLINE_ERROR(kLog, "the program does not crash (exit code %d)",
                    first.exit_code);
      return kExitFailure;
    }
    oracle = [&](const std::string& candidate) {
      Execution execution;
      return execute(candidate, &execution, probe_timeout_us) &&
             execution.crashed();
    };
  } else if (oracle_name == "fail") {
    if (first.outcome != Outcome::kFailed) {
      REDLINE_ERROR(kLog, "the program does not fail");
      return kExitFailure;
    }
    std::string signature = first.ExceptionSignature();
    if (signature.empty()) {
      REDLINE_LOG(kLog,
                  "preserving the failure (no exception message captured)");
    } else {
      REDLINE_LOG(kLog, "preserving the failure `%s`", signature.c_str());
    }
    oracle = [&, signature](const std::string& candidate) {
      Execution execution;
      return execute(candidate, &execution, probe_timeout_us) &&
             execution.outcome == Outcome::kFailed &&
             execution.ExceptionSignature() == signature;
    };
  } else if (oracle_name == "output") {
    expected_output = first.stdout_output;
    oracle = [&](const std::string& candidate) {
      Execution execution;
      return execute(candidate, &execution, probe_timeout_us) &&
             execution.outcome == first.outcome &&
             execution.stdout_output == expected_output;
    };
  } else {
    REDLINE_ERROR(kLog, "unknown oracle '%s'", oracle_name.c_str());
    return kExitUsage;
  }

  std::unique_ptr<fast::Tree> tree = program->CopyTree();
  MinimizationResult result =
      configured.MinimizeWith(reducers, tree.get(), oracle, &checker);
  std::string minimized = fast::Print(tree->root());

  // Verify the result once more (a few attempts: the target is not fully
  // deterministic, see the fuzzer's determinism check).
  bool holds = false;
  for (int attempt = 0; attempt < 3 && !holds; attempt++) {
    holds = oracle(minimized);
  }
  std::string out_path = flags.GetString("out");
  if (!out_path.empty()) {
    if (!WriteStringToFile(out_path, minimized, &error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return kExitFailure;
    }
  } else {
    printf("%s\n", minimized.c_str());
  }
  std::vector<const Component*> components;
  components.reserve(reducers.size());
  for (Reducer* reducer : reducers) {
    components.push_back(reducer);
  }
  fputs(FormatStatsTable("reducers", components).c_str(), stderr);
  REDLINE_LOG(kLog,
              "%zu -> %zu nodes (%.1f%%), %d tests (%d accepted, %d "
              "syntax-rejected) in %.1f s%s; "
              "property %s on the result",
              result.nodes_before, result.nodes_after,
              result.nodes_before
                  ? 100.0 * static_cast<double>(result.nodes_after) /
                        static_cast<double>(result.nodes_before)
                  : 0.0,
              result.tests, result.accepted, result.syntax_rejected,
              static_cast<double>(result.time_us) / 1e6,
              result.budget_exhausted ? " (budget exhausted)" : "",
              holds ? "HOLDS" : "DOES NOT HOLD");
  return holds ? kExitOk : kExitFailure;
}

}  // namespace

const Command kMinimizeCommand = {
    "minimize",
    "Minimize a program while an oracle (edges, crash, fail, output) holds",
    kUsage,
    Run,
};

}  // namespace redline
