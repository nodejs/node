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

#ifndef SRC_TRACED_PROBES_FTRACE_DISCOVER_VENDOR_TRACEPOINTS_H_
#define SRC_TRACED_PROBES_FTRACE_DISCOVER_VENDOR_TRACEPOINTS_H_

#include <map>
#include <string>
#include <vector>

#include "src/traced/probes/ftrace/atrace_hal_wrapper.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"
#include "src/traced/probes/ftrace/proto_translation_table.h"

namespace perfetto {
namespace vendor_tracepoints {

// Exposed for testing.
std::vector<GroupAndName> DiscoverTracepoints(AtraceHalWrapper* hal,
                                              FtraceProcfs* ftrace,
                                              const std::string& category);

// Returns a map from vendor category to events we should enable
std::map<std::string, std::vector<GroupAndName>> DiscoverVendorTracepoints(
    AtraceHalWrapper* hal,
    FtraceProcfs* ftrace);

}  // namespace vendor_tracepoints
}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_DISCOVER_VENDOR_TRACEPOINTS_H_
