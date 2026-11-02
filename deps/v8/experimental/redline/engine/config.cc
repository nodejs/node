// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/config.h"

#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string_view>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/json.h"
#include "experimental/redline/base/json_parser.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/corpus/corpus.h"
#include "experimental/redline/engine/amplifier.h"
#include "experimental/redline/engine/prelude.h"
#include "experimental/redline/generators/generator.h"
#include "experimental/redline/mutators/mutator.h"

namespace redline {

namespace {

std::vector<std::string> SplitWhitespace(const std::string& text) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && isspace(static_cast<unsigned char>(text[i]))) {
      i++;
    }
    size_t start = i;
    while (i < text.size() && !isspace(static_cast<unsigned char>(text[i]))) {
      i++;
    }
    if (i > start) {
      out.push_back(text.substr(start, i - start));
    }
  }
  return out;
}

// --- selection -------------------------------------------------------------

// "init": {"strategy": "generate"|"import", "params": {...}} (DESIGN.md 5.1).
// Both strategies set the fields the engine already uses:
//   generate: {"programs": N}                    -> initial_programs
//   import:   {"paths": [...], "root": DIR,      -> imports, import_root,
//              "keep": "new-coverage"|"all",     -> import_all
//              "minimize": BOOL}                 -> minimize_imports
bool ReadInitConfig(const JsonValue& init, FuzzConfig* c, std::string* error) {
  if (init.is_null()) {
    return true;
  }
  const std::string usage =
      "config: \"init\" must be {\"strategy\": \"generate\"|\"import\", "
      "\"params\": {...}}";
  if (!init.is_object()) {
    *error = usage;
    return false;
  }
  for (const auto& [key, value] : init.as_object()) {
    if (key != "strategy" && key != "params") {
      *error = usage;
      return false;
    }
  }
  const std::string strategy = init.GetString("strategy", "");
  const JsonValue& params = init.Get("params");
  if (!params.is_null() && !params.is_object()) {
    *error = usage;
    return false;
  }
  std::vector<std::string> allowed;
  if (strategy == "generate") {
    allowed = {"programs"};
  } else if (strategy == "import") {
    allowed = {"paths", "root", "keep", "minimize"};
  } else {
    *error = "config: unknown init strategy \"" + strategy +
             "\" (known: generate, import)";
    return false;
  }
  if (params.is_object()) {
    for (const auto& [key, value] : params.as_object()) {
      if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
        *error = StringPrintf(
            "config: unknown parameter \"%s\" of init strategy \"%s\" (known: "
            "%s)",
            key.c_str(), strategy.c_str(), JoinNames(allowed).c_str());
        return false;
      }
    }
  }
  if (strategy == "generate") {
    c->imports.clear();
    if (params.is_object()) {
      c->initial_programs =
          static_cast<int>(params.GetInt("programs", c->initial_programs));
    }
    return true;
  }
  if (!params.is_object()) {
    return true;
  }
  const JsonValue& paths = params.Get("paths");
  if (!paths.is_null()) {
    if (!paths.is_array()) {
      *error =
          "config: init.params.paths must be a list of files or directories";
      return false;
    }
    c->imports.clear();
    for (const JsonValue& path : paths.as_array()) {
      if (!path.is_string()) {
        *error =
            "config: init.params.paths must be a list of files or directories";
        return false;
      }
      c->imports.push_back(path.as_string());
    }
  }
  c->import_root = params.GetString("root", c->import_root);
  c->minimize_imports = params.GetBool("minimize", c->minimize_imports);
  if (params.Has("keep")) {
    const std::string keep = params.GetString("keep", "");
    if (keep != "new-coverage" && keep != "all") {
      *error = "config: init.params.keep must be \"new-coverage\" or \"all\"";
      return false;
    }
    c->import_all = keep == "all";
  }
  return true;
}

// "accept": {"strategy": "new-edges"} (DESIGN.md 5.5). The criterion that
// admits a sample into the corpus; its determinism check is configured in
// stages.check. `new-edges` is the only criterion today: a new one adds a
// name here and returns its frozen reason to Commit (the minimizer oracle).
bool ReadAcceptConfig(const JsonValue& accept, FuzzConfig* c,
                      std::string* error) {
  if (accept.is_null()) {
    return true;
  }
  if (!accept.is_object()) {
    *error = "config: \"accept\" must be {\"strategy\": \"new-edges\"}";
    return false;
  }
  for (const auto& [key, value] : accept.as_object()) {
    if (key != "strategy") {
      *error =
          "config: unknown key \"" + key + "\" in \"accept\" (known: strategy)";
      return false;
    }
  }
  const std::string strategy = accept.GetString("strategy", "");
  if (strategy != "new-edges") {
    *error = "config: unknown accept strategy \"" + strategy +
             "\" (known: new-edges)";
    return false;
  }
  c->accept_strategy = strategy;
  return true;
}

// "amplify": {"strategy": "flags"|"none", "params": {...}} (DESIGN.md 5.8).
// The strategy validates its params when it is built.
bool ReadAmplifyConfig(const JsonValue& amplify, FuzzConfig* c,
                       std::string* error) {
  if (amplify.is_null()) {
    return true;
  }
  if (!amplify.is_object()) {
    *error =
        "config: \"amplify\" must be {\"strategy\": NAME, \"params\": {...}}";
    return false;
  }
  for (const auto& [key, value] : amplify.as_object()) {
    if (key != "strategy" && key != "params") {
      *error = "config: unknown key \"" + key +
               "\" in \"amplify\" (known: strategy, params)";
      return false;
    }
  }
  const std::string strategy =
      amplify.GetString("strategy", c->amplify_strategy);
  const JsonValue& params = amplify.Get("params");
  std::string why;
  if (CreateAmplifyStrategy(strategy, params, &why) == nullptr) {
    *error = "config: " + why;
    return false;
  }
  c->amplify_strategy = strategy;
  c->amplify_params = params.is_null() ? "{}" : params.ToString();
  return true;
}

// Reads one {"strategy": NAME, "params": {...}} object into `out`. A plain
// string is accepted as a strategy name without parameters.
bool ReadSelectionConfig(const JsonValue& object, const char* point,
                         SelectionConfig* out, std::string* error) {
  const JsonValue& value = object.Get(point);
  if (value.is_null()) {
    return true;
  }
  if (value.is_string()) {
    out->strategy = value.as_string();
    return true;
  }
  if (!value.is_object()) {
    *error = StringPrintf(
        "config: \"selection.%s\" must be {\"strategy\": NAME, \"params\": "
        "{...}}",
        point);
    return false;
  }
  for (const auto& [key, member] : value.as_object()) {
    if (key == "strategy") {
      if (!member.is_string()) {
        *error = StringPrintf(
            "config: \"selection.%s.strategy\" must be a string", point);
        return false;
      }
      out->strategy = member.as_string();
    } else if (key == "params") {
      if (!member.is_object()) {
        *error = StringPrintf(
            "config: \"selection.%s.params\" must be an object", point);
        return false;
      }
      out->params = member;
    } else {
      *error = StringPrintf("config: unknown key \"%s\" in \"selection.%s\"",
                            key.c_str(), point);
      return false;
    }
  }
  return true;
}

// Merges name -> weight pairs into cfg->params.weights (later pairs win).
// JsonValue has no setters, so the object is rebuilt as text and parsed.
bool AddWeightOverrides(
    SelectionConfig* cfg,
    const std::vector<std::pair<std::string, double>>& pairs,
    std::string* error) {
  if (pairs.empty()) {
    return true;
  }
  std::vector<std::pair<std::string, double>> merged;
  if (!ReadWeightOverrides(cfg->params, &merged, error)) {
    return false;
  }
  for (const auto& [name, weight] : pairs) {
    bool replaced = false;
    for (auto& existing : merged) {
      if (existing.first == name) {
        existing.second = weight;
        replaced = true;
      }
    }
    if (!replaced) {
      merged.emplace_back(name, weight);
    }
  }
  JsonObject weights;
  for (const auto& [name, weight] : merged) {
    weights.Add(name, weight);
  }
  JsonObject params;
  if (cfg->params.is_object()) {
    for (const auto& [key, member] : cfg->params.as_object()) {
      if (key != "weights") {
        params.AddRaw(key, member.ToString());
      }
    }
  }
  params.AddRaw("weights", weights.ToString());
  return JsonValue::Parse(params.ToString(), &cfg->params, error);
}

// Reads the legacy {"Name": weight} object under `key` into `pairs`.
bool ReadLegacyWeights(const JsonValue& object, const char* key,
                       std::vector<std::pair<std::string, double>>* pairs,
                       std::string* error) {
  const JsonValue& weights = object.Get(key);
  if (weights.is_null()) {
    return true;
  }
  if (!weights.is_object()) {
    *error =
        StringPrintf("config: \"%s\" must be an object of name -> weight", key);
    return false;
  }
  for (const auto& [name, value] : weights.as_object()) {
    if (!value.is_number() || value.as_number() < 0) {
      *error =
          StringPrintf("config: weight of \"%s\" must be a non-negative number",
                       name.c_str());
      return false;
    }
    pairs->emplace_back(name, value.as_number());
  }
  return true;
}

std::string SelectionConfigJson(const SelectionConfig& cfg,
                                std::string_view default_name) {
  JsonObject o;
  o.Add("strategy",
        cfg.strategy.empty() ? std::string(default_name) : cfg.strategy);
  o.AddRaw("params", cfg.params.is_null() ? "{}" : cfg.params.ToString());
  return o.ToString();
}

// Parses "Name=W,Name2=W2" from the command line.
bool ParseWeightList(const std::vector<std::string>& items,
                     std::vector<std::pair<std::string, double>>* out,
                     std::string* error) {
  for (const std::string& item : items) {
    size_t eq = item.find('=');
    if (eq == std::string::npos || eq == 0 || eq + 1 >= item.size()) {
      *error = StringPrintf("expected NAME=WEIGHT, got '%s'", item.c_str());
      return false;
    }
    const char* start = item.c_str() + eq + 1;
    char* end = nullptr;
    double weight = strtod(start, &end);
    if (end == start || *end != '\0' || !std::isfinite(weight) ||
        weight < 0.0) {
      *error =
          StringPrintf("weight of '%s' must be a non-negative number, got '%s'",
                       item.substr(0, eq).c_str(), start);
      return false;
    }
    out->emplace_back(item.substr(0, eq), weight);
  }
  return true;
}

void ReadMinimizerOptions(const JsonValue& object, MinimizerOptions* m) {
  if (!object.is_object()) {
    return;
  }
  m->max_tests = static_cast<int>(object.GetInt("max_tests", m->max_tests));
  m->max_time_ms = static_cast<uint64_t>(
      object.GetInt("max_time_ms", static_cast<int64_t>(m->max_time_ms)));
  m->max_rounds = static_cast<int>(object.GetInt("max_rounds", m->max_rounds));
  m->target_time_ms = static_cast<uint64_t>(
      object.GetInt("target_time_ms", static_cast<int64_t>(m->target_time_ms)));
  m->min_tests = static_cast<int>(object.GetInt("min_tests", m->min_tests));
}

std::string MinimizerOptionsJson(const MinimizerOptions& m) {
  JsonObject o;
  o.Add("max_tests", m.max_tests);
  o.Add("max_time_ms", static_cast<uint64_t>(m.max_time_ms));
  o.Add("max_rounds", m.max_rounds);
  o.Add("target_time_ms", static_cast<uint64_t>(m.target_time_ms));
  o.Add("min_tests", m.min_tests);
  return o.ToString();
}

// --- stages ----------------------------------------------------------------

// Reads the three common knobs of a stage ("log", "capture", "stats").
bool ReadStageOptions(Stage stage, const JsonValue& object, StageOptions* out,
                      std::string* error) {
  std::string stage_name(StageName(stage));
  if (object.Has("log")) {
    const JsonValue& log = object.Get("log");
    if (!log.is_string() ||
        (log.as_string() != "events" && log.as_string() != "off")) {
      *error =
          "config: stages." + stage_name + ".log must be \"events\" or \"off\"";
      return false;
    }
    out->events = log.as_string() == "events";
  }
  if (object.Has("capture")) {
    std::string detail;
    if (!CaptureRule::FromJson(stage, object.Get("capture"), &out->capture,
                               &detail)) {
      *error = "config: stages." + stage_name + ": " + detail;
      return false;
    }
  }
  if (object.Has("stats")) {
    out->stats = object.GetBool("stats", out->stats);
  }
  return true;
}

// The names of a constant list of keys, for an error message.
std::string JoinKeys(std::initializer_list<std::string_view> keys) {
  return JoinNames(std::vector<std::string>(keys.begin(), keys.end()));
}

// The "stages" block: every stage with its own parameters (which are the
// policy fields of FuzzConfig) plus log/capture/stats.
bool ReadStages(const JsonValue& json, FuzzConfig* c, std::string* error) {
  const JsonValue& stages = json.Get("stages");
  if (stages.is_null()) {
    return true;
  }
  if (!stages.is_object()) {
    *error = "config: \"stages\" must be an object";
    return false;
  }
  // Known parameter keys per stage; "log", "capture" and "stats" are
  // common to all.
  static constexpr std::initializer_list<std::string_view> kCommon = {
      "log", "capture", "stats"};
  static constexpr std::initializer_list<std::string_view> kKeys[kNumStages] = {
      {},                                         // pick
      {"enabled", "min_values", "extra_values"},  // prefix
      {"consecutive", "rounds"},                  // mutate
      {"min_reruns", "min_stable_reruns", "max_reruns",
       "flaky_threshold"},  // check
      {"enabled", "max_tests", "max_time_ms", "max_rounds", "target_time_ms",
       "min_tests", "probe_timeout_factor", "probe_timeout_extra_ms",
       "probe_timeout_min_ms", "verify_attempts", "reducers"},  // minimize
      {"save_corpus"},                                          // publish
  };
  for (const auto& [stage_name, object] : stages.as_object()) {
    Stage stage;
    if (!ParseStage(stage_name, &stage)) {
      *error = "config: unknown stage \"" + stage_name +
               "\" (known: pick, prefix, mutate, check, minimize, publish)";
      return false;
    }
    if (!object.is_object()) {
      *error = "config: stages." + stage_name + " must be an object";
      return false;
    }
    const std::initializer_list<std::string_view>& keys =
        kKeys[static_cast<int>(stage)];
    for (const auto& [key, value] : object.as_object()) {
      bool known =
          std::find(kCommon.begin(), kCommon.end(), key) != kCommon.end() ||
          std::find(keys.begin(), keys.end(), key) != keys.end();
      if (!known) {
        *error = "config: unknown key \"";
        *error += key;
        *error += "\" in stages.";
        *error += stage_name;
        *error += " (known: ";
        *error += JoinKeys(keys);
        *error += keys.size() == 0 ? "" : ", ";
        *error += JoinKeys(kCommon);
        *error += ')';
        return false;
      }
    }
    if (!ReadStageOptions(stage, object, &c->stage(stage), error)) {
      return false;
    }
    switch (stage) {
      case Stage::kPick:
        break;
      case Stage::kPrefix:
        c->value_prefix = object.GetBool("enabled", c->value_prefix);
        c->prefix_min_values =
            static_cast<int>(object.GetInt("min_values", c->prefix_min_values));
        c->prefix_extra_values = static_cast<int>(
            object.GetInt("extra_values", c->prefix_extra_values));
        break;
      case Stage::kMutate:
        c->consecutive_mutations = static_cast<int>(
            object.GetInt("consecutive", c->consecutive_mutations));
        c->mutation_rounds =
            static_cast<int>(object.GetInt("rounds", c->mutation_rounds));
        break;
      case Stage::kCheck:
        c->min_reruns =
            static_cast<int>(object.GetInt("min_reruns", c->min_reruns));
        c->min_stable_reruns = static_cast<int>(
            object.GetInt("min_stable_reruns", c->min_stable_reruns));
        c->max_reruns =
            static_cast<int>(object.GetInt("max_reruns", c->max_reruns));
        c->flaky_threshold = static_cast<int>(
            object.GetInt("flaky_threshold", c->flaky_threshold));
        break;
      case Stage::kMinimize:
        c->minimize = object.GetBool("enabled", c->minimize);
        ReadMinimizerOptions(object, &c->minimizer);
        c->probe_timeout_factor =
            object.GetNumber("probe_timeout_factor", c->probe_timeout_factor);
        c->probe_timeout_extra_ms = object.GetNumber("probe_timeout_extra_ms",
                                                     c->probe_timeout_extra_ms);
        c->probe_timeout_min_ms =
            object.GetNumber("probe_timeout_min_ms", c->probe_timeout_min_ms);
        c->verify_attempts = static_cast<int>(
            object.GetInt("verify_attempts", c->verify_attempts));
        if (object.Has("reducers")) {
          const JsonValue& list = object.Get("reducers");
          if (!list.is_array()) {
            *error =
                "config: stages.minimize.reducers must be a list of reducer "
                "names";
            return false;
          }
          c->minimizer_reducers.clear();
          for (const JsonValue& name : list.as_array()) {
            if (!name.is_string()) {
              *error =
                  "config: stages.minimize.reducers must be a list of reducer "
                  "names";
              return false;
            }
            c->minimizer_reducers.push_back(name.as_string());
          }
        }
        break;
      case Stage::kPublish:
        c->save_corpus = object.GetBool("save_corpus", c->save_corpus);
        break;
    }
  }
  return true;
}

std::string StagesJson(const FuzzConfig& c) {
  auto common = [&](Stage stage, JsonObject* o) {
    const StageOptions& opt = c.stage(stage);
    o->Add("log", opt.events ? "events" : "off");
    o->AddRaw("capture", opt.capture.ToJson());
    o->Add("stats", opt.stats);
  };
  JsonObject stages;
  {
    JsonObject o;
    common(Stage::kPick, &o);
    stages.AddRaw("pick", o.ToString());
  }
  {
    JsonObject o;
    o.Add("enabled", c.value_prefix);
    o.Add("min_values", c.prefix_min_values)
        .Add("extra_values", c.prefix_extra_values);
    common(Stage::kPrefix, &o);
    stages.AddRaw("prefix", o.ToString());
  }
  {
    JsonObject o;
    o.Add("consecutive", c.consecutive_mutations)
        .Add("rounds", c.mutation_rounds);
    common(Stage::kMutate, &o);
    stages.AddRaw("mutate", o.ToString());
  }
  {
    JsonObject o;
    o.Add("min_reruns", c.min_reruns)
        .Add("min_stable_reruns", c.min_stable_reruns);
    o.Add("max_reruns", c.max_reruns).Add("flaky_threshold", c.flaky_threshold);
    common(Stage::kCheck, &o);
    stages.AddRaw("check", o.ToString());
  }
  {
    JsonObject o;
    o.Add("enabled", c.minimize);
    o.Add("max_tests", c.minimizer.max_tests)
        .Add("max_time_ms", c.minimizer.max_time_ms);
    o.Add("max_rounds", c.minimizer.max_rounds)
        .Add("target_time_ms", c.minimizer.target_time_ms);
    o.Add("min_tests", c.minimizer.min_tests);
    o.Add("probe_timeout_factor", c.probe_timeout_factor)
        .Add("probe_timeout_extra_ms", c.probe_timeout_extra_ms);
    o.Add("probe_timeout_min_ms", c.probe_timeout_min_ms)
        .Add("verify_attempts", c.verify_attempts);
    {
      // The effective order: without a configured list, the default one.
      std::vector<std::string> reducers = c.minimizer_reducers;
      if (reducers.empty()) {
        reducers.assign(Minimizer::DefaultOrder().begin(),
                        Minimizer::DefaultOrder().end());
      }
      o.AddArray("reducers", reducers);
    }
    common(Stage::kMinimize, &o);
    stages.AddRaw("minimize", o.ToString());
  }
  {
    JsonObject o;
    o.Add("save_corpus", c.save_corpus);
    common(Stage::kPublish, &o);
    stages.AddRaw("publish", o.ToString());
  }
  return stages.ToString();
}

bool ApplyJson(const JsonValue& json, FuzzConfig* c, std::string* error) {
  if (!json.is_object()) {
    *error = "config: top level must be an object";
    return false;
  }
  c->jobs = static_cast<int>(json.GetInt("jobs", c->jobs));
  c->seed =
      static_cast<uint64_t>(json.GetInt("seed", static_cast<int64_t>(c->seed)));
  c->out_dir = json.GetString("out_dir", c->out_dir);
  c->import_root = json.GetString("import_root", c->import_root);
  c->duration_sec = json.GetNumber("duration_sec", c->duration_sec);
  c->max_execs = static_cast<uint64_t>(
      json.GetInt("max_execs", static_cast<int64_t>(c->max_execs)));
  c->stats_interval_sec =
      json.GetNumber("stats_interval_sec", c->stats_interval_sec);
  c->component_stats = json.GetBool("component_stats", c->component_stats);
  c->save_corpus = json.GetBool("save_corpus", c->save_corpus);
  c->introspect = json.GetBool("introspect", c->introspect);
  c->introspect_timeout_ms =
      json.GetNumber("introspect_timeout_ms", c->introspect_timeout_ms);
  c->v8_flags = json.GetString("v8_flags", c->v8_flags);
  c->syntax_check = json.GetBool("syntax_check", c->syntax_check);
  c->consecutive_mutations = static_cast<int>(
      json.GetInt("consecutive_mutations", c->consecutive_mutations));
  c->mutation_rounds =
      static_cast<int>(json.GetInt("mutation_rounds", c->mutation_rounds));
  c->min_reruns = static_cast<int>(json.GetInt("min_reruns", c->min_reruns));
  c->min_stable_reruns =
      static_cast<int>(json.GetInt("min_stable_reruns", c->min_stable_reruns));
  c->max_reruns = static_cast<int>(json.GetInt("max_reruns", c->max_reruns));
  c->flaky_threshold =
      static_cast<int>(json.GetInt("flaky_threshold", c->flaky_threshold));
  c->value_prefix = json.GetBool("value_prefix", c->value_prefix);
  c->minimize = json.GetBool("minimize", c->minimize);
  c->minimize_crashes = json.GetBool("minimize_crashes", c->minimize_crashes);
  c->minimize_imports = json.GetBool("minimize_imports", c->minimize_imports);
  c->import_all = json.GetBool("import_all", c->import_all);
  c->initial_programs =
      static_cast<int>(json.GetInt("initial_programs", c->initial_programs));
  if (json.Has("scheduler")) {
    c->selection.seeds =
        SelectionConfig{json.GetString("scheduler", ""), JsonValue()};
  }
  c->crash_reruns =
      static_cast<int>(json.GetInt("crash_reruns", c->crash_reruns));
  c->crash_reruns_stop_after = static_cast<int>(
      json.GetInt("crash_reruns_stop_after", c->crash_reruns_stop_after));
  c->max_crashes_per_signature = static_cast<int>(
      json.GetInt("max_crashes_per_signature", c->max_crashes_per_signature));
  c->minimize_crashes_per_signature = static_cast<int>(json.GetInt(
      "minimize_crashes_per_signature", c->minimize_crashes_per_signature));
  c->crash_history =
      static_cast<int>(json.GetInt("crash_history", c->crash_history));
  c->crash_history_budget_sec =
      json.GetNumber("crash_history_budget_sec", c->crash_history_budget_sec);
  c->target.timeout_us = static_cast<uint64_t>(
      json.GetNumber("timeout_ms",
                     static_cast<double>(c->target.timeout_us) / 1000.0) *
      1000.0);
  c->target.max_execs_before_respawn = static_cast<int>(json.GetInt(
      "max_execs_before_respawn", c->target.max_execs_before_respawn));
  if (json.Has("d8")) {
    c->target.executable = json.GetString("d8", "");
  }
  const JsonValue& d8_args = json.Get("d8_args");
  if (d8_args.is_array()) {
    c->target.args.clear();
    for (const JsonValue& arg : d8_args.as_array()) {
      if (arg.is_string()) {
        c->target.args.push_back(arg.as_string());
      }
    }
  }
  // Appended to the d8 flags, like --d8-extra-args (presets use it).
  const JsonValue& d8_extra_args = json.Get("d8_extra_args");
  if (d8_extra_args.is_array()) {
    for (const JsonValue& arg : d8_extra_args.as_array()) {
      if (arg.is_string()) {
        c->target.args.push_back(arg.as_string());
      }
    }
  }
  ReadMinimizerOptions(json.Get("minimizer"), &c->minimizer);
  ReadMinimizerOptions(json.Get("crash_minimizer"), &c->crash_minimizer);
  const JsonValue& imports = json.Get("imports");
  if (imports.is_array()) {
    c->imports.clear();
    for (const JsonValue& path : imports.as_array()) {
      if (path.is_string()) {
        c->imports.push_back(path.as_string());
      }
    }
  }
  if (json.Has("prelude")) {
    c->prelude_source = json.GetString("prelude", "default");
  }
  // When both "imports" and "init" are present (such as in a full config dump
  // where "imports" is edited in place), keep explicit "imports" if "init"
  // only carries the default "generate" strategy.
  std::vector<std::string> explicit_imports = c->imports;
  if (!ReadInitConfig(json.Get("init"), c, error) ||
      !ReadAcceptConfig(json.Get("accept"), c, error) ||
      !ReadAmplifyConfig(json.Get("amplify"), c, error)) {
    return false;
  }
  if (c->imports.empty() && !explicit_imports.empty() &&
      json.Get("init").GetString("strategy", "") == "generate" &&
      imports.is_array() && !imports.as_array().empty()) {
    c->imports = std::move(explicit_imports);
  }
  const JsonValue& selection = json.Get("selection");
  if (!selection.is_null() && !selection.is_object()) {
    *error = "config: \"selection\" must be an object";
    return false;
  }
  if (!ReadSelectionConfig(selection, "seeds", &c->selection.seeds, error) ||
      !ReadSelectionConfig(selection, "partners", &c->selection.partners,
                           error) ||
      !ReadSelectionConfig(selection, "mutators", &c->selection.mutators,
                           error) ||
      !ReadSelectionConfig(selection, "generators", &c->selection.generators,
                           error)) {
    return false;
  }
  // Legacy shorthands: {"mutators": {"Name": weight}}, same for generators.
  std::vector<std::pair<std::string, double>> pairs;
  if (!ReadLegacyWeights(json, "mutators", &pairs, error) ||
      !AddWeightOverrides(&c->selection.mutators, pairs, error)) {
    return false;
  }
  pairs.clear();
  if (!ReadLegacyWeights(json, "generators", &pairs, error) ||
      !AddWeightOverrides(&c->selection.generators, pairs, error)) {
    return false;
  }
  // Tunables: {"name": value}.
  const JsonValue& tunables = json.Get("tunables");
  if (!tunables.is_null()) {
    if (!tunables.is_object()) {
      *error = "config: \"tunables\" must be an object of {\"name\": number}";
      return false;
    }
    for (const auto& [name, value] : tunables.as_object()) {
      if (!value.is_number()) {
        *error = "config: tunable \"" + name + "\" must be a number";
        return false;
      }
      if (!TunableRegistry::Instance().Set(name, value.as_number(), error)) {
        *error = "config: " + *error;
        return false;
      }
    }
  }
  // The "stages" block last: it groups the same parameters by stage and
  // wins over the flat keys above.
  return ReadStages(json, c, error);
}

}  // namespace

// static
bool FuzzConfig::Load(const Flags& flags, FuzzConfig* c, std::string* error,
                      bool for_run) {
  TunableRegistry::Instance().Reset();
  c->target.args = TargetOptions::DefaultArgs();
  // Fuzzing favours a short timeout: a mutant that runs longer than that is
  // almost always an endless loop, and it would eat the time of ten others.
  c->target.timeout_us = static_cast<uint64_t>(500) * 1000;
  c->prelude_source = "default";

  // Layer 1b: a preset (DESIGN.md 8), a configuration for one use case:
  // --preset=NAME loads experimental/redline/presets/NAME.json (found next to
  // the binary, like the testdata), --preset=FILE.json any file. --config and
  // the flags override it.
  std::string preset = flags.GetString("preset");
  if (!preset.empty()) {
    std::string path = preset;
    if (path.find('/') == std::string::npos &&
        path.find(".json") == std::string::npos) {
      path = RedlineDataDir("presets") + "/" + preset + ".json";
    }
    std::string text;
    if (!ReadFileToString(path, &text, error)) {
      *error = "--preset=" + preset + ": " + *error;
      return false;
    }
    JsonValue json;
    if (!JsonValue::Parse(text, &json, error) || !ApplyJson(json, c, error)) {
      *error = "--preset=" + preset + ": " + *error;
      return false;
    }
  }

  // Layer 2: config file, or inline JSON ('{...}') for a one-off override
  // of any key from the command line.
  std::string config_arg = flags.GetString("config");
  if (!config_arg.empty()) {
    std::string text;
    std::string where = "--config";
    if (config_arg[0] == '{') {
      text = config_arg;
    } else {
      where = config_arg;
      if (!ReadFileToString(config_arg, &text, error)) {
        return false;
      }
    }
    JsonValue json;
    if (!JsonValue::Parse(text, &json, error)) {
      *error = where + ":" + *error;
      return false;
    }
    if (!ApplyJson(json, c, error)) {
      return false;
    }
  }

  // Layer 3: command line. Each target flag overrides the file only when
  // given; the executable falls back to the file, then to $REDLINE_D8.
  {
    std::string d8 = flags.GetString("d8");
    if (d8.empty()) {
      d8 = c->target.executable;
    }
    if (d8.empty()) {
      const char* from_env = getenv("REDLINE_D8");
      if (from_env != nullptr) {
        d8 = from_env;
      }
    }
    if (d8.empty() && for_run) {
      *error =
          "No target given: pass --d8=PATH, set \"d8\" in the config or "
          "$REDLINE_D8";
      return false;
    }
    if (for_run && !d8.empty() && access(d8.c_str(), X_OK) != 0) {
      *error = StringPrintf("--d8=%s is not an executable file: %s", d8.c_str(),
                            strerror(errno));
      return false;
    }
    c->target.executable = d8;
    if (flags.Has("d8-args")) {
      c->target.args = SplitWhitespace(flags.GetString("d8-args"));
    }
    for (const std::string& arg :
         SplitWhitespace(flags.GetString("d8-extra-args"))) {
      c->target.args.push_back(arg);
    }
    for (const std::string& entry : flags.GetList("d8-env")) {
      size_t eq = entry.find('=');
      if (eq == std::string::npos) {
        *error = StringPrintf(
            "--d8-env entry '%s' is not of the form KEY=VALUE", entry.c_str());
        return false;
      }
      c->target.extra_env.emplace_back(entry.substr(0, eq),
                                       entry.substr(eq + 1));
    }
    if (flags.Has("timeout-ms")) {
      int64_t timeout_ms = flags.GetInt("timeout-ms", 1000);
      if (timeout_ms <= 0) {
        *error = "--timeout-ms must be positive";
        return false;
      }
      c->target.timeout_us = static_cast<uint64_t>(timeout_ms) * 1000;
    }
    if (flags.Has("max-execs-before-respawn")) {
      c->target.max_execs_before_respawn =
          static_cast<int>(flags.GetInt("max-execs-before-respawn", 1000));
    }
    c->target.capture_output =
        flags.GetBool("capture-output", c->target.capture_output);
  }

  c->jobs = static_cast<int>(flags.GetInt("jobs", c->jobs));
  c->seed = static_cast<uint64_t>(
      flags.GetInt("seed", static_cast<int64_t>(c->seed)));
  c->out_dir = flags.GetString("out", c->out_dir);
  c->import_root = flags.GetString("import-root", c->import_root);
  c->duration_sec = flags.GetDouble("duration", c->duration_sec);
  c->max_execs = static_cast<uint64_t>(
      flags.GetInt("max-execs", static_cast<int64_t>(c->max_execs)));
  c->stats_interval_sec =
      flags.GetDouble("stats-interval", c->stats_interval_sec);
  c->component_stats = flags.GetBool("component-stats", c->component_stats);
  c->save_corpus = flags.GetBool("save-corpus", c->save_corpus);
  c->quiet = flags.GetBool("quiet", c->quiet);
  c->introspect = flags.GetBool("introspect", c->introspect);
  c->syntax_check = flags.GetBool("syntax-check", c->syntax_check);
  c->consecutive_mutations = static_cast<int>(
      flags.GetInt("consecutive-mutations", c->consecutive_mutations));
  c->mutation_rounds =
      static_cast<int>(flags.GetInt("rounds", c->mutation_rounds));
  c->min_reruns = static_cast<int>(flags.GetInt("min-reruns", c->min_reruns));
  c->min_stable_reruns =
      static_cast<int>(flags.GetInt("min-stable-reruns", c->min_stable_reruns));
  c->max_reruns = static_cast<int>(flags.GetInt("max-reruns", c->max_reruns));
  c->flaky_threshold =
      static_cast<int>(flags.GetInt("flaky-threshold", c->flaky_threshold));
  c->value_prefix = flags.GetBool("value-prefix", c->value_prefix);
  c->minimize = flags.GetBool("minimize", c->minimize);
  c->minimize_crashes = flags.GetBool("minimize-crashes", c->minimize_crashes);
  c->minimize_imports = flags.GetBool("minimize-imports", c->minimize_imports);
  c->import_all = flags.GetBool("import-all", c->import_all);
  c->initial_programs =
      static_cast<int>(flags.GetInt("initial-programs", c->initial_programs));
  c->minimizer.max_tests = static_cast<int>(
      flags.GetInt("minimizer-max-tests", c->minimizer.max_tests));
  c->minimizer.max_time_ms = static_cast<uint64_t>(flags.GetInt(
      "minimizer-max-time-ms", static_cast<int64_t>(c->minimizer.max_time_ms)));
  c->minimizer.target_time_ms = static_cast<uint64_t>(
      flags.GetInt("minimizer-target-time-ms",
                   static_cast<int64_t>(c->minimizer.target_time_ms)));
  c->crash_reruns =
      static_cast<int>(flags.GetInt("crash-reruns", c->crash_reruns));
  c->max_crashes_per_signature = static_cast<int>(
      flags.GetInt("max-crashes-per-signature", c->max_crashes_per_signature));
  c->minimize_crashes_per_signature = static_cast<int>(flags.GetInt(
      "minimize-crashes-per-signature", c->minimize_crashes_per_signature));
  c->crash_history =
      static_cast<int>(flags.GetInt("crash-history", c->crash_history));
  c->crash_history_budget_sec =
      flags.GetDouble("crash-history-budget", c->crash_history_budget_sec);
  c->crash_minimizer.max_tests = static_cast<int>(
      flags.GetInt("crash-minimizer-max-tests", c->crash_minimizer.max_tests));
  c->crash_minimizer.max_time_ms = static_cast<uint64_t>(
      flags.GetInt("crash-minimizer-max-time-ms",
                   static_cast<int64_t>(c->crash_minimizer.max_time_ms)));
  if (flags.Has("scheduler")) {
    // The flag names a strategy without parameters: it replaces the
    // whole seeds setting, including parameters a config file gave to
    // another strategy.
    c->selection.seeds =
        SelectionConfig{flags.GetString("scheduler", ""), JsonValue()};
  }
  c->v8_flags = flags.GetString("v8-flags", c->v8_flags);
  for (const std::string& path : flags.GetList("import")) {
    c->imports.push_back(path);
  }
  std::vector<std::pair<std::string, double>> pairs;
  if (!ParseWeightList(flags.GetList("mutator-weights"), &pairs, error)) {
    return false;
  }
  for (const std::string& name : flags.GetList("disable-mutator")) {
    pairs.emplace_back(name, 0.0);
  }
  if (!AddWeightOverrides(&c->selection.mutators, pairs, error)) {
    return false;
  }
  pairs.clear();
  if (!ParseWeightList(flags.GetList("generator-weights"), &pairs, error)) {
    return false;
  }
  for (const std::string& name : flags.GetList("disable-generator")) {
    pairs.emplace_back(name, 0.0);
  }
  if (!AddWeightOverrides(&c->selection.generators, pairs, error)) {
    return false;
  }
  c->prefix_min_values =
      static_cast<int>(flags.GetInt("prefix-min-values", c->prefix_min_values));
  c->prefix_extra_values = static_cast<int>(
      flags.GetInt("prefix-extra-values", c->prefix_extra_values));
  for (const std::string& rule : flags.GetList("capture", ';')) {
    Stage stage;
    CaptureRule capture;
    if (!CaptureRule::FromFlag(rule, &stage, &capture, error)) {
      return false;
    }
    c->stage(stage).capture = capture;
  }
  if (flags.Has("tunable") && !TunableRegistry::Instance().SetFromList(
                                  flags.GetString("tunable"), error)) {
    return false;
  }
  for (const std::string& name : flags.GetList("stage-log")) {
    Stage stage;
    if (!ParseStage(name, &stage)) {
      *error = "--stage-log: unknown stage '" + name +
               "' (known: pick, prefix, mutate, check, minimize, publish)";
      return false;
    }
    c->stage(stage).events = true;
  }
  if (flags.Has("prelude")) {
    c->prelude_source = flags.GetString("prelude", "default");
  }

  // Resolve the prelude.
  if (c->prelude_source.empty() || c->prelude_source == "default") {
    c->prelude = std::string(DefaultPrelude());
    c->prelude_source = "default";
  } else if (c->prelude_source == "none") {
    c->prelude.clear();
  } else if (!ReadFileToString(c->prelude_source, &c->prelude, error)) {
    return false;
  }

  // Validation.
  if (c->jobs < 1) {
    *error = "--jobs must be at least 1";
    return false;
  }
  // The run seed is drawn only when there is a run; --dump-config shows 0
  // (= from the clock) like the configuration said.
  if (c->seed == 0 && for_run) {
    c->seed = WallClockNowMs() ^ 0x5eed5eed5eedull;
  }
  if (c->consecutive_mutations < 1) {
    c->consecutive_mutations = 1;
  }
  if (c->mutation_rounds < 0) {
    c->mutation_rounds = 0;
  }
  if (c->prefix_min_values < 1) {
    c->prefix_min_values = 1;
  }
  if (c->prefix_extra_values < 0) {
    c->prefix_extra_values = 0;
  }
  if (c->min_stable_reruns < 1) {
    c->min_stable_reruns = 1;
  }
  if (c->min_reruns < c->min_stable_reruns) {
    c->min_reruns = c->min_stable_reruns;
  }
  if (c->max_reruns < c->min_reruns) {
    c->max_reruns = c->min_reruns;
  }
  if (c->flaky_threshold < 1) {
    c->flaky_threshold = 1;
  }
  if (c->crash_reruns < 1) {
    c->crash_reruns = 1;
  }
  if (c->crash_reruns_stop_after < 1) {
    c->crash_reruns_stop_after = 1;
  }
  if (c->max_crashes_per_signature < 1) {
    c->max_crashes_per_signature = 1;
  }
  if (c->crash_history < 0) {
    c->crash_history = 0;
  }
  if (c->verify_attempts < 1) {
    c->verify_attempts = 1;
  }
  c->probe_timeout_factor = std::max(0.0, c->probe_timeout_factor);
  c->probe_timeout_extra_ms = std::max(0.0, c->probe_timeout_extra_ms);
  c->probe_timeout_min_ms = std::max(0.0, c->probe_timeout_min_ms);
  c->introspect_timeout_ms = std::max(1.0, c->introspect_timeout_ms);
  if (!ValidateReducers(c->minimizer_reducers, error)) {
    return false;
  }
  return ValidateSelection(c->selection, error);
}

// static
bool FuzzConfig::ValidateReducers(const std::vector<std::string>& names,
                                  std::string* error) {
  std::vector<std::string> known;
  for (const auto& reducer : Minimizer::CreateDefaultReducers()) {
    known.push_back(reducer->name());
  }
  for (const std::string& name : names) {
    if (std::find(known.begin(), known.end(), name) == known.end()) {
      *error = "config: unknown reducer \"" + name +
               "\" in stages.minimize.reducers (known: " + JoinNames(known) +
               ")";
      return false;
    }
  }
  return true;
}

namespace {

// The selectors built from `selection` exactly as the fuzzer builds them:
// validation and the full configuration dump both go through here, so what
// is reported is what will run.
struct BuiltSelectors {
  std::unique_ptr<ProgramSelector> seeds;
  std::unique_ptr<ProgramSelector> partners;
  std::unique_ptr<MutatorSet> mutators;      // owns the mutator selector
  std::unique_ptr<GeneratorSet> generators;  // owns the generator selector
};

bool BuildSelectors(const FuzzConfig::SelectionSettings& selection,
                    BuiltSelectors* out, std::string* error) {
  out->seeds = SelectorRegistry<const Program>::Instance().Create(
      selection.seeds, error);
  if (out->seeds == nullptr) {
    *error = "selection.seeds: " + *error;
    return false;
  }
  SelectionConfig partners = selection.partners;
  if (partners.strategy.empty()) {
    partners.strategy = "uniform-partner";
  }
  out->partners =
      SelectorRegistry<const Program>::Instance().Create(partners, error);
  if (out->partners == nullptr) {
    *error = "selection.partners: " + *error;
    return false;
  }
  out->mutators = MutatorSet::CreateDefault();
  out->generators = GeneratorSet::CreateDefault();
  std::unique_ptr<Selector<Mutator>> mutator_selector =
      SelectorRegistry<Mutator>::Instance().Create(selection.mutators, error);
  if (mutator_selector == nullptr ||
      !out->mutators->SetSelector(std::move(mutator_selector), error)) {
    *error = "selection.mutators: " + *error;
    return false;
  }
  std::unique_ptr<Selector<Generator>> generator_selector =
      SelectorRegistry<Generator>::Instance().Create(selection.generators,
                                                     error);
  if (generator_selector == nullptr ||
      !out->generators->SetSelector(std::move(generator_selector), error)) {
    *error = "selection.generators: " + *error;
    return false;
  }
  return true;
}

}  // namespace

std::string FuzzConfig::ToJson(Detail detail) const {
  const bool full = detail == Detail::kFull;
  JsonObject o;
  o.Add("d8", target.executable);
  o.AddArray("d8_args", target.args);
  o.Add("timeout_ms", static_cast<double>(target.timeout_us) / 1000.0);
  o.Add("max_execs_before_respawn", target.max_execs_before_respawn);
  o.Add("prelude", prelude_source);
  o.Add("jobs", jobs);
  o.Add("seed", static_cast<uint64_t>(seed));
  o.Add("out_dir", out_dir);
  o.AddArray("imports", imports);
  o.Add("import_root", import_root);
  o.Add("duration_sec", duration_sec);
  o.Add("max_execs", static_cast<uint64_t>(max_execs));
  o.Add("stats_interval_sec", stats_interval_sec);
  o.Add("introspect", introspect);
  o.Add("introspect_timeout_ms", introspect_timeout_ms);
  o.Add("v8_flags", v8_flags);
  o.Add("syntax_check", syntax_check);
  o.Add("component_stats", component_stats);
  o.Add("minimize_crashes", minimize_crashes);
  o.Add("minimize_imports", minimize_imports);
  o.Add("import_all", import_all);
  o.Add("initial_programs", initial_programs);
  o.Add("crash_reruns", crash_reruns);
  o.Add("crash_reruns_stop_after", crash_reruns_stop_after);
  o.Add("max_crashes_per_signature", max_crashes_per_signature);
  o.Add("minimize_crashes_per_signature", minimize_crashes_per_signature);
  o.Add("crash_history", crash_history);
  o.Add("crash_history_budget_sec", crash_history_budget_sec);
  o.AddRaw("crash_minimizer", MinimizerOptionsJson(crash_minimizer));
  {
    // The Init strategy as the fields describe it, so that loading the
    // output back gives the same configuration.
    JsonObject init, params;
    if (imports.empty()) {
      init.Add("strategy", "generate");
      params.Add("programs", initial_programs);
    } else {
      init.Add("strategy", "import");
      params.AddArray("paths", imports);
      params.Add("root", import_root);
      params.Add("keep", import_all ? "all" : "new-coverage");
      params.Add("minimize", minimize_imports);
    }
    init.AddRaw("params", params.ToString());
    o.AddRaw("init", init.ToString());
    JsonObject accept;
    accept.Add("strategy", accept_strategy);
    o.AddRaw("accept", accept.ToString());
    JsonObject amplify;
    amplify.Add("strategy", amplify_strategy);
    amplify.AddRaw("params", amplify_params);
    o.AddRaw("amplify", amplify.ToString());
  }
  JsonObject sel;
  BuiltSelectors built;
  std::string ignored;
  if (full && BuildSelectors(selection, &built, &ignored)) {
    // Every parameter of the chosen strategies, as the strategies see
    // them (all weights, including the ones left at their default).
    auto point = [](std::string_view strategy, const JsonObject& params) {
      JsonObject p;
      p.Add("strategy", strategy);
      p.AddRaw("params", params.ToString());
      return p.ToString();
    };
    sel.AddRaw("seeds", point(built.seeds->name(), built.seeds->Params()));
    sel.AddRaw("partners",
               point(built.partners->name(), built.partners->Params()));
    sel.AddRaw("mutators", point(built.mutators->selector().name(),
                                 built.mutators->selector().Params()));
    sel.AddRaw("generators", point(built.generators->selector().name(),
                                   built.generators->selector().Params()));
  } else {
    sel.AddRaw("seeds",
               SelectionConfigJson(
                   selection.seeds,
                   SelectorRegistry<const Program>::Instance().default_name()));
    sel.AddRaw("partners",
               SelectionConfigJson(selection.partners, "uniform-partner"));
    sel.AddRaw("mutators",
               SelectionConfigJson(
                   selection.mutators,
                   SelectorRegistry<Mutator>::Instance().default_name()));
    sel.AddRaw("generators",
               SelectionConfigJson(
                   selection.generators,
                   SelectorRegistry<Generator>::Instance().default_name()));
  }
  o.AddRaw("selection", sel.ToString());
  o.AddRaw("stages", StagesJson(*this));
  // Compact: only the tunables that were changed (--help-tunables lists
  // them all). Full: every one of them with its current value.
  o.AddRaw("tunables", full ? TunableRegistry::Instance().AllJson()
                            : TunableRegistry::Instance().NonDefaultsJson());
  return o.ToString();
}

// static
bool FuzzConfig::LoadJsonForTest(const JsonValue& json, FuzzConfig* config,
                                 std::string* error) {
  return ApplyJson(json, config, error);
}

// static
bool FuzzConfig::ValidateSelection(const SelectionSettings& selection,
                                   std::string* error) {
  // Build every selector once, exactly as the fuzzer will, so that an
  // unknown strategy, parameter or weight name is reported before any
  // target is started.
  BuiltSelectors built;
  return BuildSelectors(selection, &built, error);
}

// static
std::string FuzzConfig::JsonHelp() {
  std::string help =
      "config file (--config=FILE or --config='{...}', JSON; flags override "
      "it, and it\n"
      "overrides --preset=NAME|FILE, a configuration per area in "
      "experimental/redline/presets;\n"
      "`redline fuzz --dump-config` prints the complete effective "
      "configuration with\n"
      "every default, ready to be edited and passed back):\n"
      "  {\n"
      "    \"d8\": \"out/fuzzbuild/d8\", \"d8_args\": [\"--expose-gc\", ...],\n"
      "    \"d8_extra_args\": [\"--maglev\"],  // appended to d8_args (like "
      "--d8-extra-args)\n"
      "    \"timeout_ms\": 1000, \"jobs\": 8, \"seed\": 1, \"out_dir\": "
      "\"redline-out\",\n"
      "    \"imports\": [\"test/mjsunit\"], \"import_root\": \".\", "
      "\"prelude\": \"default\",\n"
      "    \"duration_sec\": 0, \"max_execs\": 0, \"stats_interval_sec\": 30,\n"
      "    \"introspect\": true, \"introspect_timeout_ms\": 20000,\n"
      "    \"syntax_check\": false,           // parse mutants in-process "
      "before executing them\n"
      "    \"stages\": {                       // the worker loop, stage by "
      "stage\n"
      "      \"pick\":     {\"stats\": true},\n"
      "      \"prefix\":   {\"enabled\": true, \"min_values\": 10, "
      "\"extra_values\": 5},\n"
      "      \"mutate\":   {\"consecutive\": 5, \"rounds\": 0, \"log\": "
      "\"off\",\n"
      "                   \"capture\": {\"on\": [\"timeout\"], \"mutators\": "
      "[\"Splice\"], \"max\": 50}},\n"
      "      \"check\":    {\"min_reruns\": 5, \"min_stable_reruns\": 1, "
      "\"max_reruns\": 50,\n"
      "                   \"flaky_threshold\": 50, \"capture\": {\"on\": "
      "[\"flaky\"], \"max\": 20}},\n"
      "      \"minimize\": {\"enabled\": true, \"max_tests\": 300, "
      "\"max_time_ms\": 30000,\n"
      "                   \"max_rounds\": 3, \"target_time_ms\": 6000, "
      "\"min_tests\": 40,\n"
      "                   \"probe_timeout_factor\": 5, "
      "\"probe_timeout_extra_ms\": 20,\n"
      "                   \"probe_timeout_min_ms\": 50, \"verify_attempts\": "
      "3,\n"
      "                   \"reducers\": [\"statements\", \"unwrap\", "
      "\"lists\", \"expressions\"]},\n"
      "      \"publish\":  {\"save_corpus\": true}\n"
      "    },\n"
      "    \"minimize_crashes\": true, \"minimize_imports\": false,\n"
      "    \"crash_reruns\": 3, \"crash_reruns_stop_after\": 2, "
      "\"max_crashes_per_signature\": 5,\n"
      "    \"minimize_crashes_per_signature\": 2, \"crash_history\": 100,\n"
      "    \"crash_history_budget_sec\": 90,\n"
      "    \"crash_minimizer\": {\"max_tests\": 1000, \"max_time_ms\": 120000, "
      "\"max_rounds\": 4,\n"
      "                        \"target_time_ms\": 30000, \"min_tests\": "
      "200},\n"
      "    \"import_all\": false, \"initial_programs\": 32, \"save_corpus\": "
      "true,\n"
      "    \"init\": {\"strategy\": \"import\",      // or \"generate\" with "
      "{\"programs\": N}\n"
      "             \"params\": {\"paths\": [\"test/mjsunit\"], \"root\": "
      "\".\",\n"
      "                        \"keep\": \"new-coverage\", \"minimize\": "
      "false}},  // keep: or \"all\"\n"
      "    \"accept\": {\"strategy\": \"new-edges\"},\n"
      "    \"selection\": {\n"
      "      \"seeds\":      {\"strategy\": \"uniform\"},\n"
      "      \"partners\":   {\"strategy\": \"uniform-partner\", \"params\": "
      "{\"tries\": 4}},\n"
      "      \"mutators\":   {\"strategy\": \"weighted\", \"params\": "
      "{\"weights\": {\"Splice\": 4, \"Reorder\": "
      "0}}},\n"
      "      \"generators\": {\"strategy\": \"weighted\", \"params\": "
      "{\"weights\": {\"JitOptimize\": 10}}}\n"
      "    }\n"
      "  }\n"
      "\n"
      "  Every stage takes \"log\": \"events\"|\"off\" (one JSONL event per "
      "occurrence),\n"
      "  \"capture\": {\"on\": [trigger, ...], \"mutators\": [...], \"max\": "
      "N} (save the\n"
      "  programs of the stage to OUT/captures/<stage>/ when a trigger fires; "
      "triggers:\n"
      "  mutate: timeout crash failed invalid noop interesting; check: flaky "
      "timeout;\n"
      "  minimize: rejected timeout; prefix: noop) and \"stats\": true|false "
      "(row in the\n"
      "  stages table). Flags: "
      "--capture=STAGE:TRIGGER[,..][:MUTATOR[,..]][:MAX] (\';\' between\n"
      "  rules), --stage-log=STAGE[,..]. The flat keys consecutive_mutations, "
      "mutation_rounds,\n"
      "  min_reruns, min_stable_reruns, max_reruns, flaky_threshold, "
      "value_prefix, minimize,\n"
      "  minimizer, save_corpus are still accepted as shorthands for the stage "
      "parameters.\n"
      "\n"
      "  \"tunables\": {\"name\": value} sets the numbers inside generators, "
      "mutators and\n"
      "  reducers (also --tunable=name=value,...); redline fuzz "
      "--help-tunables lists\n"
      "  them with defaults, ranges and documentation. Unknown names and "
      "values outside\n"
      "  the range are errors.\n"
      "\n"
      "  \"init\" sets imports, import_root, import_all and minimize_imports "
      "(generate\n"
      "  clears the imports); the flags --import, --import-all, "
      "--initial-programs set the\n"
      "  same values. \"accept\" names the corpus admission criterion "
      "(new-edges only).\n"
      "  stages.minimize.reducers lists the reducers in order (also truncate, "
      "dataflow);\n"
      "  without it the four above run. Unknown names are errors.\n"
      "\n"
      "  Shorthands: \"scheduler\": NAME (= selection.seeds.strategy), "
      "\"mutators\" and\n"
      "  \"generators\": {\"Name\": weight} (= params.weights of the weighted "
      "strategy;\n"
      "  weight 0 disables). Unknown strategies, parameters and names are "
      "errors.\n"
      "\n"
      "  Selection strategies and their parameters (name = default  "
      "description):\n";
  help += SelectorRegistry<const Program>::Instance().Help("seeds / partners");
  help += SelectorRegistry<Mutator>::Instance().Help("mutators");
  help += SelectorRegistry<Generator>::Instance().Help("generators");
  // The amplify step of crash triage (DESIGN.md 5.8), from its registry.
  help +=
      "\n  \"amplify\": {\"strategy\": NAME, \"params\": {...}} searches d8 "
      "flags that reproduce a crash\n"
      "  that does not reproduce alone (default strategy: flags):\n";
  for (const AmplifyStrategyInfo& info : AmplifyStrategies()) {
    help += StringPrintf("    %.*s: %.*s\n", static_cast<int>(info.name.size()),
                         info.name.data(), static_cast<int>(info.doc.size()),
                         info.doc.data());
    for (const ParamSpec& p : info.params) {
      help += StringPrintf(
          "      %.*s = %.*s  %.*s\n", static_cast<int>(p.name.size()),
          p.name.data(), static_cast<int>(p.default_value.size()),
          p.default_value.data(), static_cast<int>(p.doc.size()), p.doc.data());
    }
  }
  return help;
}

}  // namespace redline
