// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/check-elimination.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/objects-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction CheckElimination::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kCheckHeapObject:
      return ReduceCheckHeapObject(node);
    case IrOpcode::kCheckString:
      return ReduceCheckString(node);
    case IrOpcode::kCheckSeqString:
      return ReduceCheckSeqString(node);
    case IrOpcode::kCheckNonEmptyString:
      return ReduceCheckNonEmptyString(node);
    default:
      break;
  }
  return NoChange();
}

Reduction CheckElimination::ReduceCheckHeapObject(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  HeapObjectMatcher m(input);
  if (m.HasValue()) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction CheckElimination::ReduceCheckString(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  HeapObjectMatcher m(input);
  if (m.HasValue() && m.Value()->IsString()) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction CheckElimination::ReduceCheckSeqString(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  HeapObjectMatcher m(input);
  if (m.HasValue() && m.Value()->IsSeqString()) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction CheckElimination::ReduceCheckNonEmptyString(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  HeapObjectMatcher m(input);
  if (m.HasValue() && m.Value()->IsString() &&
      node != jsgraph()->EmptyStringConstant()) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
