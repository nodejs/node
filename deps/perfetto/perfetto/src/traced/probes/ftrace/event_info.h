/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef SRC_TRACED_PROBES_FTRACE_EVENT_INFO_H_
#define SRC_TRACED_PROBES_FTRACE_EVENT_INFO_H_

#include <vector>

#include "perfetto/base/logging.h"
#include "src/traced/probes/ftrace/event_info_constants.h"

namespace perfetto {

// The compile time information needed to read the raw ftrace buffer.
// Specifically for each event we have a proto we fill:
//  The event name (e.g. sched_switch)
//  The event group  (e.g. sched)
//  The proto field ID of this event in the FtraceEvent proto.
//  For each field in the proto:
//    The field name (e.g. prev_comm)
//    The proto field id for this field
//    The proto field type for this field (e.g. kProtoString or kProtoUint32)
// The other fields: ftrace_event_id, ftrace_size, ftrace_offset, ftrace_type
// are zeroed.
std::vector<Event> GetStaticEventInfo();

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_EVENT_INFO_H_
