// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/js-context-specialization.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node-aux-data-inl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

class ContextSpecializationVisitor : public NullNodeVisitor {
 public:
  explicit ContextSpecializationVisitor(JSContextSpecializer* spec)
      : spec_(spec) {}

  void Post(Node* node) {
    switch (node->opcode()) {
      case IrOpcode::kJSLoadContext: {
        Reduction r = spec_->ReduceJSLoadContext(node);
        if (r.Changed() && r.replacement() != node) {
          NodeProperties::ReplaceWithValue(node, r.replacement());
          node->RemoveAllInputs();
        }
        break;
      }
      case IrOpcode::kJSStoreContext: {
        Reduction r = spec_->ReduceJSStoreContext(node);
        if (r.Changed() && r.replacement() != node) {
          NodeProperties::ReplaceWithValue(node, r.replacement());
          node->RemoveAllInputs();
        }
        break;
      }
      default:
        break;
    }
  }

 private:
  JSContextSpecializer* spec_;
};


void JSContextSpecializer::SpecializeToContext() {
  NodeProperties::ReplaceWithValue(context_,
                                   jsgraph_->Constant(info_->context()));

  ContextSpecializationVisitor visitor(this);
  jsgraph_->graph()->VisitNodeInputsFromEnd(&visitor);
}


Reduction JSContextSpecializer::ReduceJSLoadContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadContext, node->opcode());

  HeapObjectMatcher<Context> m(NodeProperties::GetValueInput(node, 0));
  // If the context is not constant, no reduction can occur.
  if (!m.HasValue()) {
    return Reducer::NoChange();
  }

  const ContextAccess& access = ContextAccessOf(node->op());

  // Find the right parent context.
  Context* context = *m.Value().handle();
  for (size_t i = access.depth(); i > 0; --i) {
    context = context->previous();
  }

  // If the access itself is mutable, only fold-in the parent.
  if (!access.immutable()) {
    // The access does not have to look up a parent, nothing to fold.
    if (access.depth() == 0) {
      return Reducer::NoChange();
    }
    const Operator* op = jsgraph_->javascript()->LoadContext(
        0, access.index(), access.immutable());
    node->set_op(op);
    Handle<Object> context_handle = Handle<Object>(context, info_->isolate());
    node->ReplaceInput(0, jsgraph_->Constant(context_handle));
    return Reducer::Changed(node);
  }
  Handle<Object> value = Handle<Object>(
      context->get(static_cast<int>(access.index())), info_->isolate());

  // Even though the context slot is immutable, the context might have escaped
  // before the function to which it belongs has initialized the slot.
  // We must be conservative and check if the value in the slot is currently the
  // hole or undefined. If it is neither of these, then it must be initialized.
  if (value->IsUndefined() || value->IsTheHole()) {
    return Reducer::NoChange();
  }

  // Success. The context load can be replaced with the constant.
  // TODO(titzer): record the specialization for sharing code across multiple
  // contexts that have the same value in the corresponding context slot.
  return Reducer::Replace(jsgraph_->Constant(value));
}


Reduction JSContextSpecializer::ReduceJSStoreContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreContext, node->opcode());

  HeapObjectMatcher<Context> m(NodeProperties::GetValueInput(node, 0));
  // If the context is not constant, no reduction can occur.
  if (!m.HasValue()) {
    return Reducer::NoChange();
  }

  const ContextAccess& access = ContextAccessOf(node->op());

  // The access does not have to look up a parent, nothing to fold.
  if (access.depth() == 0) {
    return Reducer::NoChange();
  }

  // Find the right parent context.
  Context* context = *m.Value().handle();
  for (size_t i = access.depth(); i > 0; --i) {
    context = context->previous();
  }

  const Operator* op = jsgraph_->javascript()->StoreContext(0, access.index());
  node->set_op(op);
  Handle<Object> new_context_handle = Handle<Object>(context, info_->isolate());
  node->ReplaceInput(0, jsgraph_->Constant(new_context_handle));

  return Reducer::Changed(node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
