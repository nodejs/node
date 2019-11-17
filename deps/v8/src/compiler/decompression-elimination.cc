// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/decompression-elimination.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

DecompressionElimination::DecompressionElimination(
    Editor* editor, Graph* graph, MachineOperatorBuilder* machine,
    CommonOperatorBuilder* common)
    : AdvancedReducer(editor),
      graph_(graph),
      machine_(machine),
      common_(common) {}

bool DecompressionElimination::IsReducibleConstantOpcode(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kInt64Constant:
    case IrOpcode::kHeapConstant:
      return true;
    default:
      return false;
  }
}

bool DecompressionElimination::IsValidDecompress(
    IrOpcode::Value compressOpcode, IrOpcode::Value decompressOpcode) {
  switch (compressOpcode) {
    case IrOpcode::kChangeTaggedToCompressed:
      return IrOpcode::IsDecompressOpcode(decompressOpcode);
    case IrOpcode::kChangeTaggedSignedToCompressedSigned:
      return decompressOpcode ==
                 IrOpcode::kChangeCompressedSignedToTaggedSigned ||
             decompressOpcode == IrOpcode::kChangeCompressedToTagged;
    case IrOpcode::kChangeTaggedPointerToCompressedPointer:
      return decompressOpcode ==
                 IrOpcode::kChangeCompressedPointerToTaggedPointer ||
             decompressOpcode == IrOpcode::kChangeCompressedToTagged;
    default:
      UNREACHABLE();
  }
}

Node* DecompressionElimination::GetCompressedConstant(Node* constant) {
  switch (constant->opcode()) {
    case IrOpcode::kInt64Constant:
      return graph()->NewNode(common()->Int32Constant(
          static_cast<int32_t>(OpParameter<int64_t>(constant->op()))));
      break;
    case IrOpcode::kHeapConstant:
      return graph()->NewNode(
          common()->CompressedHeapConstant(HeapConstantOf(constant->op())));
    default:
      UNREACHABLE();
  }
}

Reduction DecompressionElimination::ReduceCompress(Node* node) {
  DCHECK(IrOpcode::IsCompressOpcode(node->opcode()));

  DCHECK_EQ(node->InputCount(), 1);
  Node* input_node = node->InputAt(0);
  IrOpcode::Value input_opcode = input_node->opcode();
  if (IrOpcode::IsDecompressOpcode(input_opcode)) {
    DCHECK_EQ(input_node->InputCount(), 1);
    return Replace(input_node->InputAt(0));
  } else if (IsReducibleConstantOpcode(input_opcode)) {
    return Replace(GetCompressedConstant(input_node));
  } else {
    return NoChange();
  }
}

Reduction DecompressionElimination::ReduceDecompress(Node* node) {
  DCHECK(IrOpcode::IsDecompressOpcode(node->opcode()));

  DCHECK_EQ(node->InputCount(), 1);
  Node* input_node = node->InputAt(0);
  IrOpcode::Value input_opcode = input_node->opcode();
  if (IrOpcode::IsCompressOpcode(input_opcode)) {
    DCHECK(IsValidDecompress(input_opcode, node->opcode()));
    DCHECK_EQ(input_node->InputCount(), 1);
    return Replace(input_node->InputAt(0));
  } else {
    return NoChange();
  }
}

Reduction DecompressionElimination::ReducePhi(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kPhi);

  const int value_input_count = node->op()->ValueInputCount();

  // Check if all input are decompress nodes, and if all are the same.
  bool same_decompresses = true;
  IrOpcode::Value first_opcode = node->InputAt(0)->opcode();
  for (int i = 0; i < value_input_count; ++i) {
    Node* input = node->InputAt(i);
    if (IrOpcode::IsDecompressOpcode(input->opcode())) {
      same_decompresses &= first_opcode == input->opcode();
    } else {
      return NoChange();
    }
  }

  // By now, we know that all inputs are decompress nodes. If all are the same,
  // we can grab the first one to be used after the Phi. If we have different
  // Decompress nodes as inputs, we need to use a conservative decompression
  // after the Phi.
  const Operator* op;
  if (same_decompresses) {
    op = node->InputAt(0)->op();
  } else {
    op = machine()->ChangeCompressedToTagged();
  }

  // Rewire phi's inputs to be the compressed inputs.
  for (int i = 0; i < value_input_count; ++i) {
    Node* input = node->InputAt(i);
    DCHECK_EQ(input->InputCount(), 1);
    node->ReplaceInput(i, input->InputAt(0));
  }

  // Update the MachineRepresentation on the Phi.
  MachineRepresentation rep;
  switch (op->opcode()) {
    case IrOpcode::kChangeCompressedToTagged:
      rep = MachineRepresentation::kCompressed;
      break;
    case IrOpcode::kChangeCompressedSignedToTaggedSigned:
      rep = MachineRepresentation::kCompressedSigned;
      break;
    case IrOpcode::kChangeCompressedPointerToTaggedPointer:
      rep = MachineRepresentation::kCompressedPointer;
      break;
    default:
      UNREACHABLE();
  }
  NodeProperties::ChangeOp(node, common()->Phi(rep, value_input_count));

  // Add a decompress after the Phi. To do this, we need to replace the Phi with
  // "Phi <- Decompress".
  Node* decompress = graph()->NewNode(op, node);
  ReplaceWithValue(node, decompress);
  decompress->ReplaceInput(0, node);
  return Changed(node);
}

Reduction DecompressionElimination::ReduceTypedStateValues(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kTypedStateValues);

  bool any_change = false;
  for (int i = 0; i < node->InputCount(); ++i) {
    Node* input = node->InputAt(i);
    if (IrOpcode::IsDecompressOpcode(input->opcode())) {
      DCHECK_EQ(input->InputCount(), 1);
      node->ReplaceInput(i, input->InputAt(0));
      any_change = true;
    }
  }
  return any_change ? Changed(node) : NoChange();
}

Reduction DecompressionElimination::ReduceWord32Equal(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWord32Equal);

  DCHECK_EQ(node->InputCount(), 2);
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  if (!IrOpcode::IsCompressOpcode(lhs->opcode()) ||
      !IrOpcode::IsCompressOpcode(rhs->opcode())) {
    return NoChange();
  }
  // Input nodes for compress operation.
  lhs = lhs->InputAt(0);
  rhs = rhs->InputAt(0);

  bool changed = false;

  if (lhs->opcode() == IrOpcode::kBitcastWordToTaggedSigned) {
    Node* input = lhs->InputAt(0);
    if (IsReducibleConstantOpcode(input->opcode())) {
      node->ReplaceInput(0, GetCompressedConstant(input));
      changed = true;
    }
  }

  if (rhs->opcode() == IrOpcode::kBitcastWordToTaggedSigned) {
    Node* input = rhs->InputAt(0);
    if (IsReducibleConstantOpcode(input->opcode())) {
      node->ReplaceInput(1, GetCompressedConstant(input));
      changed = true;
    }
  }

  return changed ? Changed(node) : NoChange();
}

Reduction DecompressionElimination::ReduceWord64Equal(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWord64Equal);

  DCHECK_EQ(node->InputCount(), 2);
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  bool lhs_is_decompress = IrOpcode::IsDecompressOpcode(lhs->opcode());
  bool rhs_is_decompress = IrOpcode::IsDecompressOpcode(rhs->opcode());

  // Case where both of its inputs are Decompress nodes.
  if (lhs_is_decompress && rhs_is_decompress) {
    DCHECK_EQ(lhs->InputCount(), 1);
    node->ReplaceInput(0, lhs->InputAt(0));
    DCHECK_EQ(rhs->InputCount(), 1);
    node->ReplaceInput(1, rhs->InputAt(0));
    NodeProperties::ChangeOp(node, machine()->Word32Equal());
    return Changed(node);
  }

  bool lhs_is_constant = IsReducibleConstantOpcode(lhs->opcode());
  bool rhs_is_constant = IsReducibleConstantOpcode(rhs->opcode());

  // Case where one input is a Decompress node and the other a constant.
  if ((lhs_is_decompress && rhs_is_constant) ||
      (lhs_is_constant && rhs_is_decompress)) {
    node->ReplaceInput(
        0, lhs_is_decompress ? lhs->InputAt(0) : GetCompressedConstant(lhs));
    node->ReplaceInput(
        1, lhs_is_decompress ? GetCompressedConstant(rhs) : rhs->InputAt(0));
    NodeProperties::ChangeOp(node, machine()->Word32Equal());
    return Changed(node);
  }

  return NoChange();
}

Reduction DecompressionElimination::Reduce(Node* node) {
  DisallowHeapAccess no_heap_access;

  switch (node->opcode()) {
    case IrOpcode::kChangeTaggedToCompressed:
    case IrOpcode::kChangeTaggedSignedToCompressedSigned:
    case IrOpcode::kChangeTaggedPointerToCompressedPointer:
      return ReduceCompress(node);
    case IrOpcode::kChangeCompressedToTagged:
    case IrOpcode::kChangeCompressedSignedToTaggedSigned:
    case IrOpcode::kChangeCompressedPointerToTaggedPointer:
      return ReduceDecompress(node);
    case IrOpcode::kPhi:
      return ReducePhi(node);
    case IrOpcode::kTypedStateValues:
      return ReduceTypedStateValues(node);
    case IrOpcode::kWord32Equal:
      return ReduceWord32Equal(node);
    case IrOpcode::kWord64Equal:
      return ReduceWord64Equal(node);
    default:
      break;
  }

  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
