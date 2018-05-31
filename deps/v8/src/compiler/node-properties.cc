// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node-properties.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/verifier.h"
#include "src/handles-inl.h"
#include "src/zone/zone-handle-set.h"

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
Node* NodeProperties::GetFrameStateInput(Node* node) {
  DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));
  return node->InputAt(FirstFrameStateIndex(node));
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
bool NodeProperties::IsExceptionalCall(Node* node, Node** out_exception) {
  if (node->op()->HasProperty(Operator::kNoThrow)) return false;
  for (Edge const edge : node->use_edges()) {
    if (!NodeProperties::IsControlEdge(edge)) continue;
    if (edge.from()->opcode() == IrOpcode::kIfException) {
      if (out_exception != nullptr) *out_exception = edge.from();
      return true;
    }
  }
  return false;
}

// static
Node* NodeProperties::FindSuccessfulControlProjection(Node* node) {
  DCHECK_GT(node->op()->ControlOutputCount(), 0);
  if (node->op()->HasProperty(Operator::kNoThrow)) return node;
  for (Edge const edge : node->use_edges()) {
    if (!NodeProperties::IsControlEdge(edge)) continue;
    if (edge.from()->opcode() == IrOpcode::kIfSuccess) {
      return edge.from();
    }
  }
  return node;
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
void NodeProperties::ReplaceControlInput(Node* node, Node* control, int index) {
  DCHECK(index < node->op()->ControlInputCount());
  node->ReplaceInput(FirstControlIndex(node) + index, control);
}


// static
void NodeProperties::ReplaceEffectInput(Node* node, Node* effect, int index) {
  DCHECK(index < node->op()->EffectInputCount());
  return node->ReplaceInput(FirstEffectIndex(node) + index, effect);
}


// static
void NodeProperties::ReplaceFrameStateInput(Node* node, Node* frame_state) {
  DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));
  node->ReplaceInput(FirstFrameStateIndex(node), frame_state);
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
        DCHECK_NOT_NULL(success);
        edge.UpdateTo(success);
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
Node* NodeProperties::FindFrameStateBefore(Node* node) {
  Node* effect = NodeProperties::GetEffectInput(node);
  while (effect->opcode() != IrOpcode::kCheckpoint) {
    if (effect->opcode() == IrOpcode::kDead) return effect;
    DCHECK_EQ(1, effect->op()->EffectInputCount());
    effect = NodeProperties::GetEffectInput(effect);
  }
  Node* frame_state = GetFrameStateInput(effect);
  return frame_state;
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
void NodeProperties::CollectValueProjections(Node* node, Node** projections,
                                             size_t projection_count) {
#ifdef DEBUG
  for (size_t index = 0; index < projection_count; ++index) {
    DCHECK_NULL(projections[index]);
  }
#endif
  for (Edge const edge : node->use_edges()) {
    if (!IsValueEdge(edge)) continue;
    Node* use = edge.from();
    DCHECK_EQ(IrOpcode::kProjection, use->opcode());
    projections[ProjectionIndexOf(use->op())] = use;
  }
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
bool NodeProperties::IsSame(Node* a, Node* b) {
  for (;;) {
    if (a->opcode() == IrOpcode::kCheckHeapObject) {
      a = GetValueInput(a, 0);
      continue;
    }
    if (b->opcode() == IrOpcode::kCheckHeapObject) {
      b = GetValueInput(b, 0);
      continue;
    }
    return a == b;
  }
}

// static
NodeProperties::InferReceiverMapsResult NodeProperties::InferReceiverMaps(
    Node* receiver, Node* effect, ZoneHandleSet<Map>* maps_return) {
  HeapObjectMatcher m(receiver);
  if (m.HasValue()) {
    Handle<HeapObject> receiver = m.Value();
    Isolate* const isolate = m.Value()->GetIsolate();
    // We don't use ICs for the Array.prototype and the Object.prototype
    // because the runtime has to be able to intercept them properly, so
    // we better make sure that TurboFan doesn't outsmart the system here
    // by storing to elements of either prototype directly.
    //
    // TODO(bmeurer): This can be removed once the Array.prototype and
    // Object.prototype have NO_ELEMENTS elements kind.
    if (!isolate->IsInAnyContext(*receiver,
                                 Context::INITIAL_ARRAY_PROTOTYPE_INDEX) &&
        !isolate->IsInAnyContext(*receiver,
                                 Context::INITIAL_OBJECT_PROTOTYPE_INDEX)) {
      Handle<Map> receiver_map(receiver->map(), isolate);
      if (receiver_map->is_stable()) {
        // The {receiver_map} is only reliable when we install a stability
        // code dependency.
        *maps_return = ZoneHandleSet<Map>(receiver_map);
        return kUnreliableReceiverMaps;
      }
    }
  }
  InferReceiverMapsResult result = kReliableReceiverMaps;
  while (true) {
    switch (effect->opcode()) {
      case IrOpcode::kMapGuard: {
        Node* const object = GetValueInput(effect, 0);
        if (IsSame(receiver, object)) {
          *maps_return = MapGuardMapsOf(effect->op()).maps();
          return result;
        }
        break;
      }
      case IrOpcode::kCheckMaps: {
        Node* const object = GetValueInput(effect, 0);
        if (IsSame(receiver, object)) {
          *maps_return = CheckMapsParametersOf(effect->op()).maps();
          return result;
        }
        break;
      }
      case IrOpcode::kJSCreate: {
        if (IsSame(receiver, effect)) {
          HeapObjectMatcher mtarget(GetValueInput(effect, 0));
          HeapObjectMatcher mnewtarget(GetValueInput(effect, 1));
          if (mtarget.HasValue() && mnewtarget.HasValue() &&
              mnewtarget.Value()->IsJSFunction()) {
            Handle<JSFunction> original_constructor =
                Handle<JSFunction>::cast(mnewtarget.Value());
            if (original_constructor->has_initial_map()) {
              Handle<Map> initial_map(original_constructor->initial_map());
              if (initial_map->constructor_or_backpointer() ==
                  *mtarget.Value()) {
                *maps_return = ZoneHandleSet<Map>(initial_map);
                return result;
              }
            }
          }
          // We reached the allocation of the {receiver}.
          return kNoReceiverMaps;
        }
        break;
      }
      case IrOpcode::kStoreField: {
        // We only care about StoreField of maps.
        Node* const object = GetValueInput(effect, 0);
        FieldAccess const& access = FieldAccessOf(effect->op());
        if (access.base_is_tagged == kTaggedBase &&
            access.offset == HeapObject::kMapOffset) {
          if (IsSame(receiver, object)) {
            Node* const value = GetValueInput(effect, 1);
            HeapObjectMatcher m(value);
            if (m.HasValue()) {
              *maps_return = ZoneHandleSet<Map>(Handle<Map>::cast(m.Value()));
              return result;
            }
          }
          // Without alias analysis we cannot tell whether this
          // StoreField[map] affects {receiver} or not.
          result = kUnreliableReceiverMaps;
        }
        break;
      }
      case IrOpcode::kJSStoreMessage:
      case IrOpcode::kJSStoreModule:
      case IrOpcode::kStoreElement:
      case IrOpcode::kStoreTypedElement: {
        // These never change the map of objects.
        break;
      }
      case IrOpcode::kFinishRegion: {
        // FinishRegion renames the output of allocations, so we need
        // to update the {receiver} that we are looking for, if the
        // {receiver} matches the current {effect}.
        if (IsSame(receiver, effect)) receiver = GetValueInput(effect, 0);
        break;
      }
      case IrOpcode::kEffectPhi: {
        Node* control = GetControlInput(effect);
        if (control->opcode() != IrOpcode::kLoop) {
          DCHECK(control->opcode() == IrOpcode::kDead ||
                 control->opcode() == IrOpcode::kMerge);
          return kNoReceiverMaps;
        }

        // Continue search for receiver map outside the loop. Since operations
        // inside the loop may change the map, the result is unreliable.
        effect = GetEffectInput(effect, 0);
        result = kUnreliableReceiverMaps;
        continue;
      }
      default: {
        DCHECK_EQ(1, effect->op()->EffectOutputCount());
        if (effect->op()->EffectInputCount() != 1) {
          // Didn't find any appropriate CheckMaps node.
          return kNoReceiverMaps;
        }
        if (!effect->op()->HasProperty(Operator::kNoWrite)) {
          // Without alias/escape analysis we cannot tell whether this
          // {effect} affects {receiver} or not.
          result = kUnreliableReceiverMaps;
        }
        break;
      }
    }

    // Stop walking the effect chain once we hit the definition of
    // the {receiver} along the {effect}s.
    if (IsSame(receiver, effect)) return kNoReceiverMaps;

    // Continue with the next {effect}.
    DCHECK_EQ(1, effect->op()->EffectInputCount());
    effect = NodeProperties::GetEffectInput(effect);
  }
}

// static
MaybeHandle<Map> NodeProperties::GetMapWitness(Node* node) {
  ZoneHandleSet<Map> maps;
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &maps);
  if (result == NodeProperties::kReliableReceiverMaps && maps.size() == 1) {
    return maps[0];
  }
  return MaybeHandle<Map>();
}

// static
bool NodeProperties::HasInstanceTypeWitness(Node* receiver, Node* effect,
                                            InstanceType instance_type) {
  ZoneHandleSet<Map> receiver_maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &receiver_maps);
  switch (result) {
    case NodeProperties::kUnreliableReceiverMaps:
    case NodeProperties::kReliableReceiverMaps:
      DCHECK_NE(0, receiver_maps.size());
      for (size_t i = 0; i < receiver_maps.size(); ++i) {
        if (receiver_maps[i]->instance_type() != instance_type) return false;
      }
      return true;

    case NodeProperties::kNoReceiverMaps:
      return false;
  }
  UNREACHABLE();
}

// static
bool NodeProperties::NoObservableSideEffectBetween(Node* effect,
                                                   Node* dominator) {
  while (effect != dominator) {
    if (effect->op()->EffectInputCount() == 1 &&
        effect->op()->properties() & Operator::kNoWrite) {
      effect = NodeProperties::GetEffectInput(effect);
    } else {
      return false;
    }
  }
  return true;
}

// static
bool NodeProperties::CanBePrimitive(Node* receiver, Node* effect) {
  switch (receiver->opcode()) {
#define CASE(Opcode) case IrOpcode::k##Opcode:
    JS_CONSTRUCT_OP_LIST(CASE)
    JS_CREATE_OP_LIST(CASE)
#undef CASE
    case IrOpcode::kCheckReceiver:
    case IrOpcode::kConvertReceiver:
    case IrOpcode::kJSGetSuperConstructor:
    case IrOpcode::kJSToObject:
      return false;
    case IrOpcode::kHeapConstant: {
      Handle<HeapObject> value = HeapObjectMatcher(receiver).Value();
      return value->IsPrimitive();
    }
    default: {
      // We don't really care about the exact maps here,
      // just the instance types, which don't change
      // across potential side-effecting operations.
      ZoneHandleSet<Map> maps;
      if (InferReceiverMaps(receiver, effect, &maps) != kNoReceiverMaps) {
        // Check if all {maps} are actually JSReceiver maps.
        for (size_t i = 0; i < maps.size(); ++i) {
          if (!maps[i]->IsJSReceiverMap()) return true;
        }
        return false;
      }
      return true;
    }
  }
}

// static
bool NodeProperties::CanBeNullOrUndefined(Node* receiver, Node* effect) {
  if (CanBePrimitive(receiver, effect)) {
    switch (receiver->opcode()) {
      case IrOpcode::kCheckInternalizedString:
      case IrOpcode::kCheckNumber:
      case IrOpcode::kCheckSmi:
      case IrOpcode::kCheckString:
      case IrOpcode::kCheckSymbol:
      case IrOpcode::kJSToInteger:
      case IrOpcode::kJSToLength:
      case IrOpcode::kJSToName:
      case IrOpcode::kJSToNumber:
      case IrOpcode::kJSToNumeric:
      case IrOpcode::kJSToString:
      case IrOpcode::kToBoolean:
        return false;
      case IrOpcode::kHeapConstant: {
        Handle<HeapObject> value = HeapObjectMatcher(receiver).Value();
        Isolate* const isolate = value->GetIsolate();
        return value->IsNullOrUndefined(isolate);
      }
      default:
        return true;
    }
  }
  return false;
}

// static
Node* NodeProperties::GetOuterContext(Node* node, size_t* depth) {
  Node* context = NodeProperties::GetContextInput(node);
  while (*depth > 0 &&
         IrOpcode::IsContextChainExtendingOpcode(context->opcode())) {
    context = NodeProperties::GetContextInput(context);
    (*depth)--;
  }
  return context;
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

// static
size_t NodeProperties::HashCode(Node* node) {
  size_t h = base::hash_combine(node->op()->HashCode(), node->InputCount());
  for (Node* input : node->inputs()) {
    h = base::hash_combine(h, input->id());
  }
  return h;
}

// static
bool NodeProperties::Equals(Node* a, Node* b) {
  DCHECK_NOT_NULL(a);
  DCHECK_NOT_NULL(b);
  DCHECK_NOT_NULL(a->op());
  DCHECK_NOT_NULL(b->op());
  if (!a->op()->Equals(b->op())) return false;
  if (a->InputCount() != b->InputCount()) return false;
  Node::Inputs aInputs = a->inputs();
  Node::Inputs bInputs = b->inputs();

  auto aIt = aInputs.begin();
  auto bIt = bInputs.begin();
  auto aEnd = aInputs.end();

  for (; aIt != aEnd; ++aIt, ++bIt) {
    DCHECK_NOT_NULL(*aIt);
    DCHECK_NOT_NULL(*bIt);
    if ((*aIt)->id() != (*bIt)->id()) return false;
  }
  return true;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
