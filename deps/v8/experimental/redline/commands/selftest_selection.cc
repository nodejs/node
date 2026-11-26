// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/commands/selftest_selection.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/json.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/v8_runtime.h"
#include "experimental/redline/core/adaptive_selector.h"
#include "experimental/redline/core/builder.h"
#include "experimental/redline/core/environment.h"
#include "experimental/redline/core/program.h"
#include "experimental/redline/core/random.h"
#include "experimental/redline/core/swarm.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/corpus/corpus.h"
#include "experimental/redline/engine/amplifier.h"
#include "experimental/redline/engine/config.h"
#include "experimental/redline/engine/fuzzer.h"
#include "experimental/redline/engine/prelude.h"
#include "experimental/redline/engine/stages.h"
#include "experimental/redline/fast/parser.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/generators/codegen.h"
#include "experimental/redline/generators/generator.h"
#include "experimental/redline/generators/values_generators.h"
#include "experimental/redline/generators/wasm_module.h"
#include "experimental/redline/mutators/explore.h"
#include "experimental/redline/mutators/fixup.h"
#include "experimental/redline/mutators/jit_compound.h"
#include "experimental/redline/mutators/mutator.h"
#include "experimental/redline/mutators/probing.h"

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
    Builder builder(tree.get(), &rng, &env);
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
  Builder builder(tree.get(), &rng, &env);
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

// Removes the spaces and tabs at the end of each line. Gerrit marks trailing
// whitespace, so the golden files must not have it.
std::string StripTrailingWhitespace(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  size_t start = 0;
  while (start < text.size()) {
    size_t newline = text.find('\n', start);
    size_t end = newline == std::string::npos ? text.size() : newline;
    size_t last = end;
    while (last > start && (text[last - 1] == ' ' || text[last - 1] == '\t')) {
      last--;
    }
    out.append(text, start, last - start);
    if (newline == std::string::npos) {
      break;
    }
    out += '\n';
    start = newline + 1;
  }
  return out;
}

void CompareOrWrite(Checker* checker, const std::string& check_name,
                    const std::string& path, const std::string& raw_trace,
                    bool write_golden) {
  const std::string trace = StripTrailingWhitespace(raw_trace);
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
                "stages": {"prefix": {"min_values": 3, "extra_values": 1, "from_scratch": false},
                           "mutate": {"consecutive": 2, "rounds": 1, "scratch_codegen": 2, "log": "events",
                                      "capture": {"on": "timeout,invalid", "mutators": ["Input"], "max": 3}},
                           "check": {"min_reruns": 2, "max_reruns": 8, "capture": {"on": ["flaky"]}},
                           "minimize": {"enabled": false, "max_tests": 12},
                           "publish": {"save_corpus": false, "stats": false}}})",
        &config);
    bool ok = error.empty() && config.prefix_min_values == 3 &&
              config.prefix_extra_values == 1 && !config.prefix_from_scratch &&
              config.consecutive_mutations == 2 &&
              config.mutation_rounds == 1 &&
              config.scratch_codegen_links == 2 &&
              config.stage(Stage::kMutate).events &&
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
        again.Get("stages").Get("mutate").GetInt("scratch_codegen", 0) == 2 &&
        !again.Get("stages").Get("prefix").GetBool("from_scratch", true) &&
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

// The picks of `pick` with a fixed seed, one index for each pick.
std::vector<size_t> PickSequence(const std::function<size_t(Random&)>& pick) {
  Random rng(kSeed);
  std::vector<size_t> picks;
  for (int i = 0; i < kMutatorPicks; i++) {
    picks.push_back(pick(rng));
  }
  return picks;
}

// Swarm testing (core/swarm.h): the mask draw, its composition with the
// strategies, and the configuration.
void CheckSwarm(Checker* checker) {
  TunableRegistry::Instance().Reset();
  std::unique_ptr<MutatorSet> mutators = MutatorSet::CreateDefault();
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
  // (a) Without a mask, and with a mask of factor 1, the picks do not change.
  {
    Selector<Mutator>& selector = mutators->selector();
    const auto& candidates = mutators->candidates();
    auto any = [](const Mutator&) { return true; };
    WeightMask ones;
    ones.factor.assign(candidates.size(), 1.0);
    const std::vector<size_t> plain = PickSequence(
        [&](Random& rng) { return selector.Pick(candidates, rng, any); });
    const std::vector<size_t> none = PickSequence([&](Random& rng) {
      return PickWithMask<Mutator>(selector, candidates, rng, any, nullptr);
    });
    const std::vector<size_t> masked = PickSequence([&](Random& rng) {
      return selector.PickMasked(candidates, rng, any, ones);
    });
    checker->Check(
        "swarm: without a mask or with factor 1, weighted picks do not change",
        plain == none && plain == masked);
    AdaptiveSelector<FakeComponent> adaptive("component", {}, {});
    adaptive.SetClockForTesting([] { return uint64_t{0}; });
    FakeComponent a{"A", 1, {}}, b{"B", 2, {}}, c{"C", 0.5, {}};
    const AdaptiveSelector<FakeComponent>::Candidates fakes = {&a, &b, &c};
    std::string error;
    adaptive.Initialize(fakes, &error);
    auto all = [](const FakeComponent&) { return true; };
    WeightMask fake_ones;
    fake_ones.factor.assign(fakes.size(), 1.0);
    checker->Check(
        "swarm: without a mask or with factor 1, adaptive picks do not change",
        PickSequence([&](Random& rng) {
          return adaptive.Pick(fakes, rng, all);
        }) == PickSequence([&](Random& rng) {
          return adaptive.PickMasked(fakes, rng, all, fake_ones);
        }));
  }
  // (b) The masks of worker 0 and of the other workers.
  SwarmCandidates gens, muts;
  std::string candidates_error;
  checker->Check("swarm: the core generators and the structural mutators exist",
                 BuildSwarmCandidates(*generators, *mutators, &gens, &muts,
                                      &candidates_error),
                 candidates_error);
  const SwarmSettings settings;
  const uint64_t seed = kSeed;
  {
    const SwarmMask zero = DrawSwarmMask(settings, gens, muts, seed, 0, 7);
    bool ones = zero.mutators.factor.empty() && zero.mutators.enable.empty();
    for (double f : zero.generators.factor) {
      ones = ones && f == 1.0;
    }
    checker->Check("swarm: worker 0 gets factor 1 for all generators", ones);
  }
  {
    size_t enabled = 0, removed = 0;
    bool core_kept = true, boost_in_range = true, min_kept = true;
    for (int worker = 1; worker <= 32; worker++) {
      const SwarmMask mask =
          DrawSwarmMask(settings, gens, muts, seed, worker, 0);
      min_kept =
          min_kept && mask.kept_generators >= settings.min_active_generators;
      for (size_t i = 0; i < gens.weights.size(); i++) {
        const double f = mask.generators.factor[i];
        if (gens.core[i]) {
          core_kept = core_kept && f == 1.0;
        } else if (gens.weights[i] > 0) {
          enabled++;
          removed += f == 0.0 ? 1 : 0;
          boost_in_range = boost_in_range &&
                           (f == 0.0 || (f >= 1.0 && f <= settings.max_boost));
        }
      }
    }
    const double removed_share =
        static_cast<double>(removed) / static_cast<double>(enabled);
    checker->Check("swarm: the masks keep the core generators", core_kept);
    checker->Check("swarm: kept generators get a factor in [1, max_boost]",
                   boost_in_range);
    checker->Check("swarm: the masks keep min_active_generators", min_kept);
    checker->Check(
        "swarm: the masks remove about 1 - keep_prob of the other generators",
        std::fabs(removed_share - (1 - settings.keep_prob)) < 0.05,
        StringPrintf("%.3f removed", removed_share));
    SwarmSettings few = settings;
    few.keep_prob = 0.1;
    few.min_active_generators = 40;
    const SwarmMask small = DrawSwarmMask(few, gens, muts, seed, 1, 0);
    checker->Check("swarm: a low keep_prob still keeps min_active_generators",
                   small.kept_generators >= 40,
                   StringPrintf("%d kept", small.kept_generators));
  }
  // (c) The mask is a function of (seed, worker, epoch).
  {
    auto draw = [&](uint64_t s, int worker, uint64_t epoch) {
      return DrawSwarmMask(settings, gens, muts, s, worker, epoch)
          .generators.factor;
    };
    checker->Check("swarm: the same seed, worker and epoch give the same mask",
                   draw(seed, 3, 5) == draw(seed, 3, 5));
    checker->Check("swarm: another epoch, worker or seed gives another mask",
                   draw(seed, 3, 5) != draw(seed, 3, 6) &&
                       draw(seed, 3, 5) != draw(seed, 4, 5) &&
                       draw(seed, 3, 5) != draw(seed + 1, 3, 5));
    int enabled_mutators = 0;
    bool structural_only = true;
    for (int worker = 1; worker <= 64; worker++) {
      const SwarmMask mask =
          DrawSwarmMask(settings, gens, muts, seed, worker, 0);
      if (mask.enabled_mutator >= 0) {
        const size_t i = static_cast<size_t>(mask.enabled_mutator);
        enabled_mutators++;
        structural_only =
            structural_only && muts.structural[i] && muts.weights[i] <= 0;
      }
    }
    checker->Check(
        "swarm: some masks turn on one structural mutator with weight 0",
        enabled_mutators > 0 && enabled_mutators < 64 && structural_only,
        StringPrintf("%d of 64 masks", enabled_mutators));
  }
  // (d) The mask multiplies the weights of the adaptive strategy.
  {
    AdaptiveSelector<FakeComponent> adaptive("component", {}, {});
    adaptive.SetClockForTesting([] { return uint64_t{0}; });
    FakeComponent a{"A", 1, {}}, b{"B", 1, {}}, c{"C", 1, {}},
        off{"Off", 0, {}};
    const AdaptiveSelector<FakeComponent>::Candidates fakes = {&a, &b, &c,
                                                               &off};
    std::string error;
    adaptive.Initialize(fakes, &error);
    auto all = [](const FakeComponent&) { return true; };
    WeightMask mask;
    mask.factor = {0.0, 1.0, 3.0, 1.0};
    int counts[4] = {0, 0, 0, 0};
    for (size_t pick : PickSequence([&](Random& rng) {
           return adaptive.PickMasked(fakes, rng, all, mask);
         })) {
      counts[pick]++;
    }
    const double ratio = static_cast<double>(counts[2]) / counts[1];
    checker->Check(
        "swarm: the mask removes factor 0 and multiplies the adaptive weights",
        counts[0] == 0 && counts[3] == 0 && ratio > 2.4 && ratio < 3.7,
        StringPrintf("A %d, B %d, C %d, Off %d", counts[0], counts[1],
                     counts[2], counts[3]));
    mask.enable = {0.0, 0.0, 0.0, 1.0};
    int enabled = 0;
    for (size_t pick : PickSequence([&](Random& rng) {
           return adaptive.PickMasked(fakes, rng, all, mask);
         })) {
      enabled += pick == 3 ? 1 : 0;
    }
    checker->Check("swarm: the mask turns on a candidate with weight 0",
                   enabled > 0, StringPrintf("%d picks", enabled));
  }
  // (f) The pick functions of the mutator and generator sets use the active
  // mask of the thread.
  {
    const Environment& env = Environment::Default();
    const std::string plain_generators = TraceGeneratorPicks(*generators, env);
    const std::string plain_mutators = TraceMutatorPicks(*mutators);
    SwarmMask mask;
    mask.generators.factor.assign(gens.weights.size(), 0.0);
    std::set<std::string, std::less<>> removed;
    for (size_t i = 0; i < gens.weights.size(); i++) {
      if (gens.core[i]) {
        mask.generators.factor[i] = 1.0;
      } else {
        removed.insert(generators->candidates()[i]->name());
      }
    }
    mask.mutators.factor.assign(muts.weights.size(), 1.0);
    mask.mutators.enable.assign(muts.weights.size(), 0.0);
    for (size_t i = 0; i < muts.weights.size(); i++) {
      if (mutators->candidates()[i]->name() == "Wrap") {
        mask.mutators.enable[i] = 1.0;
      }
    }
    SetActiveSwarmMask(&mask);
    const std::string masked_generators =
        TraceGeneratorPicks(*generators, env) +
        TraceCodeGen(*mutators, *generators, env);
    const std::string masked_mutators = TraceMutatorPicks(*mutators);
    SetActiveSwarmMask(nullptr);
    // The names in the traces: the words between tabs, spaces, newlines and
    // '+'.
    std::string found;
    size_t start = 0;
    for (size_t i = 0; i <= masked_generators.size(); i++) {
      if (i == masked_generators.size() ||
          std::string_view("\t\n +").find(masked_generators[i]) !=
              std::string_view::npos) {
        const std::string word = masked_generators.substr(start, i - start);
        if (removed.count(word) != 0 && found.empty()) {
          found = word;
        }
        start = i + 1;
      }
    }
    checker->Check(
        "swarm: GeneratorSet::Pick and CodeGen use only the generators of the "
        "active mask",
        found.empty(), found);
    checker->Check(
        "swarm: MutatorSet::Pick uses the mutator that the active mask turns "
        "on",
        masked_mutators.find("\tWrap\n") != std::string::npos);
    checker->Check("swarm: without an active mask, the picks do not change",
                   TraceGeneratorPicks(*generators, env) == plain_generators &&
                       TraceMutatorPicks(*mutators) == plain_mutators &&
                       plain_generators != masked_generators.substr(
                                               0, plain_generators.size()));
  }
  // (e) The configuration: JSON, flags and the full dump.
  {
    auto load = [](std::initializer_list<const char*> args, FuzzConfig* config,
                   std::string* error) {
      std::vector<const char*> argv = {"redline", "fuzz"};
      argv.insert(argv.end(), args.begin(), args.end());
      Flags flags = Flags::Parse(static_cast<int>(argv.size()), argv.data(), 2);
      return FuzzConfig::Load(flags, config, error, /*for_run=*/false);
    };
    std::string error;
    FuzzConfig plain, on, json_on, json_off;
    bool ok = load({}, &plain, &error) && !plain.selection.swarm &&
              load({"--swarm"}, &on, &error) && on.selection.swarm &&
              load({R"(--config={"selection": {"swarm": true}})"}, &json_on,
                   &error) &&
              json_on.selection.swarm &&
              load({R"(--config={"selection": {"swarm": true}})", "--no-swarm"},
                   &json_off, &error) &&
              !json_off.selection.swarm;
    checker->Check(
        "swarm: selection.swarm is off by default, and the JSON, --swarm and "
        "--no-swarm set it",
        ok, error);
    FuzzConfig bad;
    ok = !load({R"(--config={"selection": {"swarm": 1}})"}, &bad, &error) &&
         error.find("selection.swarm") != std::string::npos;
    checker->Check("swarm: a selection.swarm that is not a boolean is an error",
                   ok, error);
    JsonValue dump;
    FuzzConfig again;
    error.clear();
    ok =
        JsonValue::Parse(on.ToJson(FuzzConfig::Detail::kFull), &dump, &error) &&
        dump.Get("selection").Get("swarm").is_bool() &&
        dump.Get("selection").Get("swarm").as_bool() &&
        dump.Get("tunables").Has("selection.swarm.epoch_execs") &&
        FuzzConfig::LoadJsonForTest(dump, &again, &error) &&
        again.selection.swarm;
    checker->Check(
        "swarm: the full dump gives selection.swarm and its tunables, and "
        "loads back",
        ok, error);
    const SwarmSettings tuned = SwarmSettings::FromTunables();
    checker->Check(
        "swarm: the selection.swarm.* tunables give the default settings",
        tuned.epoch_execs == settings.epoch_execs &&
            tuned.keep_prob == settings.keep_prob &&
            tuned.min_active_generators == settings.min_active_generators &&
            tuned.max_boost == settings.max_boost &&
            tuned.mutator_variation == settings.mutator_variation);
  }
  TunableRegistry::Instance().Reset();
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

// A --config flag with the two d8 keys, for the checks of the d8 flags.
constexpr const char* kD8ArgsConfig =
    R"(--config={"d8_args": ["--file-a"], "d8_extra_args": ["--extra-a"]})";

// Loads the configuration of `redline fuzz ARGS` and returns its d8 flags,
// with a space between two flags. If the configuration is not correct, it
// returns the error.
std::string LoadedD8Args(std::initializer_list<std::string> args) {
  std::vector<const char*> argv = {"redline", "fuzz"};
  for (const std::string& arg : args) {
    argv.push_back(arg.c_str());
  }
  Flags flags = Flags::Parse(static_cast<int>(argv.size()), argv.data(), 2);
  FuzzConfig config;
  std::string error;
  if (!FuzzConfig::Load(flags, &config, &error, /*for_run=*/false)) {
    return "error: " + error;
  }
  std::string joined;
  for (const std::string& arg : config.target.args) {
    joined += (joined.empty() ? "" : " ") + arg;
  }
  return joined;
}

// Loads the configuration of `redline fuzz --config=JSON` with all steps of
// FuzzConfig::Load, also the validation. Returns the error, or empty.
std::string LoadFullConfig(const std::string& json, FuzzConfig* config) {
  const std::string arg = "--config=" + json;
  std::vector<const char*> argv = {"redline", "fuzz", arg.c_str()};
  Flags flags = Flags::Parse(static_cast<int>(argv.size()), argv.data(), 2);
  std::string error;
  FuzzConfig::Load(flags, config, &error, /*for_run=*/false);
  TunableRegistry::Instance().Reset();
  return error;
}

// The flag --d8-args replaces the d8_args key of the configuration. The
// d8_extra_args key of the configuration stays after the flags of --d8-args.
void CheckD8ArgLayers(Checker* checker) {
  std::string got = LoadedD8Args({kD8ArgsConfig, "--d8-args=--base-a --base-b",
                                  "--d8-extra-args=--extra-b"});
  checker->Check("config: --d8-args keeps the d8_extra_args of --config",
                 got == "--base-a --base-b --extra-a --extra-b", got);
  TunableRegistry::Instance().Reset();
}

// The keys of the check and the minimizer that lower the cost of a commit:
// import_reruns, expressions_share and probe_timeout_tighten_*. The JSON and
// the flags set them, the dump gives them, and the loader clamps them.
void CheckCommitCostConfig(Checker* checker) {
  {
    const FuzzConfig defaults;
    const MinimizerOptions raw;
    checker->Check(
        "config: the defaults give 1 import rerun, adaptive slow-program "
        "minimization, a bounded `expressions` share and a probe timeout that "
        "tightens",
        defaults.import_reruns == 1 && raw.target_time_ms == 0 &&
            raw.expressions_share == 1.0 &&
            defaults.minimizer.target_time_ms == 1500 &&
            defaults.minimizer.min_tests == 16 &&
            defaults.minimizer.expressions_share == 0.15 &&
            defaults.crash_minimizer.target_time_ms == 0 &&
            defaults.crash_minimizer.expressions_share == 1.0 &&
            defaults.probe_timeout_tighten_factor == 2.5 &&
            defaults.probe_timeout_tighten_extra_ms == 50);
  }
  FuzzConfig config;
  const std::string error = LoadFullConfig(
      R"({"stages": {"check": {"import_reruns": 3},
                     "minimize": {"expressions_share": 0.5,
                                  "probe_timeout_tighten_factor": 3,
                                  "probe_timeout_tighten_extra_ms": 10}},
          "crash_minimizer": {"expressions_share": 0.75}})",
      &config);
  checker->Check("config: the JSON sets the keys of the commit cost",
                 error.empty() && config.import_reruns == 3 &&
                     config.minimizer.expressions_share == 0.5 &&
                     config.crash_minimizer.expressions_share == 0.75 &&
                     config.probe_timeout_tighten_factor == 3 &&
                     config.probe_timeout_tighten_extra_ms == 10,
                 error);
  // The dump gives the values, and it loads back to the same configuration.
  const std::string dump = config.ToJson(FuzzConfig::Detail::kFull);
  JsonValue json;
  std::string parse_error;
  bool ok = JsonValue::Parse(dump, &json, &parse_error);
  const JsonValue& stages = json.Get("stages");
  ok = ok && stages.Get("check").GetInt("import_reruns", 0) == 3 &&
       stages.Get("minimize").GetNumber("expressions_share", 0) == 0.5 &&
       stages.Get("minimize").GetNumber("probe_timeout_tighten_factor", 0) ==
           3 &&
       stages.Get("minimize").GetNumber("probe_timeout_tighten_extra_ms", 0) ==
           10 &&
       json.Get("crash_minimizer").GetNumber("expressions_share", 0) == 0.75;
  FuzzConfig again;
  ok = ok && FuzzConfig::LoadJsonForTest(json, &again, &parse_error) &&
       again.ToJson(FuzzConfig::Detail::kFull) == dump;
  TunableRegistry::Instance().Reset();
  checker->Check(
      "config: the dump gives the keys of the commit cost and loads back", ok,
      parse_error);
  // The flag, and the clamps of the loader.
  {
    const char* argv[] = {"redline", "fuzz", "--import-reruns=2"};
    Flags flags = Flags::Parse(3, argv, 2);
    FuzzConfig flagged;
    std::string flag_error;
    const bool loaded =
        FuzzConfig::Load(flags, &flagged, &flag_error, /*for_run=*/false);
    TunableRegistry::Instance().Reset();
    checker->Check("config: --import-reruns sets import_reruns",
                   loaded && flagged.import_reruns == 2, flag_error);
  }
  FuzzConfig clamped;
  const std::string clamp_error = LoadFullConfig(
      R"({"stages": {"check": {"import_reruns": 500, "max_reruns": 20},
                     "minimize": {"max_tests": -5, "max_time_ms": -10,
                                  "max_rounds": -2, "target_time_ms": -1,
                                  "min_tests": -3, "expressions_share": 7,
                                  "probe_timeout_tighten_factor": -1}},
          "crash_minimizer": {"target_time_ms": -7,
                              "expressions_share": -2}})",
      &clamped);
  FuzzConfig low;
  const std::string low_error =
      LoadFullConfig(R"({"stages": {"check": {"import_reruns": 0}}})", &low);
  FuzzConfig neg_flags;
  std::string neg_flag_error;
  {
    const char* argv[] = {"redline",
                          "fuzz",
                          "--minimizer-target-time-ms=-1",
                          "--minimizer-max-tests=-5",
                          "--minimizer-max-time-ms=-2",
                          "--crash-minimizer-max-tests=-3",
                          "--crash-minimizer-max-time-ms=-4"};
    Flags flags = Flags::Parse(7, argv, 2);
    FuzzConfig::Load(flags, &neg_flags, &neg_flag_error, /*for_run=*/false);
    TunableRegistry::Instance().Reset();
  }
  checker->Check(
      "config: the loader clamps import_reruns, minimizer limits, "
      "expressions_share and the tighten factor",
      clamp_error.empty() && low_error.empty() && neg_flag_error.empty() &&
          clamped.import_reruns == 20 && clamped.minimizer.max_tests == 0 &&
          clamped.minimizer.max_time_ms == 0 &&
          clamped.minimizer.max_rounds == 0 &&
          clamped.minimizer.target_time_ms == 0 &&
          clamped.minimizer.min_tests == 0 &&
          clamped.minimizer.expressions_share == 1.0 &&
          clamped.crash_minimizer.target_time_ms == 0 &&
          clamped.crash_minimizer.expressions_share == 0.0 &&
          clamped.probe_timeout_tighten_factor == 0.0 &&
          neg_flags.minimizer.target_time_ms == 0 &&
          neg_flags.minimizer.max_tests == 0 &&
          neg_flags.minimizer.max_time_ms == 0 &&
          neg_flags.crash_minimizer.max_tests == 0 &&
          neg_flags.crash_minimizer.max_time_ms == 0 &&
          low.import_reruns == low.min_stable_reruns,
      clamp_error + low_error + neg_flag_error);
}

// The "init" and "accept" blocks, stages.minimize.reducers and
// crash_minimizer.reducers: the values go to their fields, and the loader
// reports the mistakes.
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
    return FuzzConfig::ValidateReducers(config->minimizer_reducers,
                                        "stages.minimize.reducers", &error) &&
                   FuzzConfig::ValidateReducers(
                       config->crash_minimizer_reducers,
                       "crash_minimizer.reducers", &error)
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
                  std::vector<std::string>{"truncate", "statements"} &&
              config.crash_minimizer_reducers.empty();
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
  {
    // The crash minimizer has its own reducer order. The dump gives the
    // default order when the configuration gives none.
    FuzzConfig config;
    std::string error = load(
        R"({"crash_minimizer": {"max_rounds": 7, "reducers": ["unwrap", "dataflow"]}})",
        &config);
    JsonValue dumped;
    std::string parse_error;
    FuzzConfig defaults;
    const bool dumped_ok =
        load("{}", &defaults).empty() &&
        JsonValue::Parse(defaults.ToJson(), &dumped, &parse_error);
    const JsonValue& order = dumped.Get("crash_minimizer").Get("reducers");
    std::vector<std::string> dumped_order;
    if (order.is_array()) {
      for (const JsonValue& name : order.as_array()) {
        dumped_order.push_back(name.as_string());
      }
    }
    const bool ok = error.empty() && config.crash_minimizer.max_rounds == 7 &&
                    config.crash_minimizer_reducers ==
                        std::vector<std::string>{"unwrap", "dataflow"} &&
                    config.minimizer_reducers.empty() && dumped_ok &&
                    dumped_order == std::vector<std::string>(
                                        Minimizer::CrashOrder().begin(),
                                        Minimizer::CrashOrder().end());
    checker->Check(
        "crash_minimizer: the block sets the reducers, the dump gives the "
        "default order",
        ok, error.empty() ? parse_error : error);
    // The dump of the order that is not the default loads back to the same
    // order. An empty list gives the default order, as for
    // stages.minimize.reducers.
    FuzzConfig again;
    std::string again_error = load(config.ToJson(), &again);
    FuzzConfig empty;
    if (again_error.empty()) {
      again_error = load(R"({"crash_minimizer": {"reducers": []}})", &empty);
    }
    checker->Check(
        "crash_minimizer: the dumped reducers load back, an empty list gives "
        "the default order",
        again_error.empty() && again.crash_minimizer.max_rounds == 7 &&
            again.crash_minimizer_reducers ==
                std::vector<std::string>{"unwrap", "dataflow"} &&
            empty.crash_minimizer_reducers.empty(),
        again_error);
  }
  {
    // The keys of the flag search and the time limits of the crash
    // minimizers. FuzzConfig::Load changes values out of range to the
    // nearest valid values. Values that are not the default load back from
    // the dump.
    FuzzConfig low, high, again;
    std::string error = LoadFullConfig(
        R"({"crash_flag_runs": 3, "crash_flag_processes": 2, "crash_flag_budget_sec": -3})",
        &low);
    if (error.empty()) {
      error = LoadFullConfig(
          R"({"crash_flag_runs": 0, "crash_flag_processes": -5, "crash_flag_budget_sec": 20, "crash_amplified_budget_sec": -1, "crash_bundle_min_budget_sec": -1})",
          &high);
    }
    if (error.empty()) {
      error = load(high.ToJson(), &again);
    }
    checker->Check(
        "crash flags: the keys of the flag search",
        error.empty() && low.crash_flag_runs == 3 &&
            low.crash_flag_processes == 3 && low.crash_flag_budget_sec == 0 &&
            high.crash_flag_runs == 1 && high.crash_flag_processes == 0 &&
            again.crash_flag_runs == 1 && again.crash_flag_processes == 0 &&
            again.crash_flag_budget_sec == 20 &&
            high.crash_amplified_budget_sec == 0 &&
            again.crash_amplified_budget_sec == 0 &&
            low.crash_amplified_budget_sec == 300 &&
            high.crash_bundle_min_budget_sec == 0 &&
            again.crash_bundle_min_budget_sec == 0,
        StringPrintf("%s runs %d/%d, processes %d/%d, budget %g/%g, "
                     "amplified budget %g/%g/%g, bundle budget %g",
                     error.c_str(), low.crash_flag_runs, high.crash_flag_runs,
                     low.crash_flag_processes, high.crash_flag_processes,
                     low.crash_flag_budget_sec, again.crash_flag_budget_sec,
                     low.crash_amplified_budget_sec,
                     high.crash_amplified_budget_sec,
                     again.crash_amplified_budget_sec,
                     again.crash_bundle_min_budget_sec));
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
       "unknown reducer \"shrink\" in stages.minimize.reducers"},
      {"crash_minimizer: unknown reducer is an error",
       R"({"crash_minimizer": {"reducers": ["dataflow", "shrink"]}})",
       "unknown reducer \"shrink\" in crash_minimizer.reducers"},
      {"crash_minimizer: reducers that are not a list are an error",
       R"({"crash_minimizer": {"reducers": "dataflow"}})",
       "crash_minimizer.reducers must be a list of reducer names"},
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

// True if the code of `root` reads or writes the variable `name`. A
// property name is not a use of a variable.
bool UsesVariable(fast::Node* root, std::string_view name) {
  bool found = false;
  fast::Walk(root, [&](fast::Node* node) {
    if (auto* member = node->As<fast::MemberExpression>();
        member != nullptr && !member->computed) {
      found = found || UsesVariable(member->object, name);
      return false;
    }
    if (auto* property = node->As<fast::Property>();
        property != nullptr && !property->computed) {
      found = found || UsesVariable(property->value, name);
      return false;
    }
    if (auto* field = node->As<fast::PropertyDefinition>();
        field != nullptr && !field->computed) {
      found = found || UsesVariable(field->value, name);
      return false;
    }
    if (auto* id = node->As<fast::Identifier>()) {
      found = found || id->name == name;
    }
    return true;
  });
  return found;
}

// True if an if statement or a for statement in `root` contains another if
// statement or for statement.
bool HasNestedBlock(fast::Node* root) {
  auto is_block = [](fast::Node* node) {
    return node->As<fast::IfStatement>() != nullptr ||
           node->As<fast::ForStatement>() != nullptr;
  };
  bool found = false;
  fast::Walk(root, [&](fast::Node* node) {
    if (!is_block(node)) {
      return true;
    }
    fast::ForEachChild(node, [&](fast::Node* child) {
      fast::Walk(child, [&](fast::Node* inner) {
        found = found || is_block(inner);
        return !found;
      });
    });
    return false;
  });
  return found;
}

// Runs ObjectFlow `samples` times and parses each program. A block of the
// program must not contain a block. At the top level, the program is the
// output of ObjectFlow. In a function, the program is `function g(u) {...}`.
// The generator then sees u, a variable of unknown type, and in half of the
// samples `arguments` with the type object. The initializer of a new object
// must not use u. No code can use `arguments`: in the function of
// ObjectFlow, it is a different object.
bool CheckObjectFlowSamples(const GeneratorSet& generators, bool in_function,
                            int samples, std::string* error) {
  Generator* object_flow = generators.Find("ObjectFlow");
  if (object_flow == nullptr) {
    *error = "no generator ObjectFlow";
    return false;
  }
  const Environment& env = Environment::Default();
  Random rng(kSeed);
  for (int i = 0; i < samples; i++) {
    auto tree = std::make_unique<fast::Tree>();
    tree->set_root(tree->New<fast::Program>());
    Builder builder(tree.get(), &rng);
    GenContext ctx(builder, generators, env);
    if (in_function) {
      ctx.syntax.in_function = true;
      ctx.syntax.has_arguments = true;
      ctx.Declare("u", JsType::kUnknown);
      if (i % 2 == 0) {
        ctx.Declare("arguments", JsType::kObject);
      }
    }
    std::vector<fast::Statement*> out;
    ctx.out = &out;
    if (!object_flow->Applicable(ctx) || !object_flow->Generate(ctx, &out)) {
      *error = StringPrintf("sample %d: ObjectFlow made no code", i);
      return false;
    }
    for (fast::Statement* statement : out) {
      auto* declaration = statement->As<fast::VariableDeclaration>();
      if (in_function && declaration != nullptr &&
          declaration->declaration_kind == fast::VariableKind::kConst &&
          UsesVariable(declaration, "u")) {
        *error = StringPrintf("sample %d: a new object uses u: %s", i,
                              fast::Print(statement).c_str());
        return false;
      }
      if (UsesVariable(statement, "arguments")) {
        *error = StringPrintf("sample %d: the code uses arguments: %s", i,
                              fast::Print(statement).c_str());
        return false;
      }
      if (HasNestedBlock(statement)) {
        *error = StringPrintf("sample %d: a block contains a block: %s", i,
                              fast::Print(statement).c_str());
        return false;
      }
      tree->root()->body.push_back(statement);
    }
    std::string source = fast::Print(tree->root());
    if (in_function) {
      source = "function g(u) {\n" + source + "}\n";
    }
    fast::ParseError parse_error;
    if (fast::Parser::ParseScript(source, fast::ParseOptions(), &parse_error) ==
        nullptr) {
      *error = StringPrintf("sample %d does not parse: %s", i,
                            parse_error.message.c_str());
      return false;
    }
  }
  return true;
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
  // CodeGen recursion cap under adversarial tunable settings.
  {
    TunableRegistry& reg = TunableRegistry::Instance();
    std::string err;
    bool set_ok = reg.Set("codegen.argument.reuse_chance", 0.0, &err) &&
                  reg.Set("codegen.value.variable_chance", 0.0, &err) &&
                  reg.Set("codegen.argument.optional_stop_chance", 0.0, &err) &&
                  reg.Set("codegen.expression.max_depth", 16.0, &err) &&
                  reg.Set("codegen.argument.max_depth", 16.0, &err) &&
                  reg.Set("codegen.max_expr_depth", 10.0, &err);
    std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
    const Environment& env = Environment::Default();
    Random rng(12345);
    bool depth_ok = set_ok;
    for (int i = 0; i < 300 && depth_ok; i++) {
      auto tree = std::make_unique<fast::Tree>();
      tree->set_root(tree->New<fast::Program>());
      Builder builder(tree.get(), &rng, &env);
      GenContext ctx(builder, *generators, env);
      CodeGen cg(ctx);
      fast::Expression* e1 = cg.Expression(0);
      fast::Expression* e2 = cg.BuiltinCall();
      fast::Expression* e3 = cg.Construct();
      if (ctx.expr_depth != 0 || e1 == nullptr || e2 == nullptr ||
          e3 == nullptr) {
        depth_ok = false;
        break;
      }
      tree->root()->body.push_back(cg.Stmt(e1));
      tree->root()->body.push_back(cg.Stmt(e2));
      tree->root()->body.push_back(cg.Stmt(e3));
      fast::ParseError parse_err;
      if (fast::Parser::ParseScript(fast::Print(tree->root()),
                                    fast::ParseOptions(),
                                    &parse_err) == nullptr) {
        err = parse_err.message;
        depth_ok = false;
      }
    }
    reg.Reset();
    checker->Check("codegen: expression recursion depth stays bounded",
                   depth_ok, err);
  }
  // ObjectFlow with the weight 0 for each step that is not a block. The
  // blocks then contain loads, not more blocks.
  {
    TunableRegistry& reg = TunableRegistry::Instance();
    std::string err;
    bool ok = true;
    for (const char* step :
         {"store", "load", "update", "rebind", "accessor", "escape"}) {
      ok = ok && reg.Set(StringPrintf("generators.ObjectFlow.%s_weight", step),
                         0.0, &err);
    }
    std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
    ok = ok &&
         CheckObjectFlowSamples(*generators, /*in_function=*/false, 200, &err);
    reg.Reset();
    checker->Check("ObjectFlow: steps of weight 0 do not make nested blocks",
                   ok, err);
  }
  // ObjectFlow in a function with visible variables that are not objects of
  // ObjectFlow.
  {
    TunableRegistry& reg = TunableRegistry::Instance();
    std::string err;
    bool ok = reg.Set("generators.ObjectFlow.max_outer_objects", 8.0, &err) &&
              reg.Set("generators.ObjectFlow.visible_object_chance", 0.5, &err);
    std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
    ok = ok &&
         CheckObjectFlowSamples(*generators, /*in_function=*/true, 200, &err);
    reg.Reset();
    checker->Check(
        "ObjectFlow: new objects do not use visible variables, and no code "
        "uses arguments",
        ok, err);
  }
}

// Checks that `OsrLoop` always sets its trigger iteration strictly below the
// loop bound so the OSR native call can fire.
void CheckOsrLoopTrigger(Checker* checker) {
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
  Generator* osr_loop = generators->Find("OsrLoop");
  const Environment& env = Environment::Default();
  auto run_samples = [&](int samples, bool require_positive,
                         std::string* error) -> bool {
    if (osr_loop == nullptr) {
      *error = "no generator OsrLoop";
      return false;
    }
    Random rng(kSeed + 101);
    for (int i = 0; i < samples; i++) {
      auto tree = std::make_unique<fast::Tree>();
      tree->set_root(tree->New<fast::Program>());
      Builder builder(tree.get(), &rng, &env);
      GenContext ctx(builder, *generators, env);
      std::vector<fast::Statement*> out;
      ctx.out = &out;
      if (!osr_loop->Generate(ctx, &out) || out.empty()) {
        *error = StringPrintf("sample %d: OsrLoop made no code", i);
        return false;
      }
      fast::ForStatement* loop = nullptr;
      for (fast::Statement* s : out) {
        if (auto* fl = s->As<fast::ForStatement>()) {
          loop = fl;
          break;
        }
        if (auto* fn = s->As<fast::FunctionDeclaration>();
            fn != nullptr && fn->body != nullptr && !fn->body->body.empty()) {
          loop = fn->body->body.front()->As<fast::ForStatement>();
          break;
        }
      }
      if (loop == nullptr || loop->test == nullptr || loop->body == nullptr) {
        *error = StringPrintf("sample %d: missing OSR for loop", i);
        return false;
      }
      auto* test = loop->test->As<fast::BinaryExpression>();
      auto* bound_lit =
          test != nullptr ? test->right->As<fast::NumericLiteral>() : nullptr;
      auto* body = loop->body->As<fast::BlockStatement>();
      auto* guard = body != nullptr && !body->body.empty()
                        ? body->body.front()->As<fast::IfStatement>()
                        : nullptr;
      auto* eq = guard != nullptr ? guard->test->As<fast::BinaryExpression>()
                                  : nullptr;
      auto* k_lit =
          eq != nullptr ? eq->right->As<fast::NumericLiteral>() : nullptr;
      if (bound_lit == nullptr || k_lit == nullptr) {
        *error = StringPrintf("sample %d: unexpected OSR loop shape", i);
        return false;
      }
      const double bound = bound_lit->value;
      const double k = k_lit->value;
      const double min_k = require_positive ? 1.0 : 0.0;
      if (!(k >= min_k && k < bound)) {
        *error = StringPrintf("sample %d: trigger %g not in [%g, %g)", i, k,
                              min_k, bound);
        return false;
      }
    }
    return true;
  };
  TunableRegistry::Instance().Reset();
  std::string err;
  bool ok = run_samples(200, /*require_positive=*/true, &err);
  checker->Check("OsrLoop: trigger iteration is always below loop bound", ok,
                 err);
  std::string clamp_err;
  bool clamp_ok =
      TunableRegistry::Instance().Set(
          "generators.OsrLoop.min_trigger_iteration", 20.0, &clamp_err) &&
      run_samples(40, /*require_positive=*/false, &clamp_err);
  TunableRegistry::Instance().Reset();
  checker->Check(
      "OsrLoop: trigger iteration clamps below bound when min exceeds bound",
      clamp_ok, clamp_err);
}

// Checks `Builder::SetFocusSubtree`, `PickFocusTarget`, `ApplyFocusTierUp`,
// and the `stages.mutate.focus_chain` setting across presets.
void CheckFocusChain(Checker* checker) {
  std::unique_ptr<MutatorSet> mutators = MutatorSet::CreateDefault();
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
  const Environment& env = Environment::Default();

  const char* kTwoFunctions = R"(let g1 = 1 + 2;
let g2 = {p: 1, q: 2};
let g3 = [10, 20];
function other(a, b) {
  let u = a + b;
  return u * 2;
}
function target(x, y) {
  let z = x + y;
  return z + 1;
}
let r0 = other(g1, 3);
let r1 = target(g1, 4);
)";
  ProgramPtr program = ParseOrDie(kTwoFunctions, "selftest");
  auto find_fn = [](fast::Program* root,
                    std::string_view name) -> fast::FunctionDeclaration* {
    for (fast::Statement* s : root->body) {
      if (auto* fn = s->As<fast::FunctionDeclaration>();
          fn != nullptr && fn->name != nullptr && fn->name->name == name) {
        return fn;
      }
    }
    return nullptr;
  };
  fast::FunctionDeclaration* orig_target =
      find_fn(program->tree().root(), "target");
  const uint32_t target_body_id =
      orig_target != nullptr && orig_target->body != nullptr
          ? orig_target->body->id
          : 0;
  std::vector<std::string> orig_outside;
  for (fast::Statement* s : program->tree().root()->body) {
    if (s != orig_target) {
      orig_outside.push_back(fast::Print(s));
    }
  }
  const std::string orig_target_src =
      orig_target != nullptr ? fast::Print(orig_target) : "";

  // Focus concentrates edits of `CodeGen`, `Operation`, `Input`, `Expression`,
  // and `Combine` inside `target`'s body.
  ProgramPtr partner =
      ParseOrDie("let p0 = 7;\nlet p1 = p0 + 1;\n", "selftest");
  bool focus_ok = orig_target != nullptr && orig_target->body != nullptr;
  std::string focus_err;
  for (const char* name :
       {"CodeGen", "Operation", "Input", "Expression", "Combine"}) {
    Mutator* m = mutators->Find(name);
    if (m == nullptr) {
      focus_ok = false;
      focus_err = std::string("missing mutator ") + name;
      break;
    }
    ProgramPtr use_partner = m->needs_partner() ? partner : nullptr;
    int mutated = 0;
    for (int i = 0; i < 25 && focus_ok; i++) {
      Random rng(kSeed + 500 + static_cast<uint64_t>(i));
      std::string detail;
      std::unique_ptr<fast::Tree> tree =
          ApplyMutation(*m, *program, use_partner, rng, env, *generators,
                        /*rounds=*/1, &detail, nullptr, target_body_id);
      if (tree == nullptr) {
        continue;
      }
      mutated++;
      fast::FunctionDeclaration* new_target = find_fn(tree->root(), "target");
      if (new_target == nullptr || fast::Print(new_target) == orig_target_src) {
        focus_ok = false;
        focus_err = StringPrintf("%s did not change target", name);
        break;
      }
      std::vector<std::string> new_outside;
      for (fast::Statement* s : tree->root()->body) {
        if (s != new_target) {
          new_outside.push_back(fast::Print(s));
        }
      }
      if (new_outside != orig_outside) {
        focus_ok = false;
        focus_err = StringPrintf("%s edited code outside target", name);
        break;
      }
    }
    if (focus_ok && mutated == 0) {
      focus_ok = false;
      focus_err = StringPrintf("%s produced 0 mutations", name);
    }
  }
  checker->Check(
      "focus: SetFocusSubtree keeps CodeGen/Operation/Input/Expression/Combine "
      "inside target body",
      focus_ok, focus_err);

  // A detached focus root yields empty queries, and `SlotOf` still searches the
  // full tree while focused.
  {
    std::unique_ptr<fast::Tree> tree = program->CopyTree();
    std::unique_ptr<fast::Tree> other_tree = program->CopyTree();
    Random rng(kSeed + 800);
    Builder builder(tree.get(), &rng);
    fast::FunctionDeclaration* target_fn = find_fn(tree->root(), "target");
    fast::FunctionDeclaration* other_fn = find_fn(tree->root(), "other");
    builder.SetFocusSubtree(target_fn->body);
    const bool slot_of_outside = builder.SlotOf(other_fn).valid();
    builder.SetFocusSubtree(other_tree->root());
    const bool detached_empty =
        builder.InsertionSlots().empty() && builder.StatementSlots().empty() &&
        builder.MemberSlots().empty() && builder.ValueExpressions().empty() &&
        builder.Collect([](fast::Node*) { return true; }).empty() &&
        !builder.RandomInsertionSlot().valid();
    checker->Check(
        "focus: detached focus root returns empty queries and SlotOf stays "
        "full-tree",
        slot_of_outside && detached_empty);
  }

  // `focus_root == nullptr` produces bit-identical output to default runs.
  bool identical_ok = true;
  for (const char* name : {"CodeGen", "Operation", "Input", "Expression"}) {
    Mutator* m = mutators->Find(name);
    if (m == nullptr) {
      identical_ok = false;
      break;
    }
    for (int i = 0; i < 10 && identical_ok; i++) {
      const uint64_t seed = kSeed + 900 + static_cast<uint64_t>(i);
      Random rng_a(seed);
      Random rng_b(seed);
      std::string detail_a, detail_b;
      std::unique_ptr<fast::Tree> tree_a =
          ApplyMutation(*m, *program, nullptr, rng_a, env, *generators,
                        /*rounds=*/1, &detail_a);
      std::unique_ptr<fast::Tree> tree_b = program->CopyTree();
      Builder builder_b(tree_b.get(), &rng_b, &env);
      builder_b.SetFocusSubtree(nullptr);
      MutationContext ctx_b;
      ctx_b.env = &env;
      ctx_b.generators = generators.get();
      bool applied_b = m->Mutate(builder_b, ctx_b);
      if ((tree_a != nullptr) != applied_b) {
        identical_ok = false;
      } else if (tree_a != nullptr &&
                 fast::Print(tree_a->root()) != fast::Print(tree_b->root())) {
        identical_ok = false;
      }
    }
  }
  checker->Check("focus: nullptr focus root is bit-identical to unfocused",
                 identical_ok);

  // Empty focused body falls back cleanly to unfiltered candidates.
  {
    ProgramPtr empty_fn_prog = ParseOrDie(
        "let a = 1 + 2;\nfunction empty() {}\nfunction h(x) { return x; }\n"
        "h(a);\n",
        "selftest");
    fast::FunctionDeclaration* empty_fn =
        find_fn(empty_fn_prog->tree().root(), "empty");
    const uint32_t empty_body_id =
        empty_fn != nullptr && empty_fn->body != nullptr ? empty_fn->body->id
                                                         : 0;
    Mutator* op_mut = mutators->Find("Operation");
    Mutator* call_mut = mutators->Find("Call");
    Random rng(kSeed + 1200);
    std::string detail;
    const uint64_t op_inv_before =
        op_mut != nullptr ? op_mut->stats().invocations.load() : 0;
    std::unique_ptr<fast::Tree> op_tree =
        op_mut != nullptr
            ? ApplyMutation(*op_mut, *empty_fn_prog, nullptr, rng, env,
                            *generators, 1, &detail, nullptr, empty_body_id)
            : nullptr;
    const uint64_t op_inv_after =
        op_mut != nullptr ? op_mut->stats().invocations.load() : 0;
    std::unique_ptr<fast::Tree> call_tree =
        call_mut != nullptr
            ? ApplyMutation(*call_mut, *empty_fn_prog, nullptr, rng, env,
                            *generators, 1, &detail, nullptr, empty_body_id)
            : nullptr;
    checker->Check(
        "focus: ApplyMutation falls back without double-counting invocations",
        op_tree != nullptr && call_tree != nullptr &&
            op_inv_after == op_inv_before + 1);
  }

  // `PickFocusTarget` prefers hot functions, ignores concise arrows, and
  // `ApplyFocusTierUp` adds optimization calls for cold top-level declarations.
  {
    ProgramPtr hot_vs_cold = ParseOrDie(
        "function cold(a) { return a; }\n"
        "function hot(x) { return x + 1; }\n"
        "%PrepareFunctionForOptimization(hot);\n"
        "hot(1);\n"
        "%OptimizeFunctionOnNextCall(hot);\n"
        "hot(2);\n",
        "selftest");
    bool always_hot = true;
    Random rng(kSeed + 1300);
    for (int i = 0; i < 50; i++) {
      FocusTarget picked;
      if (!PickFocusTarget(hot_vs_cold->tree(), rng, &picked) ||
          picked.name != "hot" || !picked.is_hot || !picked.has_tier_up) {
        always_hot = false;
        break;
      }
    }
    ProgramPtr concise_only = ParseOrDie(
        "const arrow = x => x + 1;\n"
        "for (let i = 0; i < 5; i++) arrow(i);\n",
        "selftest");
    FocusTarget none_picked;
    const bool concise_ignored =
        !PickFocusTarget(concise_only->tree(), rng, &none_picked);

    ProgramPtr fn_expr_prog = ParseOrDie(
        "const g = function foo(x) { return x + 1; };\n"
        "const arrow_blk = (y) => { return y + 2; };\n"
        "%OptimizeFunctionOnNextCall(g);\n",
        "selftest");
    FocusTarget fn_expr_picked;
    const bool fn_expr_ok =
        PickFocusTarget(fn_expr_prog->tree(), rng, &fn_expr_picked) &&
        fn_expr_picked.name == "g" && fn_expr_picked.is_hot &&
        fn_expr_picked.has_tier_up && !fn_expr_picked.is_top_level_decl;

    ProgramPtr block_arrow_prog =
        ParseOrDie("const h = (y) => { return y + 2; };\n", "selftest");
    FocusTarget block_arrow_picked;
    const bool block_arrow_ok =
        PickFocusTarget(block_arrow_prog->tree(), rng, &block_arrow_picked) &&
        block_arrow_picked.name == "h" && !block_arrow_picked.has_tier_up;

    ProgramPtr method_prog = ParseOrDie(
        "class C { work(x) { return x + 1; } }\n"
        "let inst = new C();\n"
        "for (let i = 0; i < 5; i++) inst.work(i);\n",
        "selftest");
    FocusTarget method_picked;
    const bool method_ok =
        PickFocusTarget(method_prog->tree(), rng, &method_picked) &&
        method_picked.name == "work" && method_picked.is_hot &&
        method_picked.has_tier_up && !method_picked.is_top_level_decl;

    ProgramPtr cold_only =
        ParseOrDie("function cold(x) { return x + 1; }\n", "selftest");
    FocusTarget cold_picked;
    std::string tier_detail;
    bool cold_ok = PickFocusTarget(cold_only->tree(), rng, &cold_picked) &&
                   cold_picked.name == "cold" && !cold_picked.has_tier_up &&
                   cold_picked.is_top_level_decl;
    std::unique_ptr<fast::Tree> tiered =
        cold_ok ? ApplyFocusTierUp(*cold_only, cold_picked, rng, env,
                                   *generators, &tier_detail)
                : nullptr;
    std::string tiered_src =
        tiered != nullptr ? fast::Print(tiered->root()) : "";
    fast::ParseError parse_err;
    bool tier_ok = tiered != nullptr &&
                   tiered->FindNode(cold_picked.body_id) != nullptr &&
                   tiered_src.find("%PrepareFunctionForOptimization(cold)") !=
                       std::string::npos &&
                   (tiered_src.find("%OptimizeFunctionOnNextCall(cold)") !=
                        std::string::npos ||
                    tiered_src.find("%OptimizeMaglevOnNextCall(cold)") !=
                        std::string::npos) &&
                   fast::Parser::ParseScript(tiered_src, fast::ParseOptions(),
                                             &parse_err) != nullptr;
    checker->Check(
        "focus: PickFocusTarget handles all function kinds and tiers up cold "
        "ones",
        always_hot && concise_ignored && fn_expr_ok && block_arrow_ok &&
            method_ok && tier_ok,
        parse_err.message.empty() ? tier_detail : parse_err.message);
  }

  // `stages.mutate.focus_chain` config validation, dump round-trip, and preset
  // values.
  {
    FuzzConfig defaults;
    FuzzConfig cfg;
    std::string err =
        LoadFullConfig(R"({"stages": {"mutate": {"focus_chain": 0.3}}})", &cfg);
    JsonValue dumped;
    std::string parse_err;
    bool dump_ok =
        err.empty() && defaults.focus_chain == 0.0 && cfg.focus_chain == 0.3 &&
        JsonValue::Parse(cfg.ToJson(FuzzConfig::Detail::kFull), &dumped,
                         &parse_err) &&
        dumped.Get("stages").Get("mutate").GetNumber("focus_chain", -1.0) ==
            0.3;
    FuzzConfig bad_low, bad_high, bad_type;
    bool reject_ok =
        !LoadFullConfig(R"({"stages": {"mutate": {"focus_chain": -0.1}}})",
                        &bad_low)
             .empty() &&
        !LoadFullConfig(R"({"stages": {"mutate": {"focus_chain": 1.5}}})",
                        &bad_high)
             .empty() &&
        !LoadFullConfig(R"({"stages": {"mutate": {"focus_chain": "bad"}}})",
                        &bad_type)
             .empty();
    checker->Check(
        "focus: stages.mutate.focus_chain loads, dumps, and validates range",
        dump_ok && reject_ok, err + parse_err);

    auto preset_focus = [](const std::string& name, double* value,
                           std::string* error) {
      std::string arg = "--preset=" + name;
      const char* argv[] = {"redline", "fuzz", arg.c_str()};
      Flags flags = Flags::Parse(3, argv, 2);
      FuzzConfig loaded;
      bool ok = FuzzConfig::Load(flags, &loaded, error, /*for_run=*/false);
      TunableRegistry::Instance().Reset();
      if (ok) {
        *value = loaded.focus_chain;
      }
      return ok;
    };
    double maglev_fc = -1.0;
    double turbolev_fc = -1.0;
    double default_fc = -1.0;
    double fuzzilli_fc = -1.0;
    std::string preset_err;
    bool presets_ok = preset_focus("maglev", &maglev_fc, &preset_err) &&
                      preset_focus("turbolev", &turbolev_fc, &preset_err) &&
                      preset_focus("default", &default_fc, &preset_err) &&
                      preset_focus("fuzzilli", &fuzzilli_fc, &preset_err) &&
                      maglev_fc == 0.3 && turbolev_fc == 0.3 &&
                      default_fc == 0.0 && fuzzilli_fc == 0.0;
    checker->Check(
        "focus: maglev and turbolev presets set focus_chain to 0.3, default "
        "and fuzzilli keep 0.0",
        presets_ok,
        preset_err.empty()
            ? StringPrintf("maglev=%g turbolev=%g default=%g fuzzilli=%g",
                           maglev_fc, turbolev_fc, default_fc, fuzzilli_fc)
            : preset_err);
  }
}

// Verifies `RarityState` correctness: non-aging `PickPreferred`, indexed active
// heap and zero-energy eviction, O(1) active-set `PickPartner`, RAII
// `SeedLease`, `OnOutcome` fertility and default-off optional knobs.
void CheckRarityOverhaul(Checker* checker) {
  auto make_corpus = [](const std::string& params_json) {
    JsonValue params;
    std::string err;
    if (!params_json.empty()) {
      JsonValue::Parse(params_json, &params, &err);
    }
    auto& reg = SelectorRegistry<const Program>::Instance();
    auto seeds = reg.Create(SelectionConfig{"rarity", params}, &err);
    auto partners = reg.Create(SelectionConfig{"rarity-partner", params}, &err);
    return std::make_unique<Corpus>(std::move(seeds), std::move(partners));
  };
  auto make_prog = [](const std::string& src, std::vector<uint32_t> edges,
                      std::vector<std::string> flags = {}) {
    std::shared_ptr<Program> p = ParseOrDie(src, "selftest");
    p->edges = std::move(edges);
    p->flags = std::move(flags);
    return p;
  };
  Random rng(20261122);
  std::string inv_err;

  // 1. `PickPreferred` inspects without aging rejected frontier seeds.
  {
    auto corpus = make_corpus("");
    ProgramPtr s0 =
        corpus->Add(make_prog("let a = 1;", {1, 2, 3}, {"--flag-a"}));
    ProgramPtr s1 = corpus->Add(make_prog("let b = 2;", {4, 5}, {"--flag-b"}));
    ProgramPtr s2 = corpus->Add(make_prog("let c = 3;", {6}, {"--flag-c"}));
    ProgramPtr picked = corpus->PickPreferred(rng, 2, [](const Program& p) {
      return !p.flags.empty() && p.flags[0] == "--flag-b";
    });
    const bool match_ok = picked == s1 && s0->age == 0.0 &&
                          s0->picks.load() == 0 && s1->age == 1.0 &&
                          s1->picks.load() == 1 && s2->age == 0.0;
    ProgramPtr next = corpus->Pick(rng);
    const bool top_preserved =
        next == s0 && s0->age == 1.0 && s0->picks.load() == 1;
    // When all inspected candidates fail the predicate, only the fallback
    // candidate ages.
    auto corpus2 = make_corpus("");
    ProgramPtr t0 = corpus2->Add(make_prog("let a = 1;", {1, 2, 3}));
    ProgramPtr t1 = corpus2->Add(make_prog("let b = 2;", {4, 5}));
    ProgramPtr fallback =
        corpus2->PickPreferred(rng, 2, [](const Program&) { return false; });
    const bool fallback_ok = fallback == t0 && t0->age == 1.0 &&
                             t0->picks.load() == 1 && t1->age == 0.0 &&
                             t1->picks.load() == 0;
    checker->Check(
        "rarity: PickPreferred does not age or demote rejected frontier seeds",
        match_ok && top_preserved && fallback_ok &&
            corpus->CheckInvariantsForTest(&inv_err) &&
            corpus2->CheckInvariantsForTest(&inv_err),
        inv_err);
  }

  // 2. Indexed active max-heap updates order and evicts zero-energy seeds.
  {
    auto corpus = make_corpus(R"({"rare_edge_limit": 3})");
    ProgramPtr s_unique = corpus->Add(make_prog("let a = 1;", {10}));
    ProgramPtr s_shared = corpus->Add(make_prog("let b = 2;", {20, 21}));
    // Adding a second seed on {20, 21} reduces `s_shared` energy below
    // `s_unique` (2 / 2^3 = 0.25 < 1.0).
    corpus->Add(make_prog("let c = 3;", {20, 21}));
    ProgramPtr top_after_drop = corpus->Pick(rng);
    // A third seed on {20, 21} saturates both edges (count 3 == limit 3),
    // evicting all three {20, 21} seeds from the active heap.
    corpus->Add(make_prog("let d = 4;", {20, 21}));
    const bool evicted_ok = corpus->active_size() == 1 && corpus->size() == 4 &&
                            corpus->Get(s_shared->id) == s_shared &&
                            corpus->Pick(rng) == s_unique;
    // Crash edges can also saturate the last active seed cleanly.
    corpus->RecordCrashEdges({10, 10});
    const bool crash_sat_ok =
        corpus->active_size() == 0 && corpus->Pick(rng) != nullptr;
    checker->Check(
        "rarity: indexed heap reorders on updates and evicts zero-energy seeds",
        top_after_drop == s_unique && evicted_ok && crash_sat_ok &&
            corpus->CheckInvariantsForTest(&inv_err),
        inv_err);
  }

  // 3. O(1) active-set `PickPartner` avoids dead seeds and falls back when
  // empty.
  {
    auto corpus = make_corpus(R"({"rare_edge_limit": 2})");
    ProgramPtr dead_anchor;
    for (int i = 0; i < 60; i++) {
      dead_anchor =
          corpus->Add(make_prog(StringPrintf("let d%d = %d;", i, i), {50}));
    }
    ProgramPtr a0 = corpus->Add(make_prog("let x = 1;", {100}));
    ProgramPtr a1 = corpus->Add(make_prog("let y = 2;", {101}));
    bool only_active = corpus->active_size() == 2;
    for (int i = 0; i < 50; i++) {
      ProgramPtr p = corpus->PickPartner(*dead_anchor, rng);
      if (p != a0 && p != a1) {
        only_active = false;
      }
    }
    // Excludes anchor and includes leased active seeds in the active pool.
    SeedLease lease_a1 = corpus->Lease(rng);
    SeedLease lease_a0 = corpus->Lease(rng);
    bool leased_partner_ok = true;
    for (int i = 0; i < 20; i++) {
      if (corpus->PickPartner(*a0, rng) != a1) {
        leased_partner_ok = false;
      }
    }
    lease_a0.Release();
    lease_a1.Release();
    // Empty active set falls back to the full corpus and still excludes anchor.
    corpus->RecordCrashEdges({100, 101});
    bool fallback_ok = corpus->active_size() == 0;
    for (int i = 0; i < 20; i++) {
      ProgramPtr p = corpus->PickPartner(*a0, rng);
      if (p == nullptr || p == a0) {
        fallback_ok = false;
      }
    }
    checker->Check(
        "rarity: PickPartner draws only active seeds and falls back when empty",
        only_active && leased_partner_ok && fallback_ok &&
            corpus->CheckInvariantsForTest(&inv_err),
        inv_err);
  }

  // 4. RAII `SeedLease` distributes distinct top seeds, falls back when workers
  // lease all active seeds, and restores seeds on destruction.
  {
    auto corpus = make_corpus("");
    ProgramPtr s0 = corpus->Add(make_prog("let a = 1;", {1, 2, 3}, {"--a"}));
    ProgramPtr s1 = corpus->Add(make_prog("let b = 2;", {4, 5}, {"--b"}));
    ProgramPtr s2 = corpus->Add(make_prog("let c = 3;", {6}, {"--c"}));
    bool lease_ok = false;
    {
      SeedLease l0 = corpus->Lease(rng);
      SeedLease l1 = corpus->Lease(rng);
      SeedLease l2 = corpus->Lease(rng);
      const bool distinct = l0.program() == s0 && l1.program() == s1 &&
                            l2.program() == s2 && corpus->leased_size() == 3;
      // Fourth concurrent lease falls back to the highest-energy leased seed,
      // and `LeasePreferred` inspects leased seeds without aging rejected ones.
      const double s0_age_before = s0->age;
      SeedLease l3 = corpus->LeasePreferred(rng, 2, [](const Program& p) {
        return !p.flags.empty() && p.flags[0] == "--b";
      });
      const bool fallback = l3.program() == s1 && s0->age == s0_age_before &&
                            corpus->leased_size() == 3;
      // Releasing `l1` while `l3` still leases `s1` keeps `s1` leased.
      l1.Release();
      const bool refcount_ok = !l1 && corpus->leased_size() == 3 &&
                               corpus->CheckInvariantsForTest(&inv_err);
      // Releasing `l0` returns `s0` to `active_heap_` immediately.
      l0.Release();
      const bool released_one = corpus->leased_size() == 2;
      SeedLease l4 = corpus->Lease(rng);
      lease_ok = distinct && fallback && refcount_ok && released_one &&
                 l4.program() == s0 && corpus->CheckInvariantsForTest(&inv_err);
    }
    // Non-rarity strategies also support `SeedLease` cleanly.
    std::string uerr;
    auto& reg = SelectorRegistry<const Program>::Instance();
    Corpus uniform_corpus(
        reg.Create(SelectionConfig{"uniform", JsonValue()}, &uerr),
        reg.Create(SelectionConfig{"uniform-partner", JsonValue()}, &uerr));
    uniform_corpus.Add(make_prog("let u = 1;", {1}));
    SeedLease ulease = uniform_corpus.Lease(rng);
    const bool uniform_lease_ok = static_cast<bool>(ulease);
    ulease.Release();
    checker->Check(
        "rarity: SeedLease spreads concurrent workers, falls back and releases "
        "via RAII",
        lease_ok && uniform_lease_ok && !ulease && corpus->leased_size() == 0 &&
            corpus->CheckInvariantsForTest(&inv_err),
        inv_err);
  }

  // 5. `OnOutcome` fertility, `jit_seed_boost` and `max_memory_trees` ship
  // default-off and activate cleanly when configured.
  {
    auto def = make_corpus("");
    JsonValue def_params;
    std::string perr;
    JsonValue::Parse(def->seeds().Params().ToString(), &def_params, &perr);
    const bool defaults_off =
        def_params.GetInt("max_memory_trees", -1) == 0 &&
        def_params.GetNumber("jit_seed_boost", 0.0) == 1.0 &&
        def_params.GetNumber("fertility_weight", -1.0) == 0.0;
    // Fertility boost active when `fertility_weight > 0`.
    auto fert = make_corpus(R"({"fertility_weight": 1.0})");
    ProgramPtr f0 = fert->Add(make_prog("let a = 1;", {1}));
    ProgramPtr f1 = fert->Add(make_prog("let b = 2;", {2}));
    for (int i = 0; i < 10; i++) {
      fert->OnResult(*f0, false);
    }
    fert->OnResult(*f1, true);
    fert->OnResult(*f1, true);
    const bool fert_ok =
        fert->Pick(rng) == f1 && fert->CheckInvariantsForTest(&inv_err);
    // JIT boost active when `jit_seed_boost > 1`.
    auto jit = make_corpus(R"({"jit_seed_boost": 4.0})");
    jit->Add(make_prog("let a = 1;", {1}));
    ProgramPtr hot = jit->Add(make_prog("for (let i = 0; i < 5; i++) {}", {2}));
    const bool jit_ok =
        jit->Pick(rng) == hot && jit->CheckInvariantsForTest(&inv_err);
    // Tree eviction drops cold AST trees while preserving `Get()` and reparsing
    // transparently on `CopyTree()`.
    auto mem = make_corpus(R"({"rare_edge_limit": 2, "max_memory_trees": 2})");
    uint32_t cold_id = 0;
    std::string cold_src;
    for (int i = 0; i < 4; i++) {
      auto p = make_prog(StringPrintf("let m%d = %d;", i, i), {70});
      if (i == 0) {
        cold_src = p->source();
      }
      uint32_t id = mem->Add(std::move(p))->id;
      if (i == 0) {
        cold_id = id;
      }
    }
    mem->Add(make_prog("let live = 1;", {71}));
    const bool evicted = mem->resident_trees() <= 2 && mem->size() == 5;
    ProgramPtr cold_prog = mem->Get(cold_id);
    const bool was_dropped = cold_prog != nullptr && !cold_prog->has_tree();
    std::unique_ptr<fast::Tree> reparsed =
        cold_prog != nullptr ? cold_prog->CopyTree() : nullptr;
    const bool reparsed_ok = was_dropped && cold_prog->has_tree() &&
                             cold_prog->source() == cold_src &&
                             reparsed != nullptr &&
                             fast::Print(reparsed->root()) == cold_src;
    checker->Check(
        "rarity: fertility, jit_seed_boost and max_memory_trees default off "
        "and work when enabled",
        defaults_off && fert_ok && jit_ok && evicted && reparsed_ok &&
            mem->CheckInvariantsForTest(&inv_err),
        inv_err);
  }

  // 6. Randomized invariant stress over adds, leases, partners, outcomes and
  // crash edges.
  {
    auto corpus =
        make_corpus(R"({"rare_edge_limit": 4, "fertility_weight": 0.5})");
    std::vector<SeedLease> leases;
    bool ok = true;
    for (int step = 0; step < 200 && ok; step++) {
      const size_t action = rng.Index(6);
      if (corpus->size() < 3 || action == 0) {
        std::vector<uint32_t> edges;
        const size_t k = 1 + rng.Index(4);
        for (size_t j = 0; j < k; j++) {
          edges.push_back(static_cast<uint32_t>(1 + rng.Index(20)));
        }
        corpus->Add(make_prog(
            StringPrintf("let v%d = %d;", step, step), std::move(edges),
            (step % 3 == 0) ? std::vector<std::string>{"--tag"}
                            : std::vector<std::string>{}));
      } else if (action == 1) {
        if (leases.size() < 6) {
          leases.push_back(corpus->LeasePreferred(
              rng, 3, [](const Program& p) { return !p.flags.empty(); }));
        } else {
          const size_t drop = rng.Index(leases.size());
          leases.erase(leases.begin() + static_cast<ptrdiff_t>(drop));
        }
      } else if (action == 2) {
        if (!leases.empty()) {
          const size_t drop = rng.Index(leases.size());
          leases.erase(leases.begin() + static_cast<ptrdiff_t>(drop));
        }
      } else if (action == 3) {
        ProgramPtr p = corpus->PickPreferred(
            rng, 2, [](const Program& prog) { return !prog.flags.empty(); });
        if (p != nullptr) {
          corpus->PickPartner(*p, rng);
          corpus->OnResult(*p, rng.Index(4) == 0);
        }
      } else if (action == 4) {
        corpus->RecordCrashEdges({static_cast<uint32_t>(1 + rng.Index(20))});
      } else {
        ProgramPtr p = corpus->Pick(rng);
        if (p != nullptr) {
          corpus->OnResult(*p, rng.Index(5) == 0);
        }
      }
      ok = corpus->CheckInvariantsForTest(&inv_err);
    }
    leases.clear();
    ok = ok && corpus->leased_size() == 0 &&
         corpus->CheckInvariantsForTest(&inv_err);
    checker->Check(
        "rarity: randomized heap, lease, partner and fertility invariants hold",
        ok, inv_err);
  }
}

// Checks empirical pure-ES builtins, trigger natives, and stateful Workers.
void CheckBuiltinsAndWorkers(Checker* checker) {
  const Environment& env = Environment::Default();
  auto has_method = [&](JsType t, std::string_view name) {
    const BuiltinType* bt = env.ForType(t);
    return bt != nullptr && std::any_of(bt->methods.begin(), bt->methods.end(),
                                        [&](const BuiltinFunction& f) {
                                          return f.name == name;
                                        });
  };
  auto has_static = [&](std::string_view gname, std::string_view mname) {
    const BuiltinGlobal* g = env.Global(gname);
    return g != nullptr &&
           std::any_of(
               g->static_methods.begin(), g->static_methods.end(),
               [&](const BuiltinFunction& f) { return f.name == mname; });
  };
  bool env_ok =
      has_method(JsType::kIterator, "windows") &&
      has_method(JsType::kIterator, "chunks") &&
      has_method(JsType::kIterator, "join") &&
      has_method(JsType::kIterator, "includes") &&
      has_static("Iterator", "zipKeyed") && has_static("Atomics", "notify") &&
      has_static("Atomics", "pause") && has_static("Atomics", "waitAsync") &&
      !has_static("Atomics", "wait") &&
      has_method(JsType::kRegExp, "compile") &&
      has_static("RegExp", "escape") &&
      std::find(env.numbers().begin(), env.numbers().end(),
                -2.220446049250313e-16) != env.numbers().end();
  for (const char* m :
       {"find", "findIndex", "findLast", "findLastIndex", "forEach", "filter",
        "every", "some", "reduce", "reduceRight", "includes", "lastIndexOf",
        "toReversed", "toSorted", "with", "toString", "toLocaleString"}) {
    env_ok = env_ok && has_method(JsType::kTypedArray, m);
  }
  const BuiltinType* set_t = env.ForType(JsType::kSet);
  env_ok = env_ok && set_t != nullptr &&
           std::any_of(set_t->methods.begin(), set_t->methods.end(),
                       [](const BuiltinFunction& f) {
                         return f.name == "union" && !f.params.empty() &&
                                f.params[0].kind == ArgKind::kSetLike;
                       });
  for (const char* n :
       {"_DeoptimizeNow", "SetForceSlowPath", "DisableEagerInlining",
        "ShareObject", "GetUndetectable", "ArrayBufferSetDetachKey"}) {
    env_ok = env_ok &&
             std::any_of(env.natives().begin(), env.natives().end(),
                         [&](const NativeFunction& f) { return f.name == n; });
  }
  checker->Check(
      "environment: pure-ES builtins, set-like, Atomics, RegExp, natives",
      env_ok, env_ok ? "" : "missing model entries");

  fast::ParseError perr;
  std::unique_ptr<fast::Tree> rt = fast::Parser::ParseScript(
      "%_DeoptimizeNow();\n", fast::ParseOptions(), &perr);
  checker->Check(
      "FAST: %_DeoptimizeNow() parses and round-trips",
      rt != nullptr && fast::Print(rt->root()) == "%_DeoptimizeNow();\n",
      perr.message);

  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
  Random rng(kSeed + 202);
  auto sample_gen = [&](std::string_view name, int n,
                        auto setup) -> std::vector<std::string> {
    std::vector<std::string> out;
    Generator* g = generators->Find(name);
    if (g == nullptr) return out;
    for (int i = 0; i < n; i++) {
      auto tree = std::make_unique<fast::Tree>();
      tree->set_root(tree->New<fast::Program>());
      Builder builder(tree.get(), &rng, &env);
      GenContext ctx(builder, *generators, env);
      ctx.out = &tree->root()->body;
      setup(ctx);
      if (g->Generate(ctx, ctx.out)) {
        std::string src = fast::Print(tree->root());
        if (fast::Parser::ParseScript(src, fast::ParseOptions(), &perr)) {
          out.push_back(std::move(src));
        }
      }
    }
    return out;
  };
  auto any_has = [](const std::vector<std::string>& v, std::string_view sub) {
    return std::any_of(v.begin(), v.end(), [&](const std::string& s) {
      return s.find(sub) != std::string::npos;
    });
  };

  auto deopts = sample_gen("Deopt", 60, [](GenContext&) {});
  const bool deopt_ok = any_has(deopts, "%_DeoptimizeNow()") &&
                        any_has(deopts, "%DeoptimizeNow()");
  checker->Check("Deopt: emits %_DeoptimizeNow() and %DeoptimizeNow()",
                 deopt_ok, deopt_ok ? "" : "missing deopt variant");

  std::string terr;
  bool set_ok = TunableRegistry::Instance().Set(
                    "generators.OsrLoop.function_chance", 1.0, &terr) &&
                TunableRegistry::Instance().Set(
                    "generators.OsrLoop.caller_depth_chance", 1.0, &terr);
  auto osr = sample_gen("OsrLoop", 20, [](GenContext&) {});
  TunableRegistry::Instance().Reset();
  const bool osr_ok = set_ok && any_has(osr, "%OptimizeOsr(1)");
  checker->Check("OsrLoop: emits %OptimizeOsr(1)", osr_ok,
                 osr_ok ? "" : "missing %OptimizeOsr(1)");

  set_ok = TunableRegistry::Instance().Set(
               "generators.ClassDecl.super_class_chance", 1.0, &terr) &&
           TunableRegistry::Instance().Set(
               "generators.ClassDecl.builtin_superclass_chance", 1.0, &terr);
  auto classes = sample_gen("ClassDecl", 260, [](GenContext&) {});
  TunableRegistry::Instance().Reset();
  const bool cls_ok =
      set_ok && any_has(classes, "extends WeakSet") &&
      (any_has(classes, "Uint8Array") || any_has(classes, "Int32Array") ||
       any_has(classes, "Float16Array"));
  checker->Check("ClassDecl: extends WeakSet and TypedArrays", cls_ok,
                 cls_ok ? "" : "missing expected superclasses");

  auto proxies = sample_gen("ProxyDecl", 80, [](GenContext& ctx) {
    ctx.tree()->root()->body.push_back(
        mk::Let(ctx.tree(), "h", mk::Object(ctx.tree(), {})));
    ctx.Declare("h", JsType::kObject);
  });
  const bool proxy_ok = any_has(proxies, "isExtensible") &&
                        any_has(proxies, "preventExtensions") &&
                        any_has(proxies, "setPrototypeOf") &&
                        any_has(proxies, ", h)");
  checker->Check("ProxyDecl: covers missing traps and handler reuse", proxy_ok,
                 proxy_ok ? "" : "missing ProxyDecl trap or handler reuse");

  auto constructs = sample_gen("Construct", 50, [](GenContext&) {});
  const bool construct_ok = any_has(constructs, "Reflect.construct(");
  checker->Check("Construct: emits Reflect.construct with newTarget",
                 construct_ok, construct_ok ? "" : "missing Reflect.construct");

  set_ok = TunableRegistry::Instance().Set(
      "codegen.object_like.variable_chance", 1.0, &terr);
  auto withs = sample_gen("With", 120, [](GenContext& ctx) {
    ctx.tree()->root()->body.push_back(mk::Let(
        ctx.tree(), "obj",
        mk::Object(ctx.tree(),
                   {mk::Prop(ctx.tree(), "fooKey", mk::Num(ctx.tree(), 1))})));
    ctx.Declare("obj", JsType::kObject);
    ctx.builder.RecordInfo("obj").keys.push_back(
        {"fooKey", JsType::kNumber, -1, false});
  });
  TunableRegistry::Instance().Reset();
  const bool with_ok = set_ok && any_has(withs, "fooKey");
  checker->Check("With: accesses known object keys in with body", with_ok,
                 with_ok ? "" : "missing known key access");

  auto workers = sample_gen("WorkerSpawn", 20, [](GenContext&) {});
  auto strict_workers = sample_gen(
      "WorkerSpawn", 60, [](GenContext& ctx) { ctx.syntax.strict = true; });
  const bool worker_ok =
      workers.size() == 20 && strict_workers.size() == 60 &&
      any_has(workers, "type: \"function\"") &&
      any_has(workers, "arguments:") && any_has(workers, "SharedArrayBuffer") &&
      any_has(workers, "Atomics") && any_has(workers, "finally") &&
      any_has(workers, "postMessage(") && any_has(workers, "postMessage(0)");
  checker->Check(
      "WorkerSpawn: stateful function workers use SharedArrayBuffer and "
      "Atomics",
      worker_ok, worker_ok ? "" : "invalid WorkerSpawn output");

  bool cg_neg_bigint = false, cg_var_b64 = false, cg_ta_overload = false;
  for (int i = 0; i < 60; i++) {
    auto tree = std::make_unique<fast::Tree>();
    tree->set_root(tree->New<fast::Program>());
    Builder builder(tree.get(), &rng);
    GenContext ctx(builder, *generators, env);
    ctx.Declare("ta0", JsType::kTypedArray);
    CodeGen cg(ctx);
    fast::Expression* bi = cg.BigInt();
    if (bi->Is<fast::UnaryExpression>() &&
        bi->Cast<fast::UnaryExpression>()->op == fast::UnaryOp::kMinus) {
      cg_neg_bigint = true;
    }
    tree->root()->body.push_back(cg.Stmt(bi));
    tree->root()->body.push_back(
        cg.Stmt(cg.ArgumentFor(arg::Enum("Base64String"))));
    if (fast::Expression* ta = cg.Construct(JsType::kTypedArray)) {
      tree->root()->body.push_back(cg.Stmt(ta));
    }
    std::string src = fast::Print(tree->root());
    if (fast::Parser::ParseScript(src, fast::ParseOptions(), &perr)) {
      if (src.find("ta0") != std::string::npos) cg_ta_overload = true;
      for (size_t pos = 0; (pos = src.find('"', pos)) != std::string::npos;) {
        size_t end = src.find('"', pos + 1);
        if (end != std::string::npos && end - pos > 20) cg_var_b64 = true;
        pos = end == std::string::npos ? end : end + 1;
      }
    }
  }
  const bool cg_ok = cg_neg_bigint && cg_var_b64 && cg_ta_overload;
  checker->Check(
      "CodeGen: negative BigInts, variable Base64, TypedArray overloads", cg_ok,
      cg_ok ? "" : "missing CodeGen coverage");

  bool depth_safe = true;
  for (int start_depth : {23, 24}) {
    for (int i = 0; i < 40 && depth_safe; i++) {
      auto tree = std::make_unique<fast::Tree>();
      tree->set_root(tree->New<fast::Program>());
      Builder builder(tree.get(), &rng, &env);
      GenContext ctx(builder, *generators, env);
      ctx.expr_depth = start_depth;
      CodeGen cg(ctx);
      fast::Expression* set_like = cg.ArgumentFor(arg::SetLike());
      if (set_like == nullptr) {
        depth_safe = false;
        break;
      }
      tree->root()->body.push_back(cg.Stmt(set_like));
      if (fast::Expression* ta = cg.Construct(JsType::kTypedArray)) {
        tree->root()->body.push_back(cg.Stmt(ta));
      }
      std::vector<fast::Expression*> native_args;
      if (!NativeArgumentsFor(cg, "ArrayBufferSetDetachKey", &native_args) ||
          native_args.empty() || native_args[0] == nullptr ||
          native_args[1] == nullptr) {
        depth_safe = false;
        break;
      }
      tree->root()->body.push_back(cg.Stmt(native_args[0]));
      std::string src = fast::Print(tree->root());
      if (src.empty()) {
        depth_safe = false;
      }
    }
  }
  checker->Check(
      "CodeGen: SetLike, TypedArray and ArrayBufferSetDetachKey stay non-null "
      "at max expression depth",
      depth_safe, depth_safe ? "" : "null expression at max_expr_depth");
}

// The probe calls of `function` in the tree of `b`: the probed variable
// for each probe id.
std::map<std::string, std::string> ProbeCalls(Builder& b,
                                              std::string_view function) {
  std::map<std::string, std::string> calls;
  for (fast::Node* n : b.Collect([&](fast::Node* n) {
         auto* call = n->As<fast::CallExpression>();
         auto* callee =
             call != nullptr ? call->callee->As<fast::Identifier>() : nullptr;
         return callee != nullptr && callee->name == function &&
                call->arguments.size() >= 2 &&
                call->arguments[0]->Is<fast::StringLiteral>() &&
                call->arguments[1]->Is<fast::Identifier>();
       })) {
    auto* call = n->Cast<fast::CallExpression>();
    calls[std::string(call->arguments[0]->Cast<fast::StringLiteral>()->value)] =
        std::string(call->arguments[1]->Cast<fast::Identifier>()->name);
  }
  return calls;
}

// Checks that each probe of `function` is right after the definition of
// its variable, or at the start of the body for a parameter. Other probes
// can be between them. Returns an error, or "".
std::string CheckProbePlacement(Builder& b, std::string_view function) {
  for (const Builder::Slot& slot : b.StatementSlots()) {
    auto* probe = slot.statement()->As<fast::TryStatement>();
    if (probe == nullptr || probe->block->body.size() != 1) {
      continue;
    }
    auto* stmt = probe->block->body[0]->As<fast::ExpressionStatement>();
    auto* call = stmt != nullptr ? stmt->expression->As<fast::CallExpression>()
                                 : nullptr;
    auto* callee =
        call != nullptr ? call->callee->As<fast::Identifier>() : nullptr;
    if (callee == nullptr || callee->name != function) {
      continue;
    }
    auto* variable = call->arguments[1]->As<fast::Identifier>();
    fast::Binding* binding =
        variable != nullptr ? b.scopes().BindingOf(variable) : nullptr;
    if (binding == nullptr) {
      return "a probe of an unknown variable";
    }
    // Skip the other probes inserted at the same place.
    size_t index = slot.index;
    auto is_probe = [&](fast::Statement* s) {
      auto* t = s->As<fast::TryStatement>();
      return t != nullptr && t->block->body.size() == 1 &&
             fast::Print(t).find(function) != std::string::npos;
    };
    while (index > 0 && is_probe((*slot.list)[index - 1])) {
      index--;
    }
    if (binding->kind == fast::BindingKind::kParameter) {
      auto* fn = fast::AsFunctionData(b.EnclosingFunction(binding->identifier));
      bool directives_only = true;
      for (size_t i = 0; i < index; i++) {
        auto* e = (*slot.list)[i]->As<fast::ExpressionStatement>();
        directives_only = directives_only && e != nullptr && e->is_directive;
      }
      if (fn == nullptr || slot.list != &fn->body->body || !directives_only) {
        return "a parameter probe of " + std::string(variable->name) +
               " is not at the start of the body";
      }
    } else if (index == 0 || (*slot.list)[index - 1] !=
                                 b.EnclosingStatement(binding->identifier)) {
      return "the probe of " + std::string(variable->name) +
             " is not right after its definition";
    }
  }
  return "";
}

// Offline checks of the runtime-assisted mutators (Explore, Probing,
// Fixup) and of the PropertyAccessor mutator. The answers of the target are
// written by hand.
void CheckRuntimeAssisted(Checker* checker) {
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
  std::unique_ptr<MutatorSet> mutators = MutatorSet::CreateDefault();
  const Environment& env = Environment::Default();
  TunableRegistry& reg = TunableRegistry::Instance();
  const char* kProbed = R"(let a = {x: 1};
let b = [1, 2, 3];
function f(p, q) {
  "use strict";
  let inner = p + q;
  return inner;
}
for (let i = 0; i < 3; i++) {
  let l0 = {}; let l1 = {}; let l2 = {}; let l3 = {}; let l4 = {}; let l5 = {};
  f(l0, l1);
}
let w = new Worker(function() { let hidden = {}; hidden.x = 1; }, {type: 'function'});
function g(o) { let hidden2 = o; return hidden2; }
let w2 = new Worker(g, {type: 'function'});
const wf = function() { let hidden3 = {}; return hidden3; };
let w3 = new Worker(wf, {type: 'function'});
let y = b[0] + a.x;
)";
  ProgramPtr program = ParseOrDie(kProbed, "selftest");
  auto loop_probes = [](Builder& b, std::string_view function) {
    int n = 0;
    for (fast::Node* loop :
         b.Collect([](fast::Node* n) { return n->Is<fast::ForStatement>(); })) {
      std::string text = fast::Print(loop);
      for (size_t at = text.find(function); at != std::string::npos;
           at = text.find(function, at + 1)) {
        n++;
      }
    }
    return n;
  };

  // Explore and Probing at chance 1: every eligible definition gets a probe,
  // up to the caps.
  for (const char* name : {"Explore", "Probing"}) {
    auto* m = static_cast<RuntimeAssistedMutator*>(mutators->Find(name));
    const std::string prefix = std::string("mutators.") + name + ".";
    const std::string_view function = std::string_view(name) == "Explore"
                                          ? ExploreMutator::kProbeFunction
                                          : ProbingMutator::kProbeFunction;
    std::string err;
    bool ok = m != nullptr;
    for (int max_probes : {32, 5}) {
      for (int seed = 0; ok && seed < 10; seed++) {
        reg.Reset();
        ok = reg.Set(prefix + "max_probes", max_probes, &err) &&
             (std::string_view(name) == "Explore"
                  ? reg.Set(prefix + "untyped_chance", 1, &err) &&
                        reg.Set(prefix + "typed_chance", 1, &err)
                  : reg.Set(prefix + "chance", 1, &err) &&
                        reg.Set(prefix + "unreferenced_candidate_chance", 1,
                                &err));
        std::unique_ptr<fast::Tree> tree = program->CopyTree();
        Random rng(kSeed + 900 + static_cast<uint64_t>(seed));
        Builder b(tree.get(), &rng, &env);
        MutationContext ctx;
        ctx.env = &env;
        ctx.generators = generators.get();
        if (!ok || !m->Mutate(b, ctx)) {
          ok = false;
          err = err.empty() ? "no probe" : err;
          break;
        }
        std::map<std::string, std::string> calls = ProbeCalls(b, function);
        const int loops = loop_probes(b, function);
        std::string placement = CheckProbePlacement(b, function);
        if (calls.size() != ctx.probes.size() ||
            static_cast<int>(calls.size()) > max_probes) {
          err = StringPrintf("%zu probes, %zu calls, max %d", ctx.probes.size(),
                             calls.size(), max_probes);
        } else if (loops > 4) {
          err = StringPrintf("%d probes in the loop body", loops);
        } else if (max_probes == 32 && loops != 4) {
          err = StringPrintf("%d probes in the loop body, not 4", loops);
        }
        for (const auto& [id, variable] : calls) {
          if (variable == "hidden" || variable == "hidden2" ||
              variable == "hidden3" || variable == "o") {
            err = "a probe in a Worker function: " + variable;
          }
        }
        if (err.empty() && !placement.empty()) {
          err = placement;
        }
        ok = err.empty();
      }
    }
    reg.Reset();
    checker->Check(
        StringPrintf("%s: probes right after definitions, at most 4 in loop "
                     "bodies, none in Worker functions, within the cap",
                     name)
            .c_str(),
        ok, err);
  }

  // Default tunables: at most 10 probes, at least one.
  {
    reg.Reset();
    bool ok = true;
    std::string err;
    for (const char* name : {"Explore", "Probing"}) {
      Mutator* m = mutators->Find(name);
      for (int seed = 0; ok && seed < 20; seed++) {
        Random rng(kSeed + 950 + static_cast<uint64_t>(seed));
        std::vector<ExploreProbe> probes;
        std::string detail;
        std::unique_ptr<fast::Tree> tree = ApplyMutation(
            *m, *program, nullptr, rng, env, *generators, 1, &detail, &probes);
        if (tree == nullptr || probes.empty() || probes.size() > 10) {
          ok = false;
          err = StringPrintf("%s: %zu probes", name, probes.size());
        }
      }
    }
    checker->Check("Explore/Probing: 1 to 10 probes with the default tunables",
                   ok, err);
  }

  // Keys: an array index gives a numeric element access.
  {
    auto tree = std::make_unique<fast::Tree>();
    fast::Tree* t = tree.get();
    auto access = [&](const std::string& key) {
      return fast::Print(
          RuntimeAssistedMutator::MemberAccess(t, mk::Id(t, "v"), key));
    };
    const std::string got = access("0") + " " + access("17") + " " +
                            access("01") + " " + access("4294967295") + " " +
                            access("foo") + " " + access("Symbol.iterator") +
                            " " + access("a b");
    const std::string want =
        "v[0] v[17] v[\"01\"] v[\"4294967295\"] v.foo v[Symbol.iterator] "
        "v[\"a b\"]";
    checker->Check("runtime-assisted: array index keys are numeric",
                   got == want, got);
  }

  // Probing: the probe comes before the first use, and a hit on "0" installs
  // `arr[0]`.
  {
    reg.Reset();
    std::string err;
    bool ok = reg.Set("mutators.Probing.chance", 1, &err) &&
              reg.Set("mutators.Probing.accessor_chance", 0, &err);
    ProgramPtr small = ParseOrDie(
        "let arr = [1, 2];\nlet y = arr[0] + arr.length;\n", "selftest");
    std::unique_ptr<fast::Tree> tree = small->CopyTree();
    Random rng(kSeed + 970);
    Builder b(tree.get(), &rng, &env);
    MutationContext ctx;
    ctx.env = &env;
    ctx.generators = generators.get();
    ProbingMutator* m = static_cast<ProbingMutator*>(mutators->Find("Probing"));
    ok = ok && m->Mutate(b, ctx) && ctx.probes.size() == 1;
    std::string text = fast::Print(tree->root());
    ok = ok && text.find("__redline_probe") < text.find("let y");
    if (ok) {
      const std::string fuzzout = "PROBE_HIT: {\"id\":\"" + ctx.probes[0].id +
                                  "\",\"kind\":\"get\",\"key\":\"0\"}\n";
      std::string detail;
      ok = m->Apply(b, ctx.probes, fuzzout, ctx, &detail) == 1;
      text = fast::Print(tree->root());
      ok = ok && text.find("arr[0] = ") != std::string::npos &&
           text.find("__redline_probe") == std::string::npos;
    }
    reg.Reset();
    checker->Check(
        "Probing: the probe precedes the uses and a hit on \"0\" installs "
        "arr[0]",
        ok, text);
  }

  // Explore: an action that fails on a later visit is not applied.
  {
    reg.Reset();
    bool ok = true;
    std::string err;
    for (bool later_failure : {false, true}) {
      ProgramPtr small = ParseOrDie("let o = {k: 1};\n", "selftest");
      std::unique_ptr<fast::Tree> tree = small->CopyTree();
      Random rng(kSeed + 980);
      Builder b(tree.get(), &rng, &env);
      MutationContext ctx;
      ctx.env = &env;
      ctx.generators = generators.get();
      Mutator* m = mutators->Find("Explore");
      if (!m->Mutate(b, ctx) || ctx.probes.size() != 1) {
        ok = false;
        err = "no probe";
        break;
      }
      const std::string& id = ctx.probes[0].id;
      std::string fuzzout =
          "EXPLORE_ACTION: {\"op\":\"getProp\",\"name\":\"k\","
          "\"id\":\"" +
          id + "\"}\n";
      if (later_failure) {
        fuzzout += "EXPLORE_FAILURE: " + id + "\n";
      }
      std::string detail;
      const int applied = ApplyExploreActions(b, ctx.probes, fuzzout, &detail);
      const std::string text = fast::Print(tree->root());
      if (applied != (later_failure ? 0 : 1) ||
          text.find("__redline_explore") != std::string::npos ||
          (text.find("o.k") != std::string::npos) == later_failure) {
        ok = false;
        err = text;
      }
    }
    checker->Check("Explore: an action that throws on a later visit is dropped",
                   ok, err);
  }

  // Fixup: unwraps the guards whose body ran and whose handler did not run,
  // with a block for lexical declarations. Other try statements stay. The
  // seeds give other orders of the nested guards.
  {
    const char* kGuards = R"(try { let x = 1; x++; } catch {}
try { var v = 2; v++; } catch (e) {}
try { class K {} } catch {}
try { const c = 3; } catch {}
try { function h() {} } catch {}
try { throw 1; } catch {}
try { v--; } catch (e) { v++; }
try { v--; } finally {}
if (v) try { v++; } catch {}
try { "use strict"; } catch {}
function never() { try { v++; } catch {} }
try { try { v--; } catch {} v++; } catch {}
new Worker(function() { try { v++; } catch {} }, {type: 'function'});
)";
    ProgramPtr guards = ParseOrDie(kGuards, "selftest");
    auto* m = static_cast<FixupMutator*>(mutators->Find("Fixup"));
    std::string err;
    bool ok = m != nullptr;
    for (int seed = 0; ok && seed < 8; seed++) {
      reg.Reset();
      ok = reg.Set("mutators.Fixup.max_guards", 64, &err);
      std::unique_ptr<fast::Tree> tree = guards->CopyTree();
      Random rng(kSeed + 990 + static_cast<uint64_t>(seed));
      Builder b(tree.get(), &rng, &env);
      MutationContext ctx;
      ctx.env = &env;
      ctx.generators = generators.get();
      if (!ok || !m->Mutate(b, ctx) || ctx.probes.size() != 11) {
        ok = false;
        err =
            StringPrintf("%zu guards instrumented, want 11", ctx.probes.size());
        break;
      }
      // The answers of the target: each body ran except the one in
      // never(), and the handler of `throw 1` ran.
      std::string fuzzout;
      for (fast::Node* n : b.Collect(
               [](fast::Node* n) { return n->Is<fast::TryStatement>(); })) {
        auto* guard = n->Cast<fast::TryStatement>();
        std::string text = fast::Print(guard->block);
        size_t at = text.find("__redline_fixup(\"");
        if (at == std::string::npos || at > 8) {
          continue;
        }
        const std::string id =
            text.substr(at + 17, text.find('"', at + 17) - at - 17);
        if (b.EnclosingFunction(guard) == nullptr) {
          fuzzout += "FIXUP_RAN: " + id + "\n";
        }
        if (guard->block->body.size() > 1 &&
            guard->block->body[1]->Is<fast::ThrowStatement>()) {
          fuzzout += "FIXUP_THREW: " + id + "\n";
        }
      }
      std::string detail;
      const int applied = m->Apply(b, ctx.probes, fuzzout, ctx, &detail);
      const std::string text = fast::Print(tree->root());
      const std::vector<fast::Statement*>& body = tree->root()->body;
      using K = fast::NodeKind;
      const K kWant[] = {
          K::kBlockStatement,       // let
          K::kVariableDeclaration,  // var
          K::kExpressionStatement,  // v++
          K::kBlockStatement,       // class
          K::kBlockStatement,       // const
          K::kBlockStatement,       // function
          K::kTryStatement,         // throw
          K::kTryStatement,         // a handler that is not empty
          K::kTryStatement,         // finally
          K::kIfStatement,
          K::kBlockStatement,        // a string that can become a directive
          K::kFunctionDeclaration,   // never(), its guard stays
          K::kExpressionStatement,   // v-- of the inner nested guard
          K::kExpressionStatement,   // v++ of the outer nested guard
          K::kExpressionStatement};  // the Worker
      bool shape = applied == 9 && body.size() == std::size(kWant);
      for (size_t i = 0; shape && i < body.size(); i++) {
        shape = body[i]->kind == kWant[i];
      }
      std::string parse_error;
      Lineage lineage;
      if (!shape ||
          !body[9]
               ->Cast<fast::IfStatement>()
               ->consequent->Is<fast::BlockStatement>() ||
          !body[11]
               ->Cast<fast::FunctionDeclaration>()
               ->body->body[0]
               ->Is<fast::TryStatement>() ||
          text.find("throw 1;\n} catch {}") == std::string::npos ||
          text.find("__redline_fixup") != std::string::npos ||
          Program::Parse(text, lineage, &parse_error) == nullptr) {
        ok = false;
        err = StringPrintf("seed %d, applied %d: ", seed, applied) + text +
              parse_error;
      }
    }
    reg.Reset();
    checker->Check(
        "Fixup: unwraps guards that ran clean, nested too, keeps a block for "
        "let/const/class/function, keeps other guards and skips Worker "
        "functions",
        ok, err);
  }

  // PropertyAccessor: its own mutator of weight 1. Expression makes no
  // accessors.
  {
    reg.Reset();
    const char* kAccessors = R"(let o = {a: 1, m() { return 1; }};
class C {
  f = 2;
  meth() { return 3; }
}
o.b = 5;
)";
    ProgramPtr accessors = ParseOrDie(kAccessors, "selftest");
    Mutator* pa = mutators->Find("PropertyAccessor");
    Mutator* expression = mutators->Find("Expression");
    bool ok = pa != nullptr && expression != nullptr && pa->weight() == 1;
    std::string err = ok ? "" : "no PropertyAccessor mutator of weight 1";
    for (int i = 0; ok && i < 40; i++) {
      Random rng(kSeed + 1000 + static_cast<uint64_t>(i));
      std::string detail;
      std::unique_ptr<fast::Tree> tree = ApplyMutation(
          *pa, *accessors, nullptr, rng, env, *generators, 1, &detail);
      const std::string text = tree != nullptr ? fast::Print(tree->root()) : "";
      std::string parse_error;
      Lineage lineage;
      if (tree == nullptr ||
          (text.find("get ") == std::string::npos &&
           text.find("set ") == std::string::npos &&
           text.find("defineProperty") == std::string::npos) ||
          Program::Parse(text, lineage, &parse_error) == nullptr) {
        ok = false;
        err = detail + ": " + text + parse_error;
      }
      Random rng2(kSeed + 1000 + static_cast<uint64_t>(i));
      tree = ApplyMutation(*expression, *accessors, nullptr, rng2, env,
                           *generators, 1, &detail);
      if (detail.find("getter") != std::string::npos ||
          detail.find("setter") != std::string::npos ||
          detail.find("accessor") != std::string::npos) {
        ok = false;
        err = "Expression: " + detail;
      }
    }
    checker->Check(
        "PropertyAccessor: weight 1, makes valid getters and setters; "
        "Expression makes none",
        ok, err);
  }
}

// Receiver-targeted JIT: with the receiver chance at 1, JitOptimize,
// JitBaseline and HotLoop compile methods, accessors and constructors, and
// call them with their receiver.
void CheckReceiverJit(Checker* checker) {
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
  const Environment& env = Environment::Default();
  TunableRegistry& reg = TunableRegistry::Instance();
  const char* kReceivers = R"(class A {
  constructor(x) { this.x = x; }
  m(a) { return a + 1; }
  get g() { return 1; }
  set s(v) { this.x = v; }
  static sm() { return 2; }
  static get sg() { return 3; }
}
let inst = new A(1);
let o = {k(v) { return v; }, get og() { return 4; }};
)";
  ProgramPtr program = ParseOrDie(kReceivers, "selftest");
  reg.Reset();
  std::string err;
  bool ok = reg.Set("generators.JitOptimize.receiver_chance", 1, &err) &&
            reg.Set("generators.HotLoop.receiver_chance", 1, &err);
  // What the outputs must contain, over all samples.
  const std::vector<std::string> kWanted = {
      "inst.m(",
      "o.k(",
      "A.sm(",
      "new A(",
      "Object.getOwnPropertyDescriptor(Object.getPrototypeOf(inst), \"g\").get",
      "Object.getOwnPropertyDescriptor(Object.getPrototypeOf(inst), \"s\").set",
      "Object.getOwnPropertyDescriptor(A, \"sg\").get",
      "Object.getOwnPropertyDescriptor(o, \"og\").get",
      ".call(inst",
      ".call(o",
      "%PrepareFunctionForOptimization(jit",
      "%CompileBaseline(jit",
      "OnNextCall(A)",
  };
  std::set<std::string> seen;
  for (const char* name : {"JitOptimize", "JitBaseline", "HotLoop"}) {
    Generator* gen = generators->Find(name);
    if (gen == nullptr) {
      ok = false;
      err = std::string("no generator ") + name;
      break;
    }
    for (int i = 0; ok && i < 150; i++) {
      std::unique_ptr<fast::Tree> tree = program->CopyTree();
      Random rng(kSeed + 1100 + static_cast<uint64_t>(i));
      Builder b(tree.get(), &rng, &env);
      Builder::Slot slot{&tree->root()->body, tree->root()->body.size(),
                         tree->root()};
      GenContext ctx = MakeGenContext(b, slot, *generators, env);
      std::vector<fast::Statement*> out;
      ctx.out = &out;
      if (!gen->Generate(ctx, &out) || out.empty()) {
        continue;
      }
      b.InsertAll(slot, out);
      const std::string text = fast::Print(tree->root());
      std::string parse_error;
      Lineage lineage;
      if (Program::Parse(text, lineage, &parse_error) == nullptr) {
        ok = false;
        err = std::string(name) + ": " + parse_error + "\n" + text;
        break;
      }
      for (const std::string& wanted : kWanted) {
        if (text.find(wanted) != std::string::npos) {
          seen.insert(wanted);
        }
      }
    }
  }
  for (const std::string& wanted : kWanted) {
    if (ok && seen.count(wanted) == 0) {
      ok = false;
      err = "never generated: " + wanted;
    }
  }
  reg.Reset();
  checker->Check(
      "JIT generators: methods, getters, setters and constructors are "
      "compiled and called with their receiver",
      ok, err);
}

// Verifies environment-backed type lookups and `Builder` type inference.
void CheckEnvTyper(Checker* checker) {
  const Environment& env = Environment::Default();

  // 1. Direct O(1) lookup helpers on `Environment`.
  {
    std::string lookup_err;
    bool lookups_ok = true;
    auto check_eq = [&](const char* label, JsType got, JsType want) {
      if (got != want) {
        lookups_ok = false;
        if (!lookup_err.empty()) lookup_err += ", ";
        lookup_err += std::string(label) + "=" + JsTypeName(got) + " (want " +
                      JsTypeName(want) + ")";
      }
    };
    check_eq("ctor Float16Array", env.LookupGlobalCtorType("Float16Array"),
             JsType::kTypedArray);
    check_eq("ctor WeakSet", env.LookupGlobalCtorType("WeakSet"),
             JsType::kWeakSet);
    check_eq("ctor WeakMap", env.LookupGlobalCtorType("WeakMap"),
             JsType::kWeakMap);
    check_eq("ctor Set", env.LookupGlobalCtorType("Set"), JsType::kSet);
    check_eq("ctor Map", env.LookupGlobalCtorType("Map"), JsType::kMap);
    check_eq("ctor RegExp", env.LookupGlobalCtorType("RegExp"),
             JsType::kRegExp);
    check_eq("ctor Promise", env.LookupGlobalCtorType("Promise"),
             JsType::kPromise);
    check_eq("ctor ArrayBuffer", env.LookupGlobalCtorType("ArrayBuffer"),
             JsType::kArrayBuffer);
    check_eq("ctor DataView", env.LookupGlobalCtorType("DataView"),
             JsType::kDataView);
    check_eq("ctor SuppressedError",
             env.LookupGlobalCtorType("SuppressedError"), JsType::kError);
    check_eq("ctor String", env.LookupGlobalCtorType("String"),
             JsType::kUnknown);
    check_eq("ctor empty", env.LookupGlobalCtorType(""), JsType::kUnknown);
    check_eq("ns ctor Intl.Locale",
             env.LookupNamespaceCtorType("Intl", "Locale"),
             JsType::kIntlLocale);
    check_eq("ns ctor Temporal.PlainDate",
             env.LookupNamespaceCtorType("Temporal", "PlainDate"),
             JsType::kTemporalPlainDate);
    check_eq("ns ctor Temporal.ZonedDateTime",
             env.LookupNamespaceCtorType("Temporal", "ZonedDateTime"),
             JsType::kTemporalZonedDateTime);
    check_eq("ns ctor Temporal.Now",
             env.LookupNamespaceCtorType("Temporal", "Now"), JsType::kUnknown);
    check_eq("ns ctor Intl.DateTimeFormat",
             env.LookupNamespaceCtorType("Intl", "DateTimeFormat"),
             JsType::kUnknown);
    check_eq("call String", env.LookupGlobalCallReturnType("String"),
             JsType::kString);
    check_eq("call Number", env.LookupGlobalCallReturnType("Number"),
             JsType::kNumber);
    check_eq("call Date", env.LookupGlobalCallReturnType("Date"),
             JsType::kString);
    check_eq("call parseInt", env.LookupGlobalCallReturnType("parseInt"),
             JsType::kNumber);
    check_eq("call encodeURI", env.LookupGlobalCallReturnType("encodeURI"),
             JsType::kString);
    check_eq("call gc", env.LookupGlobalCallReturnType("gc"), JsType::kNullish);
    check_eq("call Map", env.LookupGlobalCallReturnType("Map"),
             JsType::kUnknown);
    check_eq("call SuppressedError",
             env.LookupGlobalCallReturnType("SuppressedError"),
             JsType::kUnknown);
    check_eq("ns call Object.keys",
             env.LookupNamespaceCallReturnType("Object", "keys"),
             JsType::kArray);
    check_eq("ns call Array.from",
             env.LookupNamespaceCallReturnType("Array", "from"),
             JsType::kArray);
    check_eq("ns call Array.fromAsync",
             env.LookupNamespaceCallReturnType("Array", "fromAsync"),
             JsType::kPromise);
    check_eq("ns call Iterator.zip",
             env.LookupNamespaceCallReturnType("Iterator", "zip"),
             JsType::kIterator);
    check_eq("ns call Reflect.get",
             env.LookupNamespaceCallReturnType("Reflect", "get"),
             JsType::kUnknown);
    check_eq("ns call Reflect.ownKeys",
             env.LookupNamespaceCallReturnType("Reflect", "ownKeys"),
             JsType::kArray);
    check_eq("ns call Promise.resolve",
             env.LookupNamespaceCallReturnType("Promise", "resolve"),
             JsType::kPromise);
    check_eq("ns call Promise.reject",
             env.LookupNamespaceCallReturnType("Promise", "reject"),
             JsType::kPromise);
    check_eq("ns call JSON.parse",
             env.LookupNamespaceCallReturnType("JSON", "parse"),
             JsType::kUnknown);
    check_eq("ns call JSON.stringify",
             env.LookupNamespaceCallReturnType("JSON", "stringify"),
             JsType::kString);
    check_eq("ns call Uint8Array.fromBase64",
             env.LookupNamespaceCallReturnType("Uint8Array", "fromBase64"),
             JsType::kTypedArray);
    check_eq("ns call Temporal.Now.instant",
             env.LookupNamespaceCallReturnType("Temporal.Now", "instant"),
             JsType::kTemporalInstant);
    check_eq("ns call Temporal.PlainDate.compare",
             env.LookupNamespaceCallReturnType("Temporal.PlainDate", "compare"),
             JsType::kNumber);
    check_eq("ns call WebAssembly.validate",
             env.LookupNamespaceCallReturnType("WebAssembly", "validate"),
             JsType::kBoolean);
    check_eq("method Array.values",
             env.LookupMethodReturnType(JsType::kArray, "values"),
             JsType::kIterator);
    check_eq("method TypedArray.subarray",
             env.LookupMethodReturnType(JsType::kTypedArray, "subarray"),
             JsType::kTypedArray);
    check_eq("method TypedArray.map",
             env.LookupMethodReturnType(JsType::kTypedArray, "map"),
             JsType::kTypedArray);
    check_eq("method Iterator.map",
             env.LookupMethodReturnType(JsType::kIterator, "map"),
             JsType::kIterator);
    check_eq("method Iterator.toArray",
             env.LookupMethodReturnType(JsType::kIterator, "toArray"),
             JsType::kArray);
    check_eq("method String.match",
             env.LookupMethodReturnType(JsType::kString, "match"),
             JsType::kUnknown);
    check_eq("method String.split",
             env.LookupMethodReturnType(JsType::kString, "split"),
             JsType::kArray);
    check_eq("method Date.getTime",
             env.LookupMethodReturnType(JsType::kDate, "getTime"),
             JsType::kNumber);
    check_eq("method TemporalPlainDate.until",
             env.LookupMethodReturnType(JsType::kTemporalPlainDate, "until"),
             JsType::kTemporalDuration);
    check_eq("method Map.toString",
             env.LookupMethodReturnType(JsType::kMap, "toString"),
             JsType::kString);
    check_eq("method Unknown.values",
             env.LookupMethodReturnType(JsType::kUnknown, "values"),
             JsType::kUnknown);
    check_eq("prop TypedArray.buffer",
             env.LookupPropertyType(JsType::kTypedArray, "buffer"),
             JsType::kArrayBuffer);
    check_eq("prop Array.length",
             env.LookupPropertyType(JsType::kArray, "length"), JsType::kNumber);
    check_eq("prop Map.size", env.LookupPropertyType(JsType::kMap, "size"),
             JsType::kNumber);
    check_eq("prop RegExp.flags",
             env.LookupPropertyType(JsType::kRegExp, "flags"), JsType::kString);
    check_eq("prop RegExp.global",
             env.LookupPropertyType(JsType::kRegExp, "global"),
             JsType::kBoolean);
    check_eq("prop TemporalZonedDateTime.epochNanoseconds",
             env.LookupPropertyType(JsType::kTemporalZonedDateTime,
                                    "epochNanoseconds"),
             JsType::kBigInt);
    check_eq("prop IntlLocale.numeric",
             env.LookupPropertyType(JsType::kIntlLocale, "numeric"),
             JsType::kBoolean);
    check_eq("ns prop Math.PI", env.LookupNamespacePropertyType("Math", "PI"),
             JsType::kNumber);
    check_eq("ns prop RegExp.lastMatch",
             env.LookupNamespacePropertyType("RegExp", "lastMatch"),
             JsType::kString);
    check_eq("ns prop Symbol.iterator",
             env.LookupNamespacePropertyType("Symbol", "iterator"),
             JsType::kSymbol);
    checker->Check("env-typer: Environment O(1) lookup helpers", lookups_ok,
                   lookup_err);
  }

  // 2. Typed inference across re-parse and `env == nullptr` compatibility.
  {
    const char* kCode = R"(
let ta = new Float16Array(8);
let taSub = ta.subarray(1);
let taMap = ta.map(x => x);
let u8b64 = Uint8Array.fromBase64("AA==");
let ab = ta.buffer;
let abSlice = ab.slice(0, 4);
let abRes = ab.resizable;
let arr = [1, 2, 3];
let arrLen = arr.length;
let it = arr.values();
let it2 = it.map(x => x).take(2);
let itArr = it2.toArray();
let itZip = Iterator.zip([arr]);
let ws = new WeakSet();
let ws2 = ws.add({});
let st = new Set([1]);
let st2 = st.union(st);
let mp = new Map();
let mp2 = mp.set(1, 2);
let mpSize = mp.size;
let re = new RegExp("a");
let reFlags = re.flags;
let reComp = re.compile(/b/);
let pr = Promise.resolve(1);
let pr2 = pr.then(() => 2);
let prRej = Promise.reject(1);
let loc = new Intl.Locale("en");
let locLang = loc.language;
let locNum = loc.numeric;
let pd = Temporal.PlainDate.from("2024-01-31");
let dur = pd.until(pd);
let inst = Temporal.Now.instant();
let zdt = new Temporal.ZonedDateTime(0n, "UTC");
let zdtNs = zdt.epochNanoseconds;
let str = "abc";
let matchArr = str.match(/a/);
let splitArr = str.split("b");
let keysArr = Object.keys({});
let err = new SuppressedError(1, 2, "e");
let errMsg = err.message;
let pi = Math.PI;
let lastM = RegExp.lastMatch;
let dateStr = Date();
let optAb = ta?.buffer;
function f() {
  class Float16Array {}
  let shadowedCtor = new Float16Array();
  let Object = { keys() { return 1; } };
  let shadowedCall = Object.keys();
}
)";
    ProgramPtr program = ParseOrDie(kCode, "selftest");
    std::unique_ptr<fast::Tree> tree = program->CopyTree();
    Random rng(kSeed + 1200);
    Builder builder(tree.get(), &rng, &env);
    auto type_of = [](Builder& b, std::string_view name) {
      for (const fast::Binding* binding : b.scopes().bindings()) {
        if (binding->name == name) {
          return b.GuessType(binding);
        }
      }
      return JsType::kUnknown;
    };
    std::string detail;
    auto expect = [&](std::string_view name, JsType want) {
      JsType got = type_of(builder, name);
      if (got != want) {
        if (!detail.empty()) detail += ", ";
        detail += std::string(name) + "=" + JsTypeName(got) + " (want " +
                  JsTypeName(want) + ")";
        return false;
      }
      return true;
    };
    bool ok =
        expect("ta", JsType::kTypedArray) &&
        expect("taSub", JsType::kTypedArray) &&
        expect("taMap", JsType::kTypedArray) &&
        expect("u8b64", JsType::kTypedArray) &&
        expect("ab", JsType::kArrayBuffer) &&
        expect("abSlice", JsType::kArrayBuffer) &&
        expect("abRes", JsType::kBoolean) && expect("arr", JsType::kArray) &&
        expect("arrLen", JsType::kNumber) && expect("it", JsType::kIterator) &&
        expect("it2", JsType::kIterator) && expect("itArr", JsType::kArray) &&
        expect("itZip", JsType::kIterator) && expect("ws", JsType::kWeakSet) &&
        expect("ws2", JsType::kWeakSet) && expect("st", JsType::kSet) &&
        expect("st2", JsType::kSet) && expect("mp", JsType::kMap) &&
        expect("mp2", JsType::kMap) && expect("mpSize", JsType::kNumber) &&
        expect("re", JsType::kRegExp) && expect("reFlags", JsType::kString) &&
        expect("reComp", JsType::kRegExp) && expect("pr", JsType::kPromise) &&
        expect("pr2", JsType::kPromise) && expect("prRej", JsType::kPromise) &&
        expect("loc", JsType::kIntlLocale) &&
        expect("locLang", JsType::kString) &&
        expect("locNum", JsType::kBoolean) &&
        expect("pd", JsType::kTemporalPlainDate) &&
        expect("dur", JsType::kTemporalDuration) &&
        expect("inst", JsType::kTemporalInstant) &&
        expect("zdt", JsType::kTemporalZonedDateTime) &&
        expect("zdtNs", JsType::kBigInt) && expect("str", JsType::kString) &&
        expect("matchArr", JsType::kUnknown) &&
        expect("splitArr", JsType::kArray) &&
        expect("keysArr", JsType::kArray) && expect("err", JsType::kError) &&
        expect("errMsg", JsType::kString) && expect("pi", JsType::kNumber) &&
        expect("lastM", JsType::kString) &&
        expect("dateStr", JsType::kString) &&
        expect("optAb", JsType::kArrayBuffer) &&
        expect("shadowedCtor", JsType::kObject) &&
        expect("shadowedCall", JsType::kUnknown);

    // Check `VisibleAtSlot` preserves typed receivers at the end of the file.
    Builder::Slot end_slot{&tree->root()->body, tree->root()->body.size(),
                           tree->root()};
    std::vector<Variable> visible = builder.VisibleAtSlot(end_slot);
    auto visible_is = [&](std::string_view name, JsType want) {
      for (const Variable& v : visible) {
        if (v.name == name) {
          return v.type == want;
        }
      }
      return false;
    };
    ok = ok && visible_is("taSub", JsType::kTypedArray) &&
         visible_is("it2", JsType::kIterator) &&
         visible_is("ab", JsType::kArrayBuffer) &&
         visible_is("loc", JsType::kIntlLocale) &&
         visible_is("ws2", JsType::kWeakSet);

    // Without an `Environment`, `Builder` stays backward-compatible and safe.
    Builder no_env_builder(tree.get(), &rng);
    const bool no_env_ok =
        type_of(no_env_builder, "ta") == JsType::kTypedArray &&
        type_of(no_env_builder, "arrLen") == JsType::kNumber &&
        type_of(no_env_builder, "ab") == JsType::kUnknown &&
        type_of(no_env_builder, "taSub") == JsType::kUnknown;
    no_env_builder.SetEnvironment(&env);
    const bool set_env_ok =
        type_of(no_env_builder, "ab") == JsType::kArrayBuffer &&
        type_of(no_env_builder, "taSub") == JsType::kTypedArray;
    checker->Check(
        "env-typer: re-parse preserves typed receivers and env==nullptr stays "
        "safe",
        ok && no_env_ok && set_env_ok, detail);
  }

  // 3. Partial/null AST nodes and deep AST chains stay safe and bounded.
  {
    auto tree = std::make_unique<fast::Tree>();
    tree->set_root(tree->New<fast::Program>());
    Random rng(kSeed + 1201);
    Builder builder(tree.get(), &rng, &env);

    auto* empty_new = tree->New<fast::NewExpression>();
    auto* empty_call = tree->New<fast::CallExpression>();
    auto* empty_member = tree->New<fast::MemberExpression>();
    auto* call_on_null = tree->New<fast::CallExpression>();
    call_on_null->callee = empty_member;
    auto* empty_unary = tree->New<fast::UnaryExpression>();
    auto* empty_binary = tree->New<fast::BinaryExpression>();
    auto* empty_chain = tree->New<fast::ChainExpression>();
    auto* empty_seq = tree->New<fast::SequenceExpression>();
    const bool null_safe =
        builder.GuessExpressionType(nullptr) == JsType::kUnknown &&
        builder.GuessExpressionType(empty_new) == JsType::kObject &&
        builder.GuessExpressionType(empty_call) == JsType::kUnknown &&
        builder.GuessExpressionType(empty_member) == JsType::kUnknown &&
        builder.GuessExpressionType(call_on_null) == JsType::kUnknown &&
        builder.GuessExpressionType(empty_unary) == JsType::kNumber &&
        builder.GuessExpressionType(empty_binary) == JsType::kUnknown &&
        builder.GuessExpressionType(empty_chain) == JsType::kUnknown &&
        builder.GuessExpressionType(empty_seq) == JsType::kUnknown &&
        Builder::GuessType(empty_call, &env) == JsType::kUnknown;

    // 2000-deep chained AST must terminate safely without stack overflow.
    fast::Expression* deep = tree->New<fast::Identifier>();
    deep->Cast<fast::Identifier>()->name = "ta";
    for (int i = 0; i < 2000; ++i) {
      auto* m = tree->New<fast::MemberExpression>();
      m->object = deep;
      auto* prop = tree->New<fast::Identifier>();
      prop->name = "buffer";
      m->property = prop;
      deep = m;
    }
    const bool deep_ok =
        builder.GuessExpressionType(deep) == JsType::kUnknown &&
        Builder::GuessType(deep, &env) == JsType::kUnknown;

    checker->Check("env-typer: null AST nodes and deep expressions stay safe",
                   null_safe && deep_ok);
  }
}

// Offline checks for `Operation` and `Call` mutator parity: RegExp flags and
// pattern edits, BigInt edits, member keys, spread toggles, and optional
// chaining guards.
void CheckOperationAndCallParity(Checker* checker) {
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
  std::unique_ptr<MutatorSet> mutators = MutatorSet::CreateDefault();
  const Environment& env = Environment::Default();
  TunableRegistry& reg = TunableRegistry::Instance();
  Mutator* op_mut = mutators->Find("Operation");
  Mutator* call_mut = mutators->Find("Call");
  std::string v8_err;
  const bool v8_ok = InitializeV8(FuzzConfig().v8_flags, &v8_err);
  std::unique_ptr<SyntaxChecker> syntax;
  if (v8_ok) {
    syntax = std::make_unique<SyntaxChecker>();
  }
  auto accepts = [&](const std::string& src, std::string* err) {
    fast::ParseError perr;
    if (fast::Parser::ParseScript(src, fast::ParseOptions(), &perr) ==
        nullptr) {
      *err = "FAST: " + perr.message + "\n" + src;
      return false;
    }
    if (syntax != nullptr && !syntax->Accepts(src, err)) {
      *err = "V8: " + *err + "\n" + src;
      return false;
    }
    return true;
  };

  // 1. RegExp literal mutations: neutral flags, `u` <-> `v` mutual exclusion,
  // and safe in-place pattern piece edits.
  {
    const char* kRegexps = R"(let r0 = /abc/u;
let r1 = /[\p{L}--[a-z]]/v;
let r2 = /a\q/;
let r3 = /[a-z]+/gi;
let r4 = /(?:foo)+/m;
let r5 = /^bar$/s;
let r6 = /\d+/;
)";
    ProgramPtr prog = ParseOrDie(kRegexps, "selftest");
    bool ok = op_mut != nullptr && v8_ok;
    std::string err = ok ? "" : "missing Operation mutator or V8";
    bool saw_flag = false;
    bool saw_mode = false;
    bool saw_piece = false;
    bool saw_replace = false;
    for (int i = 0; ok && i < 200; i++) {
      Random rng(kSeed + 1200 + static_cast<uint64_t>(i));
      std::string detail;
      std::unique_ptr<fast::Tree> tree = ApplyMutation(
          *op_mut, *prog, nullptr, rng, env, *generators, 2, &detail);
      if (tree == nullptr) {
        continue;
      }
      if (detail.find("regexp flags") != std::string::npos) saw_flag = true;
      if (detail.find("regexp mode") != std::string::npos) saw_mode = true;
      if (detail.find("regexp pattern:") != std::string::npos) saw_piece = true;
      if (detail.find("regexp replaced") != std::string::npos ||
          detail.find("regexp pattern replaced") != std::string::npos) {
        saw_replace = true;
      }
      fast::Walk(tree->root(), [&](fast::Node* n) {
        if (auto* re = n->As<fast::RegExpLiteral>()) {
          bool has_u = re->regexp_flags.find('u') != std::string_view::npos;
          bool has_v = re->regexp_flags.find('v') != std::string_view::npos;
          if (re->pattern.empty() || (has_u && has_v)) {
            ok = false;
            err = "invalid regexp /" + std::string(re->pattern) + "/" +
                  std::string(re->regexp_flags);
          }
        }
        return ok;
      });
      if (ok && !accepts(fast::Print(tree->root()), &err)) {
        ok = false;
      }
    }
    checker->Check(
        "Operation: RegExp flag, mode (`u`/`v` exclusive), and pattern edits "
        "stay valid",
        ok && saw_flag && saw_mode && saw_piece && saw_replace,
        err.empty() ? StringPrintf("flag=%d mode=%d piece=%d replace=%d",
                                   saw_flag, saw_mode, saw_piece, saw_replace)
                    : err);
  }

  // 2. BigInt literal mutations: boundaries, bit/shift edits, safe sign
  // toggles (never `--1n`, `-1n ** 2n`, or negative non-computed keys).
  {
    const char* kBigInts = R"(let a = 1n;
let b = -42n;
let c = 1n ** 2n;
let d = 0x10n;
let e = 340282366920938463463374607431768211456n;
let o = {1n: 1};
class C { 2n() {} 3n = 4; }
)";
    ProgramPtr prog = ParseOrDie(kBigInts, "selftest");
    bool ok = op_mut != nullptr && v8_ok;
    std::string err;
    bool saw_value = false;
    bool saw_neg = false;
    bool saw_pos = false;
    for (int mode = 0; ok && mode < 2; mode++) {
      reg.Reset();
      if (mode == 1) {
        ok = reg.Set("mutators.Operation.bigint_sign_chance", 1, &err);
      }
      for (int i = 0; ok && i < 120; i++) {
        Random rng(kSeed + 1400 + static_cast<uint64_t>(mode * 200 + i));
        std::string detail;
        std::unique_ptr<fast::Tree> tree = ApplyMutation(
            *op_mut, *prog, nullptr, rng, env, *generators, 2, &detail);
        if (tree == nullptr) {
          continue;
        }
        if (detail.find("bigint ") != std::string::npos &&
            detail.find("bigint sign") == std::string::npos) {
          saw_value = true;
        }
        if (detail.find("-> -") != std::string::npos) saw_neg = true;
        if (detail.find("bigint sign -") != std::string::npos) saw_pos = true;
        std::string src = fast::Print(tree->root());
        if (!accepts(src, &err)) {
          ok = false;
        }
      }
    }
    reg.Reset();
    checker->Check(
        "Operation: BigInt value and sign edits stay valid in all contexts",
        ok && saw_value && saw_neg && saw_pos,
        err.empty() ? StringPrintf("value=%d neg=%d pos=%d", saw_value, saw_neg,
                                   saw_pos)
                    : err);
  }

  // 3. Member property key edits: known keys, env names, numeric indices,
  // well-known symbols, and `let.x` statement or `for-in` safety.
  {
    const char* kMembers = R"(let o = {alpha: 1, beta: 2};
let a = [1, 2];
let r = o.alpha + a[0] + o[Symbol.iterator];
let.x = 1;
for (let.x in a) {}
)";
    ProgramPtr prog = ParseOrDie(kMembers, "selftest");
    reg.Reset();
    std::string err;
    bool ok = op_mut != nullptr && v8_ok &&
              reg.Set("mutators.Operation.optional_chain_chance", 0, &err);
    bool saw_known = false;
    bool saw_index = false;
    bool saw_symbol = false;
    bool saw_dot_from_computed = false;
    for (int i = 0; ok && i < 200; i++) {
      Random rng(kSeed + 1800 + static_cast<uint64_t>(i));
      std::string detail;
      std::unique_ptr<fast::Tree> tree = ApplyMutation(
          *op_mut, *prog, nullptr, rng, env, *generators, 1, &detail);
      if (tree == nullptr) {
        continue;
      }
      if (detail.find(".alpha -> .beta") != std::string::npos) saw_known = true;
      if (detail.find("-> [Symbol.") != std::string::npos) saw_symbol = true;
      if (detail.find("-> [") != std::string::npos &&
          detail.find("Symbol.") == std::string::npos) {
        saw_index = true;
      }
      if (detail.find("[0] -> .") != std::string::npos ||
          detail.find("[Symbol.iterator] -> .") != std::string::npos) {
        saw_dot_from_computed = true;
      }
      if (!accepts(fast::Print(tree->root()), &err)) {
        ok = false;
      }
    }
    reg.Reset();
    checker->Check(
        "Operation: member key edits cover known keys, indices, and symbols",
        ok && saw_known && saw_index && saw_symbol && saw_dot_from_computed,
        err.empty()
            ? StringPrintf("known=%d index=%d symbol=%d dot=%d", saw_known,
                           saw_index, saw_symbol, saw_dot_from_computed)
            : err);
  }

  // 4. Optional chaining toggles on members: valid positions toggle both ways,
  // forbidden positions never turn into `?.`.
  {
    const char* kValidOpt = R"(let r = o.a.b + o[0] + o?.c + delete o.d;
class C { #p = 1; m() { return this.#p; } }
)";
    const char* kForbiddenOpt = R"(o.a = 1;
o.a.b += 2;
o.a++;
++o.b.c;
for (o.a in x) {}
for (o.a.b of x) {}
[o.a] = [1];
({k: o.a.b} = x);
new o.a();
new o.a.b();
o.a`hi`;
o.a.b`hi`;
class D extends B { m() { return super.x + super[0]; } }
)";
    ProgramPtr valid_prog = ParseOrDie(kValidOpt, "selftest");
    ProgramPtr forbid_prog = ParseOrDie(kForbiddenOpt, "selftest");
    reg.Reset();
    std::string err;
    bool ok = op_mut != nullptr && v8_ok &&
              reg.Set("mutators.Operation.optional_chain_chance", 1, &err);
    bool saw_on = false;
    bool saw_off = false;
    for (int i = 0; ok && i < 150; i++) {
      Random rng(kSeed + 2000 + static_cast<uint64_t>(i));
      std::string detail;
      std::unique_ptr<fast::Tree> tree = ApplyMutation(
          *op_mut, *valid_prog, nullptr, rng, env, *generators, 1, &detail);
      if (tree == nullptr) {
        continue;
      }
      if (detail.find("member -> optional member") != std::string::npos) {
        saw_on = true;
      }
      if (detail.find("optional member -> member") != std::string::npos) {
        saw_off = true;
      }
      if (!accepts(fast::Print(tree->root()), &err)) {
        ok = false;
      }
    }
    for (int i = 0; ok && i < 150; i++) {
      Random rng(kSeed + 2200 + static_cast<uint64_t>(i));
      std::string detail;
      std::unique_ptr<fast::Tree> tree = ApplyMutation(
          *op_mut, *forbid_prog, nullptr, rng, env, *generators, 1, &detail);
      if (tree == nullptr) {
        continue;
      }
      std::string src = fast::Print(tree->root());
      if (src.find("?.") != std::string::npos) {
        ok = false;
        err = "forbidden optional member created: " + detail + "\n" + src;
      } else if (!accepts(src, &err)) {
        ok = false;
      }
    }
    reg.Reset();
    checker->Check(
        "Operation: optional member toggles work and skip forbidden LHS/new/"
        "super/tag positions",
        ok && saw_on && saw_off,
        err.empty() ? StringPrintf("on=%d off=%d", saw_on, saw_off) : err);
  }

  // 5. Call mutator: spread toggles on call and new, and optional call toggles
  // with negative guards for `super(...)`, `new`, LHS chains, and tags.
  {
    const char* kCalls = R"(let a = [1, 2];
let r = f(a, 1) + f(...a) + new C(a, 2) + new C(...a) + g?.(a);
)";
    const char* kForbiddenCalls = R"(class D extends B {
  constructor() { super(1); }
}
new C(1);
new (f())(1);
f().x = 1;
f().x++;
for (f().x in o) {}
for (f().x of o) {}
f()`tag`;
)";
    ProgramPtr calls_prog = ParseOrDie(kCalls, "selftest");
    ProgramPtr forbid_prog = ParseOrDie(kForbiddenCalls, "selftest");
    reg.Reset();
    std::string err;
    bool ok = call_mut != nullptr && v8_ok &&
              reg.Set("mutators.Call.spread_toggle_chance", 1, &err) &&
              reg.Set("mutators.Call.optional_call_chance", 0, &err);
    bool saw_spread = false;
    bool saw_unwrap = false;
    for (int i = 0; ok && i < 120; i++) {
      Random rng(kSeed + 2400 + static_cast<uint64_t>(i));
      std::string detail;
      std::unique_ptr<fast::Tree> tree = ApplyMutation(
          *call_mut, *calls_prog, nullptr, rng, env, *generators, 1, &detail);
      if (tree == nullptr) {
        continue;
      }
      if (detail.find("unwrapped spread argument") != std::string::npos) {
        saw_unwrap = true;
      } else if (detail.find("spread argument") != std::string::npos) {
        saw_spread = true;
      }
      if (!accepts(fast::Print(tree->root()), &err)) {
        ok = false;
      }
    }
    reg.Reset();
    ok = ok && reg.Set("mutators.Call.optional_call_chance", 1, &err) &&
         reg.Set("mutators.Call.spread_toggle_chance", 0, &err);
    bool saw_opt_on = false;
    bool saw_opt_off = false;
    for (int i = 0; ok && i < 120; i++) {
      Random rng(kSeed + 2600 + static_cast<uint64_t>(i));
      std::string detail;
      std::unique_ptr<fast::Tree> tree = ApplyMutation(
          *call_mut, *calls_prog, nullptr, rng, env, *generators, 1, &detail);
      if (tree == nullptr) {
        continue;
      }
      if (detail.find("call -> optional call") != std::string::npos) {
        saw_opt_on = true;
      }
      if (detail.find("optional call -> call") != std::string::npos) {
        saw_opt_off = true;
      }
      if (!accepts(fast::Print(tree->root()), &err)) {
        ok = false;
      }
    }
    for (int i = 0; ok && i < 120; i++) {
      Random rng(kSeed + 2800 + static_cast<uint64_t>(i));
      std::string detail;
      std::unique_ptr<fast::Tree> tree = ApplyMutation(
          *call_mut, *forbid_prog, nullptr, rng, env, *generators, 1, &detail);
      if (tree == nullptr) {
        continue;
      }
      std::string src = fast::Print(tree->root());
      if (src.find("?.") != std::string::npos) {
        ok = false;
        err = "forbidden optional call created: " + detail + "\n" + src;
      } else if (!accepts(src, &err)) {
        ok = false;
      }
    }
    reg.Reset();
    checker->Check(
        "Call: spread toggles and optional call toggles stay valid and respect "
        "grammar guards",
        ok && saw_spread && saw_unwrap && saw_opt_on && saw_opt_off,
        err.empty()
            ? StringPrintf("spread=%d unwrap=%d on=%d off=%d", saw_spread,
                           saw_unwrap, saw_opt_on, saw_opt_off)
            : err);
  }
}

// Verifies `JitCompound` preset weights, patterns, strict mode, `natives_`,
// and `Builder::focus_subtree()` behavior.
void CheckJitCompound(Checker* checker) {
  std::unique_ptr<MutatorSet> defaults = MutatorSet::CreateDefault();
  const Environment& env = Environment::Default();
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();

  // Default weight is `0.0`, and `maglev` or `turbolev` set it to `0.5`.
  Mutator* jc_default = defaults->Find("JitCompound");
  bool weight_ok = jc_default != nullptr && jc_default->weight() == 0.0 &&
                   !jc_default->needs_partner() &&
                   defaults->WeightOf(jc_default) == 0.0;
  std::string weight_err;
  auto preset_weight = [&](const std::string& name, double* out) {
    std::string arg = "--preset=" + name;
    const char* argv[] = {"redline", "fuzz", arg.c_str()};
    Flags flags = Flags::Parse(3, argv, 2);
    FuzzConfig cfg;
    std::string err;
    bool loaded = FuzzConfig::Load(flags, &cfg, &err, /*for_run=*/false);
    TunableRegistry::Instance().Reset();
    if (!loaded) {
      weight_err = name + ": " + err;
      return false;
    }
    std::unique_ptr<MutatorSet> mset = MutatorSet::CreateDefault();
    std::unique_ptr<Selector<Mutator>> sel =
        SelectorRegistry<Mutator>::Instance().Create(cfg.selection.mutators,
                                                     &err);
    if (sel == nullptr || !mset->SetSelector(std::move(sel), &err)) {
      weight_err = name + ": " + err;
      return false;
    }
    Mutator* m = mset->Find("JitCompound");
    if (m == nullptr) {
      weight_err = name + ": missing JitCompound";
      return false;
    }
    *out = mset->WeightOf(m);
    return true;
  };
  double w_maglev = -1.0;
  double w_turbolev = -1.0;
  double w_default = -1.0;
  double w_fuzzilli = -1.0;
  weight_ok = weight_ok && preset_weight("maglev", &w_maglev) &&
              preset_weight("turbolev", &w_turbolev) &&
              preset_weight("default", &w_default) &&
              preset_weight("fuzzilli", &w_fuzzilli) && w_maglev == 0.5 &&
              w_turbolev == 0.5 && w_default == 0.0 && w_fuzzilli == 0.0;
  checker->Check(
      "JitCompound: default weight is 0.0 and JIT presets set it to 0.5",
      weight_ok,
      weight_err.empty()
          ? StringPrintf("maglev=%g turbolev=%g default=%g fuzzilli=%g",
                         w_maglev, w_turbolev, w_default, w_fuzzilli)
          : weight_err);

  const std::vector<const char*> kInputs = {
      "",
      "\"use strict\";\nlet x = 1;\nfunction sf(a, b) { return a + b; }\n"
      "sf(x, 2);\n",
      "class Box { constructor(v) { this.v = v; } "
      "bump(d) { return this.v + d; } }\nnew Box(1).bump(2);\n",
      "function hot(a, b) { return a + b; }\nhot(1, 2);\n",
      "const add = (x, y) => x + y;\nadd(1, 2);\n",
      "let acc = 0;\nfor (let i = 0; i < 6; i++) acc += i;\n",
  };
  const std::vector<JitCompoundPattern> kPatterns = {
      JitCompoundPattern::kLocalAllocation,
      JitCompoundPattern::kPolymorphicWarmup,
      JitCompoundPattern::kInlinedDeopt,
      JitCompoundPattern::kOsrLoopTransition,
  };

  // All four patterns emit valid JS across diverse inputs and strict mode.
  bool patterns_ok = true;
  std::string pat_err;
  std::vector<bool> saw_deopt_variants(4, false);
  for (JitCompoundPattern pat : kPatterns) {
    JitCompoundMutator mut(/*natives=*/true);
    mut.set_forced_pattern(pat);
    for (size_t in_idx = 0; in_idx < kInputs.size() && patterns_ok; in_idx++) {
      ProgramPtr prog = ParseOrDie(kInputs[in_idx], "selftest");
      for (int sample = 0; sample < 8 && patterns_ok; sample++) {
        Random rng(kSeed + 2000 + in_idx * 100 + static_cast<size_t>(sample));
        std::string detail;
        std::unique_ptr<fast::Tree> tree = ApplyMutation(
            mut, *prog, nullptr, rng, env, *generators, 1, &detail);
        if (tree == nullptr) {
          patterns_ok = false;
          pat_err = StringPrintf("pattern %d no-op on input %zu",
                                 static_cast<int>(pat), in_idx);
          break;
        }
        const std::string text = fast::Print(tree->root());
        std::set<const fast::Node*> seen_nodes;
        fast::Walk(tree->root(), [&](fast::Node* n) {
          if (!seen_nodes.insert(n).second) {
            patterns_ok = false;
            pat_err = StringPrintf("shared AST node in %s", detail.c_str());
            return false;
          }
          return true;
        });
        if (!patterns_ok) {
          break;
        }
        fast::ParseError parse_err;
        if (fast::Parser::ParseScript(text, fast::ParseOptions(), &parse_err) ==
            nullptr) {
          patterns_ok = false;
          pat_err = StringPrintf("parse error (%s): %s\n%s", detail.c_str(),
                                 parse_err.message.c_str(), text.c_str());
          break;
        }
        if (pat == JitCompoundPattern::kInlinedDeopt) {
          if (text.find("Object.defineProperty") != std::string::npos) {
            saw_deopt_variants[0] = true;
          }
          if (text.find("Object.setPrototypeOf") != std::string::npos) {
            saw_deopt_variants[1] = true;
          }
          if (text.find("Array.isArray") != std::string::npos) {
            saw_deopt_variants[2] = true;
          }
          if (text.find("%_DeoptimizeNow()") != std::string::npos) {
            saw_deopt_variants[3] = true;
          }
        }
      }
    }
  }
  if (patterns_ok && !(saw_deopt_variants[0] && saw_deopt_variants[1] &&
                       saw_deopt_variants[2] && saw_deopt_variants[3])) {
    patterns_ok = false;
    pat_err = "missing Pattern C transition variant";
  }
  checker->Check(
      "JitCompound: all four patterns emit valid JS and strict-mode code",
      patterns_ok, pat_err);

  // Hoisted `FunctionDeclaration` callers above their definition avoid TDZ.
  {
    ProgramPtr tdz_prog = ParseOrDie(
        "print(f(1));\nfunction f(x) { return x + 1; }\n", "selftest");
    JitCompoundMutator mut(/*natives=*/true);
    mut.set_forced_pattern(JitCompoundPattern::kInlinedDeopt);
    Random rng(kSeed + 2500);
    std::string detail;
    std::unique_ptr<fast::Tree> tree = ApplyMutation(
        mut, *tdz_prog, nullptr, rng, env, *generators, 1, &detail);
    std::string text = tree != nullptr ? fast::Print(tree->root()) : "";
    fast::ParseError parse_err;
    const bool tdz_safe =
        tree != nullptr && text.find("var jc") != std::string::npos &&
        text.find("let jc0") == std::string::npos &&
        fast::Parser::ParseScript(text, fast::ParseOptions(), &parse_err) !=
            nullptr;
    checker->Check(
        "JitCompound: Pattern C uses var counter to avoid hoisted caller TDZ",
        tdz_safe, parse_err.message);
  }

  // `natives_ = false` emits zero `%` native calls across all patterns.
  bool no_natives_ok = true;
  std::string no_nat_err;
  for (JitCompoundPattern pat : kPatterns) {
    JitCompoundMutator mut(/*natives=*/false);
    mut.set_forced_pattern(pat);
    for (size_t in_idx = 0; in_idx < kInputs.size() && no_natives_ok;
         in_idx++) {
      ProgramPtr prog = ParseOrDie(kInputs[in_idx], "selftest");
      for (int sample = 0; sample < 6 && no_natives_ok; sample++) {
        Random rng(kSeed + 3000 + in_idx * 100 + static_cast<size_t>(sample));
        std::string detail;
        std::unique_ptr<fast::Tree> tree = ApplyMutation(
            mut, *prog, nullptr, rng, env, *generators, 1, &detail);
        if (tree == nullptr) {
          no_natives_ok = false;
          no_nat_err = "natives_=false returned nullptr";
          break;
        }
        bool has_native_node = false;
        fast::Walk(tree->root(), [&](fast::Node* n) {
          if (n->Is<fast::NativeCallExpression>()) {
            has_native_node = true;
          }
          return true;
        });
        const std::string text = fast::Print(tree->root());
        fast::ParseOptions no_nat_opts;
        no_nat_opts.allow_natives_syntax = false;
        fast::ParseError parse_err;
        if (has_native_node || text.find('%') != std::string::npos ||
            fast::Parser::ParseScript(text, no_nat_opts, &parse_err) ==
                nullptr) {
          no_natives_ok = false;
          no_nat_err = StringPrintf("native leaked for pattern %d: %s\n%s",
                                    static_cast<int>(pat),
                                    parse_err.message.c_str(), text.c_str());
          break;
        }
      }
    }
  }
  checker->Check(
      "JitCompound: natives_=false emits zero natives and parses cleanly",
      no_natives_ok, no_nat_err);

  // `Builder::SetFocusSubtree` keeps all edits inside the focused subtree.
  {
    const char* kTwoFns = R"(let g0 = 1;
function other(a) { return a + 1; }
function target(x, y) {
  let z = x + y;
  return z;
}
other(g0);
target(g0, 2);
)";
    ProgramPtr prog = ParseOrDie(kTwoFns, "selftest");
    fast::FunctionDeclaration* orig_target = nullptr;
    std::vector<std::string> orig_outside;
    for (fast::Statement* s : prog->tree().root()->body) {
      if (auto* fn = s->As<fast::FunctionDeclaration>();
          fn != nullptr && fn->name != nullptr && fn->name->name == "target") {
        orig_target = fn;
      } else {
        orig_outside.push_back(fast::Print(s));
      }
    }
    const uint32_t target_body_id = orig_target->body->id;
    const std::string orig_target_src = fast::Print(orig_target);
    bool focus_ok = true;
    std::string focus_err;
    for (int i = 0; i < 20 && focus_ok; i++) {
      JitCompoundMutator mut(/*natives=*/true);
      if (i < 4) {
        mut.set_forced_pattern(kPatterns[static_cast<size_t>(i)]);
      }
      Random rng(kSeed + 4000 + static_cast<uint64_t>(i));
      std::string detail;
      std::unique_ptr<fast::Tree> tree =
          ApplyMutation(mut, *prog, nullptr, rng, env, *generators, 1, &detail,
                        nullptr, target_body_id);
      if (tree == nullptr) {
        focus_ok = false;
        focus_err = "focused JitCompound returned nullptr";
        break;
      }
      fast::FunctionDeclaration* new_target = nullptr;
      std::vector<std::string> new_outside;
      for (fast::Statement* s : tree->root()->body) {
        if (auto* fn = s->As<fast::FunctionDeclaration>();
            fn != nullptr && fn->name != nullptr &&
            fn->name->name == "target") {
          new_target = fn;
        } else {
          new_outside.push_back(fast::Print(s));
        }
      }
      if (new_target == nullptr || fast::Print(new_target) == orig_target_src ||
          new_outside != orig_outside) {
        focus_ok = false;
        focus_err = "focused JitCompound modified code outside target";
        break;
      }
    }
    // A leaf statement focus with no slot or function returns false cleanly.
    std::unique_ptr<fast::Tree> leaf_tree = prog->CopyTree();
    Random leaf_rng(kSeed + 4100);
    Builder leaf_builder(leaf_tree.get(), &leaf_rng);
    leaf_builder.SetFocusSubtree(leaf_tree->root()->body[0]);
    MutationContext leaf_ctx;
    leaf_ctx.env = &env;
    leaf_ctx.generators = generators.get();
    JitCompoundMutator leaf_mut(/*natives=*/true);
    const bool leaf_noop = !leaf_mut.Mutate(leaf_builder, leaf_ctx);
    checker->Check(
        "JitCompound: SetFocusSubtree restricts edits to the target subtree",
        focus_ok && leaf_noop, focus_err);
  }
}

// Decodes an unsigned LEB128 integer from `bytes` at `*pos`. Returns false if
// the encoding runs past the end of `bytes`.
bool ReadWasmU32(const std::vector<uint8_t>& bytes, size_t* pos,
                 uint32_t* out) {
  uint32_t result = 0;
  uint32_t shift = 0;
  while (*pos < bytes.size() && shift < 35) {
    uint8_t byte = bytes[(*pos)++];
    result |= static_cast<uint32_t>(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0) {
      *out = result;
      return true;
    }
    shift += 7;
  }
  return false;
}

// Returns the canonical order rank of a WebAssembly section id, or -1 if
// unknown. Section 13 (`Tag`) sits between Memory (5) and Global (6), and
// section 12 (`DataCount`) sits between Element (9) and Code (10).
int WasmSectionRank(uint8_t id) {
  switch (id) {
    case 1:
      return 1;
    case 2:
      return 2;
    case 3:
      return 3;
    case 4:
      return 4;
    case 5:
      return 5;
    case 13:
      return 6;
    case 6:
      return 7;
    case 7:
      return 8;
    case 8:
      return 9;
    case 9:
      return 10;
    case 12:
      return 11;
    case 10:
      return 12;
    case 11:
      return 13;
    default:
      return -1;
  }
}

// Parses top-level section headers of a WebAssembly binary. Records every seen
// section id in `sections` and copies section 10 (Code) payload to `code_sec`.
bool ParseWasmSections(const std::vector<uint8_t>& bytes,
                       std::set<uint8_t>* sections,
                       std::vector<uint8_t>* code_sec) {
  if (bytes.size() < 8) {
    return false;
  }
  static constexpr uint8_t kHeader[8] = {0x00, 0x61, 0x73, 0x6D,
                                         0x01, 0x00, 0x00, 0x00};
  for (size_t i = 0; i < 8; ++i) {
    if (bytes[i] != kHeader[i]) {
      return false;
    }
  }
  size_t pos = 8;
  int prev_rank = 0;
  while (pos < bytes.size()) {
    const uint8_t id = bytes[pos++];
    if (id != 0) {
      const int rank = WasmSectionRank(id);
      if (rank <= prev_rank) {
        return false;
      }
      prev_rank = rank;
    }
    uint32_t sec_size = 0;
    if (!ReadWasmU32(bytes, &pos, &sec_size) || pos + sec_size > bytes.size()) {
      return false;
    }
    sections->insert(id);
    if (id == 10 && code_sec != nullptr) {
      code_sec->assign(bytes.begin() + static_cast<ptrdiff_t>(pos),
                       bytes.begin() + static_cast<ptrdiff_t>(pos + sec_size));
    }
    pos += sec_size;
  }
  return pos == bytes.size();
}

// Checks WebAssembly MVP binary emission, `max_body_size`, JS interop, clean
// imports, tier-up, and `natives = false` gating.
void CheckWasmModule(Checker* checker) {
  TunableRegistry& reg = TunableRegistry::Instance();
  reg.Reset();

  std::string v8_err;
  const bool v8_ok = InitializeV8(FuzzConfig().v8_flags, &v8_err);
  std::unique_ptr<SyntaxChecker> syntax;
  if (v8_ok) {
    syntax = std::make_unique<SyntaxChecker>();
  }

  // 1. Check 200 generated modules with V8 and their MVP sections.
  Random rng(kSeed + 9001);
  std::set<uint8_t> seen_sections;
  bool all_valid = v8_ok;
  std::string first_err = v8_err;
  bool saw_loop = false;
  bool saw_select = false;
  bool saw_br_if = false;
  bool saw_global_set = false;
  bool saw_store = false;
  bool saw_call_indirect = false;
  bool saw_trunc_sat = false;
  bool saw_indirect_caller = false;
  for (int i = 0; i < 200; ++i) {
    WasmModule m = RandomWasmModule(rng);
    std::set<uint8_t> mod_sections;
    std::vector<uint8_t> code_sec;
    if (!ParseWasmSections(m.bytes, &mod_sections, &code_sec)) {
      all_valid = false;
      first_err = StringPrintf("module %d: bad Wasm binary framing", i);
      break;
    }
    seen_sections.insert(mod_sections.begin(), mod_sections.end());
    if (m.indirect_caller >= 0 && m.table_callees >= 2) {
      saw_indirect_caller = true;
    }
    for (size_t k = 0; k < code_sec.size(); ++k) {
      const uint8_t op = code_sec[k];
      if (op == 0x03 && k + 1 < code_sec.size() && code_sec[k + 1] == 0x40) {
        saw_loop = true;
      } else if (op == 0x1B) {
        saw_select = true;
      } else if (op == 0x0D) {
        saw_br_if = true;
      } else if (op == 0x24) {
        saw_global_set = true;
      } else if (op >= 0x36 && op <= 0x3E) {
        saw_store = true;
      } else if (op == 0x11) {
        saw_call_indirect = true;
      } else if (op == 0xFC) {
        saw_trunc_sat = true;
      }
    }
    std::string wasm_err;
    if (syntax != nullptr &&
        !syntax->ValidateWasm(m.bytes.data(), m.bytes.size(), &wasm_err,
                              m.has_js_string_builtins)) {
      all_valid = false;
      first_err = StringPrintf("module %d invalid: %s", i, wasm_err.c_str());
      break;
    }
  }
  const bool sections_ok = seen_sections.count(1) && seen_sections.count(2) &&
                           seen_sections.count(3) && seen_sections.count(4) &&
                           seen_sections.count(5) && seen_sections.count(6) &&
                           seen_sections.count(7) && seen_sections.count(9) &&
                           seen_sections.count(10) && seen_sections.count(11);
  const bool ops_ok = saw_loop && saw_select && saw_br_if && saw_global_set &&
                      saw_store && saw_call_indirect && saw_trunc_sat &&
                      saw_indirect_caller;
  checker->Check(
      "WasmModule: 200 modules pass V8 validation with all MVP sections",
      all_valid && sections_ok && ops_ok,
      !all_valid     ? first_err
      : !sections_ok ? "missing Wasm section"
                     : (ops_ok ? "200/200 valid" : "missing MVP opcode"));

  // 2. Check that `max_body_size` bounds and scales generated code size.
  std::string terr;
  bool body_set_ok =
      reg.Set("generators.WasmModule.max_imports", 0, &terr) &&
      reg.Set("generators.WasmModule.max_functions", 2, &terr) &&
      reg.Set("generators.WasmModule.max_globals", 0, &terr) &&
      reg.Set("generators.WasmModule.memory_probability", 0, &terr) &&
      reg.Set("generators.WasmModule.table_probability", 0, &terr) &&
      reg.Set("generators.WasmModule.max_locals", 2, &terr);
  size_t small_code_bytes = 0;
  size_t large_code_bytes = 0;
  bool body_valid = body_set_ok && v8_ok;
  if (body_valid) {
    body_valid = reg.Set("generators.WasmModule.max_body_size", 1, &terr);
    Random small_rng(kSeed + 9101);
    for (int i = 0; body_valid && i < 40; ++i) {
      WasmModule m = RandomWasmModule(small_rng);
      std::set<uint8_t> secs;
      std::vector<uint8_t> code;
      body_valid = ParseWasmSections(m.bytes, &secs, &code) &&
                   syntax->ValidateWasm(m.bytes.data(), m.bytes.size(), &terr,
                                        m.has_js_string_builtins);
      small_code_bytes += code.size();
    }
    body_valid =
        body_valid && reg.Set("generators.WasmModule.max_body_size", 60, &terr);
    Random large_rng(kSeed + 9101);
    for (int i = 0; body_valid && i < 40; ++i) {
      WasmModule m = RandomWasmModule(large_rng);
      std::set<uint8_t> secs;
      std::vector<uint8_t> code;
      body_valid = ParseWasmSections(m.bytes, &secs, &code) &&
                   syntax->ValidateWasm(m.bytes.data(), m.bytes.size(), &terr,
                                        m.has_js_string_builtins);
      large_code_bytes += code.size();
    }
  }
  reg.Reset();
  checker->Check("WasmModule: max_body_size scales function body size cleanly",
                 body_valid && large_code_bytes > small_code_bytes * 3,
                 StringPrintf("small=%zu large=%zu %s", small_code_bytes,
                              large_code_bytes, terr.c_str()));

  // 3. Check `GenWasmModule` default weight, clean imports, tier-up, and the
  // JS-to-Wasm wrapper.
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
  const Environment& env = Environment::Default();
  Generator* wasm_gen = generators->Find("WasmModule");
  const bool weight_zero =
      wasm_gen != nullptr && generators->WeightOf(wasm_gen) == 0.0;

  ProgramPtr base = ParseOrDie(kProgram, "selftest");
  bool js_ok = weight_zero && v8_ok;
  std::string js_err;
  bool saw_tier_up = false;
  bool saw_js_wrapper = false;
  bool saw_throwing_import = false;
  bool saw_clean_import = false;
  bool saw_bad_import = false;
  for (int i = 0; js_ok && i < 60; ++i) {
    std::unique_ptr<fast::Tree> tree = base->CopyTree();
    Random grng(kSeed + 9201 + static_cast<uint64_t>(i));
    Builder b(tree.get(), &grng);
    Builder::Slot slot{&tree->root()->body, tree->root()->body.size(),
                       tree->root()};
    GenContext ctx = MakeGenContext(b, slot, *generators, env);
    std::vector<fast::Statement*> out;
    ctx.out = &out;
    if (!wasm_gen->Generate(ctx, &out) || out.empty()) {
      js_ok = false;
      js_err = "WasmModule generated nothing";
      break;
    }
    b.InsertAll(slot, out);
    const std::string src = fast::Print(tree->root());
    if (!syntax->Accepts(src, &js_err)) {
      js_ok = false;
      break;
    }
    if (src.find("%WasmTierUpFunction(") != std::string::npos) {
      saw_tier_up = true;
    }
    if (src.find("%PrepareFunctionForOptimization(") != std::string::npos &&
        src.find("%OptimizeFunctionOnNextCall(") != std::string::npos) {
      saw_js_wrapper = true;
    }
    if (src.find("throw new Error(\"wasm\")") != std::string::npos) {
      saw_throwing_import = true;
    }
    if (src.find("f0: (") != std::string::npos) {
      saw_clean_import = true;
    }
    if (src.find("f0: C") != std::string::npos ||
        src.find("f1: C") != std::string::npos) {
      saw_bad_import = true;
    }
  }
  checker->Check(
      "WasmModule: emits valid JS with tier-up, wrapper, and clean imports",
      js_ok && saw_tier_up && saw_js_wrapper && saw_throwing_import &&
          saw_clean_import && !saw_bad_import,
      !js_ok ? js_err
             : StringPrintf("weight0=%d tier=%d wrap=%d throw=%d clean=%d "
                            "bad=%d",
                            weight_zero, saw_tier_up, saw_js_wrapper,
                            saw_throwing_import, saw_clean_import,
                            saw_bad_import));

  // 4. Check that `ctx.natives = false` emits zero `%` native calls.
  bool no_natives_ok =
      wasm_gen != nullptr && v8_ok &&
      reg.Set("generators.WasmModule.tier_up_probability", 1.0, &terr) &&
      reg.Set("generators.WasmModule.js_inline_probability", 1.0, &terr);
  std::string nn_err = terr;
  for (int i = 0; no_natives_ok && i < 40; ++i) {
    std::unique_ptr<fast::Tree> tree = base->CopyTree();
    Random grng(kSeed + 9301 + static_cast<uint64_t>(i));
    Builder b(tree.get(), &grng);
    Builder::Slot slot{&tree->root()->body, tree->root()->body.size(),
                       tree->root()};
    GenContext ctx = MakeGenContext(b, slot, *generators, env);
    ctx.natives = false;
    std::vector<fast::Statement*> out;
    ctx.out = &out;
    if (!wasm_gen->Generate(ctx, &out) || out.empty()) {
      no_natives_ok = false;
      nn_err = "WasmModule generated nothing with natives=false";
      break;
    }
    b.InsertAll(slot, out);
    const std::string src = fast::Print(tree->root());
    if (!syntax->Accepts(src, &nn_err)) {
      no_natives_ok = false;
      break;
    }
    if (src.find('%') != std::string::npos) {
      no_natives_ok = false;
      nn_err = "found `%` native call with natives=false";
      break;
    }
  }
  reg.Reset();
  checker->Check("WasmModule: emits zero `%` natives when natives is false",
                 no_natives_ok, nn_err);

  // 5. Check Stage 2 feature families individually with `ValidateWasm`:
  // Memory64/Multi-memory/Atomics, WasmGC, SIMD, EH (`try_table`), JSPI, and
  // JS string builtins.
  auto validate_family =
      [&](const char* label,
          const std::vector<std::pair<const char*, double>>& overrides,
          uint8_t required_opcode, uint8_t required_section, bool require_mem64,
          bool require_multi_mem, bool require_shared, bool require_jspi,
          bool require_js_string) {
        reg.Reset();
        std::string ferr;
        bool ok = v8_ok;
        for (const auto& [k, v] : overrides) {
          ok = ok && reg.Set(k, v, &ferr);
        }
        bool saw_op = (required_opcode == 0);
        bool saw_sec = (required_section == 0);
        bool saw_mem64 = !require_mem64;
        bool saw_multi_mem = !require_multi_mem;
        bool saw_shared = !require_shared;
        bool saw_jspi = !require_jspi;
        bool saw_js_string = !require_js_string;
        Random frng(kSeed + 9401 + static_cast<uint64_t>(required_opcode) * 31);
        for (int i = 0; ok && i < 80; ++i) {
          WasmModule m = RandomWasmModule(frng);
          std::set<uint8_t> secs;
          std::vector<uint8_t> code;
          if (!ParseWasmSections(m.bytes, &secs, &code)) {
            ok = false;
            ferr = StringPrintf("module %d: bad framing", i);
            break;
          }
          if (!syntax->ValidateWasm(m.bytes.data(), m.bytes.size(), &ferr,
                                    m.has_js_string_builtins)) {
            ok = false;
            ferr = StringPrintf("module %d invalid: %s", i, ferr.c_str());
            break;
          }
          if (m.jspi_export >= 0 &&
              (m.jspi_export < static_cast<int>(m.table_callees) ||
               m.jspi_export == m.indirect_caller ||
               m.jspi_export != static_cast<int>(m.functions.size()) - 1)) {
            ok = false;
            ferr = StringPrintf(
                "module %d: bad jspi_export=%d callees=%u "
                "indirect=%d funcs=%zu",
                i, m.jspi_export, m.table_callees, m.indirect_caller,
                m.functions.size());
            break;
          }
          if (required_section != 0 && secs.count(required_section)) {
            saw_sec = true;
          }
          if (required_opcode != 0) {
            for (uint8_t b : code) {
              if (b == required_opcode) {
                saw_op = true;
                break;
              }
            }
          }
          if (m.memories.size() >= 2) {
            saw_multi_mem = true;
          }
          for (const WasmMemoryInfo& mi : m.memories) {
            if (mi.is_memory64) {
              saw_mem64 = true;
            }
            if (mi.is_shared) {
              saw_shared = true;
            }
          }
          if (m.jspi_import >= 0 && m.jspi_export >= 0) {
            saw_jspi = true;
          }
          if (m.has_js_string_builtins) {
            saw_js_string = true;
          }
        }
        reg.Reset();
        const bool all_seen = saw_op && saw_sec && saw_mem64 && saw_multi_mem &&
                              saw_shared && saw_jspi && saw_js_string;
        checker->Check(
            label, ok && all_seen,
            !ok ? ferr
                : StringPrintf("op=%d sec=%d m64=%d multi=%d sh=%d jspi=%d "
                               "str=%d",
                               saw_op, saw_sec, saw_mem64, saw_multi_mem,
                               saw_shared, saw_jspi, saw_js_string));
      };

  validate_family(
      "WasmModule: Stage 2 Memory64, multi-memory, shared atomics validate",
      {{"generators.WasmModule.memory_probability", 1.0},
       {"generators.WasmModule.memory64_probability", 1.0},
       {"generators.WasmModule.shared_memory_probability", 1.0},
       {"generators.WasmModule.multi_memory_probability", 1.0},
       {"generators.WasmModule.atomic_op_probability", 1.0},
       {"generators.WasmModule.bulk_memory_probability", 1.0}},
      0xFE, 12, true, true, true, false, false);
  validate_family(
      "WasmModule: Stage 2 WasmGC rec groups, structs, arrays, casts validate",
      {{"generators.WasmModule.gc_probability", 1.0},
       {"generators.WasmModule.js_string_builtins_probability", 1.0}},
      0xFB, 12, false, false, false, false, true);
  validate_family("WasmModule: Stage 2 SIMD v128 and relaxed SIMD validate",
                  {{"generators.WasmModule.simd_probability", 1.0},
                   {"generators.WasmModule.memory_probability", 1.0}},
                  0xFD, 0, false, false, false, false, false);
  validate_family("WasmModule: Stage 2 EH try_table, tag, and JSPI validate",
                  {{"generators.WasmModule.eh_probability", 1.0},
                   {"generators.WasmModule.jspi_probability", 1.0},
                   {"generators.WasmModule.table_probability", 1.0}},
                  0x1F, 13, false, false, false, true, false);

  // 6. Check AST node uniqueness and Stage 2 JS harness emission.
  reg.Reset();
  bool uniq_ok = wasm_gen != nullptr && v8_ok;
  std::string uniq_err;
  bool saw_suspending = false;
  bool saw_promising = false;
  bool saw_jstag = false;
  bool saw_resizable = false;
  bool saw_guarded_tierup = false;
  for (int i = 0; uniq_ok && i < 80; ++i) {
    std::unique_ptr<fast::Tree> tree = base->CopyTree();
    Random grng(kSeed + 9601 + static_cast<uint64_t>(i));
    Builder b(tree.get(), &grng);
    Builder::Slot slot{&tree->root()->body, tree->root()->body.size(),
                       tree->root()};
    GenContext ctx = MakeGenContext(b, slot, *generators, env);
    std::vector<fast::Statement*> out;
    ctx.out = &out;
    if (!wasm_gen->Generate(ctx, &out) || out.empty()) {
      uniq_ok = false;
      uniq_err = "WasmModule generated nothing";
      break;
    }
    b.InsertAll(slot, out);
    std::set<const fast::Node*> seen_nodes;
    fast::Walk(tree->root(), [&](fast::Node* n) {
      if (!seen_nodes.insert(n).second) {
        uniq_ok = false;
        uniq_err = StringPrintf("duplicate AST node %p at seed %d",
                                static_cast<const void*>(n), i);
        return false;
      }
      return true;
    });
    if (!uniq_ok) {
      break;
    }
    const std::string src = fast::Print(tree->root());
    if (!syntax->Accepts(src, &uniq_err)) {
      uniq_ok = false;
      break;
    }
    if (src.find("WebAssembly.Suspending") != std::string::npos) {
      saw_suspending = true;
    }
    if (src.find("WebAssembly.promising") != std::string::npos) {
      saw_promising = true;
    }
    if (src.find("WebAssembly.JSTag") != std::string::npos) {
      saw_jstag = true;
    }
    if (src.find("toResizableBuffer") != std::string::npos) {
      saw_resizable = true;
    }
    if (src.find("typeof ") != std::string::npos &&
        src.find("%WasmTierUpFunction(") != std::string::npos) {
      saw_guarded_tierup = true;
    }
  }
  checker->Check(
      "WasmModule: Stage 2 AST nodes are unique and JS harness emits hooks",
      uniq_ok && saw_suspending && saw_promising && saw_jstag &&
          saw_resizable && saw_guarded_tierup,
      !uniq_ok ? uniq_err
               : StringPrintf("sus=%d prom=%d jstag=%d rab=%d tier=%d",
                              saw_suspending, saw_promising, saw_jstag,
                              saw_resizable, saw_guarded_tierup));

  // 7. Check empty-file generation and extreme tunable bounds.
  ProgramPtr empty_base = ParseOrDie("", "selftest_empty");
  bool edge_ok = wasm_gen != nullptr && v8_ok;
  std::string edge_err;
  reg.Reset();
  edge_ok =
      edge_ok && reg.Set("generators.WasmModule.max_params", 0, &edge_err) &&
      reg.Set("generators.WasmModule.max_locals", 0, &edge_err) &&
      reg.Set("generators.WasmModule.max_expr_depth", 1, &edge_err) &&
      reg.Set("generators.WasmModule.max_functions", 1, &edge_err) &&
      reg.Set("generators.WasmModule.max_imports", 0, &edge_err) &&
      reg.Set("generators.WasmModule.memory_probability", 1.0, &edge_err) &&
      reg.Set("generators.WasmModule.table_probability", 1.0, &edge_err) &&
      reg.Set("generators.WasmModule.gc_probability", 1.0, &edge_err) &&
      reg.Set("generators.WasmModule.simd_probability", 1.0, &edge_err) &&
      reg.Set("generators.WasmModule.eh_probability", 1.0, &edge_err) &&
      reg.Set("generators.WasmModule.jspi_probability", 1.0, &edge_err) &&
      reg.Set("generators.WasmModule.js_string_builtins_probability", 1.0,
              &edge_err) &&
      reg.Set("generators.WasmModule.start_function_probability", 1.0,
              &edge_err);
  Random erng(kSeed + 9801);
  for (int i = 0; edge_ok && i < 40; ++i) {
    WasmModule m = RandomWasmModule(erng);
    std::set<uint8_t> secs;
    std::vector<uint8_t> code;
    if (!ParseWasmSections(m.bytes, &secs, &code) ||
        !syntax->ValidateWasm(m.bytes.data(), m.bytes.size(), &edge_err,
                              m.has_js_string_builtins)) {
      edge_ok = false;
      break;
    }
    std::unique_ptr<fast::Tree> tree = empty_base->CopyTree();
    Random grng(kSeed + 9901 + static_cast<uint64_t>(i));
    Builder b(tree.get(), &grng);
    Builder::Slot slot{&tree->root()->body, tree->root()->body.size(),
                       tree->root()};
    GenContext ctx = MakeGenContext(b, slot, *generators, env);
    std::vector<fast::Statement*> out;
    ctx.out = &out;
    if (!wasm_gen->Generate(ctx, &out) || out.empty()) {
      edge_ok = false;
      edge_err = "empty-file generation returned empty";
      break;
    }
    b.InsertAll(slot, out);
    std::set<const fast::Node*> seen_nodes;
    fast::Walk(tree->root(), [&](fast::Node* n) {
      if (!seen_nodes.insert(n).second) {
        edge_ok = false;
        edge_err = StringPrintf("duplicate AST node %p on empty file",
                                static_cast<const void*>(n));
        return false;
      }
      return true;
    });
    if (!edge_ok) {
      break;
    }
    const std::string src = fast::Print(tree->root());
    if (!syntax->Accepts(src, &edge_err)) {
      edge_ok = false;
      break;
    }
  }
  reg.Reset();
  checker->Check(
      "WasmModule: empty-file generation and extreme tunable bounds pass",
      edge_ok, edge_err);
}

// Checks ES2025 Set/Map methods, Iterator chunking/padding/stepping,
// polymorphic constructors, species overrides, and Temporal boundary years.
void CheckEs2025Closures(Checker* checker) {
  TunableRegistry& reg = TunableRegistry::Instance();
  reg.Reset();

  const Environment& env = Environment::Default();
  auto has_param_name = [&](JsType type, std::string_view mname,
                            std::string_view pname) {
    const BuiltinType* bt = env.ForType(type);
    return bt != nullptr &&
           std::any_of(bt->methods.begin(), bt->methods.end(),
                       [&](const BuiltinFunction& f) {
                         return f.name == mname && !f.params.empty() &&
                                f.params[0].kind == ArgKind::kRange &&
                                f.params[0].name == pname;
                       });
  };
  auto ctor_has_param_name = [&](std::string_view gname,
                                 std::string_view pname) {
    const BuiltinGlobal* g = env.Global(gname);
    return g != nullptr && !g->params.empty() &&
           g->params[0].kind == ArgKind::kRange && g->params[0].name == pname;
  };
  auto bag_has_key = [&](std::string_view bag_name, std::string_view key,
                         ArgKind kind) {
    const OptionsBag* b = env.Options(bag_name);
    return b != nullptr &&
           std::any_of(b->keys.begin(), b->keys.end(), [&](const OptionKey& k) {
             return k.key == key && k.value.kind == kind;
           });
  };
  auto enum_has = [&](std::string_view ename, std::string_view val) {
    const EnumList* e = env.Enum(ename);
    return e != nullptr && std::find(e->values.begin(), e->values.end(), val) !=
                               e->values.end();
  };
  const bool model_ok =
      has_param_name(JsType::kIterator, "chunks", "ChunkSize") &&
      has_param_name(JsType::kIterator, "windows", "ChunkSize") &&
      bag_has_key("IteratorZipOptions", "mode", ArgKind::kEnum) &&
      bag_has_key("IteratorZipOptions", "padding", ArgKind::kIterable) &&
      bag_has_key("IteratorZipKeyedOptions", "mode", ArgKind::kEnum) &&
      bag_has_key("IteratorZipKeyedOptions", "padding", ArgKind::kValue) &&
      ctor_has_param_name("Temporal.PlainDate", "TemporalYear") &&
      ctor_has_param_name("Temporal.PlainDateTime", "TemporalYear") &&
      ctor_has_param_name("Temporal.PlainYearMonth", "TemporalYear") &&
      enum_has("PlainDateString", "0001-01-01") &&
      enum_has("PlainDateString", "9999-12-31") &&
      enum_has("PlainYearMonthString", "0001-01") &&
      enum_has("PlainYearMonthString", "-000001-01");
  checker->Check(
      "ES2025 model: chunk sizes, zip padding, and Temporal boundary years",
      model_ok, model_ok ? "" : "missing typed model entries");

  std::string v8_err;
  const bool v8_ok = InitializeV8(FuzzConfig().v8_flags, &v8_err);
  std::unique_ptr<SyntaxChecker> syntax;
  if (v8_ok) {
    syntax = std::make_unique<SyntaxChecker>();
  }
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();

  auto sample = [&](std::string_view name, int count, uint64_t seed_off,
                    bool* all_valid,
                    std::string* err) -> std::vector<std::string> {
    std::vector<std::string> out;
    Generator* g = generators->Find(name);
    if (g == nullptr || !v8_ok) {
      *all_valid = false;
      *err = g == nullptr ? "missing generator" : v8_err;
      return out;
    }
    *all_valid = true;
    for (int i = 0; i < count; ++i) {
      auto tree = std::make_unique<fast::Tree>();
      tree->set_root(tree->New<fast::Program>());
      Random rng(kSeed + seed_off + static_cast<uint64_t>(i));
      Builder b(tree.get(), &rng, &env);
      GenContext ctx(b, *generators, env);
      // Keep nested bodies non-recursive so tree-uniqueness checks test this
      // generator rather than unrelated recursive generators.
      ctx.max_depth = 0;
      ctx.out = &tree->root()->body;
      if (!g->Generate(ctx, ctx.out) || ctx.out->empty()) {
        continue;
      }
      std::set<const fast::Node*> seen_nodes;
      const fast::Node* shared_node = nullptr;
      fast::Walk(tree->root(), [&](fast::Node* n) {
        if (!seen_nodes.insert(n).second) {
          shared_node = n;
          return false;
        }
        return true;
      });
      if (shared_node != nullptr) {
        *all_valid = false;
        *err = std::string("shared AST node ") + shared_node->kind_name() +
               " in: " + fast::Print(tree->root()).substr(0, 400);
        break;
      }
      const std::string src = fast::Print(tree->root());
      std::string perr;
      if (!syntax->Accepts(src, &perr)) {
        *all_valid = false;
        *err = perr;
        break;
      }
      out.push_back(src);
    }
    return out;
  };
  auto any_has = [](const std::vector<std::string>& v, std::string_view sub) {
    return std::any_of(v.begin(), v.end(), [&](const std::string& s) {
      return s.find(sub) != std::string::npos;
    });
  };

  bool ok = false;
  std::string err;
  auto mapset = sample("MapSetOps", 160, 9500, &ok, &err);
  bool set_all = true;
  for (std::string_view m :
       {".union(", ".intersection(", ".difference(", ".symmetricDifference(",
        ".isSubsetOf(", ".isSupersetOf(", ".isDisjointFrom(",
        "keys:", ".getOrInsert(", ".getOrInsertComputed("}) {
    if (!any_has(mapset, m)) {
      set_all = false;
      err = std::string("MapSetOps missing ") + std::string(m);
      break;
    }
  }
  checker->Check("MapSetOps: emits ES2025 Set methods and Map.getOrInsert*",
                 ok && set_all, err);

  auto iters = sample("IteratorChain", 220, 9700, &ok, &err);
  const bool iter_ok =
      ok && any_has(iters, ".chunks(") && any_has(iters, ".windows(") &&
      (any_has(iters, "(1025") || any_has(iters, "(4294967295")) &&
      any_has(iters, "padding:") && any_has(iters, ".next()") &&
      any_has(iters, ".return()") && any_has(iters, "for (const [") &&
      any_has(iters, "for (const {");
  checker->Check(
      "IteratorChain: emits chunks/windows bounds, zip padding, next/return, "
      "and destructuring",
      iter_ok, iter_ok ? "" : (ok ? "missing IteratorChain feature" : err));

  auto cdecl = sample("ConstructorFunctionDecl", 100, 9950, &ok, &err);
  const bool cdecl_ok = ok && any_has(cdecl, "if (!new.target)") &&
                        any_has(cdecl, "this?.constructor || Object") &&
                        any_has(cdecl, "this.constructor");
  checker->Check(
      "ConstructorFunctionDecl: emits new.target guard and "
      "this.constructor",
      cdecl_ok, cdecl_ok ? "" : (ok ? "missing constructor idiom" : err));

  auto cfn = sample("ConstructorFunction", 80, 10100, &ok, &err);
  const bool cfn_ok = ok && any_has(cfn, "this?.constructor || Object");
  checker->Check("ConstructorFunction: emits polymorphic new.target guard",
                 cfn_ok, cfn_ok ? "" : (ok ? "missing poly guard" : err));

  auto cls = sample("ClassDecl", 100, 10200, &ok, &err);
  const bool cls_ok =
      ok && any_has(cls, "new.target") && any_has(cls, "this.constructor");
  checker->Check("ClassDecl: emits new.target and this.constructor inspection",
                 cls_ok, cls_ok ? "" : (ok ? "missing class meta" : err));

  auto proto = sample("PrototypeSet", 100, 10350, &ok, &err);
  const bool proto_ok = ok && any_has(proto, ".constructor =") &&
                        any_has(proto, "Symbol.species");
  checker->Check("PrototypeSet: emits constructor and Symbol.species overrides",
                 proto_ok, proto_ok ? "" : (ok ? "missing species" : err));

  auto shape = sample("ShapeChange", 120, 10500, &ok, &err);
  const bool shape_ok = ok && any_has(shape, ".constructor =") &&
                        any_has(shape, "Symbol.species");
  checker->Check("ShapeChange: emits constructor and Symbol.species overrides",
                 shape_ok, shape_ok ? "" : (ok ? "missing species" : err));

  auto temp = sample("TemporalOps", 160, 10700, &ok, &err);
  const bool temp_ok = ok && any_has(temp, "Temporal.PlainYearMonth(") &&
                       any_has(temp, "((-1),") &&
                       (any_has(temp, "(1,") || any_has(temp, "(9999,") ||
                        any_has(temp, "(1970,") || any_has(temp, "(2000,"));
  checker->Check("TemporalOps: emits PlainYearMonth and boundary years",
                 temp_ok, temp_ok ? "" : (ok ? "missing Temporal year" : err));
  reg.Reset();
}

void CheckCallSiteStress(Checker* checker) {
  std::unique_ptr<MutatorSet> mutators = MutatorSet::CreateDefault();
  std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
  const Environment& env = Environment::Default();
  Mutator* mutator = mutators->Find("CallSiteStress");
  const bool registered = mutator != nullptr && mutator->weight() == 0.0 &&
                          !mutator->needs_partner();
  checker->Check(
      "call_site_stress: registered with weight 0 and needs_partner false",
      registered);
  if (mutator == nullptr) {
    return;
  }

  auto isolate_pattern = [](const char* active_weight) {
    TunableRegistry& reg = TunableRegistry::Instance();
    reg.Reset();
    std::string err;
    const char* weights[] = {
        "mutators.CallSiteStress.tier_up_weight",
        "mutators.CallSiteStress.alloc_timeout_weight",
        "mutators.CallSiteStress.deopt_retier_weight",
        "mutators.CallSiteStress.stack_limit_weight",
        "mutators.CallSiteStress.reentrant_weight",
    };
    bool ok = true;
    for (const char* w : weights) {
      ok = ok && reg.Set(w, std::string(w) == active_weight ? 10.0 : 0.0, &err);
    }
    return ok;
  };
  std::string v8_err;
  const bool v8_ok = InitializeV8(FuzzConfig().v8_flags, &v8_err);
  std::unique_ptr<SyntaxChecker> syntax;
  if (v8_ok) {
    syntax = std::make_unique<SyntaxChecker>();
  }

  auto run_once = [&](const std::string& src, uint64_t seed,
                      std::string* printed, std::string* detail) -> bool {
    printed->clear();
    ProgramPtr prog = ParseOrDie(src, "call-site-stress");
    Random rng(seed);
    std::unique_ptr<fast::Tree> out = ApplyMutation(
        *mutator, *prog, nullptr, rng, env, *generators, 1, detail);
    if (out == nullptr) {
      return false;
    }
    absl::flat_hash_set<const fast::Node*> seen_nodes;
    bool unique_tree = true;
    fast::Walk(out->root(), [&](fast::Node* n) {
      if (!seen_nodes.insert(n).second) {
        unique_tree = false;
        return false;
      }
      return true;
    });
    if (!unique_tree) {
      return false;
    }
    *printed = fast::Print(out->root());
    fast::ParseError parse_err;
    if (fast::Parser::ParseScript(*printed, fast::ParseOptions(), &parse_err) ==
        nullptr) {
      return false;
    }
    if (syntax != nullptr) {
      std::string serr;
      if (!syntax->Accepts(*printed, &serr)) {
        return false;
      }
    }
    return true;
  };

  // Exercises each CallSiteStress pattern under isolated tunable weights.
  {
    const std::string src = R"(let obj = {a: 1};
function foo(x) { return x.a; }
const r = foo(obj);
let unused = r;
)";
    bool ok = isolate_pattern("mutators.CallSiteStress.tier_up_weight");
    std::string out, detail;
    ok =
        ok && run_once(src, 1, &out, &detail) &&
        out.find("%PrepareFunctionForOptimization(foo)") != std::string::npos &&
        (out.find("%OptimizeFunctionOnNextCall(foo)") != std::string::npos ||
         out.find("%OptimizeMaglevOnNextCall(foo)") != std::string::npos);
    checker->Check("call_site_stress: tier-up pattern prepares and optimizes",
                   ok, out);
  }
  {
    const std::string src = R"(function foo(x) { return x + 1; }
const y = foo(1);
let z = y + 2;
)";
    bool ok = isolate_pattern("mutators.CallSiteStress.alloc_timeout_weight");
    std::string out, detail;
    ok = ok && run_once(src, 2, &out, &detail) &&
         out.find("let y;") != std::string::npos &&
         out.find("%SetAllocationTimeout(-1, ") != std::string::npos &&
         out.find("finally") != std::string::npos &&
         out.find("%SetAllocationTimeout(-1, -1, true)") != std::string::npos &&
         out.find("catch") == std::string::npos;
    checker->Check(
        "call_site_stress: alloc-timeout splits escaping const and uses "
        "try/finally",
        ok, out);
  }
  {
    const std::string src = R"(function foo(x) { return x + 1; }
let y = foo(1);
)";
    bool ok = isolate_pattern("mutators.CallSiteStress.deopt_retier_weight");
    std::string out, detail;
    ok = ok && run_once(src, 3, &out, &detail) &&
         (out.find("%DeoptimizeFunction(foo)") != std::string::npos ||
          out.find("%CompileBaseline(foo)") != std::string::npos);
    checker->Check(
        "call_site_stress: deopt/retier pattern emits deopt or baseline "
        "native",
        ok, out);
  }
  {
    const std::string src = R"(function foo(x) { return x + 1; }
let a = foo(1);
let b = foo(2);
)";
    bool ok = isolate_pattern("mutators.CallSiteStress.stack_limit_weight");
    std::string out1, out2, detail;
    bool ran1 = ok && run_once(src, 4, &out1, &detail);
    bool has1 =
        ran1 &&
        out1.find("function __redline_near_stack") != std::string::npos &&
        out1.find("RangeError") != std::string::npos &&
        out1.find("__redline_near_stack(() => foo(") != std::string::npos;
    bool reused = false;
    if (has1) {
      ProgramPtr p1 = ParseOrDie(out1, "call-site-stress-2");
      Random rng(5);
      std::unique_ptr<fast::Tree> t2 = ApplyMutation(
          *mutator, *p1, nullptr, rng, env, *generators, 1, &detail);
      if (t2 != nullptr) {
        out2 = fast::Print(t2->root());
        size_t first = out2.find("function __redline_near_stack");
        size_t second =
            first == std::string::npos
                ? std::string::npos
                : out2.find("function __redline_near_stack", first + 1);
        fast::ParseError parse_err;
        reused = first != std::string::npos && second == std::string::npos &&
                 fast::Parser::ParseScript(out2, fast::ParseOptions(),
                                           &parse_err) != nullptr;
      }
    }
    checker->Check(
        "call_site_stress: stack-limit inserts helper once and reuses it",
        reused, !has1 ? ("run1: " + out1) : ("run2: " + out2));

    // Skips `stack_limit` when `await` sits inside call arguments.
    const std::string inner_await_src =
        "async function g(h, p) { h(await p); }\n";
    std::string async_out;
    bool skipped_await =
        isolate_pattern("mutators.CallSiteStress.stack_limit_weight") &&
        !run_once(inner_await_src, 6, &async_out, &detail);
    checker->Check(
        "call_site_stress: skips stack-limit wrapper around inner await arg",
        skipped_await, async_out);

    // Wraps `await h()` cleanly outside the synchronous arrow thunk.
    const std::string outer_await_src = "async function g(h) { await h(); }\n";
    std::string outer_out;
    bool ok_outer =
        isolate_pattern("mutators.CallSiteStress.stack_limit_weight") &&
        run_once(outer_await_src, 6, &outer_out, &detail) &&
        outer_out.find("await __redline_near_stack(() => h(), ") !=
            std::string::npos;
    checker->Check(
        "call_site_stress: preserves outer await around stack-limit call",
        ok_outer, outer_out);
  }
  {
    const std::string src = R"(let obj = {a: 1};
function foo(o) { return o.a; }
foo(obj);
)";
    std::string err;
    bool ok = isolate_pattern("mutators.CallSiteStress.reentrant_weight") &&
              TunableRegistry::Instance().Set(
                  "mutators.CallSiteStress.accessor_chance", 1.0, &err);
    std::string out_acc, detail;
    ok = ok && run_once(src, 7, &out_acc, &detail) &&
         out_acc.find("Object.defineProperty") != std::string::npos &&
         out_acc.find("get: () =>") != std::string::npos;
    checker->Check(
        "call_site_stress: reentrant pattern installs entry-budgeted accessor",
        ok, out_acc);

    bool ok_shape =
        isolate_pattern("mutators.CallSiteStress.reentrant_weight") &&
        TunableRegistry::Instance().Set(
            "mutators.CallSiteStress.accessor_chance", 0.0, &err);
    std::string out_shape;
    ok_shape = ok_shape && run_once(src, 7, &out_shape, &detail) &&
               out_shape.find("1.5") != std::string::npos;
    checker->Check(
        "call_site_stress: reentrant shape perturbation mutates property",
        ok_shape, out_shape);
  }

  // Checks filtering of unsafe call sites, control-flow guards, and edge cases.
  {
    TunableRegistry::Instance().Reset();
    std::string out, detail;
    bool rejected =
        !run_once("for (let i = 0; i < 1000; i++) f(i);\n", 1, &out, &detail) &&
        !run_once("function rec(n) { return rec(n - 1); }\n", 1, &out,
                  &detail) &&
        !run_once("class C { m(n) { return this.m(n - 1); } }\n", 1, &out,
                  &detail) &&
        !run_once("let o = { m(n) { return this.m(n - 1); } };\n", 1, &out,
                  &detail) &&
        !run_once("for (let i = init(); i < 3; i++) {}\n", 1, &out, &detail) &&
        !run_once("let xs = [1].map(x => inc(x));\n", 1, &out, &detail) &&
        !run_once("let f = null;\nf?.();\n", 1, &out, &detail) &&
        !run_once("let o = {m() { return 1; }};\no?.m();\n", 1, &out,
                  &detail) &&
        !run_once("let o = {a: {m() { return 1; }}};\no?.a.m();\n", 1, &out,
                  &detail) &&
        !run_once("d8.file.execute('a.js');\n", 1, &out, &detail) &&
        !run_once("Realm.create();\n", 1, &out, &detail) &&
        !run_once("os.system('true');\n", 1, &out, &detail) &&
        !run_once("globalThis.quit();\n", 1, &out, &detail) &&
        !run_once("this.print(1);\n", 1, &out, &detail) &&
        !run_once("globalThis.d8.file.execute('a.js');\n", 1, &out, &detail);
    checker->Check(
        "call_site_stress: rejects large loops, self-recursion, for-init, "
        "arrow calls, ChainExpression, and shell globals",
        rejected, out);

    bool member_ok =
        isolate_pattern("mutators.CallSiteStress.tier_up_weight") &&
        run_once("let obj = {m(x) { return x; }};\nobj.m(1);\n", 8, &out,
                 &detail) &&
        out.find("try {") != std::string::npos &&
        out.find("%PrepareFunctionForOptimization(obj.m)") != std::string::npos;
    checker->Check(
        "call_site_stress: guards member callee tier-up in try/catch",
        member_ok, out);

    // Skips escaping destructuring for alloc-timeout and wraps `var`.
    bool destruct_skipped =
        isolate_pattern("mutators.CallSiteStress.alloc_timeout_weight") &&
        !run_once(
            "function foo() { return {a: 1, b: 2}; }\n"
            "const {a, b} = foo();\nlet z = a + b;\n",
            2, &out, &detail);
    checker->Check(
        "call_site_stress: alloc-timeout skips escaping destructuring",
        destruct_skipped, out);

    bool var_wrapped =
        isolate_pattern("mutators.CallSiteStress.alloc_timeout_weight") &&
        run_once(
            "function foo(x) { return x + 1; }\n"
            "var y = foo(1);\nlet z = y + 2;\n",
            2, &out, &detail) &&
        out.find("var y = foo(1);") != std::string::npos &&
        out.find("let y;") == std::string::npos &&
        out.find("%SetAllocationTimeout(-1, ") != std::string::npos &&
        out.find("finally") != std::string::npos;
    checker->Check(
        "call_site_stress: alloc-timeout wraps escaping var in try/finally",
        var_wrapped, out);

    // Blockifies and guards single-statement control bodies.
    bool blockified =
        isolate_pattern("mutators.CallSiteStress.tier_up_weight") &&
        run_once(
            "function foo(v) { return v + 1; }\n"
            "let c = true;\nlet x = 0;\nif (c) x = foo(1);\n",
            1, &out, &detail) &&
        out.find("if (c) {") != std::string::npos &&
        out.find("try {") != std::string::npos &&
        out.find("%PrepareFunctionForOptimization(foo)") != std::string::npos;
    checker->Check(
        "call_site_stress: blockifies and guards single-statement if body",
        blockified, out);

    // Short-circuit and ternary call ancestors guard warm-up clones.
    bool logical_guarded =
        isolate_pattern("mutators.CallSiteStress.tier_up_weight") &&
        run_once(
            "function foo(v) { return v.y; }\nlet x = null;\nx && foo(x);\n", 1,
            &out, &detail) &&
        out.find("try {") != std::string::npos &&
        out.find("%PrepareFunctionForOptimization(foo)") != std::string::npos;
    checker->Check(
        "call_site_stress: guards short-circuit logical call warm-up",
        logical_guarded, out);

    bool cond_guarded =
        isolate_pattern("mutators.CallSiteStress.tier_up_weight") &&
        run_once(
            "function foo(v) { return v + 1; }\n"
            "let c = false;\nlet r = c ? foo(1) : 0;\n",
            1, &out, &detail) &&
        out.find("try {") != std::string::npos &&
        out.find("%PrepareFunctionForOptimization(foo)") != std::string::npos;
    checker->Check("call_site_stress: guards ternary call warm-up",
                   cond_guarded, out);

    // A destructuring default runs only for an undefined value.
    bool default_guarded =
        isolate_pattern("mutators.CallSiteStress.tier_up_weight") &&
        run_once(
            "function foo(v) { return v.y; }\n"
            "let o = {a: 1};\nlet {a = foo(null)} = o;\n",
            1, &out, &detail) &&
        out.find("try {") != std::string::npos &&
        out.find("%PrepareFunctionForOptimization(foo)") != std::string::npos;
    checker->Check("call_site_stress: guards destructuring default warm-up",
                   default_guarded, out);

    // A call site in a function gets one warm-up clone only.
    bool fn_warmup_capped =
        isolate_pattern("mutators.CallSiteStress.tier_up_weight") &&
        run_once(
            "function foo(v) { return v + 1; }\n"
            "function g() { let r = foo(7); return r; }\n",
            1, &out, &detail);
    if (fn_warmup_capped) {
      size_t calls = 0;
      for (size_t pos = out.find("foo(7)"); pos != std::string::npos;
           pos = out.find("foo(7)", pos + 1)) {
        calls++;
      }
      fn_warmup_capped =
          calls == 2 &&
          out.find("%PrepareFunctionForOptimization(foo)") != std::string::npos;
    }
    checker->Check("call_site_stress: caps warm-up clones in a function at 1",
                   fn_warmup_capped, out);

    // `yield` inside call arguments excludes stack-limit and arrow accessor.
    std::string err;
    bool yield_stack_skipped =
        isolate_pattern("mutators.CallSiteStress.stack_limit_weight") &&
        !run_once("function* g(h) { return h(yield 1); }\n", 6, &out, &detail);
    bool yield_acc_fallback =
        isolate_pattern("mutators.CallSiteStress.reentrant_weight") &&
        TunableRegistry::Instance().Set(
            "mutators.CallSiteStress.accessor_chance", 1.0, &err) &&
        run_once("function* g(h, obj) { return h(yield 1, obj); }\n", 7, &out,
                 &detail) &&
        out.find("Object.defineProperty") == std::string::npos &&
        out.find("get: () =>") == std::string::npos &&
        out.find("1.5") != std::string::npos;
    checker->Check(
        "call_site_stress: excludes yield args from stack-limit and accessor",
        yield_stack_skipped && yield_acc_fallback, out);

    // Skip implicit `arguments` inside a class `static {}` block, and allocate
    // zero arena nodes when `Mutate` returns false.
    {
      const std::string static_src =
          "function g() { arguments; class C { static { foo(); } } }\n";
      bool static_ok =
          isolate_pattern("mutators.CallSiteStress.reentrant_weight");
      for (uint64_t seed = 11; seed < 31 && static_ok; seed++) {
        if (!run_once(static_src, seed, &out, &detail) ||
            out.find("arguments[") != std::string::npos ||
            out.find("defineProperty(arguments") != std::string::npos) {
          static_ok = false;
        }
      }
      // Visible object + disabled patterns still runs candidate collection
      // and must leave `id_limit()` unchanged when `Mutate` returns false.
      const std::string noop_src = "let obj = {a: 1};\nfoo(1);\n";
      ProgramPtr noop_prog = ParseOrDie(noop_src, "call-site-noop");
      std::unique_ptr<fast::Tree> noop_tree = noop_prog->CopyTree();
      const uint32_t before_ids = noop_tree->id_limit();
      bool zero_ok = isolate_pattern("none");
      Random noop_rng(11);
      Builder noop_builder(noop_tree.get(), &noop_rng, &env);
      MutationContext noop_ctx;
      noop_ctx.env = &env;
      noop_ctx.generators = generators.get();
      const bool noop_returned_false = !mutator->Mutate(noop_builder, noop_ctx);
      const bool no_orphan_allocs = noop_tree->id_limit() == before_ids;
      checker->Check(
          "call_site_stress: skips implicit arguments in static block and "
          "keeps tree unchanged on no-op",
          static_ok && zero_ok && noop_returned_false && no_orphan_allocs, out);
    }

    // Skip shell-global member-expression arguments and Worker stack-limit.
    {
      bool harness_arg_skipped =
          isolate_pattern("mutators.CallSiteStress.reentrant_weight") &&
          !run_once("foo(d8.file);\n", 12, &out, &detail);
      bool worker_stack_skipped =
          isolate_pattern("mutators.CallSiteStress.stack_limit_weight") &&
          !run_once("new Worker(() => { foo(); }, {type: 'function'});\n", 12,
                    &out, &detail);
      checker->Check(
          "call_site_stress: skips harness member arg and Worker stack-limit",
          harness_arg_skipped && worker_stack_skipped, out);
    }

    // `SetFocusSubtree` keeps all edits inside the focused subtree.
    {
      const std::string focus_src = R"(function other(x) { return x + 1; }
function target(a) {
  return other(a);
}
other(1);
target(2);
)";
      ProgramPtr prog = ParseOrDie(focus_src, "call-site-focus");
      fast::FunctionDeclaration* orig_target = nullptr;
      std::vector<std::string> orig_outside;
      for (fast::Statement* s : prog->tree().root()->body) {
        if (auto* fn = s->As<fast::FunctionDeclaration>();
            fn != nullptr && fn->name != nullptr &&
            fn->name->name == "target") {
          orig_target = fn;
        } else {
          orig_outside.push_back(fast::Print(s));
        }
      }
      const uint32_t target_body_id = orig_target->body->id;
      bool stack_blocked_in_focus =
          isolate_pattern("mutators.CallSiteStress.stack_limit_weight");
      if (stack_blocked_in_focus) {
        std::unique_ptr<fast::Tree> focus_tree = prog->CopyTree();
        fast::FunctionDeclaration* focus_target =
            focus_tree->root()->body[1]->Cast<fast::FunctionDeclaration>();
        Random rng(20);
        Builder focus_builder(focus_tree.get(), &rng, &env);
        focus_builder.SetFocusSubtree(focus_target->body);
        MutationContext focus_ctx;
        focus_ctx.env = &env;
        focus_ctx.generators = generators.get();
        stack_blocked_in_focus = !mutator->Mutate(focus_builder, focus_ctx);
      }
      bool tier_confined =
          isolate_pattern("mutators.CallSiteStress.tier_up_weight");
      if (tier_confined) {
        Random rng(21);
        std::unique_ptr<fast::Tree> tree =
            ApplyMutation(*mutator, *prog, nullptr, rng, env, *generators, 1,
                          &detail, nullptr, target_body_id);
        tier_confined = (tree != nullptr);
        if (tier_confined) {
          std::vector<std::string> new_outside;
          for (fast::Statement* s : tree->root()->body) {
            if (auto* fn = s->As<fast::FunctionDeclaration>();
                fn == nullptr || fn->name == nullptr ||
                fn->name->name != "target") {
              new_outside.push_back(fast::Print(s));
            }
          }
          tier_confined = (new_outside == orig_outside);
        }
      }
      // Focusing a single leaf statement must not edit its enclosing list.
      TunableRegistry::Instance().Reset();
      std::unique_ptr<fast::Tree> leaf_tree = prog->CopyTree();
      Random leaf_rng(22);
      Builder leaf_builder(leaf_tree.get(), &leaf_rng, &env);
      leaf_builder.SetFocusSubtree(leaf_tree->root()->body[2]);
      MutationContext leaf_ctx;
      leaf_ctx.env = &env;
      leaf_ctx.generators = generators.get();
      const bool leaf_noop = !mutator->Mutate(leaf_builder, leaf_ctx);
      checker->Check(
          "call_site_stress: SetFocusSubtree isolates edits and blocks "
          "top-level helper insertion",
          stack_blocked_in_focus && tier_confined && leaf_noop);
    }

    // `SetNatives(false)` emits zero `%` native calls and keeps pure-JS paths.
    {
      TunableRegistry::Instance().Reset();
      mutator->SetNatives(false);
      const std::string src = R"(let obj = {a: 1};
function foo(o) { return o.a; }
foo(obj);
)";
      bool no_natives_ok = true;
      for (int i = 0; i < 10 && no_natives_ok; i++) {
        ProgramPtr prog = ParseOrDie(src, "call-site-nonat");
        Random rng(100 + static_cast<uint64_t>(i));
        std::unique_ptr<fast::Tree> tree = ApplyMutation(
            *mutator, *prog, nullptr, rng, env, *generators, 1, &detail);
        if (tree == nullptr) {
          no_natives_ok = false;
          break;
        }
        bool has_native = false;
        fast::Walk(tree->root(), [&](fast::Node* n) {
          if (n->Is<fast::NativeCallExpression>()) {
            has_native = true;
          }
          return true;
        });
        const std::string text = fast::Print(tree->root());
        fast::ParseOptions no_nat_opts;
        no_nat_opts.allow_natives_syntax = false;
        fast::ParseError perr;
        if (has_native || text.find('%') != std::string::npos ||
            fast::Parser::ParseScript(text, no_nat_opts, &perr) == nullptr) {
          no_natives_ok = false;
          out = text;
          break;
        }
      }
      const bool tier_disabled =
          isolate_pattern("mutators.CallSiteStress.tier_up_weight") &&
          !run_once(src, 120, &out, &detail);
      mutator->SetNatives(true);
      checker->Check(
          "call_site_stress: SetNatives(false) emits zero natives and disables "
          "native patterns",
          no_natives_ok && tier_disabled, out);
    }
  }

  TunableRegistry::Instance().Reset();
}

void CheckBoundedCallArguments(Checker* checker) {
  std::string params;
  for (int i = 0; i < 512; i++) {
    if (i > 0) {
      params += ", ";
    }
    params += "p" + std::to_string(i);
  }
  const std::string source = "function foo(" + params +
                             ") {}\n"
                             "foo();\n"
                             "foo();\n"
                             "class C { m(" +
                             params +
                             ") {} }\n"
                             "let c = new C();\n";
  ProgramPtr program = ParseOrDie(source, "selftest");

  // 1. `Builder::InfoOf` caps derived function parameters and method arity.
  {
    std::unique_ptr<fast::Tree> tree = program->CopyTree();
    Random rng(1);
    Builder builder(tree.get(), &rng);
    const BindingInfo* fn_info = builder.InfoOf("foo");
    const BindingInfo* obj_info = builder.InfoOf("c");
    int method_arity = -1;
    if (obj_info != nullptr) {
      for (const KnownKey& key : obj_info->keys) {
        if (key.name == "m") {
          method_arity = key.arity;
          break;
        }
      }
    }
    checker->Check(
        "codegen: Builder::InfoOf caps derived parameters and method arity",
        fn_info != nullptr &&
            fn_info->parameters.size() == Builder::kMaxRecordedParameters &&
            method_arity == static_cast<int>(Builder::kMaxRecordedParameters),
        StringPrintf("fn=%zu method=%d cap=%zu",
                     fn_info != nullptr ? fn_info->parameters.size() : 0,
                     method_arity, Builder::kMaxRecordedParameters));
  }

  // 2. `CodeGen::ArgumentsFor` caps explicit parameter vectors and obeys its
  // tunable.
  {
    TunableRegistry& registry = TunableRegistry::Instance();
    registry.Reset();
    std::unique_ptr<fast::Tree> tree = program->CopyTree();
    Random rng(7);
    Builder builder(tree.get(), &rng);
    std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
    Builder::Slot slot{&builder.root()->body, builder.root()->body.size(),
                       builder.root()};
    GenContext ctx =
        MakeGenContext(builder, slot, *generators, Environment::Default());
    CodeGen cg(ctx);
    std::vector<JsType> huge(512, JsType::kNumber);
    const size_t default_count = cg.ArgumentsFor(huge).size();
    std::string err;
    const bool set_ok =
        registry.Set("codegen.call.max_typed_arguments", 5, &err);
    const size_t tuned_count = cg.ArgumentsFor(huge).size();
    registry.Reset();
    checker->Check(
        "codegen: ArgumentsFor caps huge parameter lists and obeys its tunable",
        set_ok && default_count == Builder::kMaxRecordedParameters &&
            tuned_count == 5,
        StringPrintf("default=%zu tuned=%zu err=%s", default_count, tuned_count,
                     err.c_str()));
  }

  // 3. `Prefix` on a 512-parameter function stays bounded across 50 seeds.
  {
    std::unique_ptr<MutatorSet> mutators = MutatorSet::CreateDefault();
    std::unique_ptr<GeneratorSet> generators = GeneratorSet::CreateDefault();
    Mutator* prefix = mutators->Find("Prefix");
    size_t max_nodes = 0;
    size_t max_bytes = 0;
    bool all_ok = prefix != nullptr;
    for (uint64_t seed = 1; seed <= 50 && all_ok; seed++) {
      Random rng(seed);
      std::string detail;
      std::unique_ptr<fast::Tree> mutated = ApplyMutation(
          *prefix, *program, nullptr, rng, Environment::Default(), *generators,
          /*rounds=*/1, &detail);
      if (mutated == nullptr) {
        continue;
      }
      const size_t nodes = fast::CountNodes(mutated->root());
      const size_t bytes = fast::Print(mutated->root()).size();
      max_nodes = std::max(max_nodes, nodes);
      max_bytes = std::max(max_bytes, bytes);
      if (nodes > 4000 || bytes > 65536) {
        all_ok = false;
      }
    }
    checker->Check(
        "codegen: Prefix stays bounded on a 512-parameter function", all_ok,
        StringPrintf("max %zu nodes, %zu bytes", max_nodes, max_bytes));
  }

  // 4. `CountNodesUpTo` and `ExceedsMutantNodeLimit` detect oversized trees.
  {
    const size_t exact = program->node_count();
    const size_t exact_up_to =
        fast::CountNodesUpTo(program->tree().root(), exact);
    const size_t capped_up_to =
        fast::CountNodesUpTo(program->tree().root(), 10);
    const bool at_limit = ExceedsMutantNodeLimit(program->tree(), exact);
    const bool below_limit = ExceedsMutantNodeLimit(program->tree(), exact - 1);
    checker->Check(
        "engine: CountNodesUpTo and ExceedsMutantNodeLimit bound tree size",
        exact > 500 && exact_up_to == exact && capped_up_to == 11 &&
            !at_limit && below_limit,
        StringPrintf("exact=%zu up_to=%zu capped=%zu", exact, exact_up_to,
                     capped_up_to));
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
  CheckD8ArgLayers(checker);
  CheckCommitCostConfig(checker);
  CheckAdaptiveWindow(checker);
  CheckSwarm(checker);
  CheckAmplifyConfig(checker);
  CheckWrapInTry(checker);
  CheckOsrLoopTrigger(checker);
  CheckFocusChain(checker);
  CheckRarityOverhaul(checker);
  CheckRuntimeAssisted(checker);
  CheckReceiverJit(checker);
  CheckOperationAndCallParity(checker);
  CheckJitCompound(checker);
  CheckCallSiteStress(checker);
  CheckEdgeCases(checker);
  CheckBuiltinsAndWorkers(checker);
  CheckEnvTyper(checker);
  CheckWasmModule(checker);
  CheckEs2025Closures(checker);
  CheckBoundedCallArguments(checker);
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
    // The flag --d8-args keeps the d8_extra_args of the preset. A d8_args
    // key in --config removes them.
    std::string text;
    JsonValue json;
    if (!ok || !ReadFileToString(path, &text, &error) ||
        !JsonValue::Parse(text, &json, &error) ||
        !json.Get("d8_extra_args").is_array() ||
        json.Get("d8_extra_args").as_array().empty()) {
      continue;
    }
    std::string preset_flags;
    for (const JsonValue& flag : json.Get("d8_extra_args").as_array()) {
      if (flag.is_string()) {
        preset_flags += " " + flag.as_string();
      }
    }
    std::string got =
        LoadedD8Args({arg, "--d8-args=--base-a", "--d8-extra-args=--extra-b"});
    checker->Check(
        ("presets: --d8-args keeps the d8_extra_args of " + BaseName(path))
            .c_str(),
        got == "--base-a" + preset_flags + " --extra-b", got);
    got = LoadedD8Args({arg, kD8ArgsConfig, "--d8-args=--base-a",
                        "--d8-extra-args=--extra-b"});
    checker->Check(("presets: a d8_args key removes the d8_extra_args of " +
                    BaseName(path))
                       .c_str(),
                   got == "--base-a --extra-a --extra-b", got);
  }
  TunableRegistry::Instance().Reset();
}

}  // namespace redline
