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

#include "src/profiling/memory/bookkeeping_dump.h"

namespace perfetto {
namespace profiling {
namespace {
using ::perfetto::protos::pbzero::ProfilePacket;
// This needs to be lower than the maximum acceptable chunk size, because this
// is checked *before* writing another submessage. We conservatively assume
// submessages can be up to 100k here for a 500k chunk size.
// DropBox has a 500k chunk limit, and each chunk needs to parse as a proto.
uint32_t kPacketSizeThreshold = 400000;
}  // namespace

void DumpState::WriteMap(const Interned<Mapping> map) {
  intern_state_->WriteMap(map, GetCurrentInternedData());
}

void DumpState::WriteFrame(Interned<Frame> frame) {
  intern_state_->WriteFrame(frame, GetCurrentInternedData());
}

void DumpState::WriteBuildIDString(const Interned<std::string>& str) {
  intern_state_->WriteBuildIDString(str, GetCurrentInternedData());
}

void DumpState::WriteMappingPathString(const Interned<std::string>& str) {
  intern_state_->WriteMappingPathString(str, GetCurrentInternedData());
}

void DumpState::WriteFunctionNameString(const Interned<std::string>& str) {
  intern_state_->WriteFunctionNameString(str, GetCurrentInternedData());
}

void DumpState::WriteAllocation(const HeapTracker::CallstackAllocations& alloc,
                                bool dump_at_max_mode) {
  if (intern_state_->IsCallstackNew(alloc.node->id())) {
    callstacks_to_dump_.emplace(alloc.node);
  }

  auto* heap_samples = GetCurrentProcessHeapSamples();
  ProfilePacket::HeapSample* sample = heap_samples->add_samples();
  sample->set_callstack_id(alloc.node->id());
  if (dump_at_max_mode) {
    sample->set_self_max(alloc.value.retain_max.max);
  } else {
    sample->set_self_allocated(alloc.value.totals.allocated);
    sample->set_self_freed(alloc.value.totals.freed);
  }
  sample->set_alloc_count(alloc.allocation_count);
  sample->set_free_count(alloc.free_count);

  auto it = current_process_idle_allocs_.find(alloc.node->id());
  if (it != current_process_idle_allocs_.end())
    sample->set_self_idle(it->second);
}

void DumpState::DumpCallstacks(GlobalCallstackTrie* callsites) {
  // We need a way to signal to consumers when they have fully consumed the
  // InternedData they need to understand the sequence of continued
  // ProfilePackets. The way we do that is to mark the last ProfilePacket as
  // continued, then emit the InternedData, and then an empty ProfilePacket
  // to terminate the sequence.
  //
  // This is why we set_continued at the beginning of this function, and
  // MakeProfilePacket at the end.
  if (current_trace_packet_)
    current_profile_packet_->set_continued(true);
  for (GlobalCallstackTrie::Node* node : callstacks_to_dump_) {
    intern_state_->WriteCallstack(node, callsites, GetCurrentInternedData());
  }
  MakeProfilePacket();
}

void DumpState::AddIdleBytes(uint64_t callstack_id, uint64_t bytes) {
  current_process_idle_allocs_[callstack_id] += bytes;
}

ProfilePacket::ProcessHeapSamples* DumpState::GetCurrentProcessHeapSamples() {
  if (currently_written() > kPacketSizeThreshold) {
    if (current_profile_packet_)
      current_profile_packet_->set_continued(true);
    MakeProfilePacket();
  }

  if (current_process_heap_samples_ == nullptr) {
    current_process_heap_samples_ =
        current_profile_packet_->add_process_dumps();
    current_process_fill_header_(current_process_heap_samples_);
  }

  return current_process_heap_samples_;
}

protos::pbzero::InternedData* DumpState::GetCurrentInternedData() {
  if (currently_written() > kPacketSizeThreshold)
    MakeTracePacket();

  if (current_interned_data_ == nullptr)
    current_interned_data_ = current_trace_packet_->set_interned_data();

  return current_interned_data_;
}

}  // namespace profiling
}  // namespace perfetto
