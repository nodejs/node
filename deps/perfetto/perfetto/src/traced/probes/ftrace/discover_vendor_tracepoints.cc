/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/traced/probes/ftrace/discover_vendor_tracepoints.h"

#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/traced/probes/ftrace/atrace_wrapper.h"

namespace perfetto {
namespace vendor_tracepoints {

std::vector<GroupAndName> DiscoverTracepoints(AtraceHalWrapper* hal,
                                              FtraceProcfs* ftrace,
                                              const std::string& category) {
  ftrace->DisableAllEvents();
  hal->EnableCategories({category});

  std::vector<GroupAndName> events;
  for (const std::string& group_name : ftrace->ReadEnabledEvents()) {
    size_t pos = group_name.find('/');
    PERFETTO_CHECK(pos != std::string::npos);
    events.push_back(
        GroupAndName(group_name.substr(0, pos), group_name.substr(pos + 1)));
  }

  hal->DisableAllCategories();
  ftrace->DisableAllEvents();
  return events;
}

std::map<std::string, std::vector<GroupAndName>> DiscoverVendorTracepoints(
    AtraceHalWrapper* hal,
    FtraceProcfs* ftrace) {
  std::map<std::string, std::vector<GroupAndName>> results;
  for (const auto& category : hal->ListCategories()) {
    results.emplace(category, DiscoverTracepoints(hal, ftrace, category));
  }
  return results;
}

}  // namespace vendor_tracepoints
}  // namespace perfetto
