// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-truncation.h"

#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-reducer-inl.h"

namespace v8::internal::maglev {

void TruncationProcessor::UnwrapInputs(NodeBase* node) {
  for (int i = 0; i < node->input_count(); i++) {
    ValueNode* input = node->input(i).node();
    if (!input) continue;
    node->change_input(i, input->UnwrapIdentities());
  }
}

BlockProcessResult TruncationProcessor::PreProcessBasicBlock(
    BasicBlock* block) {
  reducer_.set_current_block(block);
  current_node_index_ = 0;
  return BlockProcessResult::kContinue;
}

void TruncationProcessor::PostProcessBasicBlock(BasicBlock* block) {
  reducer_.FlushNodesToBlock();
}

void TruncationProcessor::PreProcessNode(Node* node,
                                         const ProcessingState& state) {
#ifdef DEBUG
  reducer_.StartNewPeriod();
#endif  // DEBUG
  if (reducer_.has_graph_labeller()) {
    reducer_.SetCurrentProvenance(
        reducer_.graph_labeller()->GetNodeProvenance(node));
  }
  reducer_.SetNewNodePosition(BasicBlockPosition::At(state.node_index()));
  UnwrapInputs(node);
}

void TruncationProcessor::PostProcessNode(Node*) {
#ifdef DEBUG
  reducer_.SetCurrentProvenance(MaglevGraphLabeller::Provenance{});
  reducer_.SetNewNodePosition(BasicBlockPosition::End());
#endif  // DEBUG
}

void TruncationProcessor::PreProcessNode(Phi* node,
                                         const ProcessingState& state) {
  UnwrapInputs(node);
}
void TruncationProcessor::PostProcessNode(Phi*) {}

void TruncationProcessor::PreProcessNode(ControlNode* node,
                                         const ProcessingState& state) {
  UnwrapInputs(node);
  reducer_.SetNewNodePosition(BasicBlockPosition::End());
}
void TruncationProcessor::PostProcessNode(ControlNode*) {}

int TruncationProcessor::NonInt32InputCount(ValueNode* node) {
  int non_int32_input_count = 0;
  for (Input input : node->inputs()) {
    ValueNode* input_node = input.node();
    if (!input_node->is_int32()) {
      if (input_node->Is<Float64Constant>() &&
          input_node->GetStaticRange().IsSafeInt()) {
        // We can truncate Float64 constants if they're in the safe integer
        // range.
        continue;
      }
      if (input_node->Is<ChangeInt32ToFloat64>()) {
        // We can always truncate this safe conversion.
        continue;
      }
      non_int32_input_count++;
    }
  }
  return non_int32_input_count;
}

void TruncationProcessor::ConvertInputsToFloat64(ValueNode* node) {
  for (int i = 0; i < node->input_count(); i++) {
    ValueNode* input = node->input_node(i);
    if (input->is_int32()) {
      ValueNode* converted_input =
          reducer_.AddNewNodeNoInputConversion<ChangeInt32ToFloat64>({input});
      node->change_input(i, converted_input);
    }
  }
}

ValueNode* TruncationProcessor::GetTruncatedInt32Input(ValueNode* node,
                                                       int index) {
  ValueNode* input = node->input_node(index);
  if (auto f64_cst = input->TryCast<Float64Constant>()) {
    DCHECK(input->GetStaticRange().IsSafeInt());
    input = GetTruncatedInt32Constant(f64_cst->value().get_scalar());
  } else if (input->Is<ChangeInt32ToFloat64>()) {
    input = input->input(0).node();
  }
  DCHECK(input->is_int32());
  return input;
}

void TruncationProcessor::EnsureTruncatedInt32Inputs(ValueNode* node) {
  for (int i = 0; i < node->input_count(); i++) {
    node->change_input(i, GetTruncatedInt32Input(node, i));
  }
}

ProcessResult TruncationProcessor::ProcessTruncatedConversion(ValueNode* node) {
  if (NonInt32InputCount(node) == 0) {
    node->OverwriteWithIdentityTo(GetTruncatedInt32Input(node, 0));
    return ProcessResult::kRemove;
  }
  return ProcessResult::kContinue;
}

ValueNode* TruncationProcessor::GetTruncatedInt32Constant(double constant) {
  return reducer_.GetInt32Constant(DoubleToInt32(constant));
}

bool TruncationProcessor::is_tracing_enabled() {
  return reducer_.is_tracing_enabled();
}

}  // namespace v8::internal::maglev
