// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_BLOCK_POSITION_H_
#define V8_COMPILER_BACKEND_BLOCK_POSITION_H_

#include "src/compiler/backend/instruction.h"

namespace v8::internal::compiler {

// Reorder InstructionBlocks using pgo execution count. The algorithm is based
// on Pettis-Hansen (P.H.) code positioning heuristics using the top-down
// approach. It starts by placing the entry block, thereafter selecting the best
// successor from last placed block with the largest execution count as long as
// it has not already been placed. If no more successors can be selected from
// last block, pick the unselected block with largest connection to the selected
// blocks. This contnues until all basic blocks are placed.
class V8_EXPORT_PRIVATE BlockPositionNumberer {
 public:
  BlockPositionNumberer(InstructionSequence* sequence, Zone* zone)
      : code_(sequence), zone_(zone) {}
  ZoneVector<int32_t> ComputeProfileGuidedBlockPosition();

 private:
  const InstructionBlocks& instruction_blocks() const;
  InstructionBlock* InstructionBlockAt(RpoNumber rpo_number) const;
  std::pair<const InstructionBlock*, uint64_t> GetBestSuccessor(
      const InstructionBlock* block, const std::set<RpoNumber>& visited) const;
  const InstructionBlock* GetBestSuccessorFromPlacedBlocks(
      const std::set<RpoNumber>& visited) const;
  ZoneVector<int32_t> ComputeTopdownPosition(const InstructionBlock* entry);
  Zone* zone() { return zone_; }

  InstructionSequence* const code_;
  Zone* zone_;
};

}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_BACKEND_BLOCK_POSITION_H_
