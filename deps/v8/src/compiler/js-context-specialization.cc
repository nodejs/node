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


MaybeHandle<Context> JSContextSpecialization::GetSpecializationContext(
    Node* node) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadContext ||
         node->opcode() == IrOpcode::kJSStoreContext);
  Node* const object = NodeProperties::GetValueInput(node, 0);
  return NodeProperties::GetSpecializationContext(object, context());
}


Reduction JSContextSpecialization::ReduceJSLoadContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadContext, node->opcode());

  // Get the specialization context from the node.
  Handle<Context> context;
  if (!GetSpecializationContext(node).ToHandle(&context)) return NoChange();

  // Find the right parent context.
  const ContextAccess& access = ContextAccessOf(node->op());
  for (size_t i = access.depth(); i > 0; --i) {
    context = handle(context->previous(), isolate());
  }

  // If the access itself is mutable, only fold-in the parent.
  if (!access.immutable()) {
    // The access does not have to look up a parent, nothing to fold.
    if (access.depth() == 0) {
      return NoChange();
    }
    const Operator* op = jsgraph_->javascript()->LoadContext(
        0, access.index(), access.immutable());
    node->ReplaceInput(0, jsgraph_->Constant(context));
    NodeProperties::ChangeOp(node, op);
    return Changed(node);
  }
  Handle<Object> value =
      handle(context->get(static_cast<int>(access.index())), isolate());

  // Even though the context slot is immutable, the context might have escaped
  // before the function to which it belongs has initialized the slot.
  // We must be conservative and check if the value in the slot is currently the
  // hole or undefined. If it is neither of these, then it must be initialized.
  if (value->IsUndefined() || value->IsTheHole()) {
    return NoChange();
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

  // Get the specialization context from the node.
  Handle<Context> context;
  if (!GetSpecializationContext(node).ToHandle(&context)) return NoChange();

  // The access does not have to look up a parent, nothing to fold.
  const ContextAccess& access = ContextAccessOf(node->op());
  if (access.depth() == 0) {
    return NoChange();
  }

  // Find the right parent context.
  for (size_t i = access.depth(); i > 0; --i) {
    context = handle(context->previous(), isolate());
  }

  node->ReplaceInput(0, jsgraph_->Constant(context));
  NodeProperties::ChangeOp(node, javascript()->StoreContext(0, access.index()));
  return Changed(node);
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
