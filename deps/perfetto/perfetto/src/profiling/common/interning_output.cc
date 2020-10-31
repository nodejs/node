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

#include "src/profiling/common/interning_output.h"

#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_packet.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace {
// Flags used to distinguish distinct types of interned strings.
constexpr int kDumpedBuildID = 1 << 0;
constexpr int kDumpedMappingPath = 1 << 1;
constexpr int kDumpedFunctionName = 1 << 2;
}  // namespace

namespace perfetto {
namespace profiling {

// static
void InterningOutputTracker::WriteFixedInterningsPacket(
    TraceWriter* trace_writer) {
  constexpr const uint8_t kEmptyString[] = "";
  // Explicitly reserve intern ID 0 for the empty string, so unset string
  // fields get mapped to this.
  auto packet = trace_writer->NewTracePacket();
  auto* interned_data = packet->set_interned_data();
  auto* interned_string = interned_data->add_build_ids();
  interned_string->set_iid(0);
  interned_string->set_str(kEmptyString, 0);

  interned_string = interned_data->add_mapping_paths();
  interned_string->set_iid(0);
  interned_string->set_str(kEmptyString, 0);

  interned_string = interned_data->add_function_names();
  interned_string->set_iid(0);
  interned_string->set_str(kEmptyString, 0);

  packet->set_incremental_state_cleared(true);
}

void InterningOutputTracker::WriteMap(const Interned<Mapping> map,
                                      protos::pbzero::InternedData* out) {
  auto map_it_and_inserted = dumped_mappings_.emplace(map.id());
  if (map_it_and_inserted.second) {
    for (const Interned<std::string>& str : map->path_components)
      WriteMappingPathString(str, out);

    WriteBuildIDString(map->build_id, out);

    protos::pbzero::Mapping* mapping = out->add_mappings();
    mapping->set_iid(map.id());
    mapping->set_exact_offset(map->exact_offset);
    mapping->set_start_offset(map->start_offset);
    mapping->set_start(map->start);
    mapping->set_end(map->end);
    mapping->set_load_bias(map->load_bias);
    mapping->set_build_id(map->build_id.id());
    for (const Interned<std::string>& str : map->path_components)
      mapping->add_path_string_ids(str.id());
  }
}

void InterningOutputTracker::WriteFrame(Interned<Frame> frame,
                                        protos::pbzero::InternedData* out) {
  WriteMap(frame->mapping, out);
  WriteFunctionNameString(frame->function_name, out);
  bool inserted;
  std::tie(std::ignore, inserted) = dumped_frames_.emplace(frame.id());
  if (inserted) {
    protos::pbzero::Frame* frame_proto = out->add_frames();
    frame_proto->set_iid(frame.id());
    frame_proto->set_function_name_id(frame->function_name.id());
    frame_proto->set_mapping_id(frame->mapping.id());
    frame_proto->set_rel_pc(frame->rel_pc);
  }
}

void InterningOutputTracker::WriteBuildIDString(
    const Interned<std::string>& str,
    protos::pbzero::InternedData* out) {
  auto it_and_inserted = dumped_strings_.emplace(str.id(), 0);
  auto it = it_and_inserted.first;
  // This is for the rare case that the same string is used as two different
  // types (e.g. a function name that matches a path segment). In that case
  // we need to emit the string as all of its types.
  if ((it->second & kDumpedBuildID) == 0) {
    protos::pbzero::InternedString* interned_string = out->add_build_ids();
    interned_string->set_iid(str.id());
    interned_string->set_str(str.data());
    it->second |= kDumpedBuildID;
  }
}

void InterningOutputTracker::WriteMappingPathString(
    const Interned<std::string>& str,
    protos::pbzero::InternedData* out) {
  auto it_and_inserted = dumped_strings_.emplace(str.id(), 0);
  auto it = it_and_inserted.first;
  // This is for the rare case that the same string is used as two different
  // types (e.g. a function name that matches a path segment). In that case
  // we need to emit the string as all of its types.
  if ((it->second & kDumpedMappingPath) == 0) {
    protos::pbzero::InternedString* interned_string = out->add_mapping_paths();
    interned_string->set_iid(str.id());
    interned_string->set_str(str.data());
    it->second |= kDumpedMappingPath;
  }
}

void InterningOutputTracker::WriteFunctionNameString(
    const Interned<std::string>& str,
    protos::pbzero::InternedData* out) {
  auto it_and_inserted = dumped_strings_.emplace(str.id(), 0);
  auto it = it_and_inserted.first;
  // This is for the rare case that the same string is used as two different
  // types (e.g. a function name that matches a path segment). In that case
  // we need to emit the string as all of its types.
  if ((it->second & kDumpedFunctionName) == 0) {
    protos::pbzero::InternedString* interned_string = out->add_function_names();
    interned_string->set_iid(str.id());
    interned_string->set_str(str.data());
    it->second |= kDumpedFunctionName;
  }
}

void InterningOutputTracker::WriteCallstack(GlobalCallstackTrie::Node* node,
                                            GlobalCallstackTrie* trie,
                                            protos::pbzero::InternedData* out) {
  bool inserted;
  std::tie(std::ignore, inserted) = dumped_callstacks_.emplace(node->id());
  if (inserted) {
    // There need to be two separate loops over built_callstack because
    // protozero cannot interleave different messages.
    auto built_callstack = trie->BuildInverseCallstack(node);
    for (const Interned<Frame>& frame : built_callstack)
      WriteFrame(frame, out);

    protos::pbzero::Callstack* callstack = out->add_callstacks();
    callstack->set_iid(node->id());
    for (auto frame_it = built_callstack.crbegin();
         frame_it != built_callstack.crend(); ++frame_it) {
      const Interned<Frame>& frame = *frame_it;
      callstack->add_frame_ids(frame.id());
    }
  }
}

void InterningOutputTracker::ClearHistory() {
  dumped_strings_.clear();
  dumped_frames_.clear();
  dumped_mappings_.clear();
  dumped_callstacks_.clear();
}

}  // namespace profiling
}  // namespace perfetto
