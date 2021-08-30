// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-generic-lowering.h"

#include "src/ast/ast.h"
#include "src/builtins/builtins-constructor.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/processed-feedback.h"
#include "src/compiler/simplified-operator.h"
#include "src/objects/feedback-cell.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/scope-info.h"
#include "src/objects/template-objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

CallDescriptor::Flags FrameStateFlagForCall(Node* node) {
  return OperatorProperties::HasFrameStateInput(node->op())
             ? CallDescriptor::kNeedsFrameState
             : CallDescriptor::kNoFlags;
}

}  // namespace

JSGenericLowering::JSGenericLowering(JSGraph* jsgraph, Editor* editor,
                                     JSHeapBroker* broker)
    : AdvancedReducer(editor), jsgraph_(jsgraph), broker_(broker) {}

JSGenericLowering::~JSGenericLowering() = default;


Reduction JSGenericLowering::Reduce(Node* node) {
  switch (node->opcode()) {
#define DECLARE_CASE(x, ...) \
  case IrOpcode::k##x:       \
    Lower##x(node);          \
    break;
    JS_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE
    default:
      // Nothing to see.
      return NoChange();
  }
  return Changed(node);
}

#define REPLACE_STUB_CALL(Name)                       \
  void JSGenericLowering::LowerJS##Name(Node* node) { \
    ReplaceWithBuiltinCall(node, Builtin::k##Name);   \
  }
REPLACE_STUB_CALL(ToLength)
REPLACE_STUB_CALL(ToNumber)
REPLACE_STUB_CALL(ToNumberConvertBigInt)
REPLACE_STUB_CALL(ToNumeric)
REPLACE_STUB_CALL(ToName)
REPLACE_STUB_CALL(ToObject)
REPLACE_STUB_CALL(ToString)
REPLACE_STUB_CALL(ForInEnumerate)
REPLACE_STUB_CALL(AsyncFunctionEnter)
REPLACE_STUB_CALL(AsyncFunctionReject)
REPLACE_STUB_CALL(AsyncFunctionResolve)
REPLACE_STUB_CALL(FulfillPromise)
REPLACE_STUB_CALL(PerformPromiseThen)
REPLACE_STUB_CALL(PromiseResolve)
REPLACE_STUB_CALL(RejectPromise)
REPLACE_STUB_CALL(ResolvePromise)
#undef REPLACE_STUB_CALL

void JSGenericLowering::ReplaceWithBuiltinCall(Node* node, Builtin builtin) {
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  Callable callable = Builtins::CallableFor(isolate(), builtin);
  ReplaceWithBuiltinCall(node, callable, flags);
}

void JSGenericLowering::ReplaceWithBuiltinCall(Node* node, Callable callable,
                                               CallDescriptor::Flags flags) {
  ReplaceWithBuiltinCall(node, callable, flags, node->op()->properties());
}

void JSGenericLowering::ReplaceWithBuiltinCall(
    Node* node, Callable callable, CallDescriptor::Flags flags,
    Operator::Properties properties) {
  const CallInterfaceDescriptor& descriptor = callable.descriptor();
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      zone(), descriptor, descriptor.GetStackParameterCount(), flags,
      properties);
  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  node->InsertInput(zone(), 0, stub_code);
  NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
}

void JSGenericLowering::ReplaceWithRuntimeCall(Node* node,
                                               Runtime::FunctionId f,
                                               int nargs_override) {
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  Operator::Properties properties = node->op()->properties();
  const Runtime::Function* fun = Runtime::FunctionForId(f);
  int nargs = (nargs_override < 0) ? fun->nargs : nargs_override;
  auto call_descriptor =
      Linkage::GetRuntimeCallDescriptor(zone(), f, nargs, properties, flags);
  Node* ref = jsgraph()->ExternalConstant(ExternalReference::Create(f));
  Node* arity = jsgraph()->Int32Constant(nargs);
  node->InsertInput(zone(), 0, jsgraph()->CEntryStubConstant(fun->result_size));
  node->InsertInput(zone(), nargs + 1, ref);
  node->InsertInput(zone(), nargs + 2, arity);
  NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
}

void JSGenericLowering::ReplaceUnaryOpWithBuiltinCall(
    Node* node, Builtin builtin_without_feedback,
    Builtin builtin_with_feedback) {
  DCHECK(JSOperator::IsUnaryWithFeedback(node->opcode()));
  const FeedbackParameter& p = FeedbackParameterOf(node->op());
  if (CollectFeedbackInGenericLowering() && p.feedback().IsValid()) {
    Callable callable = Builtins::CallableFor(isolate(), builtin_with_feedback);
    Node* slot = jsgraph()->UintPtrConstant(p.feedback().slot.ToInt());
    const CallInterfaceDescriptor& descriptor = callable.descriptor();
    CallDescriptor::Flags flags = FrameStateFlagForCall(node);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), descriptor, descriptor.GetStackParameterCount(), flags,
        node->op()->properties());
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    STATIC_ASSERT(JSUnaryOpNode::ValueIndex() == 0);
    STATIC_ASSERT(JSUnaryOpNode::FeedbackVectorIndex() == 1);
    DCHECK_EQ(node->op()->ValueInputCount(), 2);
    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 2, slot);
    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  } else {
    node->RemoveInput(JSUnaryOpNode::FeedbackVectorIndex());
    ReplaceWithBuiltinCall(node, builtin_without_feedback);
  }
}

#define DEF_UNARY_LOWERING(Name)                                    \
  void JSGenericLowering::LowerJS##Name(Node* node) {               \
    ReplaceUnaryOpWithBuiltinCall(node, Builtin::k##Name,           \
                                  Builtin::k##Name##_WithFeedback); \
  }
DEF_UNARY_LOWERING(BitwiseNot)
DEF_UNARY_LOWERING(Decrement)
DEF_UNARY_LOWERING(Increment)
DEF_UNARY_LOWERING(Negate)
#undef DEF_UNARY_LOWERING

void JSGenericLowering::ReplaceBinaryOpWithBuiltinCall(
    Node* node, Builtin builtin_without_feedback,
    Builtin builtin_with_feedback) {
  DCHECK(JSOperator::IsBinaryWithFeedback(node->opcode()));
  Builtin builtin;
  const FeedbackParameter& p = FeedbackParameterOf(node->op());
  if (CollectFeedbackInGenericLowering() && p.feedback().IsValid()) {
    Node* slot = jsgraph()->UintPtrConstant(p.feedback().slot.ToInt());
    STATIC_ASSERT(JSBinaryOpNode::LeftIndex() == 0);
    STATIC_ASSERT(JSBinaryOpNode::RightIndex() == 1);
    STATIC_ASSERT(JSBinaryOpNode::FeedbackVectorIndex() == 2);
    DCHECK_EQ(node->op()->ValueInputCount(), 3);
    node->InsertInput(zone(), 2, slot);
    builtin = builtin_with_feedback;
  } else {
    node->RemoveInput(JSBinaryOpNode::FeedbackVectorIndex());
    builtin = builtin_without_feedback;
  }

  ReplaceWithBuiltinCall(node, builtin);
}

#define DEF_BINARY_LOWERING(Name)                                    \
  void JSGenericLowering::LowerJS##Name(Node* node) {                \
    ReplaceBinaryOpWithBuiltinCall(node, Builtin::k##Name,           \
                                   Builtin::k##Name##_WithFeedback); \
  }
// Binary ops.
DEF_BINARY_LOWERING(Add)
DEF_BINARY_LOWERING(BitwiseAnd)
DEF_BINARY_LOWERING(BitwiseOr)
DEF_BINARY_LOWERING(BitwiseXor)
DEF_BINARY_LOWERING(Divide)
DEF_BINARY_LOWERING(Exponentiate)
DEF_BINARY_LOWERING(Modulus)
DEF_BINARY_LOWERING(Multiply)
DEF_BINARY_LOWERING(ShiftLeft)
DEF_BINARY_LOWERING(ShiftRight)
DEF_BINARY_LOWERING(ShiftRightLogical)
DEF_BINARY_LOWERING(Subtract)
// Compare ops.
DEF_BINARY_LOWERING(Equal)
DEF_BINARY_LOWERING(GreaterThan)
DEF_BINARY_LOWERING(GreaterThanOrEqual)
DEF_BINARY_LOWERING(InstanceOf)
DEF_BINARY_LOWERING(LessThan)
DEF_BINARY_LOWERING(LessThanOrEqual)
#undef DEF_BINARY_LOWERING

void JSGenericLowering::LowerJSStrictEqual(Node* node) {
  // The === operator doesn't need the current context.
  NodeProperties::ReplaceContextInput(node, jsgraph()->NoContextConstant());
  DCHECK_EQ(node->op()->ControlInputCount(), 1);
  node->RemoveInput(NodeProperties::FirstControlIndex(node));

  Builtin builtin;
  const FeedbackParameter& p = FeedbackParameterOf(node->op());
  if (CollectFeedbackInGenericLowering() && p.feedback().IsValid()) {
    Node* slot = jsgraph()->UintPtrConstant(p.feedback().slot.ToInt());
    STATIC_ASSERT(JSStrictEqualNode::LeftIndex() == 0);
    STATIC_ASSERT(JSStrictEqualNode::RightIndex() == 1);
    STATIC_ASSERT(JSStrictEqualNode::FeedbackVectorIndex() == 2);
    DCHECK_EQ(node->op()->ValueInputCount(), 3);
    node->InsertInput(zone(), 2, slot);
    builtin = Builtin::kStrictEqual_WithFeedback;
  } else {
    node->RemoveInput(JSStrictEqualNode::FeedbackVectorIndex());
    builtin = Builtin::kStrictEqual;
  }

  Callable callable = Builtins::CallableFor(isolate(), builtin);
  ReplaceWithBuiltinCall(node, callable, CallDescriptor::kNoFlags,
                         Operator::kEliminatable);
}

namespace {

// The megamorphic load builtin can be used as a performance optimization in
// some cases - unlike the full builtin, the megamorphic builtin does fewer
// checks and does not collect feedback.
bool ShouldUseMegamorphicLoadBuiltin(FeedbackSource const& source,
                                     JSHeapBroker* broker) {
  if (broker->is_native_context_independent()) {
    // The decision to use the megamorphic load builtin is made based on
    // current feedback, and is thus context-dependent. It cannot be used when
    // generating NCI code.
    return false;
  }

  ProcessedFeedback const& feedback = broker->GetFeedback(source);

  if (feedback.kind() == ProcessedFeedback::kElementAccess) {
    return feedback.AsElementAccess().transition_groups().empty();
  } else if (feedback.kind() == ProcessedFeedback::kNamedAccess) {
    return feedback.AsNamedAccess().maps().empty();
  } else if (feedback.kind() == ProcessedFeedback::kInsufficient) {
    return false;
  } else if (feedback.kind() == ProcessedFeedback::kMinimorphicPropertyAccess) {
    // MinimorphicPropertyAccess is used for dynamic map checks and the IC state
    // is either monomorphic or polymorphic. So it will still benefit from
    // collecting feedback, so don't use megamorphic builtin.
    return false;
  }
  UNREACHABLE();
}
}  // namespace

void JSGenericLowering::LowerJSHasProperty(Node* node) {
  JSHasPropertyNode n(node);
  const PropertyAccess& p = n.Parameters();
  if (!p.feedback().IsValid()) {
    node->RemoveInput(JSHasPropertyNode::FeedbackVectorIndex());
    ReplaceWithBuiltinCall(node, Builtin::kHasProperty);
  } else {
    STATIC_ASSERT(n.FeedbackVectorIndex() == 2);
    n->InsertInput(zone(), 2,
                   jsgraph()->TaggedIndexConstant(p.feedback().index()));
    ReplaceWithBuiltinCall(node, Builtin::kKeyedHasIC);
  }
}

void JSGenericLowering::LowerJSLoadProperty(Node* node) {
  JSLoadPropertyNode n(node);
  const PropertyAccess& p = n.Parameters();
  FrameState frame_state = n.frame_state();
  Node* outer_state = frame_state.outer_frame_state();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 2);
  if (outer_state->opcode() != IrOpcode::kFrameState) {
    n->RemoveInput(n.FeedbackVectorIndex());
    n->InsertInput(zone(), 2,
                   jsgraph()->TaggedIndexConstant(p.feedback().index()));
    ReplaceWithBuiltinCall(
        node, ShouldUseMegamorphicLoadBuiltin(p.feedback(), broker())
                  ? Builtin::kKeyedLoadICTrampoline_Megamorphic
                  : Builtin::kKeyedLoadICTrampoline);
  } else {
    n->InsertInput(zone(), 2,
                   jsgraph()->TaggedIndexConstant(p.feedback().index()));
    ReplaceWithBuiltinCall(
        node, ShouldUseMegamorphicLoadBuiltin(p.feedback(), broker())
                  ? Builtin::kKeyedLoadIC_Megamorphic
                  : Builtin::kKeyedLoadIC);
  }
}

void JSGenericLowering::LowerJSLoadNamed(Node* node) {
  JSLoadNamedNode n(node);
  NamedAccess const& p = n.Parameters();
  FrameState frame_state = n.frame_state();
  Node* outer_state = frame_state.outer_frame_state();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 1);
  if (!p.feedback().IsValid()) {
    n->RemoveInput(n.FeedbackVectorIndex());
    node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
    ReplaceWithBuiltinCall(node, Builtin::kGetProperty);
  } else if (outer_state->opcode() != IrOpcode::kFrameState) {
    n->RemoveInput(n.FeedbackVectorIndex());
    node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
    node->InsertInput(zone(), 2,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    ReplaceWithBuiltinCall(
        node, ShouldUseMegamorphicLoadBuiltin(p.feedback(), broker())
                  ? Builtin::kLoadICTrampoline_Megamorphic
                  : Builtin::kLoadICTrampoline);
  } else {
    node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
    node->InsertInput(zone(), 2,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    ReplaceWithBuiltinCall(
        node, ShouldUseMegamorphicLoadBuiltin(p.feedback(), broker())
                  ? Builtin::kLoadIC_Megamorphic
                  : Builtin::kLoadIC);
  }
}

void JSGenericLowering::LowerJSLoadNamedFromSuper(Node* node) {
  JSLoadNamedFromSuperNode n(node);
  NamedAccess const& p = n.Parameters();
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  // Node inputs: receiver, home object, FeedbackVector.
  // LoadSuperIC expects: receiver, lookup start object, name, slot,
  // FeedbackVector.
  Node* home_object_map = effect = graph()->NewNode(
      jsgraph()->simplified()->LoadField(AccessBuilder::ForMap()),
      n.home_object(), effect, control);
  Node* home_object_proto = effect = graph()->NewNode(
      jsgraph()->simplified()->LoadField(AccessBuilder::ForMapPrototype()),
      home_object_map, effect, control);
  n->ReplaceInput(n.HomeObjectIndex(), home_object_proto);
  NodeProperties::ReplaceEffectInput(node, effect);
  STATIC_ASSERT(n.FeedbackVectorIndex() == 2);
  // If the code below will be used for the invalid feedback case, it needs to
  // be double-checked that the FeedbackVector parameter will be the
  // UndefinedConstant.
  DCHECK(p.feedback().IsValid());
  node->InsertInput(zone(), 2, jsgraph()->HeapConstant(p.name()));
  node->InsertInput(zone(), 3,
                    jsgraph()->TaggedIndexConstant(p.feedback().index()));
  ReplaceWithBuiltinCall(node, Builtin::kLoadSuperIC);
}

void JSGenericLowering::LowerJSLoadGlobal(Node* node) {
  JSLoadGlobalNode n(node);
  const LoadGlobalParameters& p = n.Parameters();
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  FrameState frame_state = n.frame_state();
  Node* outer_state = frame_state.outer_frame_state();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 0);
  if (outer_state->opcode() != IrOpcode::kFrameState) {
    n->RemoveInput(n.FeedbackVectorIndex());
    node->InsertInput(zone(), 0, jsgraph()->HeapConstant(p.name()));
    node->InsertInput(zone(), 1,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    Callable callable = CodeFactory::LoadGlobalIC(isolate(), p.typeof_mode());
    ReplaceWithBuiltinCall(node, callable, flags);
  } else {
    node->InsertInput(zone(), 0, jsgraph()->HeapConstant(p.name()));
    node->InsertInput(zone(), 1,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    Callable callable =
        CodeFactory::LoadGlobalICInOptimizedCode(isolate(), p.typeof_mode());
    ReplaceWithBuiltinCall(node, callable, flags);
  }
}

void JSGenericLowering::LowerJSGetIterator(Node* node) {
  // TODO(v8:9625): Currently, the GetIterator operator is desugared in the
  // native context specialization phase. Thus, the following generic lowering
  // is not reachable unless that phase is disabled (e.g. for
  // native-context-independent code).
  // We can add a check in native context specialization to avoid desugaring
  // the GetIterator operator when feedback is megamorphic. This would reduce
  // the size of the compiled code as it would insert 1 call to the builtin
  // instead of 2 calls resulting from the generic lowering of the LoadNamed
  // and Call operators.

  JSGetIteratorNode n(node);
  GetIteratorParameters const& p = n.Parameters();
  Node* load_slot =
      jsgraph()->TaggedIndexConstant(p.loadFeedback().slot.ToInt());
  Node* call_slot =
      jsgraph()->TaggedIndexConstant(p.callFeedback().slot.ToInt());
  STATIC_ASSERT(n.FeedbackVectorIndex() == 1);
  node->InsertInput(zone(), 1, load_slot);
  node->InsertInput(zone(), 2, call_slot);

  ReplaceWithBuiltinCall(node, Builtin::kGetIteratorWithFeedback);
}

void JSGenericLowering::LowerJSStoreProperty(Node* node) {
  JSStorePropertyNode n(node);
  const PropertyAccess& p = n.Parameters();
  FrameState frame_state = n.frame_state();
  Node* outer_state = frame_state.outer_frame_state();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 3);
  if (outer_state->opcode() != IrOpcode::kFrameState) {
    n->RemoveInput(n.FeedbackVectorIndex());
    node->InsertInput(zone(), 3,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    ReplaceWithBuiltinCall(node, Builtin::kKeyedStoreICTrampoline);
  } else {
    node->InsertInput(zone(), 3,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    ReplaceWithBuiltinCall(node, Builtin::kKeyedStoreIC);
  }
}

void JSGenericLowering::LowerJSStoreNamed(Node* node) {
  JSStoreNamedNode n(node);
  NamedAccess const& p = n.Parameters();
  FrameState frame_state = n.frame_state();
  Node* outer_state = frame_state.outer_frame_state();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 2);
  if (!p.feedback().IsValid()) {
    n->RemoveInput(n.FeedbackVectorIndex());
    node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
    ReplaceWithRuntimeCall(node, Runtime::kSetNamedProperty);
  } else if (outer_state->opcode() != IrOpcode::kFrameState) {
    n->RemoveInput(n.FeedbackVectorIndex());
    node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
    node->InsertInput(zone(), 3,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    ReplaceWithBuiltinCall(node, Builtin::kStoreICTrampoline);
  } else {
    node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
    node->InsertInput(zone(), 3,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    ReplaceWithBuiltinCall(node, Builtin::kStoreIC);
  }
}

void JSGenericLowering::LowerJSStoreNamedOwn(Node* node) {
  JSStoreNamedOwnNode n(node);
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  StoreNamedOwnParameters const& p = n.Parameters();
  FrameState frame_state = n.frame_state();
  Node* outer_state = frame_state.outer_frame_state();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 2);
  if (outer_state->opcode() != IrOpcode::kFrameState) {
    n->RemoveInput(n.FeedbackVectorIndex());
    node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
    node->InsertInput(zone(), 3,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    Callable callable = CodeFactory::StoreOwnIC(isolate());
    ReplaceWithBuiltinCall(node, callable, flags);
  } else {
    node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
    node->InsertInput(zone(), 3,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    Callable callable = CodeFactory::StoreOwnICInOptimizedCode(isolate());
    ReplaceWithBuiltinCall(node, callable, flags);
  }
}

void JSGenericLowering::LowerJSStoreGlobal(Node* node) {
  JSStoreGlobalNode n(node);
  const StoreGlobalParameters& p = n.Parameters();
  FrameState frame_state = n.frame_state();
  Node* outer_state = frame_state.outer_frame_state();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 1);
  if (outer_state->opcode() != IrOpcode::kFrameState) {
    n->RemoveInput(n.FeedbackVectorIndex());
    node->InsertInput(zone(), 0, jsgraph()->HeapConstant(p.name()));
    node->InsertInput(zone(), 2,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    ReplaceWithBuiltinCall(node, Builtin::kStoreGlobalICTrampoline);
  } else {
    node->InsertInput(zone(), 0, jsgraph()->HeapConstant(p.name()));
    node->InsertInput(zone(), 2,
                      jsgraph()->TaggedIndexConstant(p.feedback().index()));
    ReplaceWithBuiltinCall(node, Builtin::kStoreGlobalIC);
  }
}

void JSGenericLowering::LowerJSStoreDataPropertyInLiteral(Node* node) {
  JSStoreDataPropertyInLiteralNode n(node);
  FeedbackParameter const& p = n.Parameters();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 4);
  RelaxControls(node);
  node->InsertInput(zone(), 5,
                    jsgraph()->TaggedIndexConstant(p.feedback().index()));
  ReplaceWithRuntimeCall(node, Runtime::kDefineDataPropertyInLiteral);
}

void JSGenericLowering::LowerJSStoreInArrayLiteral(Node* node) {
  JSStoreInArrayLiteralNode n(node);
  FeedbackParameter const& p = n.Parameters();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 3);
  RelaxControls(node);
  node->InsertInput(zone(), 3,
                    jsgraph()->TaggedIndexConstant(p.feedback().index()));
  ReplaceWithBuiltinCall(node, Builtin::kStoreInArrayLiteralIC);
}

void JSGenericLowering::LowerJSDeleteProperty(Node* node) {
  ReplaceWithBuiltinCall(node, Builtin::kDeleteProperty);
}

void JSGenericLowering::LowerJSGetSuperConstructor(Node* node) {
  Node* active_function = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* function_map = effect = graph()->NewNode(
      jsgraph()->simplified()->LoadField(AccessBuilder::ForMap()),
      active_function, effect, control);

  RelaxControls(node);
  node->ReplaceInput(0, function_map);
  node->ReplaceInput(1, effect);
  node->ReplaceInput(2, control);
  node->TrimInputCount(3);
  NodeProperties::ChangeOp(node, jsgraph()->simplified()->LoadField(
                                     AccessBuilder::ForMapPrototype()));
}

void JSGenericLowering::LowerJSHasInPrototypeChain(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kHasInPrototypeChain);
}

void JSGenericLowering::LowerJSOrdinaryHasInstance(Node* node) {
  ReplaceWithBuiltinCall(node, Builtin::kOrdinaryHasInstance);
}

void JSGenericLowering::LowerJSHasContextExtension(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSLoadContext(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}


void JSGenericLowering::LowerJSStoreContext(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}


void JSGenericLowering::LowerJSCreate(Node* node) {
  ReplaceWithBuiltinCall(node, Builtin::kFastNewObject);
}


void JSGenericLowering::LowerJSCreateArguments(Node* node) {
  CreateArgumentsType const type = CreateArgumentsTypeOf(node->op());
  switch (type) {
    case CreateArgumentsType::kMappedArguments:
      ReplaceWithRuntimeCall(node, Runtime::kNewSloppyArguments);
      break;
    case CreateArgumentsType::kUnmappedArguments:
      ReplaceWithRuntimeCall(node, Runtime::kNewStrictArguments);
      break;
    case CreateArgumentsType::kRestParameter:
      ReplaceWithRuntimeCall(node, Runtime::kNewRestParameter);
      break;
  }
}


void JSGenericLowering::LowerJSCreateArray(Node* node) {
  CreateArrayParameters const& p = CreateArrayParametersOf(node->op());
  int const arity = static_cast<int>(p.arity());
  auto interface_descriptor = ArrayConstructorDescriptor{};
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      zone(), interface_descriptor, arity + 1, CallDescriptor::kNeedsFrameState,
      node->op()->properties());
  // If this fails, we might need to update the parameter reordering code
  // to ensure that the additional arguments passed via stack are pushed
  // between top of stack and JS arguments.
  DCHECK_EQ(interface_descriptor.GetStackParameterCount(), 0);
  Node* stub_code = jsgraph()->ArrayConstructorStubConstant();
  Node* stub_arity = jsgraph()->Int32Constant(arity);
  MaybeHandle<AllocationSite> const maybe_site = p.site();
  Handle<AllocationSite> site;
  DCHECK_IMPLIES(broker()->is_native_context_independent(),
                 maybe_site.is_null());
  Node* type_info = maybe_site.ToHandle(&site) ? jsgraph()->HeapConstant(site)
                                               : jsgraph()->UndefinedConstant();
  Node* receiver = jsgraph()->UndefinedConstant();
  node->InsertInput(zone(), 0, stub_code);
  node->InsertInput(zone(), 3, stub_arity);
  node->InsertInput(zone(), 4, type_info);
  node->InsertInput(zone(), 5, receiver);
  NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
}

void JSGenericLowering::LowerJSCreateArrayIterator(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSCreateAsyncFunctionObject(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSCreateCollectionIterator(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSCreateBoundFunction(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSObjectIsArray(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSCreateObject(Node* node) {
  ReplaceWithBuiltinCall(node, Builtin::kCreateObjectWithoutProperties);
}

void JSGenericLowering::LowerJSParseInt(Node* node) {
  ReplaceWithBuiltinCall(node, Builtin::kParseInt);
}

void JSGenericLowering::LowerJSRegExpTest(Node* node) {
  ReplaceWithBuiltinCall(node, Builtin::kRegExpPrototypeTestFast);
}

void JSGenericLowering::LowerJSCreateClosure(Node* node) {
  JSCreateClosureNode n(node);
  CreateClosureParameters const& p = n.Parameters();
  Handle<SharedFunctionInfo> const shared_info = p.shared_info();
  STATIC_ASSERT(n.FeedbackCellIndex() == 0);
  node->InsertInput(zone(), 0, jsgraph()->HeapConstant(shared_info));
  node->RemoveInput(4);  // control

  // Use the FastNewClosure builtin only for functions allocated in new space.
  if (p.allocation() == AllocationType::kYoung) {
    ReplaceWithBuiltinCall(node, Builtin::kFastNewClosure);
  } else {
    ReplaceWithRuntimeCall(node, Runtime::kNewClosure_Tenured);
  }
}

void JSGenericLowering::LowerJSCreateFunctionContext(Node* node) {
  const CreateFunctionContextParameters& parameters =
      CreateFunctionContextParametersOf(node->op());
  Handle<ScopeInfo> scope_info = parameters.scope_info();
  int slot_count = parameters.slot_count();
  ScopeType scope_type = parameters.scope_type();
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);

  if (slot_count <= ConstructorBuiltins::MaximumFunctionContextSlots()) {
    Callable callable =
        CodeFactory::FastNewFunctionContext(isolate(), scope_type);
    node->InsertInput(zone(), 0, jsgraph()->HeapConstant(scope_info));
    node->InsertInput(zone(), 1, jsgraph()->Int32Constant(slot_count));
    ReplaceWithBuiltinCall(node, callable, flags);
  } else {
    node->InsertInput(zone(), 0, jsgraph()->HeapConstant(scope_info));
    ReplaceWithRuntimeCall(node, Runtime::kNewFunctionContext);
  }
}

void JSGenericLowering::LowerJSCreateGeneratorObject(Node* node) {
  node->RemoveInput(4);  // control
  ReplaceWithBuiltinCall(node, Builtin::kCreateGeneratorObject);
}

void JSGenericLowering::LowerJSCreateIterResultObject(Node* node) {
  ReplaceWithBuiltinCall(node, Builtin::kCreateIterResultObject);
}

void JSGenericLowering::LowerJSCreateStringIterator(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSCreateKeyValueArray(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSCreatePromise(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSCreateTypedArray(Node* node) {
  ReplaceWithBuiltinCall(node, Builtin::kCreateTypedArray);
}

void JSGenericLowering::LowerJSCreateLiteralArray(Node* node) {
  JSCreateLiteralArrayNode n(node);
  CreateLiteralParameters const& p = n.Parameters();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 0);
  node->InsertInput(zone(), 1,
                    jsgraph()->TaggedIndexConstant(p.feedback().index()));
  node->InsertInput(zone(), 2, jsgraph()->HeapConstant(p.constant()));
  node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.flags()));

  // Use the CreateShallowArrayLiteral builtin only for shallow boilerplates
  // without properties up to the number of elements that the stubs can handle.
  if ((p.flags() & AggregateLiteral::kIsShallow) != 0 &&
      p.length() < ConstructorBuiltins::kMaximumClonedShallowArrayElements) {
    ReplaceWithBuiltinCall(node, Builtin::kCreateShallowArrayLiteral);
  } else {
    ReplaceWithRuntimeCall(node, Runtime::kCreateArrayLiteral);
  }
}

void JSGenericLowering::LowerJSGetTemplateObject(Node* node) {
  JSGetTemplateObjectNode n(node);
  GetTemplateObjectParameters const& p = n.Parameters();
  SharedFunctionInfoRef shared = MakeRef(broker(), p.shared());
  TemplateObjectDescriptionRef description = MakeRef(broker(), p.description());

  DCHECK_EQ(node->op()->ControlInputCount(), 1);
  node->RemoveInput(NodeProperties::FirstControlIndex(node));

  STATIC_ASSERT(JSGetTemplateObjectNode::FeedbackVectorIndex() == 0);
  node->InsertInput(zone(), 0, jsgraph()->Constant(shared));
  node->InsertInput(zone(), 1, jsgraph()->Constant(description));
  node->InsertInput(zone(), 2,
                    jsgraph()->UintPtrConstant(p.feedback().index()));

  ReplaceWithBuiltinCall(node, Builtin::kGetTemplateObject);
}

void JSGenericLowering::LowerJSCreateEmptyLiteralArray(Node* node) {
  JSCreateEmptyLiteralArrayNode n(node);
  FeedbackParameter const& p = n.Parameters();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 0);
  node->InsertInput(zone(), 1,
                    jsgraph()->TaggedIndexConstant(p.feedback().index()));
  node->RemoveInput(4);  // control
  ReplaceWithBuiltinCall(node, Builtin::kCreateEmptyArrayLiteral);
}

void JSGenericLowering::LowerJSCreateArrayFromIterable(Node* node) {
  ReplaceWithBuiltinCall(node, Builtin::kIterableToListWithSymbolLookup);
}

void JSGenericLowering::LowerJSCreateLiteralObject(Node* node) {
  JSCreateLiteralObjectNode n(node);
  CreateLiteralParameters const& p = n.Parameters();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 0);
  node->InsertInput(zone(), 1,
                    jsgraph()->TaggedIndexConstant(p.feedback().index()));
  node->InsertInput(zone(), 2, jsgraph()->HeapConstant(p.constant()));
  node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.flags()));

  // Use the CreateShallowObjectLiteratal builtin only for shallow boilerplates
  // without elements up to the number of properties that the stubs can handle.
  if ((p.flags() & AggregateLiteral::kIsShallow) != 0 &&
      p.length() <=
          ConstructorBuiltins::kMaximumClonedShallowObjectProperties) {
    ReplaceWithBuiltinCall(node, Builtin::kCreateShallowObjectLiteral);
  } else {
    ReplaceWithRuntimeCall(node, Runtime::kCreateObjectLiteral);
  }
}

void JSGenericLowering::LowerJSCloneObject(Node* node) {
  JSCloneObjectNode n(node);
  CloneObjectParameters const& p = n.Parameters();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 1);
  node->InsertInput(zone(), 1, jsgraph()->SmiConstant(p.flags()));
  node->InsertInput(zone(), 2,
                    jsgraph()->TaggedIndexConstant(p.feedback().index()));
  ReplaceWithBuiltinCall(node, Builtin::kCloneObjectIC);
}

void JSGenericLowering::LowerJSCreateEmptyLiteralObject(Node* node) {
  ReplaceWithBuiltinCall(node, Builtin::kCreateEmptyLiteralObject);
}

void JSGenericLowering::LowerJSCreateLiteralRegExp(Node* node) {
  JSCreateLiteralRegExpNode n(node);
  CreateLiteralParameters const& p = n.Parameters();
  STATIC_ASSERT(n.FeedbackVectorIndex() == 0);
  node->InsertInput(zone(), 1,
                    jsgraph()->TaggedIndexConstant(p.feedback().index()));
  node->InsertInput(zone(), 2, jsgraph()->HeapConstant(p.constant()));
  node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.flags()));
  ReplaceWithBuiltinCall(node, Builtin::kCreateRegExpLiteral);
}


void JSGenericLowering::LowerJSCreateCatchContext(Node* node) {
  Handle<ScopeInfo> scope_info = ScopeInfoOf(node->op());
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(scope_info));
  ReplaceWithRuntimeCall(node, Runtime::kPushCatchContext);
}

void JSGenericLowering::LowerJSCreateWithContext(Node* node) {
  Handle<ScopeInfo> scope_info = ScopeInfoOf(node->op());
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(scope_info));
  ReplaceWithRuntimeCall(node, Runtime::kPushWithContext);
}

void JSGenericLowering::LowerJSCreateBlockContext(Node* node) {
  Handle<ScopeInfo> scope_info = ScopeInfoOf(node->op());
  node->InsertInput(zone(), 0, jsgraph()->HeapConstant(scope_info));
  ReplaceWithRuntimeCall(node, Runtime::kPushBlockContext);
}

namespace {

bool CollectCallAndConstructFeedback(JSHeapBroker* broker) {
  // Call and construct feedback is a special case. Besides shape feedback, we
  // also increment the call count, which is later used to make inlining
  // decisions.  The call count is only comparable/reliable if it is incremented
  // for all calls inside a function. This is not the case in default turbofan
  // mode, in which many calls may be inlined and will thus never reach generic
  // lowering (where we insert the feedback-collecting builtin call).
  // Therefore it should only be collected in native context independent code,
  // where we 1. know every call will reach generic lowering, and 2. we must
  // collect full feedback to properly tier up later.
  return broker->is_native_context_independent();
}

}  // namespace

// TODO(jgruber,v8:8888): Should this collect feedback?
void JSGenericLowering::LowerJSConstructForwardVarargs(Node* node) {
  ConstructForwardVarargsParameters p =
      ConstructForwardVarargsParametersOf(node->op());
  int const arg_count = static_cast<int>(p.arity() - 2);
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  Callable callable = CodeFactory::ConstructForwardVarargs(isolate());
  // If this fails, we might need to update the parameter reordering code
  // to ensure that the additional arguments passed via stack are pushed
  // between top of stack and JS arguments.
  DCHECK_EQ(callable.descriptor().GetStackParameterCount(), 0);
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      zone(), callable.descriptor(), arg_count + 1, flags);
  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  Node* stub_arity = jsgraph()->Int32Constant(arg_count);
  Node* start_index = jsgraph()->Uint32Constant(p.start_index());
  Node* receiver = jsgraph()->UndefinedConstant();
  node->InsertInput(zone(), 0, stub_code);
  node->InsertInput(zone(), 3, stub_arity);
  node->InsertInput(zone(), 4, start_index);
  node->InsertInput(zone(), 5, receiver);
  NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
}

void JSGenericLowering::LowerJSConstruct(Node* node) {
  JSConstructNode n(node);
  ConstructParameters const& p = n.Parameters();
  int const arg_count = p.arity_without_implicit_args();
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);

  static constexpr int kReceiver = 1;
  static constexpr int kMaybeFeedbackVector = 1;

  if (CollectFeedbackInGenericLowering() &&
      CollectCallAndConstructFeedback(broker()) && p.feedback().IsValid()) {
    const int stack_argument_count =
        arg_count + kReceiver + kMaybeFeedbackVector;
    Callable callable =
        Builtins::CallableFor(isolate(), Builtin::kConstruct_WithFeedback);
    // If this fails, we might need to update the parameter reordering code
    // to ensure that the additional arguments passed via stack are pushed
    // between top of stack and JS arguments.
    DCHECK_EQ(callable.descriptor().GetStackParameterCount(),
              kMaybeFeedbackVector);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), stack_argument_count, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* stub_arity = jsgraph()->Int32Constant(arg_count);
    Node* slot = jsgraph()->UintPtrConstant(p.feedback().index());
    Node* receiver = jsgraph()->UndefinedConstant();
    Node* feedback_vector = node->RemoveInput(n.FeedbackVectorIndex());
    // Register argument inputs are followed by stack argument inputs (such as
    // feedback_vector). Both are listed in ascending order. Note that
    // the receiver is implicitly placed on the stack and is thus inserted
    // between explicitly-specified register and stack arguments.
    // TODO(jgruber): Implement a simpler way to specify these mutations.
    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 3, stub_arity);
    node->InsertInput(zone(), 4, slot);
    node->InsertInput(zone(), 5, feedback_vector);
    node->InsertInput(zone(), 6, receiver);
    // After: {code, target, new_target, arity, slot, vector, receiver,
    // ...args}.

    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  } else {
    const int stack_argument_count = arg_count + kReceiver;
    Callable callable = Builtins::CallableFor(isolate(), Builtin::kConstruct);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), stack_argument_count, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* stub_arity = jsgraph()->Int32Constant(arg_count);
    Node* receiver = jsgraph()->UndefinedConstant();
    node->RemoveInput(n.FeedbackVectorIndex());
    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 3, stub_arity);
    node->InsertInput(zone(), 4, receiver);

    // After: {code, target, new_target, arity, receiver, ...args}.

    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  }
}

void JSGenericLowering::LowerJSConstructWithArrayLike(Node* node) {
  JSConstructWithArrayLikeNode n(node);
  ConstructParameters const& p = n.Parameters();
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  const int arg_count = p.arity_without_implicit_args();
  DCHECK_EQ(arg_count, 1);

  static constexpr int kReceiver = 1;
  static constexpr int kArgumentList = 1;
  static constexpr int kMaybeFeedbackVector = 1;

  if (CollectFeedbackInGenericLowering() &&
      CollectCallAndConstructFeedback(broker()) && p.feedback().IsValid()) {
    const int stack_argument_count =
        arg_count - kArgumentList + kReceiver + kMaybeFeedbackVector;
    Callable callable = Builtins::CallableFor(
        isolate(), Builtin::kConstructWithArrayLike_WithFeedback);
    // If this fails, we might need to update the parameter reordering code
    // to ensure that the additional arguments passed via stack are pushed
    // between top of stack and JS arguments.
    DCHECK_EQ(callable.descriptor().GetStackParameterCount(),
              kMaybeFeedbackVector);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), stack_argument_count, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* receiver = jsgraph()->UndefinedConstant();
    Node* slot = jsgraph()->UintPtrConstant(p.feedback().index());
    Node* feedback_vector = node->RemoveInput(n.FeedbackVectorIndex());
    // Register argument inputs are followed by stack argument inputs (such as
    // feedback_vector). Both are listed in ascending order. Note that
    // the receiver is implicitly placed on the stack and is thus inserted
    // between explicitly-specified register and stack arguments.
    // TODO(jgruber): Implement a simpler way to specify these mutations.
    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 4, slot);
    node->InsertInput(zone(), 5, feedback_vector);
    node->InsertInput(zone(), 6, receiver);
    // After: {code, target, new_target, arguments_list, slot, vector,
    // receiver}.

    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  } else {
    const int stack_argument_count = arg_count - kArgumentList + kReceiver;
    Callable callable =
        Builtins::CallableFor(isolate(), Builtin::kConstructWithArrayLike);
    // If this fails, we might need to update the parameter reordering code
    // to ensure that the additional arguments passed via stack are pushed
    // between top of stack and JS arguments.
    DCHECK_EQ(callable.descriptor().GetStackParameterCount(), 0);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), stack_argument_count, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* receiver = jsgraph()->UndefinedConstant();
    node->RemoveInput(n.FeedbackVectorIndex());
    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 4, receiver);

    // After: {code, target, new_target, arguments_list, receiver}.

    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  }
}

void JSGenericLowering::LowerJSConstructWithSpread(Node* node) {
  JSConstructWithSpreadNode n(node);
  ConstructParameters const& p = n.Parameters();
  int const arg_count = p.arity_without_implicit_args();
  DCHECK_GE(arg_count, 1);
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);

  static constexpr int kReceiver = 1;
  static constexpr int kTheSpread = 1;  // Included in `arg_count`.
  static constexpr int kMaybeFeedbackVector = 1;

  if (CollectFeedbackInGenericLowering() &&
      CollectCallAndConstructFeedback(broker()) && p.feedback().IsValid()) {
    const int stack_argument_count =
        arg_count + kReceiver + kMaybeFeedbackVector;
    Callable callable = Builtins::CallableFor(
        isolate(), Builtin::kConstructWithSpread_WithFeedback);
    // If this fails, we might need to update the parameter reordering code
    // to ensure that the additional arguments passed via stack are pushed
    // between top of stack and JS arguments.
    DCHECK_EQ(callable.descriptor().GetStackParameterCount(),
              kTheSpread + kMaybeFeedbackVector);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), stack_argument_count, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* slot = jsgraph()->UintPtrConstant(p.feedback().index());

    // The single available register is needed for `slot`, thus `spread` remains
    // on the stack here.
    Node* stub_arity = jsgraph()->Int32Constant(arg_count - kTheSpread);
    Node* receiver = jsgraph()->UndefinedConstant();
    Node* feedback_vector = node->RemoveInput(n.FeedbackVectorIndex());
    Node* spread = node->RemoveInput(n.LastArgumentIndex());

    // Register argument inputs are followed by stack argument inputs (such as
    // feedback_vector). Both are listed in ascending order. Note that
    // the receiver is implicitly placed on the stack and is thus inserted
    // between explicitly-specified register and stack arguments.
    // TODO(jgruber): Implement a simpler way to specify these mutations.
    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 3, stub_arity);
    node->InsertInput(zone(), 4, slot);
    // Arguments in the stack should be inserted in reversed order, ie, the last
    // arguments defined in the interface descriptor should be inserted first.
    DCHECK_EQ(callable.descriptor().GetStackArgumentOrder(),
              StackArgumentOrder::kJS);
    node->InsertInput(zone(), 5, feedback_vector);
    node->InsertInput(zone(), 6, spread);
    node->InsertInput(zone(), 7, receiver);
    // After: {code, target, new_target, arity, slot, vector, spread, receiver,
    // ...args}.

    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  } else {
    const int stack_argument_count = arg_count + kReceiver - kTheSpread;
    Callable callable = CodeFactory::ConstructWithSpread(isolate());
    // If this fails, we might need to update the parameter reordering code
    // to ensure that the additional arguments passed via stack are pushed
    // between top of stack and JS arguments.
    DCHECK_EQ(callable.descriptor().GetStackParameterCount(), 0);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), stack_argument_count, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());

    // We pass the spread in a register, not on the stack.
    Node* stub_arity = jsgraph()->Int32Constant(arg_count - kTheSpread);
    Node* receiver = jsgraph()->UndefinedConstant();
    DCHECK(n.FeedbackVectorIndex() > n.LastArgumentIndex());
    node->RemoveInput(n.FeedbackVectorIndex());
    Node* spread = node->RemoveInput(n.LastArgumentIndex());

    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 3, stub_arity);
    node->InsertInput(zone(), 4, spread);
    node->InsertInput(zone(), 5, receiver);

    // After: {code, target, new_target, arity, spread, receiver, ...args}.

    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  }
}

// TODO(jgruber,v8:8888): Should this collect feedback?
void JSGenericLowering::LowerJSCallForwardVarargs(Node* node) {
  CallForwardVarargsParameters p = CallForwardVarargsParametersOf(node->op());
  int const arg_count = static_cast<int>(p.arity() - 2);
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  Callable callable = CodeFactory::CallForwardVarargs(isolate());
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      zone(), callable.descriptor(), arg_count + 1, flags);
  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  Node* stub_arity = jsgraph()->Int32Constant(arg_count);
  Node* start_index = jsgraph()->Uint32Constant(p.start_index());
  node->InsertInput(zone(), 0, stub_code);
  node->InsertInput(zone(), 2, stub_arity);
  node->InsertInput(zone(), 3, start_index);
  NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
}

void JSGenericLowering::LowerJSCall(Node* node) {
  JSCallNode n(node);
  CallParameters const& p = n.Parameters();
  int const arg_count = p.arity_without_implicit_args();
  ConvertReceiverMode const mode = p.convert_mode();

  Node* feedback_vector = n.feedback_vector();
  node->RemoveInput(n.FeedbackVectorIndex());

  if (CollectFeedbackInGenericLowering() &&
      CollectCallAndConstructFeedback(broker()) && p.feedback().IsValid()) {
    Callable callable = CodeFactory::Call_WithFeedback(isolate(), mode);
    CallDescriptor::Flags flags = FrameStateFlagForCall(node);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), arg_count + 1, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* stub_arity = jsgraph()->Int32Constant(arg_count);
    Node* slot = jsgraph()->UintPtrConstant(p.feedback().index());
    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 2, stub_arity);
    node->InsertInput(zone(), 3, slot);
    node->InsertInput(zone(), 4, feedback_vector);
    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  } else {
    Callable callable = CodeFactory::Call(isolate(), mode);
    CallDescriptor::Flags flags = FrameStateFlagForCall(node);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), arg_count + 1, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* stub_arity = jsgraph()->Int32Constant(arg_count);
    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 2, stub_arity);
    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  }
}

void JSGenericLowering::LowerJSCallWithArrayLike(Node* node) {
  JSCallWithArrayLikeNode n(node);
  CallParameters const& p = n.Parameters();
  const int arg_count = p.arity_without_implicit_args();
  DCHECK_EQ(arg_count, 1);  // The arraylike object.
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);

  static constexpr int kArgumentsList = 1;
  static constexpr int kReceiver = 1;

  if (CollectFeedbackInGenericLowering() &&
      CollectCallAndConstructFeedback(broker()) && p.feedback().IsValid()) {
    const int stack_argument_count = arg_count - kArgumentsList + kReceiver;
    Callable callable = Builtins::CallableFor(
        isolate(), Builtin::kCallWithArrayLike_WithFeedback);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), stack_argument_count, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* receiver = n.receiver();
    Node* arguments_list = n.Argument(0);
    Node* feedback_vector = n.feedback_vector();
    Node* slot = jsgraph()->UintPtrConstant(p.feedback().index());

    // Shuffling inputs.
    // Before: {target, receiver, arguments_list, vector}.

    node->ReplaceInput(1, arguments_list);
    node->ReplaceInput(2, feedback_vector);
    node->ReplaceInput(3, receiver);

    // Now: {target, arguments_list, vector, receiver}.

    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 3, slot);

    // After: {code, target, arguments_list, slot, vector, receiver}.

    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  } else {
    const int stack_argument_count = arg_count - kArgumentsList + kReceiver;
    Callable callable = CodeFactory::CallWithArrayLike(isolate());
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), stack_argument_count, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* receiver = n.receiver();
    Node* arguments_list = n.Argument(0);

    // Shuffling inputs.
    // Before: {target, receiver, arguments_list, vector}.

    node->RemoveInput(n.FeedbackVectorIndex());
    node->InsertInput(zone(), 0, stub_code);
    node->ReplaceInput(2, arguments_list);
    node->ReplaceInput(3, receiver);

    // After: {code, target, arguments_list, receiver}.

    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  }
}

void JSGenericLowering::LowerJSCallWithSpread(Node* node) {
  JSCallWithSpreadNode n(node);
  CallParameters const& p = n.Parameters();
  int const arg_count = p.arity_without_implicit_args();
  DCHECK_GE(arg_count, 1);  // At least the spread.
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);

  static constexpr int kReceiver = 1;
  static constexpr int kTheSpread = 1;
  static constexpr int kMaybeFeedbackVector = 1;

  if (CollectFeedbackInGenericLowering() &&
      CollectCallAndConstructFeedback(broker()) && p.feedback().IsValid()) {
    const int stack_argument_count =
        arg_count - kTheSpread + kReceiver + kMaybeFeedbackVector;
    Callable callable =
        Builtins::CallableFor(isolate(), Builtin::kCallWithSpread_WithFeedback);
    // If this fails, we might need to update the parameter reordering code
    // to ensure that the additional arguments passed via stack are pushed
    // between top of stack and JS arguments.
    DCHECK_EQ(callable.descriptor().GetStackParameterCount(),
              kMaybeFeedbackVector);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), stack_argument_count, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* slot = jsgraph()->UintPtrConstant(p.feedback().index());

    // We pass the spread in a register, not on the stack.
    Node* stub_arity = jsgraph()->Int32Constant(arg_count - kTheSpread);

    // Register argument inputs are followed by stack argument inputs (such as
    // feedback_vector). Both are listed in ascending order. Note that
    // the receiver is implicitly placed on the stack and is thus inserted
    // between explicitly-specified register and stack arguments.
    // TODO(jgruber): Implement a simpler way to specify these mutations.

    // Shuffling inputs.
    // Before: {target, receiver, ...args, spread, vector}.
    Node* feedback_vector = node->RemoveInput(n.FeedbackVectorIndex());
    Node* spread = node->RemoveInput(n.LastArgumentIndex());
    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 2, stub_arity);
    node->InsertInput(zone(), 3, spread);
    node->InsertInput(zone(), 4, slot);
    node->InsertInput(zone(), 5, feedback_vector);
    // After: {code, target, arity, spread, slot, vector, receiver, ...args}.

    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  } else {
    const int stack_argument_count = arg_count - kTheSpread + kReceiver;
    Callable callable = CodeFactory::CallWithSpread(isolate());
    // If this fails, we might need to update the parameter reordering code
    // to ensure that the additional arguments passed via stack are pushed
    // between top of stack and JS arguments.
    DCHECK_EQ(callable.descriptor().GetStackParameterCount(), 0);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        zone(), callable.descriptor(), stack_argument_count, flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());

    // We pass the spread in a register, not on the stack.
    Node* stub_arity = jsgraph()->Int32Constant(arg_count - kTheSpread);

    // Shuffling inputs.
    // Before: {target, receiver, ...args, spread, vector}.

    node->RemoveInput(n.FeedbackVectorIndex());
    Node* spread = node->RemoveInput(n.LastArgumentIndex());

    node->InsertInput(zone(), 0, stub_code);
    node->InsertInput(zone(), 2, stub_arity);
    node->InsertInput(zone(), 3, spread);

    // After: {code, target, arity, spread, receiver, ...args}.

    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  }
}

void JSGenericLowering::LowerJSCallRuntime(Node* node) {
  const CallRuntimeParameters& p = CallRuntimeParametersOf(node->op());
  ReplaceWithRuntimeCall(node, p.id(), static_cast<int>(p.arity()));
}

#if V8_ENABLE_WEBASSEMBLY
// Will be lowered in SimplifiedLowering.
void JSGenericLowering::LowerJSWasmCall(Node* node) {}
#endif  // V8_ENABLE_WEBASSEMBLY

void JSGenericLowering::LowerJSForInPrepare(Node* node) {
  JSForInPrepareNode n(node);
  Effect effect(node);            // {node} is kept in the effect chain.
  Control control = n.control();  // .. but not in the control chain.
  Node* enumerator = n.enumerator();
  Node* slot =
      jsgraph()->UintPtrConstant(n.Parameters().feedback().slot.ToInt());

  std::vector<Edge> use_edges;
  for (Edge edge : node->use_edges()) use_edges.push_back(edge);

  // {node} will be changed to a builtin call (see below). The returned value
  // is a fixed array containing {cache_array} and {cache_length}.
  // TODO(jgruber): This is awkward; what we really want is two return values,
  // the {cache_array} and {cache_length}, or better yet three return values
  // s.t. we can avoid the graph rewrites below. Builtin support for multiple
  // return types is unclear though.

  Node* result_fixed_array = node;
  Node* cache_type = enumerator;  // Just to clarify the rename.
  Node* cache_array;
  Node* cache_length;

  cache_array = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), result_fixed_array,
      jsgraph()->IntPtrConstant(FixedArray::OffsetOfElementAt(0) -
                                kHeapObjectTag),
      effect, control);
  cache_length = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), result_fixed_array,
      jsgraph()->IntPtrConstant(FixedArray::OffsetOfElementAt(1) -
                                kHeapObjectTag),
      effect, control);

  // Update the uses of {node}.
  for (Edge edge : use_edges) {
    Node* const user = edge.from();
    if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
    } else if (NodeProperties::IsControlEdge(edge)) {
      edge.UpdateTo(control);
    } else {
      DCHECK(NodeProperties::IsValueEdge(edge));
      switch (ProjectionIndexOf(user->op())) {
        case 0:
          Replace(user, cache_type);
          break;
        case 1:
          Replace(user, cache_array);
          break;
        case 2:
          Replace(user, cache_length);
          break;
        default:
          UNREACHABLE();
      }
    }
  }

  // Finally, change the original node into a builtin call. This happens here,
  // after graph rewrites, since the Call does not have a control output and
  // thus must not have any control uses. Any previously existing control
  // outputs have been replaced by the graph rewrite above.
  node->InsertInput(zone(), n.FeedbackVectorIndex(), slot);
  ReplaceWithBuiltinCall(node, Builtin::kForInPrepare);
}

void JSGenericLowering::LowerJSForInNext(Node* node) {
  JSForInNextNode n(node);
  node->InsertInput(
      zone(), 0,
      jsgraph()->UintPtrConstant(n.Parameters().feedback().slot.ToInt()));
  ReplaceWithBuiltinCall(node, Builtin::kForInNext);
}

void JSGenericLowering::LowerJSLoadMessage(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}


void JSGenericLowering::LowerJSStoreMessage(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSLoadModule(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSStoreModule(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSGetImportMeta(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kGetImportMetaObject);
}

void JSGenericLowering::LowerJSGeneratorStore(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSGeneratorRestoreContinuation(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSGeneratorRestoreContext(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSGeneratorRestoreInputOrDebugPos(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSGeneratorRestoreRegister(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

namespace {

StackCheckKind StackCheckKindOfJSStackCheck(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSStackCheck);
  return OpParameter<StackCheckKind>(op);
}

}  // namespace

void JSGenericLowering::LowerJSStackCheck(Node* node) {
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* limit = effect =
      graph()->NewNode(machine()->Load(MachineType::Pointer()),
                       jsgraph()->ExternalConstant(
                           ExternalReference::address_of_jslimit(isolate())),
                       jsgraph()->IntPtrConstant(0), effect, control);

  StackCheckKind stack_check_kind = StackCheckKindOfJSStackCheck(node->op());
  Node* check = effect = graph()->NewNode(
      machine()->StackPointerGreaterThan(stack_check_kind), limit, effect);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  NodeProperties::ReplaceControlInput(node, if_false);
  NodeProperties::ReplaceEffectInput(node, effect);
  Node* efalse = if_false = node;

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* ephi = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, merge);

  // Wire the new diamond into the graph, {node} can still throw.
  NodeProperties::ReplaceUses(node, node, ephi, merge, merge);
  NodeProperties::ReplaceControlInput(merge, if_false, 1);
  NodeProperties::ReplaceEffectInput(ephi, efalse, 1);

  // This iteration cuts out potential {IfSuccess} or {IfException} projection
  // uses of the original node and places them inside the diamond, so that we
  // can change the original {node} into the slow-path runtime call.
  for (Edge edge : merge->use_edges()) {
    if (!NodeProperties::IsControlEdge(edge)) continue;
    if (edge.from()->opcode() == IrOpcode::kIfSuccess) {
      NodeProperties::ReplaceUses(edge.from(), nullptr, nullptr, merge);
      NodeProperties::ReplaceControlInput(merge, edge.from(), 1);
      edge.UpdateTo(node);
    }
    if (edge.from()->opcode() == IrOpcode::kIfException) {
      NodeProperties::ReplaceEffectInput(edge.from(), node);
      edge.UpdateTo(node);
    }
  }

  // Turn the stack check into a runtime call. At function entry, the runtime
  // function takes an offset argument which is subtracted from the stack
  // pointer prior to the stack check (i.e. the check is `sp - offset >=
  // limit`).
  if (stack_check_kind == StackCheckKind::kJSFunctionEntry) {
    node->InsertInput(zone(), 0,
                      graph()->NewNode(machine()->LoadStackCheckOffset()));
    ReplaceWithRuntimeCall(node, Runtime::kStackGuardWithGap);
  } else {
    ReplaceWithRuntimeCall(node, Runtime::kStackGuard);
  }
}

void JSGenericLowering::LowerJSDebugger(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kHandleDebuggerStatement);
}

Zone* JSGenericLowering::zone() const { return graph()->zone(); }


Isolate* JSGenericLowering::isolate() const { return jsgraph()->isolate(); }


Graph* JSGenericLowering::graph() const { return jsgraph()->graph(); }


CommonOperatorBuilder* JSGenericLowering::common() const {
  return jsgraph()->common();
}


MachineOperatorBuilder* JSGenericLowering::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
