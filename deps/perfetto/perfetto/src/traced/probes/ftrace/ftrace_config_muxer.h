/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACED_PROBES_FTRACE_FTRACE_CONFIG_MUXER_H_
#define SRC_TRACED_PROBES_FTRACE_FTRACE_CONFIG_MUXER_H_

#include <map>
#include <set>

#include "src/traced/probes/ftrace/compact_sched.h"
#include "src/traced/probes/ftrace/ftrace_config_utils.h"
#include "src/traced/probes/ftrace/ftrace_controller.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"
#include "src/traced/probes/ftrace/proto_translation_table.h"

namespace perfetto {

// State held by the muxer per data source, used to parse ftrace according to
// that data source's config.
struct FtraceDataSourceConfig {
  FtraceDataSourceConfig(EventFilter _event_filter,
                         CompactSchedConfig _compact_sched,
                         std::vector<std::string> _atrace_apps,
                         std::vector<std::string> _atrace_categories)
      : event_filter(std::move(_event_filter)),
        compact_sched(_compact_sched),
        atrace_apps(std::move(_atrace_apps)),
        atrace_categories(std::move(_atrace_categories)) {}

  // The event filter allows to quickly check if a certain ftrace event with id
  // x is enabled for this data source.
  EventFilter event_filter;

  // Configuration of the optional compact encoding of scheduling events.
  const CompactSchedConfig compact_sched;

  // Used only in Android for ATRACE_EVENT/os.Trace() userspace annotations.
  std::vector<std::string> atrace_apps;
  std::vector<std::string> atrace_categories;
};

// Ftrace is a bunch of globally modifiable persistent state.
// Given a number of FtraceConfig's we need to find the best union of all
// the settings to make everyone happy while also watching out for anybody
// messing with the ftrace settings at the same time as us.
//
// Specifically FtraceConfigMuxer takes in a *requested* FtraceConfig
// (|SetupConfig|), makes a best effort attempt to modify the ftrace
// debugfs files to honor those settings without interrupting other perfetto
// traces already in progress or other users of ftrace, then returns an
// FtraceConfigId representing that config or zero on failure.
//
// When you are finished with a config you can signal that with |RemoveConfig|.
class FtraceConfigMuxer {
 public:
  // The FtraceConfigMuxer and ProtoTranslationTable
  // should outlive this instance.
  FtraceConfigMuxer(
      FtraceProcfs* ftrace,
      ProtoTranslationTable* table,
      std::map<std::string, std::vector<GroupAndName>> vendor_events);
  virtual ~FtraceConfigMuxer();

  // Ask FtraceConfigMuxer to adjust ftrace procfs settings to
  // match the requested config. Returns an id to manage this
  // config or zero on failure.
  // This is best effort. FtraceConfigMuxer may not be able to adjust the
  // buffer size right now. Events may be missing or there may be extra events
  // (if you enable an atrace category we try to give you the matching events).
  // If someone else is tracing we won't touch atrace (since it resets the
  // buffer).
  FtraceConfigId SetupConfig(const FtraceConfig& request);

  // Activate ftrace for the given config (if not already active).
  bool ActivateConfig(FtraceConfigId);

  // Undo changes for the given config. Returns false iff the id is 0
  // or already removed.
  bool RemoveConfig(FtraceConfigId);

  const FtraceDataSourceConfig* GetDataSourceConfig(FtraceConfigId id);

  // Returns the current per-cpu buffer size, as configured by this muxer
  // (without consulting debugfs). Constant for a given tracing session.
  // Note that if there are multiple concurrent tracing sessions, the first
  // session's buffer size is used for all of them.
  size_t GetPerCpuBufferSizePages();

  // public for testing
  void SetupClockForTesting(const FtraceConfig& request) {
    SetupClock(request);
  }

  std::set<GroupAndName> GetFtraceEventsForTesting(
      const FtraceConfig& request,
      const ProtoTranslationTable* table) {
    return GetFtraceEvents(request, table);
  }

  const EventFilter* GetCentralEventFilterForTesting() const {
    return &current_state_.ftrace_events;
  }

 private:
  static bool StartAtrace(const std::vector<std::string>& apps,
                          const std::vector<std::string>& categories);

  struct FtraceState {
    EventFilter ftrace_events;
    // Used only in Android for ATRACE_EVENT/os.Trace() userspace
    std::vector<std::string> atrace_apps;
    std::vector<std::string> atrace_categories;
    size_t cpu_buffer_size_pages = 0;
    bool atrace_on = false;
  };

  FtraceConfigMuxer(const FtraceConfigMuxer&) = delete;
  FtraceConfigMuxer& operator=(const FtraceConfigMuxer&) = delete;

  void SetupClock(const FtraceConfig& request);
  void SetupBufferSize(const FtraceConfig& request);
  void UpdateAtrace(const FtraceConfig& request);
  void DisableAtrace();

  // This processes the config to get the exact events.
  // group/* -> Will read the fs and add all events in group.
  // event -> Will look up the event to find the group.
  // atrace category -> Will add events in that category.
  std::set<GroupAndName> GetFtraceEvents(const FtraceConfig& request,
                                         const ProtoTranslationTable*);

  FtraceConfigId GetNextId();

  FtraceConfigId last_id_ = 1;
  FtraceProcfs* ftrace_;
  ProtoTranslationTable* table_;

  FtraceState current_state_;

  // Set of all requested tracing configurations, with the associated derived
  // data used during parsing. Note that not all of these configurations might
  // be active. When a config is present but not active, we do setup buffer
  // sizes and events, but don't enable ftrace (i.e. tracing_on).
  std::map<FtraceConfigId, FtraceDataSourceConfig> ds_configs_;

  std::map<std::string, std::vector<GroupAndName>> vendor_events_;

  // Subset of |ds_configs_| that are currently active. At any time ftrace is
  // enabled iff |active_configs_| is not empty.
  std::set<FtraceConfigId> active_configs_;
};

size_t ComputeCpuBufferSizeInPages(size_t requested_buffer_size_kb);

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_FTRACE_CONFIG_MUXER_H_
