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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROFILE_PACKET_UTILS_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROFILE_PACKET_UTILS_H_

#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_view.h"
#include "src/trace_processor/importers/proto/packet_sequence_state.h"
#include "src/trace_processor/importers/proto/stack_profile_tracker.h"

#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"

namespace perfetto {
namespace trace_processor {

class ProfilePacketUtils {
 public:
  static StackProfileTracker::SourceMapping MakeSourceMapping(
      const protos::pbzero::Mapping::Decoder& entry) {
    StackProfileTracker::SourceMapping src_mapping{};
    src_mapping.build_id = entry.build_id();
    src_mapping.exact_offset = entry.exact_offset();
    src_mapping.start_offset = entry.start_offset();
    src_mapping.start = entry.start();
    src_mapping.end = entry.end();
    src_mapping.load_bias = entry.load_bias();
    for (auto path_string_id_it = entry.path_string_ids(); path_string_id_it;
         ++path_string_id_it) {
      src_mapping.name_ids.emplace_back(*path_string_id_it);
    }
    return src_mapping;
  }

  static StackProfileTracker::SourceFrame MakeSourceFrame(
      const protos::pbzero::Frame::Decoder& entry) {
    StackProfileTracker::SourceFrame src_frame;
    src_frame.name_id = entry.function_name_id();
    src_frame.mapping_id = entry.mapping_id();
    src_frame.rel_pc = entry.rel_pc();
    return src_frame;
  }

  static StackProfileTracker::SourceCallstack MakeSourceCallstack(
      const protos::pbzero::Callstack::Decoder& entry) {
    StackProfileTracker::SourceCallstack src_callstack;
    for (auto frame_it = entry.frame_ids(); frame_it; ++frame_it)
      src_callstack.emplace_back(*frame_it);
    return src_callstack;
  }
};

class ProfilePacketInternLookup : public StackProfileTracker::InternLookup {
 public:
  explicit ProfilePacketInternLookup(PacketSequenceStateGeneration* seq_state)
      : seq_state_(seq_state) {}
  ~ProfilePacketInternLookup() override;

  base::Optional<base::StringView> GetString(
      StackProfileTracker::SourceStringId iid,
      StackProfileTracker::InternedStringType type) const override {
    protos::pbzero::InternedString::Decoder* decoder = nullptr;
    switch (type) {
      case StackProfileTracker::InternedStringType::kBuildId:
        decoder = seq_state_->LookupInternedMessage<
            protos::pbzero::InternedData::kBuildIdsFieldNumber,
            protos::pbzero::InternedString>(iid);
        break;
      case StackProfileTracker::InternedStringType::kFunctionName:
        decoder = seq_state_->LookupInternedMessage<
            protos::pbzero::InternedData::kFunctionNamesFieldNumber,
            protos::pbzero::InternedString>(iid);
        break;
      case StackProfileTracker::InternedStringType::kMappingPath:
        decoder = seq_state_->LookupInternedMessage<
            protos::pbzero::InternedData::kMappingPathsFieldNumber,
            protos::pbzero::InternedString>(iid);
        break;
    }
    if (!decoder)
      return base::nullopt;
    return base::StringView(reinterpret_cast<const char*>(decoder->str().data),
                            decoder->str().size);
  }

  base::Optional<StackProfileTracker::SourceMapping> GetMapping(
      StackProfileTracker::SourceMappingId iid) const override {
    auto* decoder = seq_state_->LookupInternedMessage<
        protos::pbzero::InternedData::kMappingsFieldNumber,
        protos::pbzero::Mapping>(iid);
    if (!decoder)
      return base::nullopt;
    return ProfilePacketUtils::MakeSourceMapping(*decoder);
  }

  base::Optional<StackProfileTracker::SourceFrame> GetFrame(
      StackProfileTracker::SourceFrameId iid) const override {
    auto* decoder = seq_state_->LookupInternedMessage<
        protos::pbzero::InternedData::kFramesFieldNumber,
        protos::pbzero::Frame>(iid);
    if (!decoder)
      return base::nullopt;
    return ProfilePacketUtils::MakeSourceFrame(*decoder);
  }

  base::Optional<StackProfileTracker::SourceCallstack> GetCallstack(
      StackProfileTracker::SourceCallstackId iid) const override {
    auto* decoder = seq_state_->LookupInternedMessage<
        protos::pbzero::InternedData::kCallstacksFieldNumber,
        protos::pbzero::Callstack>(iid);
    if (!decoder)
      return base::nullopt;
    return ProfilePacketUtils::MakeSourceCallstack(*decoder);
  }

 private:
  PacketSequenceStateGeneration* seq_state_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROFILE_PACKET_UTILS_H_
