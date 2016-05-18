// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/verifier.h"
#include "src/handles-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

// static
int NodeProperties::PastValueIndex(Node* node) {
  return FirstValueIndex(node) + node->op()->ValueInputCount();
}


// static
int NodeProperties::PastContextIndex(Node* node) {
  return FirstContextIndex(node) +
         OperatorProperties::GetContextInputCount(node->op());
}


// static
int NodeProperties::PastFrameStateIndex(Node* node) {
  return FirstFrameStateIndex(node) +
         OperatorProperties::GetFrameStateInputCount(node->op());
}


// static
int NodeProperties::PastEffectIndex(Node* node) {
  return FirstEffectIndex(node) + node->op()->EffectInputCount();
}


// static
int NodeProperties::PastControlIndex(Node* node) {
  return FirstControlIndex(node) + node->op()->ControlInputCount();
}


// static
Node* NodeProperties::GetValueInput(Node* node, int index) {
  DCHECK(0 <= index && index < node->op()->ValueInputCount());
  return node->InputAt(FirstValueIndex(node) + index);
}


// static
Node* NodeProperties::GetContextInput(Node* node) {
  DCHECK(OperatorProperties::HasContextInput(node->op()));
  return node->InputAt(FirstContextIndex(node));
}


// static
Node* NodeProperties::GetFrameStateInput(Node* node, int index) {
  DCHECK_LT(index, OperatorProperties::GetFrameStateInputCount(node->op()));
  return node->InputAt(FirstFrameStateIndex(node) + index);
}


// static
Node* NodeProperties::GetEffectInput(Node* node, int index) {
  DCHECK(0 <= index && index < node->op()->EffectInputCount());
  return node->InputAt(FirstEffectIndex(node) + index);
}


// static
Node* NodeProperties::GetControlInput(Node* node, int index) {
  DCHECK(0 <= index && index < node->op()->ControlInputCount());
  return node->InputAt(FirstControlIndex(node) + index);
}


// static
bool NodeProperties::IsValueEdge(Edge edge) {
  Node* const node = edge.from();
  return IsInputRange(edge, FirstValueIndex(node),
                      node->op()->ValueInputCount());
}


// static
bool NodeProperties::IsContextEdge(Edge edge) {
  Node* const node = edge.from();
  return IsInputRange(edge, FirstContextIndex(node),
                      OperatorProperties::GetContextInputCount(node->op()));
}


// static
bool NodeProperties::IsFrameStateEdge(Edge edge) {
  Node* const node = edge.from();
  return IsInputRange(edge, FirstFrameStateIndex(node),
                      OperatorProperties::GetFrameStateInputCount(node->op()));
}


// static
bool NodeProperties::IsEffectEdge(Edge edge) {
  Node* const node = edge.from();
  return IsInputRange(edge, FirstEffectIndex(node),
                      node->op()->EffectInputCount());
}


// static
bool NodeProperties::IsControlEdge(Edge edge) {
  Node* const node = edge.from();
  return IsInputRange(edge, FirstControlIndex(node),
                      node->op()->ControlInputCount());
}


// static
bool NodeProperties::IsExceptionalCall(Node* node) {
  if (node->op()->HasProperty(Operator::kNoThrow)) return false;
  for (Edge const edge : node->use_edges()) {
    if (!NodeProperties::IsControlEdge(edge)) continue;
    if (edge.from()->opcode() == IrOpcode::kIfException) return true;
  }
  return false;
}


// static
void NodeProperties::ReplaceValueInput(Node* node, Node* value, int index) {
  DCHECK(index < node->op()->ValueInputCount());
  node->ReplaceInput(FirstValueIndex(node) + index, value);
}


// static
void NodeProperties::ReplaceValueInputs(Node* node, Node* value) {
  int value_input_count = node->op()->ValueInputCount();
  DCHECK_LE(1, value_input_count);
  node->ReplaceInput(0, value);
  while (--value_input_count > 0) {
    node->RemoveInput(value_input_count);
  }
}


// static
void NodeProperties::ReplaceContextInput(Node* node, Node* context) {
  node->ReplaceInput(FirstContextIndex(node), context);
}


// static
void NodeProperties::ReplaceControlInput(Node* node, Node* control) {
  node->ReplaceInput(FirstControlIndex(node), control);
}


// static
void NodeProperties::ReplaceEffectInput(Node* node, Node* effect, int index) {
  DCHECK(index < node->op()->EffectInputCount());
  return node->ReplaceInput(FirstEffectIndex(node) + index, effect);
}


// static
void NodeProperties::ReplaceFrameStateInput(Node* node, int index,
                                            Node* frame_state) {
  DCHECK_LT(index, OperatorProperties::GetFrameStateInputCount(node->op()));
  node->ReplaceInput(FirstFrameStateIndex(node) + index, frame_state);
}


// static
void NodeProperties::RemoveFrameStateInput(Node* node, int index) {
  DCHECK_LT(index, OperatorProperties::GetFrameStateInputCount(node->op()));
  node->RemoveInput(FirstFrameStateIndex(node) + index);
}


// static
void NodeProperties::RemoveNonValueInputs(Node* node) {
  node->TrimInputCount(node->op()->ValueInputCount());
}


// static
void NodeProperties::RemoveValueInputs(Node* node) {
  int value_input_count = node->op()->ValueInputCount();
  while (--value_input_count >= 0) {
    node->RemoveInput(value_input_count);
  }
}


void NodeProperties::MergeControlToEnd(Graph* graph,
                                       CommonOperatorBuilder* common,
                                       Node* node) {
  graph->end()->AppendInput(graph->zone(), node);
  graph->end()->set_op(common->End(graph->end()->InputCount()));
}


// static
void NodeProperties::ReplaceUses(Node* node, Node* value, Node* effect,
                                 Node* success, Node* exception) {
  // Requires distinguishing between value, effect and control edges.
  for (Edge edge : node->use_edges()) {
    if (IsControlEdge(edge)) {
      if (edge.from()->opcode() == IrOpcode::kIfSuccess) {
        DCHECK_NOT_NULL(success);
        edge.UpdateTo(success);
      } else if (edge.from()->opcode() == IrOpcode::kIfException) {
        DCHECK_NOT_NULL(exception);
        edge.UpdateTo(exception);
      } else {
        UNREACHABLE();
      }
    } else if (IsEffectEdge(edge)) {
      DCHECK_NOT_NULL(effect);
      edge.UpdateTo(effect);
    } else {
      DCHECK_NOT_NULL(value);
      edge.UpdateTo(value);
    }
  }
}


// static
void NodeProperties::ChangeOp(Node* node, const Operator* new_op) {
  node->set_op(new_op);
  Verifier::VerifyNode(node);
}


// static
Node* NodeProperties::FindProjection(Node* node, size_t projection_index) {
  for (auto use : node->uses()) {
    if (use->opcode() == IrOpcode::kProjection &&
        ProjectionIndexOf(use->op()) == projection_index) {
      return use;
    }
  }
  return nullptr;
}


// static
void NodeProperties::CollectControlProjections(Node* node, Node** projections,
                                               size_t projection_count) {
#ifdef DEBUG
  DCHECK_LE(static_cast<int>(projection_count), node->UseCount());
  std::memset(projections, 0, sizeof(*projections) * projection_count);
#endif
  size_t if_value_index = 0;
  for (Edge const edge : node->use_edges()) {
    if (!IsControlEdge(edge)) continue;
    Node* use = edge.from();
    size_t index;
    switch (use->opcode()) {
      case IrOpcode::kIfTrue:
        DCHECK_EQ(IrOpcode::kBranch, node->opcode());
        index = 0;
        break;
      case IrOpcode::kIfFalse:
        DCHECK_EQ(IrOpcode::kBranch, node->opcode());
        index = 1;
        break;
      case IrOpcode::kIfSuccess:
        DCHECK(!node->op()->HasProperty(Operator::kNoThrow));
        index = 0;
        break;
      case IrOpcode::kIfException:
        DCHECK(!node->op()->HasProperty(Operator::kNoThrow));
        index = 1;
        break;
      case IrOpcode::kIfValue:
        DCHECK_EQ(IrOpcode::kSwitch, node->opcode());
        index = if_value_index++;
        break;
      case IrOpcode::kIfDefault:
        DCHECK_EQ(IrOpcode::kSwitch, node->opcode());
        index = projection_count - 1;
        break;
      default:
        continue;
    }
    DCHECK_LT(if_value_index, projection_count);
    DCHECK_LT(index, projection_count);
    DCHECK_NULL(projections[index]);
    projections[index] = use;
  }
#ifdef DEBUG
  for (size_t index = 0; index < projection_count; ++index) {
    DCHECK_NOT_NULL(projections[index]);
  }
#endif
}


// static
MaybeHandle<Context> NodeProperties::GetSpecializationContext(
    Node* node, MaybeHandle<Context> context) {
  switch (node->opcode()) {
    case IrOpcode::kHeapConstant:
      return Handle<Context>::cast(OpParameter<Handle<HeapObject>>(node));
    case IrOpcode::kParameter: {
      Node* const start = NodeProperties::GetValueInput(node, 0);
      DCHECK_EQ(IrOpcode::kStart, start->opcode());
      int const index = ParameterIndexOf(node->op());
      // The context is always the last parameter to a JavaScript function, and
      // {Parameter} indices start at -1, so value outputs of {Start} look like
      // this: closure, receiver, param0, ..., paramN, context.
      if (index == start->op()->ValueOutputCount() - 2) {
        return context;
      }
      break;
    }
    default:
      break;
  }
  return MaybeHandle<Context>();
}


// static
MaybeHandle<Context> NodeProperties::GetSpecializationNativeContext(
    Node* node, MaybeHandle<Context> native_context) {
  while (true) {
    switch (node->opcode()) {
      case IrOpcode::kJSLoadContext: {
        ContextAccess const& access = ContextAccessOf(node->op());
        if (access.index() != Context::NATIVE_CONTEXT_INDEX) {
          return MaybeHandle<Context>();
        }
        // Skip over the intermediate contexts, we're only interested in the
        // very last context in the context chain anyway.
        node = NodeProperties::GetContextInput(node);
        break;
      }
      case IrOpcode::kJSCreateBlockContext:
      case IrOpcode::kJSCreateCatchContext:
      case IrOpcode::kJSCreateFunctionContext:
      case IrOpcode::kJSCreateModuleContext:
      case IrOpcode::kJSCreateScriptContext:
      case IrOpcode::kJSCreateWithContext: {
        // Skip over the intermediate contexts, we're only interested in the
        // very last context in the context chain anyway.
        node = NodeProperties::GetContextInput(node);
        break;
      }
      case IrOpcode::kHeapConstant: {
        // Extract the native context from the actual {context}.
        Handle<Context> context =
            Handle<Context>::cast(OpParameter<Handle<HeapObject>>(node));
        return handle(context->native_context());
      }
      case IrOpcode::kOsrValue: {
        int const index = OpParameter<int>(node);
        if (index == Linkage::kOsrContextSpillSlotIndex) {
          return native_context;
        }
        return MaybeHandle<Context>();
      }
      case IrOpcode::kParameter: {
        Node* const start = NodeProperties::GetValueInput(node, 0);
        DCHECK_EQ(IrOpcode::kStart, start->opcode());
        int const index = ParameterIndexOf(node->op());
        // The context is always the last parameter to a JavaScript function,
        // and {Parameter} indices start at -1, so value outputs of {Start}
        // look like this: closure, receiver, param0, ..., paramN, context.
        if (index == start->op()->ValueOutputCount() - 2) {
          return native_context;
        }
        return MaybeHandle<Context>();
      }
      default:
        return MaybeHandle<Context>();
    }
  }
}


// static
MaybeHandle<JSGlobalObject> NodeProperties::GetSpecializationGlobalObject(
    Node* node, MaybeHandle<Context> native_context) {
  Handle<Context> context;
  if (GetSpecializationNativeContext(node, native_context).ToHandle(&context)) {
    return handle(context->global_object());
  }
  return MaybeHandle<JSGlobalObject>();
}


// static
Type* NodeProperties::GetTypeOrAny(Node* node) {
  return IsTyped(node) ? node->type() : Type::Any();
}


// static
bool NodeProperties::AllValueInputsAreTyped(Node* node) {
  int input_count = node->op()->ValueInputCount();
  for (int index = 0; index < input_count; ++index) {
    if (!IsTyped(GetValueInput(node, index))) return false;
  }
  return true;
}


// static
bool NodeProperties::IsInputRange(Edge edge, int first, int num) {
  if (num == 0) return false;
  int const index = edge.index();
  return first <= index && index < first + num;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
