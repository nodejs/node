// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// redline fuzz: the coverage-guided fuzzing session.

#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <string>

#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/json.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/engine/config.h"
#include "experimental/redline/engine/fuzzer.h"

namespace redline {

namespace {

constexpr char kLog[] = "fuzz";

const char kUsage[] =
    "usage: redline fuzz --d8=PATH [options]\n"
    "\n"
    "Runs a coverage-guided fuzzing session against the instrumented d8:\n"
    "imports the seed programs, then repeatedly picks a corpus program,\n"
    "mutates it, executes it, and keeps it if it reaches new deterministic\n"
    "edges (after minimization). Stops on --duration/--max-execs or Ctrl-C\n"
    "(a second Ctrl-C aborts immediately).\n"
    "\n"
    "output (--out=DIR, default redline-out):\n"
    "  DIR/corpus/NNNNNN.js   every corpus program with its lineage header\n"
    "  DIR/crashes/*.js|.txt  crashing programs (minimized) and their "
    "reports;\n"
    "                         *.orig.js the unminimized program, *.history/\n"
    "                         the programs a state-dependent crash needs "
    "first\n"
    "  DIR/crashes/signatures.txt  one line per distinct crash signature\n"
    "  DIR/events.jsonl       one JSON object per event (start, import,\n"
    "                         corpus_add, crash, flaky, stats, end)\n"
    "\n"
    "session options:\n"
    "  --import=DIR|FILE,...    seed programs (directories are searched for\n"
    "                           .js files; load() calls are inlined)\n"
    "  --import-root=DIR        base directory for load(\"test/...\") paths\n"
    "  --import-all             keep seeds that run fine but add no coverage\n"
    "  --jobs=N                 worker threads, one d8 each (default 1)\n"
    "  --seed=S                 run seed (default: clock); --jobs=1 runs are\n"
    "                           reproducible given a deterministic target\n"
    "  --duration=SEC           stop after SEC seconds\n"
    "  --max-execs=N            stop after N target executions\n"
    "  --out=DIR                output directory\n"
    "  --preset=NAME|FILE       a configuration for one use case, applied "
    "before\n"
    "                           --config and the flags: default, maglev,\n"
    "                           turbolev, runtime, parser, builtins, wasm\n"
    "                           (experimental/redline/presets/NAME.json) or a "
    "file\n"
    "  --config=FILE|'{json}'   JSON configuration (see --help-config); "
    "inline\n"
    "                           JSON sets any key without a dedicated flag\n"
    "  --stats-interval=SEC     print statistics every SEC seconds (30)\n"
    "  --no-component-stats     omit the per-mutator/reducer tables\n"
    "  --quiet                  no periodic statistics\n"
    "  --prelude=FILE|none      JS prepended at execution time (default: the\n"
    "                           mjsunit assertion shim)\n"
    "  --v8-flags=\"...\"         flags for the in-process V8 syntax oracle\n"
    "  --syntax-check           parse every mutant and minimizer probe "
    "in-process\n"
    "                           before running it (off: the target's "
    "SyntaxError\n"
    "                           counts it as invalid; saves ~1 ms of CPU per "
    "mutant)\n"
    "  --no-introspect          do not enumerate the target's globals at "
    "startup\n"
    "                           (the DiscoveredApi generator then has "
    "nothing)\n"
    "\n"
    "policy options (all also settable in the config file):\n"
    "  --scheduler=NAME         uniform (default), recency, tournament,\n"
    "                           productive, rarity (--preset=default)\n"
    "  --mutator-weights=Name=W,...    override mutator weights (0 disables)\n"
    "  --generator-weights=Name=W,...  override generator weights\n"
    "  --disable-mutator=Name,...      shorthand for weight 0\n"
    "  --disable-generator=Name,...\n"
    "  --consecutive-mutations=N  mutations chained per picked parent (5)\n"
    "  --rounds=N               sub-mutations per mutation (0: 1..7 at "
    "random,\n"
    "                           1..3 for CodeGen, like Fuzzilli)\n"
    "  --min-reruns=N           re-runs of every candidate (5)\n"
    "  --min-stable-reruns=N    consecutive re-runs without edge loss that "
    "end\n"
    "                           the determinism check (1)\n"
    "  --max-reruns=N           re-run budget per candidate (50; a candidate\n"
    "                           that has not converged by then is dropped)\n"
    "  --flaky-threshold=N      suppress an edge after N determinism\n"
    "                           failures (50; suppressed edges do not count)\n"
    "  --no-value-prefix        do not prepend fresh values to mutation "
    "parents\n"
    "  --prefix-min-values=N    fresh values of the prefix: N plus 0..M (10)\n"
    "  --prefix-extra-values=M  (5)\n"
    "  --capture=RULE[;RULE]    save the programs of a stage when a trigger "
    "fires:\n"
    "                           "
    "STAGE:TRIGGER[,TRIGGER..][:MUTATOR[,MUTATOR..]][:MAX]\n"
    "                           e.g. mutate:timeout:Splice:50 or "
    "check:flaky:20;\n"
    "                           files go to OUT/captures/<stage>/ (see "
    "--help-config)\n"
    "  --stage-log=STAGE,...    one JSONL event per occurrence of a stage\n"
    "                           (pick, prefix, mutate, check, minimize, "
    "publish)\n"
    "  --tunable=NAME=V,...     set numbers inside "
    "generators/mutators/reducers\n"
    "  --help-tunables          list them with defaults, ranges and "
    "documentation\n"
    "  --no-minimize            do not minimize new corpus programs\n"
    "  --no-minimize-crashes    do not minimize crashes\n"
    "  --minimize-imports       minimize imported seeds too\n"
    "  --minimizer-max-tests=N  oracle executions per minimization (300)\n"
    "  --minimizer-max-time-ms=N  wall-clock bound per minimization (30000)\n"
    "  --minimizer-target-time-ms=N  scale the test budget of slow programs\n"
    "                           down to about N ms per minimization (6000;\n"
    "                           0 = always max-tests)\n"
    "  --crash-reruns=N         fresh-target re-executions to check whether a\n"
    "                           crash reproduces (3)\n"
    "  --max-crashes-per-signature=N  crash files kept per crash signature\n"
    "                           (5; further ones are only counted)\n"
    "  --minimize-crashes-per-signature=N  ... of which N are minimized (2)\n"
    "  --crash-history=N        for crashes that do not reproduce alone: "
    "replay\n"
    "                           up to N preceding programs of the same target\n"
    "                           to find state-dependent crashes (100; 0 = "
    "off)\n"
    "  --crash-history-budget=SEC  time limit for that replay (90)\n"
    "  --crash-minimizer-max-tests=N, --crash-minimizer-max-time-ms=N\n"
    "                           minimization budget for crashes (1000, "
    "120000)\n"
    "  --initial-programs=N     bootstrap from generated programs until the\n"
    "                           corpus has N entries (32)\n"
    "  --no-save-corpus         do not write DIR/corpus\n"
    "  --help-config            print the config file format and exit\n"
    "  --dump-config            print the complete effective configuration "
    "(every\n"
    "                           key, tunable and strategy parameter, defaults\n"
    "                           included) as JSON and exit; needs no target\n"
    "\n";

std::atomic<Fuzzer*> g_fuzzer{nullptr};
std::atomic<int> g_interrupts{0};

void HandleInterrupt(int /*signal*/) {
  int count = g_interrupts.fetch_add(1) + 1;
  if (count >= 2) {
    _exit(130);
  }
  Fuzzer* fuzzer = g_fuzzer.load();
  if (fuzzer != nullptr) {
    fuzzer->RequestStop();
  }
  const char message[] = "\n[redline] stopping (press Ctrl-C again to abort)\n";
  ssize_t ignored = write(STDERR_FILENO, message, sizeof(message) - 1);
  (void)ignored;
}

int Run(const Flags& flags) {
  if (flags.GetBool("help-config", false)) {
    fputs(FuzzConfig::JsonHelp().c_str(), stdout);
    return kExitOk;
  }
  if (flags.GetBool("help-tunables", false)) {
    printf(
        "tunables (set with \"tunables\": {name: value} in --config or "
        "--tunable=name=value,...):\n");
    fputs(TunableRegistry::Instance().Help().c_str(), stdout);
    return kExitOk;
  }
  FuzzConfig config;
  std::string error;
  const bool dump = flags.GetBool("dump-config", false);
  if (!FuzzConfig::Load(flags, &config, &error, /*for_run=*/!dump)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitUsage;
  }
  if (dump) {
    printf("%s\n",
           JsonPretty(config.ToJson(FuzzConfig::Detail::kFull)).c_str());
    return kExitOk;
  }
  std::vector<std::string> unused = flags.UnusedFlags();
  for (const std::string& flag : unused) {
    if (flag == "log-level" || flag == "verbose") {
      continue;
    }
    REDLINE_WARN(kLog, "unknown flag --%s (ignored)", flag.c_str());
  }

  Fuzzer fuzzer(config);
  if (!fuzzer.Initialize(&error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }
  g_fuzzer.store(&fuzzer);
  struct sigaction action = {};
  action.sa_handler = HandleInterrupt;
  sigaction(SIGINT, &action, nullptr);
  sigaction(SIGTERM, &action, nullptr);
  int result = fuzzer.Run();
  g_fuzzer.store(nullptr);
  return result == 0 ? kExitOk : kExitFailure;
}

}  // namespace

const Command kFuzzCommand = {
    "fuzz",
    "Run a coverage-guided fuzzing session (the main command)",
    kUsage,
    Run,
};

}  // namespace redline
