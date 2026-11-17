// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/stages.h"

#include <cstdlib>
#include <string>
#include <vector>

#include "experimental/redline/base/log.h"

namespace redline {

namespace {

constexpr std::string_view kStageNames[kNumStages] = {
    "pick", "prefix", "mutate", "check", "minimize", "publish"};

struct TriggerInfo {
  CaptureTrigger trigger;
  std::string_view name;
  bool stages[kNumStages];  // pick, prefix, mutate, check, minimize, publish
};

constexpr TriggerInfo kTriggers[] = {
    {CaptureTrigger::kTimeout,
     "timeout",
     {false, false, true, true, true, false}},
    {CaptureTrigger::kCrash,
     "crash",
     {false, false, true, false, false, false}},
    {CaptureTrigger::kFailed,
     "failed",
     {false, false, true, false, false, false}},
    {CaptureTrigger::kInvalid,
     "invalid",
     {false, false, true, false, false, false}},
    {CaptureTrigger::kNoop, "noop", {false, true, true, false, false, false}},
    {CaptureTrigger::kInteresting,
     "interesting",
     {false, false, true, false, false, false}},
    {CaptureTrigger::kFlaky,
     "flaky",
     {false, false, false, true, false, false}},
    {CaptureTrigger::kRejected,
     "rejected",
     {false, false, false, false, true, false}},
};

const TriggerInfo& Info(CaptureTrigger trigger) {
  for (const TriggerInfo& t : kTriggers) {
    if (t.trigger == trigger) {
      return t;
    }
  }
  return kTriggers[0];
}

std::vector<std::string> SplitList(std::string_view text) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= text.size()) {
    size_t comma = text.find(',', start);
    if (comma == std::string_view::npos) {
      comma = text.size();
    }
    if (comma > start) {
      out.emplace_back(text.substr(start, comma - start));
    }
    start = comma + 1;
  }
  return out;
}

std::string TriggerList() {
  std::string out;
  for (const TriggerInfo& t : kTriggers) {
    if (!out.empty()) {
      out += ", ";
    }
    out += t.name;
  }
  return out;
}

}  // namespace

std::string_view StageName(Stage stage) {
  return kStageNames[static_cast<int>(stage)];
}

bool ParseStage(std::string_view name, Stage* out) {
  for (int i = 0; i < kNumStages; i++) {
    if (kStageNames[i] == name) {
      *out = static_cast<Stage>(i);
      return true;
    }
  }
  return false;
}

std::string_view CaptureTriggerName(CaptureTrigger trigger) {
  return Info(trigger).name;
}

bool ParseCaptureTrigger(std::string_view name, CaptureTrigger* out) {
  for (const TriggerInfo& t : kTriggers) {
    if (t.name == name) {
      *out = t.trigger;
      return true;
    }
  }
  return false;
}

bool CaptureTriggerApplies(Stage stage, CaptureTrigger trigger) {
  return Info(trigger).stages[static_cast<int>(stage)];
}

// --- CaptureRule
// --------------------------------------------------------------

bool CaptureRule::Matches(CaptureTrigger trigger,
                          std::string_view mutator) const {
  bool wanted = false;
  for (CaptureTrigger t : on) {
    if (t == trigger) {
      wanted = true;
      break;
    }
  }
  if (!wanted) {
    return false;
  }
  if (mutators.empty()) {
    return true;
  }
  for (const std::string& m : mutators) {
    if (m == mutator) {
      return true;
    }
  }
  return false;
}

namespace {

bool AddTriggers(Stage stage, const std::vector<std::string>& names,
                 CaptureRule* out, std::string* error) {
  for (const std::string& name : names) {
    CaptureTrigger trigger;
    if (!ParseCaptureTrigger(name, &trigger)) {
      *error = "unknown capture trigger '" + name +
               "' (known: " + TriggerList() + ")";
      return false;
    }
    if (!CaptureTriggerApplies(stage, trigger)) {
      *error = "capture trigger '" + name + "' does not apply to the " +
               std::string(StageName(stage)) + " stage";
      return false;
    }
    out->on.push_back(trigger);
  }
  return true;
}

}  // namespace

// static
bool CaptureRule::FromJson(Stage stage, const JsonValue& value,
                           CaptureRule* out, std::string* error) {
  *out = CaptureRule();
  if (value.is_null()) {
    return true;
  }
  if (!value.is_object()) {
    *error =
        "\"capture\" must be {\"on\": [trigger, ...], \"mutators\": [name, "
        "...], \"max\": N}";
    return false;
  }
  for (const auto& [key, member] : value.as_object()) {
    if (key == "on") {
      std::vector<std::string> names;
      if (member.is_string()) {
        names = SplitList(member.as_string());
      } else if (member.is_array()) {
        for (const JsonValue& v : member.as_array()) {
          if (!v.is_string()) {
            *error = "\"capture.on\" must be a list of trigger names";
            return false;
          }
          names.push_back(v.as_string());
        }
      } else {
        *error = "\"capture.on\" must be a list of trigger names";
        return false;
      }
      if (!AddTriggers(stage, names, out, error)) {
        return false;
      }
    } else if (key == "mutators") {
      if (!member.is_array()) {
        *error = "\"capture.mutators\" must be a list of mutator names";
        return false;
      }
      for (const JsonValue& v : member.as_array()) {
        if (!v.is_string()) {
          *error = "\"capture.mutators\" must be a list of mutator names";
          return false;
        }
        out->mutators.push_back(v.as_string());
      }
      // An empty list (no filter) is what the recorded configuration says
      // for every stage; a real filter only makes sense where mutators run.
      if (stage != Stage::kMutate && !out->mutators.empty()) {
        *error = "\"capture.mutators\" only applies to the mutate stage";
        return false;
      }
    } else if (key == "max") {
      if (!member.is_number() || member.as_number() < 0) {
        *error = "\"capture.max\" must be a non-negative number";
        return false;
      }
      out->max = static_cast<int>(member.as_int());
    } else {
      *error = "unknown key \"" + key +
               "\" in \"capture\" (known: on, mutators, max)";
      return false;
    }
  }
  return true;
}

// static
bool CaptureRule::FromFlag(std::string_view text, Stage* stage,
                           CaptureRule* out, std::string* error) {
  // STAGE:TRIGGERS[:MUTATORS][:MAX]
  std::vector<std::string> parts;
  size_t start = 0;
  while (true) {
    size_t colon = text.find(':', start);
    parts.emplace_back(text.substr(start, colon == std::string_view::npos
                                              ? std::string_view::npos
                                              : colon - start));
    if (colon == std::string_view::npos) {
      break;
    }
    start = colon + 1;
  }
  if (parts.size() < 2 || parts.size() > 4) {
    *error =
        "--capture expects "
        "STAGE:TRIGGER[,TRIGGER..][:MUTATOR[,MUTATOR..]][:MAX], got '" +
        std::string(text) + "'";
    return false;
  }
  if (!ParseStage(parts[0], stage)) {
    *error = "--capture: unknown stage '" + parts[0] +
             "' (known: pick, prefix, mutate, check, minimize, publish)";
    return false;
  }
  *out = CaptureRule();
  if (!AddTriggers(*stage, SplitList(parts[1]), out, error)) {
    return false;
  }
  // The optional parts: a mutator list and/or a number, in that order; a
  // part that is all digits is the number.
  for (size_t i = 2; i < parts.size(); i++) {
    const std::string& part = parts[i];
    bool number = !part.empty() &&
                  part.find_first_not_of("0123456789") == std::string::npos;
    if (number) {
      out->max = static_cast<int>(strtol(part.c_str(), nullptr, 10));
    } else if (*stage == Stage::kMutate) {
      out->mutators = SplitList(part);
    } else {
      *error = "--capture: mutator names only apply to the mutate stage ('" +
               part + "')";
      return false;
    }
  }
  return true;
}

std::string CaptureRule::ToJson() const {
  JsonObject o;
  std::vector<std::string> names;
  names.reserve(on.size());
  for (CaptureTrigger t : on) {
    names.emplace_back(CaptureTriggerName(t));
  }
  o.AddArray("on", names);
  o.AddArray("mutators", mutators);
  o.Add("max", max);
  return o.ToString();
}

// --- StageTable
// ---------------------------------------------------------------

std::string StageTable::ToJson() const {
  JsonObject o;
  for (int i = 0; i < kNumStages; i++) {
    StageStats::Snapshot s = stats[i].snapshot();
    JsonObject row;
    row.Add("invocations", s.invocations)
        .Add("executions", s.executions)
        .Add("time_us", s.time_us)
        .Add("exec_time_us", s.exec_time_us)
        .Add("timeouts", s.timeouts)
        .Add("successes", s.successes);
    o.AddRaw(kStageNames[i], row.ToString());
  }
  return o.ToString();
}

std::string StageTable::Format(const bool enabled[kNumStages]) const {
  StageStats::Snapshot rows[kNumStages];
  uint64_t total_execs = 0, total_time = 0, total_timeouts = 0;
  for (int i = 0; i < kNumStages; i++) {
    rows[i] = stats[i].snapshot();
    total_execs += rows[i].executions;
    total_time += rows[i].time_us;
    total_timeouts += rows[i].timeouts;
  }
  auto share = [](uint64_t part, uint64_t total) {
    return total == 0
               ? 0.0
               : 100.0 * static_cast<double>(part) / static_cast<double>(total);
  };
  std::string out = "stages\n";
  out += StringPrintf("  %-9s %12s %12s %7s %10s %7s %9s %7s %9s\n", "stage",
                      "invoked", "execs", "execs%", "time s", "time%",
                      "timeouts", "tmo%", "success%");
  for (int i = 0; i < kNumStages; i++) {
    if (!enabled[i]) {
      continue;
    }
    const StageStats::Snapshot& s = rows[i];
    out += StringPrintf(
        "  %-9.*s %12llu %12llu %6.1f%% %10.1f %6.1f%% %9llu %6.1f%% %8.1f%%\n",
        static_cast<int>(kStageNames[i].size()), kStageNames[i].data(),
        static_cast<unsigned long long>(s.invocations),
        static_cast<unsigned long long>(s.executions),
        share(s.executions, total_execs), static_cast<double>(s.time_us) / 1e6,
        share(s.time_us, total_time),
        static_cast<unsigned long long>(s.timeouts),
        share(s.timeouts, total_timeouts), share(s.successes, s.invocations));
  }
  return out;
}

}  // namespace redline
