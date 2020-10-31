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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_INCREMENTAL_STATE_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_INCREMENTAL_STATE_H_

#include <stdint.h>

#include <map>

#include "src/trace_processor/importers/proto/packet_sequence_state.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

// Stores per-packet-sequence incremental state during trace parsing, such as
// reference timestamps for delta timestamp calculation and interned messages.
class ProtoIncrementalState {
 public:
  ProtoIncrementalState(TraceProcessorContext* context) : context_(context) {}

  // Returns the PacketSequenceState for the packet sequence with the given id.
  // If this is a new sequence which we haven't tracked before, initializes and
  // inserts a new PacketSequenceState into the state map.
  PacketSequenceState* GetOrCreateStateForPacketSequence(uint32_t sequence_id) {
    auto& ptr = packet_sequence_states_[sequence_id];
    if (!ptr)
      ptr.reset(new PacketSequenceState(context_));
    return ptr.get();
  }

 private:
  // Stores unique_ptrs to ensure that pointers to a PacketSequenceState remain
  // valid even if the map rehashes.
  std::map<uint32_t, std::unique_ptr<PacketSequenceState>>
      packet_sequence_states_;

  TraceProcessorContext* context_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_INCREMENTAL_STATE_H_
