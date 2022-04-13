// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/constant-folding-reducer.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/js-heap-broker.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {
Node* TryGetConstant(JSGraph* jsgraph, Node* node) {
  Type type = NodeProperties::GetType(node);
  Node* result;
  if (type.IsNone()) {
    result = nullptr;
  } else if (type.Is(Type::Null())) {
    result = jsgraph->NullConstant();
  } else if (type.Is(Type::Undefined())) {
    result = jsgraph->UndefinedConstant();
  } else if (type.Is(Type::MinusZero())) {
    result = jsgraph->MinusZeroConstant();
  } else if (type.Is(Type::NaN())) {
    result = jsgraph->NaNConstant();
  } else if (type.Is(Type::Hole())) {
    result = jsgraph->TheHoleConstant();
  } else if (type.IsHeapConstant()) {
    result = jsgraph->Constant(type.AsHeapConstant()->Ref());
  } else if (type.Is(Type::PlainNumber()) && type.Min() == type.Max()) {
    result = jsgraph->Constant(type.Min());
  } else {
    result = nullptr;
  }
  DCHECK_EQ(result != nullptr, type.IsSingleton());
  DCHECK_IMPLIES(result != nullptr,
                 type.Equals(NodeProperties::GetType(result)));
  return result;
}

bool IsAlreadyBeingFolded(Node* node) {
  DCHECK(FLAG_assert_types);
  if (node->opcode() == IrOpcode::kFoldConstant) return true;
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsValueEdge(edge) &&
        edge.from()->opcode() == IrOpcode::kFoldConstant) {
      // Note: {node} may have gained new value uses since the time it was
      // "constant-folded", and theses uses should ideally be rewritten as well.
      // For simplicity, we ignore them here.
      return true;
    }
  }
  return false;
}
}  // namespace

ConstantFoldingReducer::ConstantFoldingReducer(Editor* editor, JSGraph* jsgraph,
                                               JSHeapBroker* broker)
    : AdvancedReducer(editor), jsgraph_(jsgraph), broker_(broker) {}

ConstantFoldingReducer::~ConstantFoldingReducer() = default;

Reduction ConstantFoldingReducer::Reduce(Node* node) {
  if (!NodeProperties::IsConstant(node) && NodeProperties::IsTyped(node) &&
      node->op()->HasProperty(Operator::kEliminatable) &&
      node->opcode() != IrOpcode::kFinishRegion) {
    Node* constant = TryGetConstant(jsgraph(), node);
    if (constant != nullptr) {
      DCHECK(NodeProperties::IsTyped(constant));
      if (!FLAG_assert_types) {
        DCHECK_EQ(node->op()->ControlOutputCount(), 0);
        ReplaceWithValue(node, constant);
        return Replace(constant);
      } else if (!IsAlreadyBeingFolded(node)) {
        // Delay the constant folding (by inserting a FoldConstant operation
        // instead) in order to keep type assertions meaningful.
        Node* fold_constant = jsgraph()->graph()->NewNode(
            jsgraph()->common()->FoldConstant(), node, constant);
        DCHECK(NodeProperties::IsTyped(fold_constant));
        ReplaceWithValue(node, fold_constant, node, node);
        fold_constant->ReplaceInput(0, node);
        DCHECK(IsAlreadyBeingFolded(node));
        DCHECK(IsAlreadyBeingFolded(fold_constant));
        return Changed(node);
      }
    }
  }
  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
