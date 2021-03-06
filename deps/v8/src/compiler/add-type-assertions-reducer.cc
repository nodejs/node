// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/add-type-assertions-reducer.h"

#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

AddTypeAssertionsReducer::AddTypeAssertionsReducer(Editor* editor,
                                                   JSGraph* jsgraph, Zone* zone)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      visited_(jsgraph->graph()->NodeCount(), zone) {}

AddTypeAssertionsReducer::~AddTypeAssertionsReducer() = default;

Reduction AddTypeAssertionsReducer::Reduce(Node* node) {
  if (node->opcode() == IrOpcode::kAssertType ||
      node->opcode() == IrOpcode::kPhi || !NodeProperties::IsTyped(node) ||
      visited_.Get(node)) {
    return NoChange();
  }
  visited_.Set(node, true);

  Type type = NodeProperties::GetType(node);
  if (!type.IsRange()) {
    return NoChange();
  }

  Node* assertion = graph()->NewNode(simplified()->AssertType(type), node);
  NodeProperties::SetType(assertion, type);

  for (Edge edge : node->use_edges()) {
    Node* const user = edge.from();
    DCHECK(!user->IsDead());
    if (NodeProperties::IsValueEdge(edge) && user != assertion) {
      edge.UpdateTo(assertion);
      Revisit(user);
    }
  }

  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
