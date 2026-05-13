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

namespace regexp {

class BytecodeAnalysis : public ZoneObject {
 public:
  BytecodeAnalysis(Isolate* isolate, Zone* zone,
                   DirectHandle<TrustedByteArray> bytecode);

  // Runs the analysis.
  void Analyze();

  void PrintBlock(uint32_t block_id);

  // Queries
  bool IsLoopHeader(uint32_t block_id) const;

  bool UsesCurrentChar(uint32_t block_id) const;
  bool LoadsCurrentChar(uint32_t block_id) const;

  uint32_t GetBlockId(uint32_t bytecode_offset) const;
  // Extended basic blocks (EBBs) are a sequence of blocks with a single entry
  // (the first block in the EBB) and multiple exits. This is used for loop
  // detection and reachability analysis.
  int GetEbbId(uint32_t block_id) const;
  uint32_t BlockStart(uint32_t block_id) const;
  uint32_t BlockEnd(uint32_t block_id) const;

 private:
  // Information about a loop in the control flow graph.
  struct LoopInfo {
    uint32_t header_block_id;
    BitVector members;
    // Pairs of (source_block_id, target_block_id) where target is outside loop
    // members.
    ZoneVector<std::pair<uint32_t, uint32_t>> exits;

    LoopInfo(uint32_t header_id, uint32_t num_blocks, Zone* zone)
        : header_block_id(header_id), members(num_blocks, zone), exits(zone) {}
  };

  // Build the basic block graph.
  // This is a single-pass builder that identifies block leaders, constructs
  // successors and predecessors, and annotates blocks with data-flow
  // properties (uses/loads current character).
  void BuildBlocks();

  // Compute reachability, extended basic blocks, and find loops.
  // This performs a DFS on the graph constructed by BuildBlocks().
  void AnalyzeControlFlow();

  // The number of "real" blocks (those corresponding to bytecode).
  uint32_t block_count() const {
    DCHECK_GE(block_starts_.size(), 1 + kBlockStartsSentinelCount);
    return static_cast<uint32_t>(block_starts_.size()) -
           kBlockStartsSentinelCount;
  }

  // A dispatch node that acts as a central hub for all backtrack targets.
  // "Backtrack" bytecodes jump to this node, and this node has edges to all
  // possible "PushBacktrack" targets. This avoids O(N*M) edges in the graph
  // where N is the number of backtrack sites and M is the number of targets.
  uint32_t backtrack_dispatch_id() const { return block_count(); }

  // Total count including the backtrack dispatch node.
  uint32_t total_block_count() const {
    return block_count() + kDispatchBlockCount;
  }

  Zone* zone_;
  Handle<TrustedByteArray> bytecode_;
  uint32_t length_;

  static constexpr uint32_t kDispatchBlockCount = 1;
  // The block_starts_ array has one extra element for the end of the last
  // block.
  static constexpr uint32_t kBlockStartsSentinelCount = 1;

  // Backtrack bytecodes may jump to any backtrack target. We collect all
  // PushBacktrack targets in the first pass.
  ZoneVector<uint32_t> backtrack_targets_;

  // Block boundaries. Index: block_id.
  // Block[i] range is [ block_starts_[i], block_starts_[i+1] )
  // The last entry in block_starts_ is equal to length_, serving as the
  // end of the last block.
  ZoneVector<uint32_t> block_starts_;

  // Extended basic blocks (single entry, multiple exits).
  // Which EBB a block belongs to. Index: block_id.
  ZoneVector<int32_t> block_to_ebb_id_;

  // Successors for each block. Index: block_id.
  // Successors are sorted and unique.
  ZoneVector<ZoneVector<uint32_t>> successors_;
  // Predecessors for each block. Index: block_id.
  // Predecessors are sorted and unique.
  ZoneVector<ZoneVector<uint32_t>> predecessors_;

  // Loops found in the graph.
  ZoneVector<LoopInfo> loops_;

  // Analysis Results
  BitVector is_loop_header_;      // Index: block_id.
  BitVector uses_current_char_;   // Index: block_id.
  BitVector loads_current_char_;  // Index: block_id.
  // Whether a block ends with a 'Backtrack' bytecode.
  BitVector terminates_with_backtrack_;  // Index: block_id.
  // List of (from, to) block pairs that represent back-edges in the DFS.
  ZoneVector<std::pair<uint32_t, uint32_t>> back_edges_;

  DisallowGarbageCollection no_gc_;
};

}  // namespace regexp
}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODE_ANALYSIS_H_
