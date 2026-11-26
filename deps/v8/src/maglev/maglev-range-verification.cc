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
  reducer_.SetNewNodePosition(BasicBlockPosition::Start());
  for (const auto [node, range] : ranges_->overrides(block)) {
    EmitRangeAssertion(node, range);
  }

  return BlockProcessResult::kContinue;
}

BlockProcessResult MaglevRangeVerificationProcessor::PostProcessBasicBlock(
    BasicBlock* block) {
  reducer_.FlushNodesToBlock();
  return BlockProcessResult::kContinue;
}

void MaglevRangeVerificationProcessor::EmitRangeAssertion(ValueNode* node,
                                                          Range range) {
  DCHECK(!IsConstantNode(node->opcode()));

  if (node->use_count() == 0 && !node->properties().is_required_when_unused()) {
    return;
  }

  if (range.is_all() || range.is_empty()) return;

  if (node->is_int32()) {
    CHECK(range.IsInt32());
    reducer_.AddNewNodeNoInputConversion<AssertRangeInt32>({node}, range);
  }

  if (node->is_float64()) {
    reducer_.AddNewNodeNoInputConversion<AssertRangeFloat64>({node}, range);
  }
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
  EmitRangeAssertion(node, ranges_->Get(reducer_.current_block(), node));
  return ProcessResult::kContinue;
}

}  // namespace v8::internal::maglev
