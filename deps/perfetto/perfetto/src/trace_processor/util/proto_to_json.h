/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_UTIL_PROTO_TO_JSON_H_
#define SRC_TRACE_PROCESSOR_UTIL_PROTO_TO_JSON_H_

#include <google/protobuf/message.h>

namespace perfetto {
namespace trace_processor {
namespace proto_to_json {

std::string MessageToJson(const google::protobuf::Message& message,
                          uint32_t indent = 0);

std::string MessageToJsonWithAnnotations(
    const google::protobuf::Message& message,
    const google::protobuf::Message* field_options_prototype,
    uint32_t indent = 0);

}  // namespace proto_to_json
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_UTIL_PROTO_TO_JSON_H_
