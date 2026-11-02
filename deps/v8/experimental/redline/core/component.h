// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_COMPONENT_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_COMPONENT_H_

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace redline {

// Counters every pluggable component (mutator, generator, reducer,
// scheduler) keeps by default. They are the data behind the periodic stats
// table and the `--stats` output of the CLI commands.
struct ComponentStats {
  std::atomic<uint64_t> invocations{0};  // times the component was asked
  std::atomic<uint64_t> produced{0};     // ... and produced a result
  std::atomic<uint64_t> invalid{0};      // result rejected by the V8 parser
  std::atomic<uint64_t> executed{0};     // results executed in the target
  std::atomic<uint64_t> succeeded{0};
  std::atomic<uint64_t> failed{0};
  std::atomic<uint64_t> timed_out{0};
  std::atomic<uint64_t> crashed{0};
  std::atomic<uint64_t> interesting{0};  // results with new deterministic edges
  std::atomic<uint64_t> time_us{0};      // time spent inside the component
  // New deterministic edges of the corpus programs credited to the component.
  std::atomic<uint64_t> new_edges{0};
  // All target executions the component caused: its results, the instrumented
  // runs of a runtime-assisted mutator, and the determinism re-runs and
  // minimizer probes of its candidates (the engine decides what it credits).
  std::atomic<uint64_t> spent{0};

  struct Snapshot {
    uint64_t invocations, produced, invalid, executed, succeeded, failed,
        timed_out, crashed, interesting, time_us, new_edges, spent;
    double success_rate() const {
      return executed ? 100.0 * static_cast<double>(succeeded) /
                            static_cast<double>(executed)
                      : 0.0;
    }
    double interesting_rate() const {
      return executed ? 100.0 * static_cast<double>(interesting) /
                            static_cast<double>(executed)
                      : 0.0;
    }
  };
  Snapshot snapshot() const {
    return {invocations.load(), produced.load(),  invalid.load(),
            executed.load(),    succeeded.load(), failed.load(),
            timed_out.load(),   crashed.load(),   interesting.load(),
            time_us.load(),     new_edges.load(), spent.load()};
  }
};

class Component {
 public:
  explicit Component(std::string name) : name_(std::move(name)) {}
  virtual ~Component() = default;
  Component(const Component&) = delete;
  Component& operator=(const Component&) = delete;

  const std::string& name() const { return name_; }
  ComponentStats& stats() { return stats_; }
  const ComponentStats& stats() const { return stats_; }
  // One line of help for `redline help` / the shell.
  virtual std::string_view description() const { return ""; }

 private:
  std::string name_;
  ComponentStats stats_;
};

// Renders a stats table (one row per component) for logging.
std::string FormatStatsTable(std::string_view title,
                             const std::vector<const Component*>& components);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_COMPONENT_H_
