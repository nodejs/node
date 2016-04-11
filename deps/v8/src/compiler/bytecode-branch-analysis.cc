// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/bytecode-branch-analysis.h"

#include "src/interpreter/bytecode-array-iterator.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

// The class contains all of the sites that contain
// branches to a particular target (bytecode offset).
class BytecodeBranchInfo final : public ZoneObject {
 public:
  explicit BytecodeBranchInfo(Zone* zone)
      : back_edge_offsets_(zone), fore_edge_offsets_(zone) {}

  void AddBranch(int source_offset, int target_offset);

  // The offsets of bytecodes that refer to this bytecode as
  // a back-edge predecessor.
  const ZoneVector<int>* back_edge_offsets() { return &back_edge_offsets_; }

  // The offsets of bytecodes that refer to this bytecode as
  // a forwards-edge predecessor.
  const ZoneVector<int>* fore_edge_offsets() { return &fore_edge_offsets_; }

 private:
  ZoneVector<int> back_edge_offsets_;
  ZoneVector<int> fore_edge_offsets_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeBranchInfo);
};


void BytecodeBranchInfo::AddBranch(int source_offset, int target_offset) {
  if (source_offset < target_offset) {
    fore_edge_offsets_.push_back(source_offset);
  } else {
    back_edge_offsets_.push_back(source_offset);
  }
}


BytecodeBranchAnalysis::BytecodeBranchAnalysis(
    Handle<BytecodeArray> bytecode_array, Zone* zone)
    : branch_infos_(zone),
      bytecode_array_(bytecode_array),
      reachable_(bytecode_array->length(), zone),
      zone_(zone) {}


void BytecodeBranchAnalysis::Analyze() {
  interpreter::BytecodeArrayIterator iterator(bytecode_array());
  bool reachable = true;
  while (!iterator.done()) {
    interpreter::Bytecode bytecode = iterator.current_bytecode();
    int current_offset = iterator.current_offset();
    // All bytecode basic blocks are generated to be forward reachable
    // and may also be backward reachable. Hence if there's a forward
    // branch targetting here the code becomes reachable.
    reachable = reachable || forward_branches_target(current_offset);
    if (reachable) {
      reachable_.Add(current_offset);
      if (interpreter::Bytecodes::IsConditionalJump(bytecode)) {
        // Only the branch is recorded, the forward path falls through
        // and is handled as normal bytecode data flow.
        AddBranch(current_offset, iterator.GetJumpTargetOffset());
      } else if (interpreter::Bytecodes::IsJump(bytecode)) {
        // Unless the branch targets the next bytecode it's not
        // reachable. If it targets the next bytecode the check at the
        // start of the loop will set the reachable flag.
        AddBranch(current_offset, iterator.GetJumpTargetOffset());
        reachable = false;
      } else if (interpreter::Bytecodes::IsJumpOrReturn(bytecode)) {
        DCHECK_EQ(bytecode, interpreter::Bytecode::kReturn);
        reachable = false;
      }
    }
    iterator.Advance();
  }
}


const ZoneVector<int>* BytecodeBranchAnalysis::BackwardBranchesTargetting(
    int offset) const {
  auto iterator = branch_infos_.find(offset);
  if (branch_infos_.end() != iterator) {
    return iterator->second->back_edge_offsets();
  } else {
    return nullptr;
  }
}


const ZoneVector<int>* BytecodeBranchAnalysis::ForwardBranchesTargetting(
    int offset) const {
  auto iterator = branch_infos_.find(offset);
  if (branch_infos_.end() != iterator) {
    return iterator->second->fore_edge_offsets();
  } else {
    return nullptr;
  }
}


void BytecodeBranchAnalysis::AddBranch(int source_offset, int target_offset) {
  BytecodeBranchInfo* branch_info = nullptr;
  auto iterator = branch_infos_.find(target_offset);
  if (branch_infos_.end() == iterator) {
    branch_info = new (zone()) BytecodeBranchInfo(zone());
    branch_infos_.insert(std::make_pair(target_offset, branch_info));
  } else {
    branch_info = iterator->second;
  }
  branch_info->AddBranch(source_offset, target_offset);
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
