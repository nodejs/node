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

#ifndef SRC_PROFILING_COMMON_INTERNING_OUTPUT_H_
#define SRC_PROFILING_COMMON_INTERNING_OUTPUT_H_

#include <map>
#include <set>

#include <stdint.h>

#include "perfetto/ext/tracing/core/trace_writer.h"
#include "src/profiling/common/callstack_trie.h"
#include "src/profiling/common/interner.h"

#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"

namespace perfetto {
namespace profiling {

class InterningOutputTracker {
 public:
  // Writes out a full packet containing the "empty" (zero) internings.
  // NB: resulting packet has |incremental_state_cleared| set.
  static void WriteFixedInterningsPacket(TraceWriter* trace_writer);

  void WriteMap(const Interned<Mapping> map, protos::pbzero::InternedData* out);
  void WriteFrame(Interned<Frame> frame, protos::pbzero::InternedData* out);
  void WriteBuildIDString(const Interned<std::string>& str,
                          protos::pbzero::InternedData* out);
  void WriteMappingPathString(const Interned<std::string>& str,
                              protos::pbzero::InternedData* out);
  void WriteFunctionNameString(const Interned<std::string>& str,
                               protos::pbzero::InternedData* out);

  // Writes out the callstack represented by the given node.
  void WriteCallstack(GlobalCallstackTrie::Node* node,
                      GlobalCallstackTrie* trie,
                      protos::pbzero::InternedData* out);

  bool IsCallstackNew(uint64_t callstack_id) {
    return dumped_callstacks_.find(callstack_id) == dumped_callstacks_.end();
  }

  void ClearHistory();

  // TODO(rsavitski): move elsewhere, used in heapprofd for orthogonal
  // reasons. Shouldn't be cleared together with the rest of the incremental
  // state.
  uint64_t* HeapprofdNextIndexMutable() { return &next_index_; }

 private:
  // Map value is a bitfield distinguishing the distinct string fields
  // the string can be emitted as, e.g. kDumpedBuildID.
  std::map<InternID, int> dumped_strings_;
  std::set<InternID> dumped_frames_;
  std::set<InternID> dumped_mappings_;
  std::set<uint64_t> dumped_callstacks_;  // uses callstack trie's node ids

  uint64_t next_index_ = 0;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_COMMON_INTERNING_OUTPUT_H_
