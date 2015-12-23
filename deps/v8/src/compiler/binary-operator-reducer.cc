// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/binary-operator-reducer.h"

#include <algorithm>

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/types-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

BinaryOperatorReducer::BinaryOperatorReducer(Editor* editor, Graph* graph,
                                             CommonOperatorBuilder* common,
                                             MachineOperatorBuilder* machine)
    : AdvancedReducer(editor),
      graph_(graph),
      common_(common),
      machine_(machine),
      dead_(graph->NewNode(common->Dead())) {}


Reduction BinaryOperatorReducer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kFloat64Mul:
      return ReduceFloat52Mul(node);
    case IrOpcode::kFloat64Div:
      return ReduceFloat52Div(node);
    default:
      break;
  }
  return NoChange();
}


Reduction BinaryOperatorReducer::ReduceFloat52Mul(Node* node) {
  if (!machine()->Is64()) return NoChange();

  Float64BinopMatcher m(node);
  if (!m.left().IsChangeInt32ToFloat64() ||
      !m.right().IsChangeInt32ToFloat64()) {
    return NoChange();
  }

  Type* type = NodeProperties::GetType(node);
  Type::RangeType* range = type->GetRange();

  // JavaScript has 52 bit precision in multiplication
  if (range == nullptr || range->Min() < 0.0 ||
      range->Max() > 0xFFFFFFFFFFFFFULL) {
    return NoChange();
  }

  Node* mul = graph()->NewNode(machine()->Int64Mul(), m.left().InputAt(0),
                               m.right().InputAt(0));
  Revisit(mul);

  Type* range_type = Type::Range(range->Min(), range->Max(), graph()->zone());

  // TODO(indutny): Is Type::Number() a proper thing here? It looks like
  // every other place is using Type:Internal() for int64 values.
  // Should we off-load range propagation to Typer?
  NodeProperties::SetType(
      mul, Type::Intersect(range_type, Type::Number(), graph()->zone()));

  Node* out = graph()->NewNode(machine()->RoundInt64ToFloat64(), mul);
  return Replace(out);
}


Reduction BinaryOperatorReducer::ReduceFloat52Div(Node* node) {
  if (!machine()->Is64()) return NoChange();

  Float64BinopMatcher m(node);
  if (!m.left().IsRoundInt64ToFloat64()) return NoChange();

  // Right value should be positive...
  if (!m.right().HasValue() || m.right().Value() <= 0) return NoChange();

  // ...integer...
  int64_t value = static_cast<int64_t>(m.right().Value());
  if (value != static_cast<int64_t>(m.right().Value())) return NoChange();

  // ...and should be a power of two.
  if (!base::bits::IsPowerOfTwo64(value)) return NoChange();

  Node* left = m.left().InputAt(0);
  Type::RangeType* range = NodeProperties::GetType(left)->GetRange();

  // The result should fit into 32bit word
  int64_t min = static_cast<int64_t>(range->Min()) / value;
  int64_t max = static_cast<int64_t>(range->Max()) / value;
  if (min < 0 || max > 0xFFFFFFFLL) {
    return NoChange();
  }

  int64_t shift = WhichPowerOf2_64(static_cast<int64_t>(m.right().Value()));

  // Replace division with 64bit right shift
  Node* shr =
      graph()->NewNode(machine()->Word64Shr(), left,
                       graph()->NewNode(common()->Int64Constant(shift)));
  Revisit(shr);

  Node* out = graph()->NewNode(machine()->RoundInt64ToFloat64(), shr);
  return Replace(out);
}


Reduction BinaryOperatorReducer::Change(Node* node, Operator const* op,
                                        Node* a) {
  node->ReplaceInput(0, a);
  node->TrimInputCount(1);
  NodeProperties::ChangeOp(node, op);
  return Changed(node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
