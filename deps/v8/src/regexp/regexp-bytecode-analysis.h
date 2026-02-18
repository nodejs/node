// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_BYTECODE_ANALYSIS_H_
#define V8_REGEXP_REGEXP_BYTECODE_ANALYSIS_H_

#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/objects/trusted-object.h"
#include "src/utils/bit-vector.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class TrustedByteArray;

class RegExpBytecodeAnalysis : public ZoneObject {
 public:
  RegExpBytecodeAnalysis(Isolate* isolate, Zone* zone,
                         DirectHandle<TrustedByteArray> bytecode);

  // Runs the analysis.
  void Analyze();

  void PrintBlock(uint32_t block_id);

  // Queries
  bool IsLoopHeader(uint32_t block_id) const;
  bool IsBackEdge(uint32_t bytecode_offset) const;

  bool UsesCurrentChar(uint32_t block_id) const;
  bool LoadsCurrentChar(uint32_t block_id) const;

  uint32_t GetBlockId(uint32_t bytecode_offset) const;
  int GetEbbId(uint32_t bytecode_offset) const;
  uint32_t BlockStart(uint32_t block_id) const;
  uint32_t BlockEnd(uint32_t block_id) const;

 private:
  struct LoopInfo {
    uint32_t header_block_id;
    BitVector members;
    // Pairs of (source_block_id, target_block_id).
    ZoneVector<std::pair<uint32_t, uint32_t>> exits;

    LoopInfo(uint32_t header_id, uint32_t num_blocks, Zone* zone)
        : header_block_id(header_id), members(num_blocks, zone), exits(zone) {}
  };

  void FindBasicBlocks();
  void AnalyzeControlFlow();
  void ComputeLoops(
      const ZoneVector<std::pair<uint32_t, uint32_t>>& back_edges);
  void AnalyzeDataFlow();

  // Helper to iterate successors of a block.
  template <typename Callback>
  void ForEachSuccessor(uint32_t block_id, Callback callback,
                        bool include_backtrack);

  uint32_t block_count() const {
    DCHECK_GE(block_starts_.size(), kSlotAtLength);
    return static_cast<uint32_t>(block_starts_.size()) - kSlotAtLength;
  }

  Zone* zone_;
  Handle<TrustedByteArray> bytecode_;
  uint32_t length_;

  // Some arrays are sized so they are safe to access at index
  // `bytecode_array.length`.
  static constexpr int kSlotAtLength = 1;

  // TODO(jgruber): For all bytecode_offset-indexed data structures: reduce
  // overhead by only considering kBytecodeAlignment. We could also merge block
  // information (such as the ebb id) into a BlockInfo struct.

  // Enable backwards walks. Index: bytecode_offset.
  ZoneVector<uint8_t> offset_to_prev_bytecode_;

  // Backtrack bytecodes may jump to any backtrack target. We collect all
  // PushBacktrack targets in the first pass.
  ZoneVector<uint32_t> backtrack_targets_;

  // Block boundaries. Index: block_id.
  // Block[i] range is [ block_starts_[i], block_starts_[i+1] )
  // The last entry in block_starts_ is equal to length_, serving as the
  // end of the last block.
  ZoneVector<uint32_t> block_starts_;

  // Which block a bytecode belongs to. Index: bytecode_offset.
  ZoneVector<int32_t> offset_to_block_id_;

  // Extended basic blocks (single entry, multiple exits).
  // Which EBB a bytecode belongs to. Index: bytecode_offset.
  ZoneVector<int32_t> offset_to_ebb_id_;

  // Predecessors for each block. Index: block_id.
  ZoneVector<ZoneSet<int>> predecessors_;

  // Loops found in the graph.
  ZoneVector<LoopInfo> loops_;

  // Analysis Results
  BitVector is_loop_header_;      // Index: block_id.
  BitVector is_back_edge_;        // Index: bytecode_offset.
  BitVector uses_current_char_;   // Index: block_id.
  BitVector loads_current_char_;  // Index: block_id.

  DisallowGarbageCollection no_gc_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODE_ANALYSIS_H_
