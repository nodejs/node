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

#ifndef TOOLS_FTRACE_PROTO_GEN_FTRACE_DESCRIPTOR_GEN_H_
#define TOOLS_FTRACE_PROTO_GEN_FTRACE_DESCRIPTOR_GEN_H_

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>

#include "perfetto/base/logging.h"
#include "tools/ftrace_proto_gen/ftrace_proto_gen.h"

namespace perfetto {

// Uses the ftrace event descriptor file to generate a
// file with just the names and field types for each ftrace event.
void GenerateFtraceDescriptors(
    const google::protobuf::DescriptorPool& descriptor_pool,
    std::ostream* fout);

}  // namespace perfetto
#endif  // TOOLS_FTRACE_PROTO_GEN_FTRACE_DESCRIPTOR_GEN_H_
