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

#ifndef TOOLS_FTRACE_PROTO_GEN_FTRACE_PROTO_GEN_H_
#define TOOLS_FTRACE_PROTO_GEN_FTRACE_PROTO_GEN_H_

#include <google/protobuf/descriptor.h>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "src/traced/probes/ftrace/format_parser.h"
#include "tools/ftrace_proto_gen/proto_gen_utils.h"

namespace perfetto {

bool GenerateProto(const std::string& group,
                   const FtraceEvent& format,
                   Proto* proto_out);

std::string EventNameToProtoName(const std::string& group,
                                 const std::string& name);
std::string EventNameToProtoFieldName(const std::string& group,
                                      const std::string& name);

std::vector<FtraceEventName> ReadWhitelist(const std::string& filename);
void GenerateFtraceEventProto(const std::vector<FtraceEventName>& raw_whitelist,
                              const std::set<std::string>& groups,
                              std::ostream* fout);
std::string SingleEventInfo(perfetto::Proto proto,
                            const std::string& group,
                            const uint32_t proto_field_id);
void GenerateEventInfo(const std::vector<std::string>& events_info,
                       std::ostream* fout);
std::string ProtoHeader();

}  // namespace perfetto

#endif  // TOOLS_FTRACE_PROTO_GEN_FTRACE_PROTO_GEN_H_
