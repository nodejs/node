// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The stages of a worker's loop, as an assembly line:
//
//   pick -> prefix -> mutate -> check -> minimize -> publish
//
//   pick      a corpus program is chosen (selection.seeds)
//   prefix    fresh value declarations are put in front of it
//   mutate    a mutator is chosen (selection.mutators) and applied, the
//             result is syntax-checked and executed; repeated along a chain
//   check     a result with new edges is re-executed until its edges are
//             known to be deterministic
//   minimize  the surviving program is reduced against those edges
//   publish   it is added to the corpus and saved
//
// Every stage has the same three knobs next to its own parameters
// (StageOptions): `log` (one JSONL event per occurrence), `capture` (save
// the programs that went through the stage when a trigger fires) and
// `stats` (a row in the stages table). The parameters themselves live in
// FuzzConfig; the "stages" block of the configuration groups them by stage.
//
// StageStats are the per-stage counters a session keeps, reported in the
// `stats` event and in the periodic tables as shares of executions, time
// and timeouts.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_STAGES_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_STAGES_H_

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/base/json.h"
#include "experimental/redline/base/json_parser.h"

namespace redline {

enum class Stage { kPick, kPrefix, kMutate, kCheck, kMinimize, kPublish };
constexpr int kNumStages = 6;

std::string_view StageName(Stage stage);
// Returns false for an unknown name.
bool ParseStage(std::string_view name, Stage* out);

// What can trigger a capture. Not every trigger applies to every stage:
//   mutate    timeout, crash, failed, invalid, noop, interesting
//   check     flaky (edges lost), timeout
//   minimize  rejected (result failed verification), timeout
//   prefix    noop
// An inapplicable trigger is a configuration error.
enum class CaptureTrigger {
  kTimeout,
  kCrash,
  kFailed,
  kInvalid,
  kNoop,
  kInteresting,
  kFlaky,
  kRejected
};

std::string_view CaptureTriggerName(CaptureTrigger trigger);
bool ParseCaptureTrigger(std::string_view name, CaptureTrigger* out);
bool CaptureTriggerApplies(Stage stage, CaptureTrigger trigger);

// "Save the programs of this stage when one of `on` happens" (for the
// mutate stage optionally only for the listed mutators), at most `max`
// times per session. Written to OUT/captures/<stage>/.
struct CaptureRule {
  std::vector<CaptureTrigger> on;
  std::vector<std::string> mutators;  // empty: any
  int max = 50;

  bool enabled() const { return !on.empty(); }
  bool Matches(CaptureTrigger trigger, std::string_view mutator) const;
  // {"on": [...], "mutators": [...], "max": N}; `stage` is used to check
  // that the triggers apply. Flag form:
  // "STAGE:TRIGGER[,TRIGGER..][:MUTATOR[,MUTATOR..]][:MAX]".
  static bool FromJson(Stage stage, const JsonValue& value, CaptureRule* out,
                       std::string* error);
  static bool FromFlag(std::string_view text, Stage* stage, CaptureRule* out,
                       std::string* error);
  std::string ToJson() const;
};

// The three knobs every stage has.
struct StageOptions {
  bool events = false;  // "log": "events" -> one JSONL event per occurrence
  CaptureRule capture;
  bool stats = true;  // a row in the stages table
};

// Per-stage counters. `successes` means: pick/prefix/publish - completed;
// mutate - executed results with new edges; check - candidates whose edges
// survived; minimize - accepted reductions.
struct StageStats {
  std::atomic<uint64_t> invocations{0};  // times the stage ran
  std::atomic<uint64_t> executions{0};   // target executions it caused
  std::atomic<uint64_t> time_us{0};  // wall time inside the stage (all workers)
  std::atomic<uint64_t> exec_time_us{0};  // ... of which target execution time
  std::atomic<uint64_t> timeouts{0};      // executions that timed out
  std::atomic<uint64_t> successes{0};

  struct Snapshot {
    uint64_t invocations, executions, time_us, exec_time_us, timeouts,
        successes;
  };
  Snapshot snapshot() const {
    return {invocations.load(),  executions.load(), time_us.load(),
            exec_time_us.load(), timeouts.load(),   successes.load()};
  }
};

// All stages of one session.
struct StageTable {
  StageStats stats[kNumStages];

  StageStats& operator[](Stage stage) { return stats[static_cast<int>(stage)]; }
  const StageStats& operator[](Stage stage) const {
    return stats[static_cast<int>(stage)];
  }

  // {"pick": {"invocations": ..., ...}, ...} for the stats event.
  std::string ToJson() const;
  // A table with shares: % of executions, % of time, % of timeouts,
  // success rate. `enabled` selects the rows (StageOptions::stats).
  std::string Format(const bool enabled[kNumStages]) const;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_STAGES_H_
