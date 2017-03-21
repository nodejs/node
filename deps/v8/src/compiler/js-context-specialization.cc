// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-context-specialization.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/contexts.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction JSContextSpecialization::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSLoadContext:
      return ReduceJSLoadContext(node);
    case IrOpcode::kJSStoreContext:
      return ReduceJSStoreContext(node);
    default:
      break;
  }
  return NoChange();
}

Reduction JSContextSpecialization::SimplifyJSLoadContext(Node* node,
                                                         Node* new_context,
                                                         size_t new_depth) {
  DCHECK_EQ(IrOpcode::kJSLoadContext, node->opcode());
  const ContextAccess& access = ContextAccessOf(node->op());
  DCHECK_LE(new_depth, access.depth());

  if (new_depth == access.depth() &&
      new_context == NodeProperties::GetContextInput(node)) {
    return NoChange();
  }

  const Operator* op = jsgraph_->javascript()->LoadContext(
      new_depth, access.index(), access.immutable());
  NodeProperties::ReplaceContextInput(node, new_context);
  NodeProperties::ChangeOp(node, op);
  return Changed(node);
}

Reduction JSContextSpecialization::SimplifyJSStoreContext(Node* node,
                                                          Node* new_context,
                                                          size_t new_depth) {
  DCHECK_EQ(IrOpcode::kJSStoreContext, node->opcode());
  const ContextAccess& access = ContextAccessOf(node->op());
  DCHECK_LE(new_depth, access.depth());

  if (new_depth == access.depth() &&
      new_context == NodeProperties::GetContextInput(node)) {
    return NoChange();
  }

  const Operator* op =
      jsgraph_->javascript()->StoreContext(new_depth, access.index());
  NodeProperties::ReplaceContextInput(node, new_context);
  NodeProperties::ChangeOp(node, op);
  return Changed(node);
}

Reduction JSContextSpecialization::ReduceJSLoadContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadContext, node->opcode());

  const ContextAccess& access = ContextAccessOf(node->op());
  size_t depth = access.depth();

  // First walk up the context chain in the graph as far as possible.
  Node* outer = NodeProperties::GetOuterContext(node, &depth);

  Handle<Context> concrete;
  if (!NodeProperties::GetSpecializationContext(outer, context())
           .ToHandle(&concrete)) {
    // We do not have a concrete context object, so we can only partially reduce
    // the load by folding-in the outer context node.
    return SimplifyJSLoadContext(node, outer, depth);
  }

  // Now walk up the concrete context chain for the remaining depth.
  for (; depth > 0; --depth) {
    concrete = handle(concrete->previous(), isolate());
  }

  if (!access.immutable()) {
    // We found the requested context object but since the context slot is
    // mutable we can only partially reduce the load.
    return SimplifyJSLoadContext(node, jsgraph()->Constant(concrete), depth);
  }

  // Even though the context slot is immutable, the context might have escaped
  // before the function to which it belongs has initialized the slot.
  // We must be conservative and check if the value in the slot is currently
  // the hole or undefined. Only if it is neither of these, can we be sure that
  // it won't change anymore.
  Handle<Object> value(concrete->get(static_cast<int>(access.index())),
                       isolate());
  if (value->IsUndefined(isolate()) || value->IsTheHole(isolate())) {
    return SimplifyJSLoadContext(node, jsgraph()->Constant(concrete), depth);
  }

  // Success. The context load can be replaced with the constant.
  // TODO(titzer): record the specialization for sharing code across multiple
  // contexts that have the same value in the corresponding context slot.
  Node* constant = jsgraph_->Constant(value);
  ReplaceWithValue(node, constant);
  return Replace(constant);
}


Reduction JSContextSpecialization::ReduceJSStoreContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreContext, node->opcode());

  const ContextAccess& access = ContextAccessOf(node->op());
  size_t depth = access.depth();

  // First walk up the context chain in the graph until we reduce the depth to 0
  // or hit a node that does not have a CreateXYZContext operator.
  Node* outer = NodeProperties::GetOuterContext(node, &depth);

  Handle<Context> concrete;
  if (!NodeProperties::GetSpecializationContext(outer, context())
           .ToHandle(&concrete)) {
    // We do not have a concrete context object, so we can only partially reduce
    // the load by folding-in the outer context node.
    return SimplifyJSStoreContext(node, outer, depth);
  }

  // Now walk up the concrete context chain for the remaining depth.
  for (; depth > 0; --depth) {
    concrete = handle(concrete->previous(), isolate());
  }

  return SimplifyJSStoreContext(node, jsgraph()->Constant(concrete), depth);
}


Isolate* JSContextSpecialization::isolate() const {
  return jsgraph()->isolate();
}


JSOperatorBuilder* JSContextSpecialization::javascript() const {
  return jsgraph()->javascript();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
