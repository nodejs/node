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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_FTRACE_DESCRIPTORS_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_FTRACE_DESCRIPTORS_H_

#include <array>

#include "perfetto/ext/base/string_view.h"
#include "perfetto/protozero/proto_utils.h"

namespace perfetto {
namespace trace_processor {

using protozero::proto_utils::ProtoSchemaType;

// We assume that no ftrace event (e.g. SchedSwitchFtraceEvent) has a proto
// field which id is >= this.
static constexpr size_t kMaxFtraceEventFields = 32;

// This file is the header for the generated descriptors for all ftrace event
// protos. These descriptors can be used to parse ftrace event protos without
// needing individual parsing logic for every event. (In proto_trace_parser.cc)
// By generating these descriptors we avoid having to build the full proto
// library. These structs deliberately don't have a ctor (nor are initialized)
// because they are used to define linker-initialized dicts in the .cc file.
struct FieldDescriptor {
  const char* name;
  ProtoSchemaType type;
};

struct MessageDescriptor {
  const char* name;
  size_t max_field_id;
  FieldDescriptor fields[kMaxFtraceEventFields];
};

MessageDescriptor* GetMessageDescriptorForId(size_t id);
MessageDescriptor* GetMessageDescriptorForName(base::StringView name);
size_t GetDescriptorsSize();

}  // namespace trace_processor
}  // namespace perfetto
#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_FTRACE_DESCRIPTORS_H_
