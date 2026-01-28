// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-range-verification.h"

#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-reducer-inl.h"
#include "src/maglev/maglev-reducer.h"

namespace v8::internal::maglev {

MaglevRangeVerificationProcessor::MaglevRangeVerificationProcessor(
    Graph* graph, NodeRanges* ranges)
    : reducer_(this, graph), ranges_(ranges) {}

BlockProcessResult MaglevRangeVerificationProcessor::PreProcessBasicBlock(
    BasicBlock* block) {
  reducer_.set_current_block(block);
  return BlockProcessResult::kContinue;
}

void MaglevRangeVerificationProcessor::PostProcessBasicBlock(
    BasicBlock* block) {
  reducer_.FlushNodesToBlock();
}

ProcessResult MaglevRangeVerificationProcessor::Process(
    ValueNode* node, const ProcessingState& state) {
  if (IsConstantNode(node->opcode())) {
    return ProcessResult::kContinue;
  }

  if (node->Is<Phi>()) {
    reducer_.SetNewNodePosition(BasicBlockPosition::Start());
  } else {
    // Add nodes after the visiting one.
    reducer_.SetNewNodePosition(BasicBlockPosition::At(state.node_index() + 1));
  }

  // TODO(victorgomes): If the range is empty, the node should be unused, can we
  // verify something about that?
  Range range = ranges_->Get(reducer_.current_block(), node);
  if (range.is_all() || range.is_empty()) return ProcessResult::kContinue;

  if (node->is_int32()) {
    CHECK(range.IsInt32());
    reducer_.AddNewNodeNoInputConversion<AssertRangeInt32>({node}, range);
  }

  if (node->is_float64()) {
    reducer_.AddNewNodeNoInputConversion<AssertRangeFloat64>({node}, range);
  }

  // TODO(victorgomes): Add a range verification when a range is narrowed after
  // branches.

  return ProcessResult::kContinue;
}

}  // namespace v8::internal::maglev
