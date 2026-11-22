// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "redline mutate": applies mutators or generators to a program and measures
// how often the result is accepted by V8's parser and, optionally, how it
// behaves when executed. This is the black-box test bench for every
// mutator/generator and the way to look at what they produce.
//
// "redline audit" is the same bench in one fixed mode: every generator from
// an empty program, run in the target, crashes counted per signature.

#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/base/v8_runtime.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/core/builder.h"
#include "experimental/redline/core/component.h"
#include "experimental/redline/core/environment.h"
#include "experimental/redline/core/program.h"
#include "experimental/redline/core/random.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/engine/config.h"
#include "experimental/redline/engine/crash.h"
#include "experimental/redline/engine/prelude.h"
#include "experimental/redline/exec/execution.h"
#include "experimental/redline/exec/runner.h"
#include "experimental/redline/exec/target_options.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/generators/generator.h"
#include "experimental/redline/mutators/mutator.h"

namespace redline {

namespace {

constexpr const char* kLog = "mutate";

const char kUsage[] =
    "usage: redline mutate [flags] FILE...\n"
    "       redline mutate [flags] --source=CODE\n"
    "       redline mutate [flags] --generate\n"
    "\n"
    "Applies mutators (or generators) to the input program(s) and reports\n"
    "how many results V8 accepts syntactically and, with --run, how they\n"
    "behave in d8. Use it to test and look at every mutator/generator.\n"
    "\n"
    "flags:\n"
    "  --mutator=NAME,...|all   mutators to apply (default: all)\n"
    "  --generator=NAME,...|all test generators instead: each sample inserts\n"
    "                           generated code at a random place of the input\n"
    "  --generate               start from an empty program\n"
    "  --n=K                    samples per mutator/generator and input (20)\n"
    "  --rounds=R               sub-mutations per sample (1)\n"
    "  --seed=S                 base seed (1); sample i uses seed S+i\n"
    "  --partner=FILE           donor program for Splice/Combine (default:\n"
    "                           the next input file, or the input itself)\n"
    "  --print                  print every produced program\n"
    "  --print-invalid          print programs V8 rejects, with the error\n"
    "  --print-failed           print programs that fail at runtime (--run)\n"
    "  --out-dir=DIR            also write samples to DIR/<name>-<i>.js\n"
    "  --run                    execute results in d8 (--d8=PATH or "
    "$REDLINE_D8)\n"
    "  --d8-args=ARGS --timeout-ms=N   target options for --run\n"
    "  --prelude=FILE|none      JS prepended at execution time (default: the\n"
    "                           built-in mjsunit assertion stubs)\n"
    "  --v8-flags=FLAGS         flags for the in-process V8 syntax oracle\n"
    "  --tunable=NAME=V,...     set numbers inside generators/mutators (the "
    "same\n"
    "                           names as redline fuzz --help-tunables)\n"
    "  --list                   list mutators and generators and exit\n"
    "  --no-introspect          with --run: do not ask the target about its "
    "globals\n"
    "  --quiet                  only print the summary\n";

const char kAuditUsage[] =
    "usage: redline audit --d8=PATH [flags]\n"
    "\n"
    "Overfitting check for generators (DESIGN.md 10). Generates --n programs\n"
    "per generator from an empty program, runs each one in d8 and counts the\n"
    "crashes per crash signature. A generator fails if one signature takes\n"
    "more than --threshold percent of its programs: it is then closer to a\n"
    "template of that crash than to a general generator. Run it against a\n"
    "d8 with known bugs. Exits non-zero if a generator fails.\n"
    "\n"
    "flags:\n"
    "  --generator=NAME,...|all generators to check (default: all)\n"
    "  --n=K                    programs per generator (400)\n"
    "  --threshold=PCT          largest allowed share of one signature (0.5)\n"
    "  --seed=S                 base seed (1)\n"
    "  --d8-args=ARGS --d8-extra-args=ARGS --timeout-ms=N   target options\n"
    "  --tunable=NAME=V,...     audit with these tunable values\n"
    "  --print                  print every program\n"
    "  --out-dir=DIR            write every program to DIR/<name>-0-<i>.js\n"
    "  --quiet                  no line per crash, only the tables\n"
    "  --no-introspect          do not ask the target about its globals\n";

struct Options {
  std::vector<std::string> files;
  std::string source;
  bool generate = false;
  std::vector<std::string> mutators;
  std::vector<std::string> generators;
  bool generator_mode = false;
  int n = 20;
  int rounds = 1;
  uint64_t seed = 1;
  std::string partner;
  bool print = false;
  bool print_invalid = false;
  bool print_failed = false;
  std::string out_dir;
  bool run = false;
  bool list = false;
  bool quiet = false;
  bool introspect = true;
  std::string v8_flags;
  std::string prelude;
  // redline audit: crash signatures per generator, judged by `threshold`.
  bool audit = false;
  double threshold = 0.5;
};

bool ParseOptions(const Flags& flags, Options* o, bool audit) {
  o->files = flags.positional();
  o->source = flags.GetString("source");
  o->generate = flags.GetBool("generate", false);
  if (flags.Has("generator")) {
    o->generator_mode = true;
    std::string g = flags.GetString("generator", "all");
    if (g != "all") {
      o->generators = flags.GetList("generator");
    }
  }
  std::string m = flags.GetString("mutator", "all");
  if (m != "all") {
    o->mutators = flags.GetList("mutator");
  }
  o->n = static_cast<int>(flags.GetInt("n", audit ? 400 : 20));
  o->rounds = static_cast<int>(flags.GetInt("rounds", 1));
  o->seed = static_cast<uint64_t>(flags.GetInt("seed", 1));
  o->partner = flags.GetString("partner");
  o->print = flags.GetBool("print", false);
  o->print_invalid = flags.GetBool("print-invalid", false);
  o->print_failed = flags.GetBool("print-failed", false);
  o->out_dir = flags.GetString("out-dir");
  o->run = flags.GetBool("run", false);
  o->list = flags.GetBool("list", false);
  o->quiet = flags.GetBool("quiet", false);
  o->introspect = flags.GetBool("introspect", true);
  o->v8_flags = flags.GetString(
      "v8-flags", "--allow-natives-syntax --js-staging --js-decorators");
  std::string prelude = flags.GetString("prelude");
  if (prelude.empty()) {
    o->prelude = std::string(DefaultPrelude());
  } else if (prelude != "none") {
    std::string error;
    if (!ReadFileToString(prelude, &o->prelude, &error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return false;
    }
  }
  if (flags.Has("tunable")) {
    std::string error;
    if (!TunableRegistry::Instance().SetFromList(flags.GetString("tunable"),
                                                 &error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return false;
    }
  }
  if (audit) {
    o->audit = true;
    o->generate = true;
    o->generator_mode = true;
    o->run = true;
    o->threshold = flags.GetDouble("threshold", o->threshold);
  }
  return true;
}

void PrintProgram(const char* title, const std::string& source) {
  printf("----- %s -----\n", title);
  fwrite(source.data(), 1, source.size(), stdout);
  if (source.empty() || source.back() != '\n') {
    fputc('\n', stdout);
  }
}

std::string FirstLine(const std::string& s) {
  size_t nl = s.find('\n');
  std::string line = nl == std::string::npos ? s : s.substr(0, nl);
  if (line.size() > 100) {
    line = line.substr(0, 97) + "...";
  }
  return line;
}

// Shared evaluation of one produced program: syntax oracle, optional
// execution, printing and statistics.
class Evaluator {
 public:
  Evaluator(const Options& options, SyntaxChecker* checker,
            ScriptRunner* runner)
      : options_(options), checker_(checker), runner_(runner) {}

  void Evaluate(Component* component, const std::string& name, int index,
                const std::string& source, const std::string& detail) {
    ComponentStats& stats = component->stats();
    std::string title =
        StringPrintf("%s #%d: %s", name.c_str(), index, detail.c_str());
    samples_[name]++;
    if (!options_.out_dir.empty()) {
      std::string path =
          StringPrintf("%s/%s-%zu-%d.js", options_.out_dir.c_str(),
                       name.c_str(), input_index_, index);
      std::string error;
      if (!WriteStringToFile(path, source, &error)) {
        REDLINE_WARN(kLog, "%s", error.c_str());
      }
    }
    std::string message;
    bool accepted = checker_->Accepts(source, &message);
    total_++;
    if (!accepted) {
      stats.invalid++;
      invalid_++;
      if (options_.print_invalid || options_.print) {
        PrintProgram(title.c_str(), source);
        printf("REJECTED: %s\n", message.c_str());
      }
      return;
    }
    if (options_.print) {
      PrintProgram(title.c_str(), source);
    }
    if (runner_ == nullptr) {
      return;
    }
    ScriptRunner::Result result;
    std::string error;
    // The prelude is an execution-time detail (test harness shims); it is
    // never part of the program itself, so it is not printed or saved.
    std::string exec_source =
        options_.prelude.empty() ? source : options_.prelude + "\n" + source;
    if (!runner_->Run(exec_source, &result, &error)) {
      REDLINE_WARN(kLog, "execution failed: %s", error.c_str());
      return;
    }
    stats.executed++;
    stats.time_us += result.execution.exec_time_us;
    executed_++;
    switch (result.execution.outcome) {
      case Outcome::kSucceeded:
        stats.succeeded++;
        succeeded_++;
        break;
      case Outcome::kFailed:
        stats.failed++;
        if (options_.print_failed) {
          if (!options_.print) {
            PrintProgram(title.c_str(), source);
          }
          printf("FAILED (exit %d): %s\n", result.execution.exit_code,
                 FirstLine(result.execution.stderr_output.empty()
                               ? result.execution.stdout_output
                               : result.execution.stderr_output)
                     .c_str());
        }
        break;
      case Outcome::kTimedOut:
        stats.timed_out++;
        if (options_.print_failed && !options_.print) {
          PrintProgram(title.c_str(), source);
        }
        if (options_.print_failed) {
          printf("TIMED OUT\n");
        }
        break;
      case Outcome::kCrashed:
        stats.crashed++;
        crashed_++;
        if (options_.audit) {
          std::string signature =
              CrashSignature::FromExecution(result.execution).text;
          crashes_[name][signature]++;
          if (!options_.quiet) {
            printf("CRASH %s: %s\n", title.c_str(), signature.c_str());
          }
          break;
        }
        PrintProgram(title.c_str(), source);
        printf("CRASHED (signal %d)\n", result.execution.signal);
        break;
    }
    if (!result.new_edges.empty()) {
      stats.interesting++;
    }
  }

  uint64_t total() const { return total_; }
  uint64_t invalid() const { return invalid_; }
  uint64_t executed() const { return executed_; }
  uint64_t succeeded() const { return succeeded_; }
  uint64_t crashed() const { return crashed_; }
  // The index of the input program being processed (part of --out-dir names).
  void set_input_index(size_t index) { input_index_ = index; }

  // redline audit: one line per generator that crashed, the generator's
  // largest share of one signature, and the verdict. Returns the number of
  // generators above the threshold.
  int PrintAudit() const {
    printf(
        "\naudit: largest share of one crash signature per generator "
        "(threshold %.2f%%)\n",
        options_.threshold);
    int failing = 0;
    for (const auto& [name, signatures] : crashes_) {
      auto top = std::max_element(
          signatures.begin(), signatures.end(),
          [](const auto& a, const auto& b) { return a.second < b.second; });
      uint64_t samples = samples_.at(name);
      double share = 100.0 * static_cast<double>(top->second) /
                     static_cast<double>(samples);
      bool over = share > options_.threshold;
      failing += over ? 1 : 0;
      printf(
          "  %s %-26s %4llu of %4llu (%5.2f%%)  %s\n", over ? "FAIL" : "ok  ",
          name.c_str(), static_cast<unsigned long long>(top->second),
          static_cast<unsigned long long>(samples), share, top->first.c_str());
    }
    printf("audit: %zu of %zu generators crashed, %d above the threshold\n",
           crashes_.size(), samples_.size(), failing);
    return failing;
  }

 private:
  const Options& options_;
  SyntaxChecker* checker_;
  ScriptRunner* runner_;
  size_t input_index_ = 0;
  uint64_t total_ = 0;
  uint64_t invalid_ = 0;
  uint64_t executed_ = 0;
  uint64_t succeeded_ = 0;
  uint64_t crashed_ = 0;
  // Programs per component, and crash signature -> count per component.
  std::map<std::string, uint64_t> samples_;
  std::map<std::string, std::map<std::string, uint64_t>> crashes_;
};

int RunBench(const Flags& flags, bool audit) {
  Options options;
  if (!ParseOptions(flags, &options, audit)) {
    return kExitUsage;
  }

  std::unique_ptr<MutatorSet> mutators = MutatorSet::CreateDefault();
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();

  if (options.list) {
    printf("mutators:\n");
    for (const auto& m : mutators->all()) {
      printf("  %-14s w=%-4g %s%s\n", m->name().c_str(), m->weight(),
             std::string(m->description()).c_str(),
             m->needs_partner() ? " [needs partner]" : "");
    }
    printf("generators:\n");
    for (const auto& g : generators->all()) {
      printf("  %-18s w=%-4g %s\n", g->name().c_str(), g->weight(),
             std::string(g->description()).c_str());
    }
    return kExitOk;
  }

  // Inputs.
  std::vector<std::pair<std::string, std::string>> inputs;  // name, source
  if (options.generate) {
    inputs.emplace_back("empty", "");
  } else if (flags.Has("source")) {
    inputs.emplace_back("<source>", options.source);
  } else {
    for (const std::string& path : options.files) {
      std::string source, error;
      if (!ReadFileToString(path, &source, &error)) {
        REDLINE_ERROR(kLog, "%s", error.c_str());
        return kExitFailure;
      }
      inputs.emplace_back(path, source);
    }
  }
  if (inputs.empty()) {
    fputs(kUsage, stderr);
    return kExitUsage;
  }
  std::vector<ProgramPtr> programs;
  for (const auto& [name, source] : inputs) {
    std::string error;
    Lineage lineage;
    lineage.origin = "import";
    lineage.detail = name;
    ProgramPtr program = Program::Parse(source, lineage, &error);
    if (program == nullptr) {
      REDLINE_ERROR(kLog, "%s: %s", name.c_str(), error.c_str());
      return kExitFailure;
    }
    programs.push_back(program);
  }
  ProgramPtr explicit_partner;
  if (!options.partner.empty()) {
    std::string source, error;
    if (!ReadFileToString(options.partner, &source, &error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return kExitFailure;
    }
    Lineage lineage;
    lineage.origin = "import";
    lineage.detail = options.partner;
    explicit_partner = Program::Parse(source, lineage, &error);
    if (explicit_partner == nullptr) {
      REDLINE_ERROR(kLog, "%s: %s", options.partner.c_str(), error.c_str());
      return kExitFailure;
    }
  }

  // Selection.
  std::vector<Mutator*> selected_mutators;
  std::vector<Generator*> selected_generators;
  if (options.generator_mode) {
    if (options.generators.empty()) {
      for (const auto& g : generators->all()) {
        selected_generators.push_back(g.get());
      }
    } else {
      for (const std::string& name : options.generators) {
        Generator* g = generators->Find(name);
        if (g == nullptr) {
          REDLINE_ERROR(kLog, "unknown generator '%s' (see --list)",
                        name.c_str());
          return kExitUsage;
        }
        selected_generators.push_back(g);
      }
    }
  } else if (options.mutators.empty()) {
    for (const auto& m : mutators->all()) {
      selected_mutators.push_back(m.get());
    }
  } else {
    for (const std::string& name : options.mutators) {
      Mutator* m = mutators->Find(name);
      if (m == nullptr) {
        REDLINE_ERROR(kLog, "unknown mutator '%s' (see --list)", name.c_str());
        return kExitUsage;
      }
      selected_mutators.push_back(m);
    }
  }

  // Oracle and target.
  std::string error;
  if (!InitializeV8(options.v8_flags, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }
  SyntaxChecker checker;
  std::unique_ptr<ScriptRunner> runner;
  std::unique_ptr<Environment> discovered;
  if (options.run) {
    TargetOptions target;
    if (!TargetOptions::FromFlags(flags, &target, &error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return kExitUsage;
    }
    runner = ScriptRunner::Create(target, 0, &error);
    if (runner == nullptr || !runner->Initialize(&error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return kExitFailure;
    }
    // Like a fuzzing session: ask the target what it offers, so that the
    // generators using the discovered API produce the same code here.
    if (options.introspect) {
      Execution execution;
      const uint64_t timeout_us =
          static_cast<uint64_t>(FuzzConfig().introspect_timeout_ms * 1000.0);
      if (runner->RunWithoutEvaluation(Environment::IntrospectionScript(),
                                       timeout_us, &execution, &error) &&
          execution.succeeded()) {
        discovered = Environment::WithDiscovered(execution.fuzzout, &error);
      }
      if (discovered == nullptr) {
        REDLINE_WARN(
            kLog,
            "environment introspection failed (%s); using the built-in model",
            error.c_str());
      } else {
        REDLINE_LOG(kLog,
                    "environment: %zu globals/namespaces with %zu members "
                    "discovered in the target",
                    discovered->discovered().size(),
                    discovered->discovered_member_count());
      }
    }
  }
  const Environment& env =
      discovered != nullptr ? *discovered : Environment::Default();
  if (!options.out_dir.empty() && !MakeDirectories(options.out_dir, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }

  Evaluator evaluator(options, &checker, runner.get());
  Stopwatch stopwatch;
  uint64_t noop = 0;
  uint64_t mutation_time_us = 0;

  for (size_t input = 0; input < programs.size(); input++) {
    evaluator.set_input_index(input);
    const ProgramPtr& parent = programs[input];
    ProgramPtr partner = explicit_partner;
    if (partner == nullptr) {
      partner = programs.size() > 1 ? programs[(input + 1) % programs.size()]
                                    : parent;
    }
    if (options.generator_mode) {
      for (Generator* g : selected_generators) {
        for (int i = 0; i < options.n; i++) {
          Random rng(options.seed + static_cast<uint64_t>(i) * 7919 +
                     Program::HashSource(g->name()) + input * 104729);
          std::unique_ptr<fast::Tree> tree = parent->CopyTree();
          Builder builder(tree.get(), &rng);
          Stopwatch sw;
          std::vector<fast::Statement*> out;
          Builder::Slot slot;
          bool generated = false;
          for (int attempt = 0; attempt < 10 && !generated; attempt++) {
            slot = builder.RandomInsertionSlot();
            if (!slot.valid()) {
              break;
            }
            GenContext ctx = MakeGenContext(builder, slot, *generators, env);
            if (!g->Applicable(ctx)) {
              continue;
            }
            ctx.out = &out;
            g->stats().invocations++;
            if (g->Generate(ctx, &out) && !out.empty()) {
              g->stats().produced++;
              generated = true;
            } else {
              out.clear();
            }
          }
          mutation_time_us += sw.ElapsedUs();
          if (!generated) {
            noop++;
            continue;
          }
          builder.InsertAll(slot, out);
          std::string source = fast::Print(tree->root());
          evaluator.Evaluate(g, g->name(), i, source,
                             StringPrintf("%zu statements", out.size()));
        }
      }
      continue;
    }
    for (Mutator* m : selected_mutators) {
      for (int i = 0; i < options.n; i++) {
        Random rng(options.seed + static_cast<uint64_t>(i) * 7919 +
                   Program::HashSource(m->name()) + input * 104729);
        Stopwatch sw;
        std::string detail;
        std::unique_ptr<fast::Tree> tree =
            ApplyMutation(*m, *parent, partner, rng, env, *generators,
                          options.rounds, &detail);
        mutation_time_us += sw.ElapsedUs();
        if (tree == nullptr) {
          noop++;
          continue;
        }
        std::string source = fast::Print(tree->root());
        evaluator.Evaluate(m, m->name(), i, source, detail);
      }
    }
  }

  // Summary.
  std::vector<const Component*> components;
  if (options.generator_mode) {
    for (Generator* g : selected_generators) {
      components.push_back(g);
    }
  } else {
    for (Mutator* m : selected_mutators) {
      components.push_back(m);
    }
  }
  std::string table = FormatStatsTable(
      options.generator_mode ? "generators" : "mutators", components);
  fputs(table.c_str(), stdout);
  if (!options.generator_mode) {
    std::string gtable = FormatStatsTable("generators (used by mutators)",
                                          generators->Components());
    fputs(gtable.c_str(), stdout);
  }
  uint64_t total = evaluator.total();
  uint64_t valid = total - evaluator.invalid();
  printf("summary: %llu produced, %llu no-op, %llu V8-accepted (%.1f%%)",
         static_cast<unsigned long long>(total),
         static_cast<unsigned long long>(noop),
         static_cast<unsigned long long>(valid),
         total ? 100.0 * static_cast<double>(valid) / static_cast<double>(total)
               : 0.0);
  if (runner != nullptr) {
    printf(", %llu executed, %llu succeeded (%.1f%%), %llu crashed",
           static_cast<unsigned long long>(evaluator.executed()),
           static_cast<unsigned long long>(evaluator.succeeded()),
           evaluator.executed()
               ? 100.0 * static_cast<double>(evaluator.succeeded()) /
                     static_cast<double>(evaluator.executed())
               : 0.0,
           static_cast<unsigned long long>(evaluator.crashed()));
  }
  printf("; mutation time %.1f ms total (%.2f ms/sample), wall %.1f s\n",
         static_cast<double>(mutation_time_us) / 1000.0,
         total + noop ? static_cast<double>(mutation_time_us) / 1000.0 /
                            static_cast<double>(total + noop)
                      : 0.0,
         stopwatch.ElapsedSeconds());
  if (options.audit && evaluator.PrintAudit() > 0) {
    return kExitFailure;
  }
  return kExitOk;
}

int Run(const Flags& flags) { return RunBench(flags, /*audit=*/false); }

int RunAudit(const Flags& flags) { return RunBench(flags, /*audit=*/true); }

}  // namespace

const Command kMutateCommand = {
    "mutate",
    "apply mutators/generators to a program and measure validity",
    kUsage,
    Run,
};

const Command kAuditCommand = {
    "audit",
    "overfitting check: crash signatures per generator, against a threshold",
    kAuditUsage,
    RunAudit,
};

}  // namespace redline
