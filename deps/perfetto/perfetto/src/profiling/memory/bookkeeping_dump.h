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

#ifndef SRC_PROFILING_MEMORY_BOOKKEEPING_DUMP_H_
#define SRC_PROFILING_MEMORY_BOOKKEEPING_DUMP_H_

#include <functional>
#include <set>

#include <inttypes.h>

#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_packet.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

#include "perfetto/ext/tracing/core/trace_writer.h"

#include "src/profiling/common/interner.h"
#include "src/profiling/common/interning_output.h"
#include "src/profiling/memory/bookkeeping.h"

namespace perfetto {
namespace profiling {

class DumpState {
 public:
  DumpState(
      TraceWriter* trace_writer,
      std::function<void(protos::pbzero::ProfilePacket::ProcessHeapSamples*)>
          process_fill_header,
      InterningOutputTracker* intern_state)
      : trace_writer_(trace_writer),
        intern_state_(intern_state),
        current_process_fill_header_(std::move(process_fill_header)) {
    MakeProfilePacket();
  }

  // This should be a temporary object, only used on the stack for dumping a
  // single process.
  DumpState(const DumpState&) = delete;
  DumpState& operator=(const DumpState&) = delete;
  DumpState(DumpState&&) = delete;
  DumpState& operator=(DumpState&&) = delete;

  void AddIdleBytes(uint64_t callstack_id, uint64_t bytes);

  void WriteAllocation(const HeapTracker::CallstackAllocations& alloc,
                       bool dump_at_max_mode);
  void DumpCallstacks(GlobalCallstackTrie* callsites);

 private:
  void WriteMap(const Interned<Mapping> map);
  void WriteFrame(const Interned<Frame> frame);
  void WriteBuildIDString(const Interned<std::string>& str);
  void WriteMappingPathString(const Interned<std::string>& str);
  void WriteFunctionNameString(const Interned<std::string>& str);

  void MakeTracePacket() {
    last_written_ = trace_writer_->written();

    if (current_trace_packet_)
      current_trace_packet_->Finalize();
    current_trace_packet_ = trace_writer_->NewTracePacket();
    current_trace_packet_->set_timestamp(
        static_cast<uint64_t>(base::GetBootTimeNs().count()));
    current_profile_packet_ = nullptr;
    current_interned_data_ = nullptr;
    current_process_heap_samples_ = nullptr;
  }

  void MakeProfilePacket() {
    MakeTracePacket();

    current_profile_packet_ = current_trace_packet_->set_profile_packet();
    uint64_t* next_index = intern_state_->HeapprofdNextIndexMutable();
    current_profile_packet_->set_index((*next_index)++);
  }

  uint64_t currently_written() {
    return trace_writer_->written() - last_written_;
  }

  protos::pbzero::ProfilePacket::ProcessHeapSamples*
  GetCurrentProcessHeapSamples();
  protos::pbzero::InternedData* GetCurrentInternedData();

  std::set<GlobalCallstackTrie::Node*> callstacks_to_dump_;

  TraceWriter* trace_writer_;
  InterningOutputTracker* intern_state_;

  protos::pbzero::ProfilePacket* current_profile_packet_ = nullptr;
  protos::pbzero::InternedData* current_interned_data_ = nullptr;
  TraceWriter::TracePacketHandle current_trace_packet_;
  protos::pbzero::ProfilePacket::ProcessHeapSamples*
      current_process_heap_samples_ = nullptr;
  std::function<void(protos::pbzero::ProfilePacket::ProcessHeapSamples*)>
      current_process_fill_header_;
  std::map<uint64_t /* callstack_id */, uint64_t> current_process_idle_allocs_;

  uint64_t last_written_ = 0;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_BOOKKEEPING_DUMP_H_
