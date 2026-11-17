// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/amplifier.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/exec/process_run.h"
#include "experimental/redline/fast/parser.h"
#include "experimental/redline/fast/printer.h"

namespace redline {

namespace {

constexpr char kLog[] = "amplify";

std::vector<std::string> SplitArgs(const std::string& text) {
  std::vector<std::string> out;
  std::istringstream stream(text);
  std::string piece;
  while (stream >> piece) {
    out.push_back(piece);
  }
  return out;
}

// A stack rule: if one of `frames` occurs in the crash output, try `args`.
struct Rule {
  std::vector<std::string> frames;
  std::string args;
};

std::vector<Rule> DefaultRules() {
  return {
      {{"HandleProfilerSignal", "CpuSampler", "TickSample"},
       "--cpu-profiler --cpu-profiler-sampling-interval=10"},
      {{"MarkCompactCollector", "Scavenger", "ConcurrentMarking", "Sweeper"},
       "--stress-compaction"},
  };
}

std::string RulesJson(const std::vector<Rule>& rules) {
  std::string out = "[";
  for (size_t i = 0; i < rules.size(); i++) {
    JsonObject o;
    o.AddArray("frames", rules[i].frames);
    o.Add("args", rules[i].args);
    out += (i > 0 ? "," : "") + o.ToString();
  }
  return out + "]";
}

// The session's flags with a new --random-seed (appended if absent), then
// the candidate's flags, then --stress-runs.
std::vector<std::string> ProcessArgs(const std::vector<std::string>& base,
                                     const std::string& extra, int stress_runs,
                                     Random& rng) {
  const std::string seed =
      "--random-seed=" + std::to_string(1 + rng.Index(2000000000));
  std::vector<std::string> args;
  bool seeded = false;
  for (const std::string& arg : base) {
    if (arg.starts_with("--random-seed=") ||
        arg.starts_with("--random_seed=")) {
      args.push_back(seed);
      seeded = true;
    } else if (!arg.starts_with("--stress-runs=") &&
               !arg.starts_with("--stress_runs=")) {
      args.push_back(arg);
    }
  }
  if (!seeded) {
    args.push_back(seed);
  }
  for (std::string& arg : SplitArgs(extra)) {
    args.push_back(std::move(arg));
  }
  args.push_back("--stress-runs=" + std::to_string(stress_runs));
  return args;
}

class NoneStrategy final : public AmplifyStrategy {
 public:
  std::string_view name() const override { return "none"; }
  AmplifyResult Amplify(const AmplifyRequest&, Random&) override {
    AmplifyResult r;
    r.note = "amplify strategy none";
    return r;
  }
  JsonObject Params() const override { return JsonObject(); }
  int per_signature() const override { return 0; }
};

class FlagsStrategy final : public AmplifyStrategy {
 public:
  struct Settings {
    double budget_sec = 180;
    int processes = 5;
    int min_hits = 4;
    double loop_time_sec = 10;
    int max_loops = 100;
    double run_timeout_sec = 30;
    int per_signature = 1;
    bool wrap = true;
    std::vector<Rule> rules = DefaultRules();
  };

  static std::vector<ParamSpec> Specs() {
    return {
        {"budget_sec", ParamSpec::Type::kNumber, "180",
         "time limit of the search for one crash"},
        {"processes", ParamSpec::Type::kNumber, "5",
         "fresh d8 processes per candidate"},
        {"min_hits", ParamSpec::Type::kNumber, "4",
         "processes with the same signature that make it amplified"},
        {"loop_time_sec", ParamSpec::Type::kNumber, "10",
         "target duration of one process (sets --stress-runs)"},
        {"max_loops", ParamSpec::Type::kNumber, "100",
         "largest --stress-runs value"},
        {"run_timeout_sec", ParamSpec::Type::kNumber, "30",
         "a process is killed after this time"},
        {"per_signature", ParamSpec::Type::kNumber, "1",
         "searches per signature and session"},
        {"wrap", ParamSpec::Type::kBool, "true",
         "if the program throws an uncaught exception, wrap its statements in "
         "try/catch"},
        {"rules", ParamSpec::Type::kObject, "built-in",
         "[{\"frames\": [substrings], \"args\": \"extra d8 flags\"}, ...]: "
         "candidates from the crash stack"},
    };
  }

  static bool Read(const JsonValue& params, Settings* s, std::string* error) {
    if (!params.is_null() && !params.is_object()) {
      *error = "amplify.params must be an object";
      return false;
    }
    if (params.is_object()) {
      for (const auto& [key, value] : params.as_object()) {
        const auto specs = Specs();
        if (std::none_of(specs.begin(), specs.end(),
                         [&](const ParamSpec& p) { return p.name == key; })) {
          *error = "unknown amplify param \"" + key + "\" for strategy flags";
          return false;
        }
      }
    }
    s->budget_sec = params.GetNumber("budget_sec", s->budget_sec);
    s->processes = static_cast<int>(params.GetInt("processes", s->processes));
    s->min_hits = static_cast<int>(params.GetInt("min_hits", s->min_hits));
    s->loop_time_sec = params.GetNumber("loop_time_sec", s->loop_time_sec);
    s->max_loops = static_cast<int>(params.GetInt("max_loops", s->max_loops));
    s->run_timeout_sec =
        params.GetNumber("run_timeout_sec", s->run_timeout_sec);
    s->per_signature =
        static_cast<int>(params.GetInt("per_signature", s->per_signature));
    s->wrap = params.GetBool("wrap", s->wrap);
    const JsonValue& rules = params.Get("rules");
    if (!rules.is_null()) {
      if (!rules.is_array()) {
        *error = "amplify.params.rules must be a list";
        return false;
      }
      s->rules.clear();
      for (const JsonValue& rule : rules.as_array()) {
        if (!rule.is_object() || !rule.Get("frames").is_array()) {
          *error =
              "every amplify rule needs \"frames\" (non-empty strings) and "
              "\"args\"";
          return false;
        }
        Rule r;
        r.args = rule.GetString("args", "");
        for (const JsonValue& frame : rule.Get("frames").as_array()) {
          if (frame.is_string() && !frame.as_string().empty()) {
            r.frames.push_back(frame.as_string());
          }
        }
        if (r.frames.empty() || r.args.empty()) {
          *error =
              "every amplify rule needs \"frames\" (non-empty strings) and "
              "\"args\"";
          return false;
        }
        s->rules.push_back(std::move(r));
      }
    }
    if (s->budget_sec <= 0 || s->processes < 1 || s->min_hits < 1 ||
        s->min_hits > s->processes || s->loop_time_sec <= 0 ||
        s->max_loops < 2 || s->run_timeout_sec <= 0 || s->per_signature < 0) {
      *error =
          "amplify flags: need budget_sec > 0, 1 <= min_hits <= processes, "
          "loop_time_sec > 0, max_loops >= 2, "
          "run_timeout_sec > 0, per_signature >= 0";
      return false;
    }
    return true;
  }

  explicit FlagsStrategy(Settings settings) : s_(std::move(settings)) {}

  std::string_view name() const override { return "flags"; }
  int per_signature() const override { return s_.per_signature; }

  JsonObject Params() const override {
    JsonObject o;
    o.Add("budget_sec", s_.budget_sec)
        .Add("processes", s_.processes)
        .Add("min_hits", s_.min_hits);
    o.Add("loop_time_sec", s_.loop_time_sec).Add("max_loops", s_.max_loops);
    o.Add("run_timeout_sec", s_.run_timeout_sec)
        .Add("per_signature", s_.per_signature);
    o.Add("wrap", s_.wrap);
    o.AddRaw("rules", RulesJson(s_.rules));
    return o;
  }

  AmplifyResult Amplify(const AmplifyRequest& request, Random& rng) override {
    AmplifyResult result;
    const Stopwatch clock;
    const uint64_t timeout_us = static_cast<uint64_t>(s_.run_timeout_sec * 1e6);
    const auto env = request.target.PlainEnvironment();
    auto write = [&](const std::string& program) {
      std::string script =
          request.prelude.empty() ? program : request.prelude + "\n" + program;
      std::string write_error;
      if (!WriteStringToFile(request.script_path, script, &write_error)) {
        result.note =
            "cannot write " + request.script_path + ": " + write_error;
        return false;
      }
      return true;
    };
    if (!write(request.program)) {
      return result;
    }
    // One plain run without extra flags: if the program ends with an
    // uncaught exception, d8 would end every --stress-runs loop after the
    // first run. Then the candidates run on the wrapped program.
    std::string wrapped;
    if (s_.wrap) {
      std::vector<std::string> args = request.target.args;
      args.push_back(request.script_path);
      const ProcessResult probe =
          RunProcess(request.target.executable, args, env, timeout_us);
      result.total_processes++;
      const bool throws =
          probe.started && !probe.timed_out && probe.signal == 0 &&
          probe.exit_code != 0 &&
          !(CrashSignature::FromOutput(probe.signal, probe.exit_code,
                                       probe.stderr_text) == request.signature);
      if (throws) {
        wrapped = WrapInTry(request.program);
        if (!wrapped.empty() && !write(wrapped)) {
          return result;
        }
      }
    }
    // Candidates: the matching rules in their order, then the plain loop.
    std::vector<std::string> candidates;
    for (const Rule& rule : s_.rules) {
      for (const std::string& frame : rule.frames) {
        if (request.stderr_text.find(frame) != std::string::npos) {
          candidates.push_back(rule.args);
          break;
        }
      }
    }
    candidates.emplace_back();
    const int loops =
        StressRunsFor(request.exec_time_us, s_.loop_time_sec, s_.max_loops);
    const uint64_t budget_us = static_cast<uint64_t>(s_.budget_sec * 1e6);
    std::string tried = wrapped.empty() ? "" : "program wrapped in try/catch; ";
    for (const std::string& extra : candidates) {
      int hits = 0, runs = 0;
      for (; runs < s_.processes; runs++) {
        if (clock.ElapsedUs() > budget_us ||
            (request.stopping && request.stopping())) {
          break;
        }
        // Stop when this candidate can no longer reach min_hits.
        if (hits + (s_.processes - runs) < s_.min_hits) {
          break;
        }
        const std::vector<std::string> args =
            ProcessArgs(request.target.args, extra, loops, rng);
        std::vector<std::string> with_script(args);
        with_script.push_back(request.script_path);
        const ProcessResult run =
            RunProcess(request.target.executable, with_script, env, timeout_us);
        result.total_processes++;
        if (!run.started) {
          result.note = "cannot start d8: " + run.error;
          return result;
        }
        if (!run.timed_out &&
            CrashSignature::FromOutput(run.signal, run.exit_code,
                                       run.stderr_text) == request.signature) {
          hits++;
        }
      }
      tried += StringPrintf("%s[%s]: %d/%d",
                            tried.empty() || tried.back() == ' ' ? "" : "; ",
                            extra.empty() ? "loop" : extra.c_str(), hits, runs);
      if (hits > result.hits || result.processes == 0) {
        result.hits = hits;
        result.processes = runs;
        result.args = (extra.empty() ? "" : extra + " ") +
                      "--stress-runs=" + std::to_string(loops);
      }
      if (hits >= s_.min_hits) {
        result.amplified = true;
        break;
      }
      if (clock.ElapsedUs() > budget_us ||
          (request.stopping && request.stopping())) {
        break;
      }
    }
    result.time_sec = static_cast<double>(clock.ElapsedUs()) / 1e6;
    result.note = tried;
    if (result.amplified) {
      result.program = wrapped;  // empty if the program did not change
    }
    REDLINE_LOG(kLog, "%s after %d processes in %.0f s: %s",
                result.amplified ? "amplified" : "not amplified",
                result.total_processes, result.time_sec, tried.c_str());
    return result;
  }

 private:
  Settings s_;
};

}  // namespace

int StressRunsFor(uint64_t exec_time_us, double loop_time_sec, int max_loops) {
  const double one =
      std::max<double>(static_cast<double>(exec_time_us), 1000.0);
  const double n = loop_time_sec * 1e6 / one;
  return static_cast<int>(
      std::clamp<double>(n, 2.0, static_cast<double>(max_loops)));
}

std::string WrapInTry(const std::string& program) {
  fast::ParseError error;
  std::unique_ptr<fast::Tree> tree =
      fast::Parser::ParseScript(program, fast::ParseOptions(), &error);
  if (tree == nullptr || tree->root() == nullptr) {
    return "";
  }
  fast::Program* root = tree->root();
  std::vector<fast::Statement*> head, body;
  for (fast::Statement* statement : root->body) {
    const auto* expression = statement->As<fast::ExpressionStatement>();
    if (body.empty() && expression != nullptr && expression->is_directive) {
      head.push_back(statement);  // the directive prologue stays in front
    } else {
      body.push_back(statement);
    }
  }
  if (body.empty()) {
    return "";
  }
  head.push_back(mk::Try(tree.get(), body, "redline_exception", {}));
  root->body = std::move(head);
  return fast::Print(root);
}

std::unique_ptr<AmplifyStrategy> CreateAmplifyStrategy(std::string_view name,
                                                       const JsonValue& params,
                                                       std::string* error) {
  if (name.empty() || name == "flags") {
    FlagsStrategy::Settings settings;
    if (!FlagsStrategy::Read(params, &settings, error)) {
      return nullptr;
    }
    return std::make_unique<FlagsStrategy>(std::move(settings));
  }
  if (name == "none") {
    if (params.is_object() && !params.as_object().empty()) {
      *error = "amplify strategy none takes no params";
      return nullptr;
    }
    return std::make_unique<NoneStrategy>();
  }
  *error = "unknown amplify strategy \"" + std::string(name) +
           "\" (known: flags, none)";
  return nullptr;
}

std::vector<AmplifyStrategyInfo> AmplifyStrategies() {
  return {
      {"flags",
       "extra d8 flags from stack rules, then the plain loop; each candidate "
       "runs fresh processes with "
       "--stress-runs",
       FlagsStrategy::Specs()},
      {"none", "the amplify step is off", {}},
  };
}

}  // namespace redline
