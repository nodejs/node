// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/commands/selftest_selection.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/json.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/core/adaptive_selector.h"
#include "experimental/redline/core/builder.h"
#include "experimental/redline/core/environment.h"
#include "experimental/redline/core/program.h"
#include "experimental/redline/core/random.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/corpus/corpus.h"
#include "experimental/redline/engine/amplifier.h"
#include "experimental/redline/engine/config.h"
#include "experimental/redline/engine/prelude.h"
#include "experimental/redline/engine/stages.h"
#include "experimental/redline/fast/parser.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/generators/generator.h"
#include "experimental/redline/mutators/mutator.h"

namespace redline {

namespace {

// The fixed seed and sizes of the traces. Changing any of them invalidates
// the golden files (regenerate with --write-golden and explain why).
constexpr uint64_t kSeed = 20261011;
constexpr int kMutatorPicks = 2000;
constexpr int kGeneratorPicks = 2000;
constexpr int kCodeGenMutations = 1000;
constexpr int kCorpusPrograms = 50;
constexpr int kCorpusPicks = 1000;

// A program with a few variables of different types, so that every
// generator finds something to work with.
constexpr const char* kProgram = R"(let n = 42;
let s = "text";
let a = [1, 2, 3];
let o = {x: 1, y: "two"};
function f(p, q) {
    let t = p + q;
    for (let i = 0; i < 3; i++) {
        t += a[i];
    }
    return t;
}
let r = f(n, 7);
class C {
    constructor(v) { this.v = v; }
    get value() { return this.v; }
}
let c = new C(r);
)";

std::shared_ptr<Program> ParseOrDie(const std::string& source,
                                    const std::string& origin) {
  Lineage lineage;
  lineage.origin = origin;
  std::string error;
  std::shared_ptr<Program> program = Program::Parse(source, lineage, &error);
  if (program == nullptr) {
    REDLINE_ERROR("selftest", "selection check program does not parse: %s",
                  error.c_str());
    abort();
  }
  return program;
}

// --- the four traces ------------------------------------------------------

// Which mutator is applied: MutatorSet::Pick with and without a partner.
std::string TraceMutatorPicks(const MutatorSet& mutators) {
  Random rng(kSeed);
  std::string trace;
  for (int i = 0; i < kMutatorPicks; i++) {
    bool has_partner = (i % 3) != 0;
    Mutator* m = mutators.Pick(rng, has_partner);
    trace += StringPrintf("%d\t%d\t%s\n", i, has_partner ? 1 : 0,
                          m != nullptr ? m->name().c_str() : "-");
  }
  return trace;
}

// Which generator is chosen for a random insertion slot of the fixed
// program: GeneratorSet::Pick.
std::string TraceGeneratorPicks(const GeneratorSet& generators,
                                const Environment& env) {
  Random rng(kSeed);
  ProgramPtr program = ParseOrDie(kProgram, "selftest");
  std::string trace;
  for (int i = 0; i < kGeneratorPicks; i++) {
    std::unique_ptr<fast::Tree> tree = program->CopyTree();
    Builder builder(tree.get(), &rng);
    Builder::Slot slot = builder.RandomInsertionSlot();
    if (!slot.valid()) {
      trace += StringPrintf("%d\t(no slot)\n", i);
      continue;
    }
    GenContext ctx = MakeGenContext(builder, slot, generators, env);
    Generator* g = generators.Pick(ctx);
    trace +=
        StringPrintf("%d\t%s\n", i, g != nullptr ? g->name().c_str() : "-");
  }
  return trace;
}

// Which generators CodeGen inserts, through the whole mutator (its own
// generator filter and slot choice included): the `detail` of each mutation.
std::string TraceCodeGen(const MutatorSet& mutators,
                         const GeneratorSet& generators,
                         const Environment& env) {
  Mutator* codegen = mutators.Find("CodeGen");
  if (codegen == nullptr) {
    return "(no CodeGen mutator)\n";
  }
  Random rng(kSeed);
  ProgramPtr program = ParseOrDie(kProgram, "selftest");
  std::string trace;
  for (int i = 0; i < kCodeGenMutations; i++) {
    std::string detail;
    std::unique_ptr<fast::Tree> tree =
        ApplyMutation(*codegen, *program, nullptr, rng, env, generators,
                      /*rounds=*/1, &detail);
    trace +=
        StringPrintf("%d\t%s\n", i, tree != nullptr ? detail.c_str() : "noop");
  }
  return trace;
}

// Which corpus program a scheduler picks next and which partner it offers,
// with the feedback a fuzzing session would give (every 7th mutation
// "interesting").
std::string TraceScheduler(const std::string& name) {
  std::string error;
  SelectorRegistry<const Program>& registry =
      SelectorRegistry<const Program>::Instance();
  std::unique_ptr<ProgramSelector> seeds =
      registry.Create(SelectionConfig{name, JsonValue()}, &error);
  if (seeds == nullptr) {
    return "(" + error + ")\n";
  }
  std::unique_ptr<ProgramSelector> partners =
      registry.Create(SelectionConfig{"uniform-partner", JsonValue()}, &error);
  if (partners == nullptr) {
    return "(" + error + ")\n";
  }
  Corpus corpus(std::move(seeds), std::move(partners));
  for (int i = 0; i < kCorpusPrograms; i++) {
    corpus.Add(ParseOrDie(StringPrintf("let v%d = %d;\n", i, i), "selftest"));
  }
  Random rng(kSeed);
  std::string trace;
  for (int i = 0; i < kCorpusPicks; i++) {
    ProgramPtr parent = corpus.Pick(rng);
    if (parent == nullptr) {
      trace += StringPrintf("%d\t-\t-\n", i);
      continue;
    }
    ProgramPtr partner = corpus.PickPartner(*parent, rng);
    corpus.OnResult(*parent, (i % 7) == 0);
    trace += StringPrintf("%d\t%u\t%u\n", i, parent->id,
                          partner != nullptr ? partner->id : 0u);
  }
  return trace;
}

// --- component outputs
// --------------------------------------------------------
//
// The output of every generator and every mutator for a fixed seed: pins
// the numbers inside them (the tunables), not only which one is picked. One
// line per output: name, index, FNV-1a hash of the program, a glimpse.

constexpr int kOutputsPerComponent = 5;

std::string Glimpse(const std::string& source) {
  std::string out;
  for (char c : source) {
    if (out.size() >= 70) {
      break;
    }
    out += (c == '\n' || c == '\t') ? ' ' : c;
  }
  return out;
}

// Sample `i` of generator `g`: inserts its code at a random place of a copy
// of `program`. Returns the tree and the generated statements in `*out`, or
// nullptr if the generator produced nothing.
std::unique_ptr<fast::Tree> GenerateSample(Generator* g, int i,
                                           const Program& program,
                                           const GeneratorSet& generators,
                                           const Environment& env,
                                           std::vector<fast::Statement*>* out) {
  Random rng(kSeed + static_cast<uint64_t>(i) * 7919 +
             Program::HashSource(g->name()));
  std::unique_ptr<fast::Tree> tree = program.CopyTree();
  Builder builder(tree.get(), &rng);
  for (int attempt = 0; attempt < 10; attempt++) {
    Builder::Slot slot = builder.RandomInsertionSlot();
    if (!slot.valid()) {
      break;
    }
    GenContext ctx = MakeGenContext(builder, slot, generators, env);
    if (!g->Applicable(ctx)) {
      continue;
    }
    ctx.out = out;
    if (g->Generate(ctx, out) && !out->empty()) {
      builder.InsertAll(slot, *out);
      return tree;
    }
    break;
  }
  return nullptr;
}

std::string TraceGeneratorOutputs(const GeneratorSet& generators,
                                  const Environment& env) {
  std::shared_ptr<Program> program = ParseOrDie(kProgram, "selftest");
  std::string trace;
  for (Generator* g : generators.candidates()) {
    for (int i = 0; i < kOutputsPerComponent; i++) {
      std::vector<fast::Statement*> out;
      std::unique_ptr<fast::Tree> tree =
          GenerateSample(g, i, *program, generators, env, &out);
      std::string result = tree != nullptr ? fast::Print(tree->root()) : "noop";
      trace += StringPrintf(
          "%s\t%d\t%016llx\t%s\n", g->name().c_str(), i,
          static_cast<unsigned long long>(Program::HashSource(result)),
          Glimpse(result).c_str());
    }
  }
  return trace;
}

std::string TraceMutatorOutputs(const MutatorSet& mutators,
                                const GeneratorSet& generators,
                                const Environment& env) {
  std::shared_ptr<Program> program = ParseOrDie(kProgram, "selftest");
  std::shared_ptr<Program> partner = ParseOrDie(
      "function g(x) { return x * 2; }\nlet q = [g(1), g(2)];\nlet m = new "
      "Map([[1, 'a']]);\n",
      "selftest");
  std::string trace;
  for (Mutator* m : mutators.candidates()) {
    for (int i = 0; i < kOutputsPerComponent; i++) {
      Random rng(kSeed + static_cast<uint64_t>(i) * 7919 +
                 Program::HashSource(m->name()));
      std::string detail;
      std::unique_ptr<fast::Tree> tree = ApplyMutation(
          *m, *program, partner, rng, env, generators, /*rounds=*/1, &detail);
      std::string result = tree != nullptr ? fast::Print(tree->root()) : "noop";
      trace += StringPrintf(
          "%s\t%d\t%016llx\t%s\t%s\n", m->name().c_str(), i,
          static_cast<unsigned long long>(Program::HashSource(result)),
          detail.c_str(), Glimpse(result).c_str());
    }
  }
  return trace;
}

// --- golden comparison ------------------------------------------------------

// Returns the first line that differs, formatted for a FAIL message.
std::string FirstDifference(const std::string& expected,
                            const std::string& actual) {
  size_t line = 1;
  size_t i = 0;
  while (i < expected.size() && i < actual.size() && expected[i] == actual[i]) {
    if (expected[i] == '\n') {
      line++;
    }
    i++;
  }
  auto line_at = [](const std::string& s, size_t pos) {
    size_t start = s.rfind('\n', pos == 0 ? 0 : pos - 1);
    start = start == std::string::npos ? 0 : start + 1;
    size_t end = s.find('\n', pos);
    return s.substr(start, (end == std::string::npos ? s.size() : end) - start);
  };
  return StringPrintf("first difference at line %zu: golden '%s', now '%s'",
                      line, line_at(expected, i).c_str(),
                      line_at(actual, i).c_str());
}

void CompareOrWrite(Checker* checker, const std::string& check_name,
                    const std::string& path, const std::string& trace,
                    bool write_golden) {
  std::string error;
  if (write_golden) {
    bool ok = WriteStringToFile(path, trace, &error);
    checker->Check(check_name.c_str(), ok, ok ? "written: " + path : error);
    return;
  }
  std::string golden;
  if (!ReadFileToString(path, &golden, &error)) {
    checker->Check(
        check_name.c_str(), false,
        "cannot read " + path + " (" + error + "); --write-golden creates it");
    return;
  }
  if (golden == trace) {
    size_t lines =
        static_cast<size_t>(std::count(trace.begin(), trace.end(), '\n'));
    checker->Check(check_name.c_str(), true,
                   StringPrintf("%zu picks identical to %s", lines,
                                BaseName(path).c_str()));
    return;
  }
  checker->Check(check_name.c_str(), false, FirstDifference(golden, trace));
}

// Parses a JSON `selection` block the way FuzzConfig::Load does and runs the
// startup validation. Returns the error message ("" when accepted).
std::string SelectionError(const std::string& selection_json) {
  std::string text = "{\"selection\": " + selection_json + "}";
  JsonValue json;
  std::string error;
  if (!JsonValue::Parse(text, &json, &error)) {
    return "test bug, invalid JSON: " + error;
  }
  FuzzConfig config;
  // The JSON part of the real loader, then the startup validation that
  // Load() runs last.
  if (!FuzzConfig::LoadJsonForTest(json, &config, &error)) {
    return error;
  }
  return FuzzConfig::ValidateSelection(config.selection, &error) ? "" : error;
}

void CheckConfigErrors(Checker* checker) {
  struct Case {
    const char* name;
    const char* json;
    bool accepted;
    const char* expect;  // substring of the error message
  };
  const Case cases[] = {
      {"selection: default block is accepted", "{}", true, ""},
      {"selection: all built-in strategies with parameters are accepted",
       R"({"seeds": {"strategy": "recency", "params": {"recent_share": 0.3}},
             "partners": {"strategy": "uniform-partner", "params": {"tries": 2}},
             "mutators": {"strategy": "weighted", "params": {"weights": {"Splice": 4, "Reorder": 0}}},
             "generators": {"strategy": "weighted", "params": {"weights": {"JitOptimize": 10}}}})",
       true, ""},
      {"selection: unknown strategy is an error",
       R"({"seeds": {"strategy": "ml-magic"}})", false,
       "unknown selection strategy 'ml-magic'"},
      {"selection: unknown parameter is an error",
       R"({"seeds": {"strategy": "tournament", "params": {"sizes": 3}}})",
       false, "unknown parameter 'sizes'"},
      {"selection: wrong parameter type is an error",
       R"({"partners": {"strategy": "uniform-partner", "params": {"tries": "four"}}})",
       false, "must be a number"},
      {"selection: unknown mutator in weights is an error",
       R"({"mutators": {"strategy": "weighted", "params": {"weights": {"Splicer": 4}}}})",
       false, "unknown mutator in weights: Splicer"},
      {"selection: unknown generator in weights is an error",
       R"({"generators": {"params": {"weights": {"JitOptimise": 1}}}})", false,
       "unknown generator in weights: JitOptimise"},
      {"selection: a parameter the strategy does not take is an error",
       R"({"mutators": {"strategy": "weighted", "params": {"tries": 1}}})",
       false, "unknown parameter 'tries'"},
      {"selection: a weight that is not a number is an error",
       R"({"mutators": {"strategy": "weighted", "params": {"weights": {"Splice": "many"}}}})",
       false, "weight of 'Splice' must be a non-negative number"},
  };
  for (const Case& c : cases) {
    std::string error = SelectionError(c.json);
    bool ok = c.accepted ? error.empty()
                         : (!error.empty() &&
                            error.find(c.expect) != std::string::npos);
    checker->Check(c.name, ok, error.empty() ? "accepted" : error);
  }
}

// The "stages" block: parameters land in the right fields, the block wins
// over the flat keys, and mistakes are reported.
void CheckStageConfig(Checker* checker) {
  auto load = [](const std::string& text, FuzzConfig* config) -> std::string {
    JsonValue json;
    std::string error;
    if (!JsonValue::Parse(text, &json, &error)) {
      return "test bug, invalid JSON: " + error;
    }
    return FuzzConfig::LoadJsonForTest(json, config, &error) ? "" : error;
  };
  {
    FuzzConfig config;
    std::string error = load(
        R"({"consecutive_mutations": 9,
                "stages": {"prefix": {"min_values": 3, "extra_values": 1},
                           "mutate": {"consecutive": 2, "rounds": 1, "log": "events",
                                      "capture": {"on": "timeout,invalid", "mutators": ["Input"], "max": 3}},
                           "check": {"min_reruns": 2, "max_reruns": 8, "capture": {"on": ["flaky"]}},
                           "minimize": {"enabled": false, "max_tests": 12},
                           "publish": {"save_corpus": false, "stats": false}}})",
        &config);
    bool ok =
        error.empty() && config.prefix_min_values == 3 &&
        config.prefix_extra_values == 1 && config.consecutive_mutations == 2 &&
        config.mutation_rounds == 1 && config.stage(Stage::kMutate).events &&
        config.stage(Stage::kMutate).capture.on.size() == 2 &&
        config.stage(Stage::kMutate).capture.mutators ==
            std::vector<std::string>{"Input"} &&
        config.stage(Stage::kMutate).capture.max == 3 &&
        config.min_reruns == 2 && config.max_reruns == 8 &&
        config.stage(Stage::kCheck)
            .capture.Matches(CaptureTrigger::kFlaky, "") &&
        !config.minimize && config.minimizer.max_tests == 12 &&
        !config.save_corpus && !config.stage(Stage::kPublish).stats;
    checker->Check(
        "stages: block sets the stage parameters and wins over flat keys", ok,
        error.empty() ? "" : error);
    // The block round-trips through ToJson (what the start event records).
    JsonValue again;
    std::string parse_error;
    bool round_trip =
        JsonValue::Parse(config.ToJson(), &again, &parse_error) &&
        again.Get("stages").Get("mutate").GetInt("consecutive", 0) == 2 &&
        again.Get("stages").Get("mutate").Get("capture").Get("on").is_array() &&
        !again.Get("stages").Get("minimize").GetBool("enabled", true);
    checker->Check(
        "stages: block round-trips through the recorded configuration",
        round_trip, parse_error);
  }
  struct Case {
    const char* name;
    const char* json;
    const char* expect;
  };
  const Case cases[] = {
      {"stages: unknown stage is an error", R"({"stages": {"mutat": {}}})",
       "unknown stage \"mutat\""},
      {"stages: unknown key is an error",
       R"({"stages": {"mutate": {"consecutiv": 3}}})",
       "unknown key \"consecutiv\" in stages.mutate"},
      {"stages: trigger that does not apply is an error",
       R"({"stages": {"check": {"capture": {"on": ["interesting"]}}}})",
       "does not apply to the check stage"},
      {"stages: bad log value is an error",
       R"({"stages": {"minimize": {"log": "verbose"}}})",
       "log must be \"events\" or \"off\""},
      {"stages: capture.mutators outside mutate is an error",
       R"({"stages": {"pick": {"capture": {"mutators": ["Splice"]}}}})",
       "only applies to the mutate stage"},
      {"stages: unknown trigger is an error",
       R"({"stages": {"mutate": {"capture": {"on": ["slow"]}}}})",
       "unknown capture trigger 'slow'"},
  };
  for (const Case& c : cases) {
    FuzzConfig config;
    std::string error = load(c.json, &config);
    checker->Check(c.name,
                   !error.empty() && error.find(c.expect) != std::string::npos,
                   error);
  }
  // The flag form of a capture rule.
  {
    Stage stage = Stage::kPick;
    CaptureRule rule;
    std::string error;
    bool ok = CaptureRule::FromFlag("mutate:timeout,crash:Splice,Combine:7",
                                    &stage, &rule, &error) &&
              stage == Stage::kMutate && rule.on.size() == 2 &&
              rule.mutators.size() == 2 && rule.max == 7 &&
              rule.Matches(CaptureTrigger::kCrash, "Combine") &&
              !rule.Matches(CaptureTrigger::kCrash, "Input");
    checker->Check("stages: --capture rule parses", ok, error);
    ok = !CaptureRule::FromFlag("publish:timeout", &stage, &rule, &error) &&
         error.find("does not apply") != std::string::npos;
    checker->Check("stages: --capture with an inapplicable trigger is an error",
                   ok, error);
  }
}

// Tunables (core/tunable.h): set from the config, validated, reported.
// A component for the adaptive selector tests.
struct FakeComponent {
  std::string label;
  double base = 1;
  ComponentStats counters;

  const std::string& name() const { return label; }
  double weight() const { return base; }
  ComponentStats& stats() { return counters; }
  const ComponentStats& stats() const { return counters; }
};

void CheckAdaptiveWindow(Checker* checker) {
  using Selector = AdaptiveSelector<FakeComponent>;
  Selector::Settings settings;
  settings.prior_executions = 10;
  Selector selector("component", {}, settings);
  uint64_t now_us = 0;
  selector.SetClockForTesting([&now_us] { return now_us; });
  FakeComponent cheap;
  cheap.label = "Cheap";
  FakeComponent costly;
  costly.label = "Costly";
  std::string error;
  const bool initialized = selector.Initialize({&cheap, &costly}, &error);
  // Same new edges; Costly spent 10 times more executions for them.
  cheap.counters.new_edges = 100;
  cheap.counters.spent = 1000;
  costly.counters.new_edges = 100;
  costly.counters.spent = 10000;
  Random rng(1);
  now_us = 1000000;
  selector.Pick({&cheap, &costly}, rng,
                [](const FakeComponent&) { return true; });
  checker->Check(
      "adaptive: no weight update before window_sec",
      initialized && selector.WeightOf(cheap) == selector.WeightOf(costly),
      error);
  now_us = 601000000;
  selector.Pick({&cheap, &costly}, rng,
                [](const FakeComponent&) { return true; });
  checker->Check(
      "adaptive: the time window updates the weights, and new edges per spent "
      "execution favour the cheap one",
      selector.WeightOf(cheap) > selector.WeightOf(costly),
      StringPrintf("Cheap %.2f, Costly %.2f", selector.WeightOf(cheap),
                   selector.WeightOf(costly)));
}

void CheckAmplifyConfig(Checker* checker) {
  struct Case {
    const char* name;
    const char* strategy;
    const char* params;
    bool accepted;
    const char* expect;  // substring of the error message
  };
  const Case cases[] = {
      {"amplify: the default strategy is accepted", "flags", "{}", true, ""},
      {"amplify: none is accepted", "none", "{}", true, ""},
      {"amplify: rules are accepted", "flags",
       R"({"rules": [{"frames": ["Heap::"], "args": "--stress-compaction"}], "processes": 3, "min_hits": 2})",
       true, ""},
      {"amplify: unknown strategy is an error", "magic", "{}", false,
       "unknown amplify strategy"},
      {"amplify: unknown param is an error", "flags", R"({"retries": 3})",
       false, "unknown amplify param"},
      {"amplify: min_hits above processes is an error", "flags",
       R"({"processes": 3, "min_hits": 4})", false, "min_hits <= processes"},
      {"amplify: a rule without args is an error", "flags",
       R"({"rules": [{"frames": ["Heap::"]}]})", false, "needs \"frames\""},
  };
  for (const Case& c : cases) {
    JsonValue params;
    std::string error;
    JsonValue::Parse(c.params, &params, &error);
    const bool built =
        CreateAmplifyStrategy(c.strategy, params, &error) != nullptr;
    const bool ok = c.accepted
                        ? built
                        : (!built && error.find(c.expect) != std::string::npos);
    checker->Check(c.name, ok, built ? "accepted" : error);
  }
  checker->Check(
      "amplify: --stress-runs follows the run time within [2, max_loops]",
      StressRunsFor(215000, 10, 100) == 46 &&
          StressRunsFor(100, 10, 100) == 100 &&
          StressRunsFor(60000000, 10, 100) == 2,
      StringPrintf("%d, %d, %d", StressRunsFor(215000, 10, 100),
                   StressRunsFor(100, 10, 100),
                   StressRunsFor(60000000, 10, 100)));
}

// WrapInTry on a real script (Redline's own prelude): the result parses, and
// everything except a leading directive prologue is in one try statement.
void CheckWrapInTry(Checker* checker) {
  const std::string wrapped = WrapInTry(std::string(DefaultPrelude()));
  fast::ParseError error;
  std::unique_ptr<fast::Tree> tree =
      wrapped.empty()
          ? nullptr
          : fast::Parser::ParseScript(wrapped, fast::ParseOptions(), &error);
  size_t statements = 0;
  bool last_is_try = false;
  if (tree != nullptr && tree->root() != nullptr) {
    for (const fast::Statement* s : tree->root()->body) {
      const auto* e = s->As<fast::ExpressionStatement>();
      if (e == nullptr || !e->is_directive) {
        statements++;
      }
    }
    last_is_try =
        !tree->root()->body.empty() &&
        tree->root()->body.back()->As<fast::TryStatement>() != nullptr;
  }
  checker->Check(
      "amplify: WrapInTry puts the program into one try statement that parses",
      statements == 1 && last_is_try,
      tree == nullptr ? "does not parse: " + error.message
                      : StringPrintf("%zu top-level statements", statements));
  checker->Check("amplify: WrapInTry of an empty program is empty",
                 WrapInTry("").empty());
}

void CheckTunables(Checker* checker) {
  TunableRegistry& registry = TunableRegistry::Instance();
  registry.Reset();
  std::vector<const Tunable*> all = registry.All();
  checker->Check("tunables: the registry is populated", !all.empty(),
                 StringPrintf("%zu tunables declared", all.size()));
  if (all.empty()) {
    return;
  }
  const Tunable* t = all.front();
  std::string error;
  // A value inside the range is accepted and visible; the recorded
  // configuration lists it as non-default.
  double inside = t->min() == t->max() ? t->min() : (t->min() + t->max()) / 2;
  bool ok = registry.Set(t->name(), inside, &error) && (*t)() == inside;
  checker->Check("tunables: a value in range is set", ok,
                 error.empty() ? std::string(t->name()) : error);
  std::string json = registry.NonDefaultsJson();
  checker->Check(
      "tunables: changed values appear in the recorded configuration",
      inside == t->default_value() ||
          json.find(std::string(t->name())) != std::string::npos,
      json);
  registry.Reset();
  checker->Check("tunables: Reset restores the default",
                 (*t)() == t->default_value());
  ok = !registry.Set(t->name(), t->max() + 1, &error) &&
       error.find("outside its range") != std::string::npos;
  checker->Check("tunables: a value outside the range is an error", ok, error);
  ok = !registry.Set("generators.NoSuchThing.chance", 0.5, &error) &&
       error.find("unknown tunable") != std::string::npos;
  checker->Check("tunables: an unknown name is an error", ok, error);
  ok = registry.SetFromList(
           std::string(t->name()) + "=" + std::to_string(inside), &error) &&
       (*t)() == inside;
  checker->Check("tunables: --tunable=name=value parses", ok, error);
  registry.Reset();
  ok = !registry.SetFromList("nonsense", &error) &&
       error.find("NAME=VALUE") != std::string::npos;
  checker->Check("tunables: a malformed --tunable is an error", ok, error);
  // Through the config loader.
  JsonValue cfg;
  FuzzConfig config;
  std::string parse;
  JsonValue::Parse("{\"tunables\": {\"generators.NoSuchThing.chance\": 1}}",
                   &cfg, &parse);
  ok = !FuzzConfig::LoadJsonForTest(cfg, &config, &error) &&
       error.find("unknown tunable") != std::string::npos;
  checker->Check("tunables: unknown name in the config file is an error", ok,
                 error);
  registry.Reset();
}

// The engine options that used to be constants, inline --config JSON, and
// the full configuration dump: complete, loadable, identical after a round
// trip.
void CheckConfigDump(Checker* checker) {
  TunableRegistry::Instance().Reset();
  std::string error;
  // The engine knobs land in their fields.
  {
    JsonValue json;
    FuzzConfig config;
    JsonValue::Parse(
        R"({"introspect_timeout_ms": 5000, "crash_reruns_stop_after": 1,
                "stages": {"minimize": {"probe_timeout_factor": 2, "probe_timeout_extra_ms": 10,
                                        "probe_timeout_min_ms": 30, "verify_attempts": 5}}})",
        &json, &error);
    bool ok = FuzzConfig::LoadJsonForTest(json, &config, &error) &&
              config.introspect_timeout_ms == 5000 &&
              config.crash_reruns_stop_after == 1 &&
              config.probe_timeout_factor == 2 &&
              config.probe_timeout_extra_ms == 10 &&
              config.probe_timeout_min_ms == 30 && config.verify_attempts == 5;
    checker->Check("config: engine options are read from the JSON", ok, error);
  }
  // Inline JSON on the command line goes through the same reader.
  {
    const char* argv[] = {
        "redline", "fuzz",
        R"(--config={"stages": {"minimize": {"verify_attempts": 7}}})"};
    Flags flags = Flags::Parse(3, argv, 2);
    FuzzConfig config;
    bool ok = FuzzConfig::Load(flags, &config, &error, /*for_run=*/false) &&
              config.verify_attempts == 7;
    checker->Check("config: --config accepts inline JSON", ok, error);
    const char* bad[] = {"redline", "fuzz", "--config={\"stages\": nonsense}"};
    flags = Flags::Parse(3, bad, 2);
    ok = !FuzzConfig::Load(flags, &config, &error, /*for_run=*/false) &&
         error.find("--config:") != std::string::npos;
    checker->Check("config: malformed inline JSON is an error naming --config",
                   ok, error);
  }
  // The full dump: every tunable and every weight, and it round-trips.
  {
    error.clear();
    FuzzConfig config;
    std::string dump = config.ToJson(FuzzConfig::Detail::kFull);
    JsonValue json;
    bool ok = JsonValue::Parse(dump, &json, &error);
    size_t tunables = ok ? json.Get("tunables").as_object().size() : 0;
    size_t weights = ok ? json.Get("selection")
                              .Get("mutators")
                              .Get("params")
                              .Get("weights")
                              .as_object()
                              .size()
                        : 0;
    ok = ok && tunables == TunableRegistry::Instance().size() &&
         weights == MutatorSet::CreateDefault()->candidates().size() &&
         json.Has("introspect_timeout_ms") &&
         json.Get("stages").Get("minimize").Has("verify_attempts");
    checker->Check(
        "config: the full dump lists every tunable and every mutator weight",
        ok, StringPrintf("%zu tunables, %zu weights", tunables, weights));
    FuzzConfig again;
    ok = FuzzConfig::LoadJsonForTest(json, &again, &error) &&
         again.ToJson(FuzzConfig::Detail::kFull) == dump;
    checker->Check(
        "config: the full dump loads back to an identical configuration", ok,
        error);
    // The compact form only lists what differs from the defaults.
    JsonValue compact;
    ok = JsonValue::Parse(config.ToJson(), &compact, &error) &&
         compact.Get("tunables").as_object().empty();
    checker->Check(
        "config: the compact form lists no tunable when all are at their "
        "default",
        ok, error);
    // The pretty form is the same JSON.
    JsonValue pretty;
    ok = JsonValue::Parse(JsonPretty(dump), &pretty, &error) &&
         pretty.ToString() == json.ToString();
    checker->Check("json: the pretty form parses to the same value", ok, error);
  }
  {
    // Event logs must stay valid UTF-8: UTF-8 is kept, other bytes
    // (a lone surrogate from a string literal) become U+FFFD.
    const std::string quoted = JsonQuote(
        "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80|\xed\xa0\x80|\xff|\xc3");
    const std::string expect =
        "\"a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80|\\ufffd\\ufffd\\ufffd|\\ufffd|"
        "\\ufffd\"";
    checker->Check("json: strings are quoted as valid UTF-8", quoted == expect,
                   quoted);
  }
  TunableRegistry::Instance().Reset();
}

// The "init" and "accept" blocks and stages.minimize.reducers: values land
// in their fields, mistakes are reported.
void CheckStrategyBlocks(Checker* checker) {
  auto load = [](const std::string& text, FuzzConfig* config) -> std::string {
    JsonValue json;
    std::string error;
    if (!JsonValue::Parse(text, &json, &error)) {
      return "test bug, invalid JSON: " + error;
    }
    if (!FuzzConfig::LoadJsonForTest(json, config, &error)) {
      return error;
    }
    return FuzzConfig::ValidateReducers(config->minimizer_reducers, &error)
               ? ""
               : error;
  };
  {
    FuzzConfig config;
    std::string error = load(
        R"({"init": {"strategy": "import", "params": {"paths": ["a.js", "dir"], "root": "/r", "keep": "all"}},
                "accept": {"strategy": "new-edges"},
                "stages": {"minimize": {"reducers": ["truncate", "statements"]}}})",
        &config);
    bool ok = error.empty() &&
              config.imports == std::vector<std::string>{"a.js", "dir"} &&
              config.import_root == "/r" && config.import_all &&
              config.minimizer_reducers ==
                  std::vector<std::string>{"truncate", "statements"};
    checker->Check("init, accept, reducers: the blocks set their fields", ok,
                   error);
    error =
        load(R"({"init": {"strategy": "generate", "params": {"programs": 5}}})",
             &config);
    ok =
        error.empty() && config.imports.empty() && config.initial_programs == 5;
    checker->Check(
        "init: generate clears the imports and sets the program count", ok,
        error);
  }
  struct Case {
    const char* name;
    const char* json;
    const char* expect;
  };
  const Case cases[] = {
      {"init: unknown strategy is an error",
       R"({"init": {"strategy": "resume"}})", "unknown init strategy"},
      {"init: unknown parameter is an error",
       R"({"init": {"strategy": "generate", "params": {"paths": []}}})",
       "unknown parameter \"paths\""},
      {"init: bad keep value is an error",
       R"({"init": {"strategy": "import", "params": {"keep": "some"}}})",
       "keep must be"},
      {"accept: unknown criterion is an error",
       R"({"accept": {"strategy": "rarity"}})", "unknown accept strategy"},
      {"minimize: unknown reducer is an error",
       R"({"stages": {"minimize": {"reducers": ["statements", "shrink"]}}})",
       "unknown reducer \"shrink\""},
  };
  for (const Case& c : cases) {
    FuzzConfig config;
    std::string error = load(c.json, &config);
    checker->Check(c.name,
                   !error.empty() && error.find(c.expect) != std::string::npos,
                   error);
  }
}

// --- FAST node kinds --------------------------------------------------------

// Samples per generator for the node-kind check. Fixed seeds: the result
// only changes when a generator changes.
constexpr int kKindSamples = 40;

// The node kinds that no generator produces, with the reason. A kind that
// a generator starts to produce must leave this list (the check says so).
struct UngeneratedKind {
  const char* kind;
  const char* reason;
};
const UngeneratedKind kUngeneratedKinds[] = {
    {"Program", "the root of every tree, never inside generated code"},
    {"EmptyStatement", "`;` has no behaviour to test"},
    {"DebuggerStatement",
     "`debugger;` does nothing without an attached debugger, and d8 runs "
     "without one"},
    {"ImportExpression",
     "import() loads module files, and a fuzzed program is one script without "
     "modules"},
};

// Which node kinds appear in the code of any generator.
std::vector<bool> GeneratedKinds() {
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
  const Environment& env = Environment::Default();
  std::shared_ptr<Program> program = ParseOrDie(kProgram, "selftest");
  std::vector<bool> seen(fast::kNumNodeKinds, false);
  for (Generator* g : generators->candidates()) {
    for (int i = 0; i < kKindSamples; i++) {
      std::vector<fast::Statement*> out;
      // The statements live in the tree's arena: keep the tree.
      std::unique_ptr<fast::Tree> tree =
          GenerateSample(g, i, *program, *generators, env, &out);
      if (tree == nullptr) {
        continue;
      }
      for (fast::Statement* statement : out) {
        fast::Walk(statement, [&](fast::Node* node) {
          seen[static_cast<size_t>(node->kind)] = true;
          return true;
        });
      }
    }
  }
  return seen;
}

void CheckEdgeCases(Checker* checker) {
  // Lexer & Parser edge cases: invalid UTF-8, invalid numeric literals,
  // `for (await using of ...)` disambiguation, and deep expression chains.
  {
    fast::ParseOptions opts;
    fast::ParseError err;
    bool rejected_overlong =
        fast::Parser::ParseScript("let \xc0\xaf = 1;", opts, &err) == nullptr &&
        fast::Parser::ParseScript("let \xf4\x90\x80\x80 = 1;", opts, &err) ==
            nullptr;
    checker->Check(
        "lexer: overlong and out-of-range UTF-8 sequences are rejected",
        rejected_overlong, err.message);

    const char* bad_numbers[] = {
        "let x = 0_1;",  "let x = 07_7;", "let x = 0x1g;", "let x = 0b12;",
        "let x = 0o78;", "let x = 1n2;",  "let x = 08a;"};
    bool all_bad_rejected = true;
    for (const char* src : bad_numbers) {
      if (fast::Parser::ParseScript(src, opts, &err) != nullptr) {
        all_bad_rejected = false;
        break;
      }
    }
    checker->Check(
        "lexer: invalid numeric separators and trailing identifier chars are "
        "rejected",
        all_bad_rejected, err.message);

    bool for_await_using_ok =
        fast::Parser::ParseScript(
            "async function f(x) { for await (using of x) {} }", opts, &err) !=
            nullptr &&
        fast::Parser::ParseScript(
            "async function f(x) { for (await using y of x) {} }", opts,
            &err) != nullptr &&
        fast::Parser::ParseScript(
            "async function f(x) { for (await using of of x) {} }", opts,
            &err) != nullptr &&
        fast::Parser::ParseScript("for (using \\u006ff of []) {}", opts,
                                  &err) != nullptr;
    checker->Check("parser: for-await with 'using' identifier vs declaration",
                   for_await_using_ok, err.message);

    std::string deep = "let x = 0";
    for (int i = 0; i < 1300; i++) {
      deep += " + 1";
    }
    deep += ";";
    bool deep_rejected =
        fast::Parser::ParseScript(deep, opts, &err) == nullptr &&
        err.message.find("nesting") != std::string::npos;
    checker->Check("parser: excessive binary expression nesting is rejected",
                   deep_rejected, err.message);
  }
  // JSON and Random edge cases.
  {
    JsonValue val;
    std::string err;
    bool surrogate_not_swallowed =
        JsonValue::Parse("\"\\uD800\\u0041\"", &val, &err) &&
        val.as_string().size() == 4 && val.as_string().back() == 'A';
    JsonValue pos_huge, neg_huge;
    bool clamp_ok = JsonValue::Parse("1e100", &pos_huge, &err) &&
                    pos_huge.as_int() == std::numeric_limits<int64_t>::max() &&
                    JsonValue::Parse("-1e100", &neg_huge, &err) &&
                    neg_huge.as_int() == std::numeric_limits<int64_t>::min();
    checker->Check(
        "json: non-surrogate \\u after high surrogate and out-of-range "
        "as_int()",
        surrogate_not_swallowed && clamp_ok, err);

    Random rng(42);
    int64_t r = rng.Range(INT64_MIN, INT64_MAX);
    (void)r;
    std::vector<double> weights = {1.0, 0.0};
    bool weighted_ok = true;
    for (int i = 0; i < 32; i++) {
      if (rng.Weighted(weights) != 0) {
        weighted_ok = false;
      }
    }
    checker->Check(
        "random: full int64 Range and trailing zero weight in Weighted",
        weighted_ok);
  }
}

}  // namespace

void RunSelectionChecks(Checker* checker, const std::string& testdata_dir,
                        bool write_golden) {
  // Defaults everywhere before the traces are compared.
  TunableRegistry::Instance().Reset();
  CheckConfigErrors(checker);
  CheckStageConfig(checker);
  CheckStrategyBlocks(checker);
  const std::string dir = testdata_dir + "/selection";
  std::string error;
  if (write_golden && !MakeDirectories(dir, &error)) {
    checker->Check("selection: create golden directory", false, error);
    return;
  }
  std::unique_ptr<MutatorSet> mutators = MutatorSet::CreateDefault();
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
  const Environment& env = Environment::Default();

  CompareOrWrite(checker, "selection: mutator picks match the golden trace",
                 dir + "/mutators.trace", TraceMutatorPicks(*mutators),
                 write_golden);
  CompareOrWrite(checker, "selection: generator picks match the golden trace",
                 dir + "/generators.trace",
                 TraceGeneratorPicks(*generators, env), write_golden);
  CompareOrWrite(checker,
                 "selection: CodeGen insertions match the golden trace",
                 dir + "/codegen.trace",
                 TraceCodeGen(*mutators, *generators, env), write_golden);
  CompareOrWrite(
      checker, "components: every generator's output matches the golden trace",
      dir + "/generator_outputs.trace", TraceGeneratorOutputs(*generators, env),
      write_golden);
  CompareOrWrite(
      checker, "components: every mutator's output matches the golden trace",
      dir + "/mutator_outputs.trace",
      TraceMutatorOutputs(*mutators, *generators, env), write_golden);
  for (const std::string& name :
       {"uniform", "recency", "tournament", "productive"}) {
    std::string check = "selection: scheduler '";
    check += name;
    check += "' picks match the golden trace";
    std::string path = dir;
    path += "/scheduler_";
    path += name;
    path += ".trace";
    CompareOrWrite(checker, check, path, TraceScheduler(name), write_golden);
  }
  CheckTunables(checker);
  CheckConfigDump(checker);
  CheckAdaptiveWindow(checker);
  CheckAmplifyConfig(checker);
  CheckWrapInTry(checker);
  CheckEdgeCases(checker);
}

void RunNodeKindChecks(Checker* checker) {
  TunableRegistry::Instance().Reset();
  const std::vector<bool> seen = GeneratedKinds();
  std::vector<std::string> missing, stale;
  for (int i = 0; i < fast::kNumNodeKinds; i++) {
    const char* name = fast::NodeKindName(static_cast<fast::NodeKind>(i));
    bool listed = std::any_of(
        std::begin(kUngeneratedKinds), std::end(kUngeneratedKinds),
        [&](const UngeneratedKind& k) { return std::string(k.kind) == name; });
    if (!seen[static_cast<size_t>(i)] && !listed) {
      missing.emplace_back(name);
    }
    if (seen[static_cast<size_t>(i)] && listed) {
      stale.emplace_back(name);
    }
  }
  auto join = [](const std::vector<std::string>& names) {
    std::string out;
    for (const std::string& name : names) {
      out += (out.empty() ? "" : ", ") + name;
    }
    return out;
  };
  checker->Check("node kinds: every FAST node kind comes out of a generator",
                 missing.empty(),
                 missing.empty() ? StringPrintf("%d kinds", fast::kNumNodeKinds)
                                 : "no generator produces " + join(missing) +
                                       " (add a generator, or list the kind in "
                                       "kUngeneratedKinds with a reason)");
  checker->Check("node kinds: the list of ungenerated kinds is up to date",
                 stale.empty(),
                 stale.empty() ? ""
                               : "generators now produce " + join(stale) +
                                     ": remove them from kUngeneratedKinds");
}

void RunPresetChecks(Checker* checker) {
  const std::string dir = RedlineDataDir("presets");
  checker->Check("presets: locate experimental/redline/presets", !dir.empty(),
                 dir);
  if (dir.empty()) {
    return;
  }
  std::vector<std::string> files = ListFiles(dir, ".json");
  checker->Check("presets: the directory has presets", !files.empty(), dir);
  for (const std::string& path : files) {
    // The same path as `redline fuzz --preset=FILE`: the preset, then
    // the validation that every configuration goes through.
    std::string arg = "--preset=" + path;
    const char* argv[] = {"redline", "fuzz", arg.c_str()};
    Flags flags = Flags::Parse(3, argv, 2);
    FuzzConfig config;
    std::string error;
    bool ok = FuzzConfig::Load(flags, &config, &error, /*for_run=*/false);
    checker->Check(("presets: " + BaseName(path) + " loads").c_str(), ok,
                   error);
  }
  TunableRegistry::Instance().Reset();
}

}  // namespace redline
