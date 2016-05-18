// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/js-generic-lowering.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

static CallDescriptor::Flags AdjustFrameStatesForCall(Node* node) {
  int count = OperatorProperties::GetFrameStateInputCount(node->op());
  if (count > 1) {
    int index = NodeProperties::FirstFrameStateIndex(node) + 1;
    do {
      node->RemoveInput(index);
    } while (--count > 1);
  }
  return count > 0 ? CallDescriptor::kNeedsFrameState
                   : CallDescriptor::kNoFlags;
}


JSGenericLowering::JSGenericLowering(bool is_typing_enabled, JSGraph* jsgraph)
    : is_typing_enabled_(is_typing_enabled), jsgraph_(jsgraph) {}


JSGenericLowering::~JSGenericLowering() {}


Reduction JSGenericLowering::Reduce(Node* node) {
  switch (node->opcode()) {
#define DECLARE_CASE(x)  \
    case IrOpcode::k##x: \
      Lower##x(node);    \
      break;
    JS_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE
    case IrOpcode::kBranch:
      // TODO(mstarzinger): If typing is enabled then simplified lowering will
      // have inserted the correct ChangeBoolToBit, otherwise we need to perform
      // poor-man's representation inference here and insert manual change.
      if (!is_typing_enabled_) {
        Node* condition = node->InputAt(0);
        Node* test = graph()->NewNode(machine()->WordEqual(), condition,
                                      jsgraph()->TrueConstant());
        node->ReplaceInput(0, test);
      }
      // Fall-through.
    default:
      // Nothing to see.
      return NoChange();
  }
  return Changed(node);
}

#define REPLACE_BINARY_OP_IC_CALL(Op, token)                                \
  void JSGenericLowering::Lower##Op(Node* node) {                           \
    CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);           \
    ReplaceWithStubCall(node, CodeFactory::BinaryOpIC(isolate(), token),    \
                        CallDescriptor::kPatchableCallSiteWithNop | flags); \
  }
REPLACE_BINARY_OP_IC_CALL(JSBitwiseOr, Token::BIT_OR)
REPLACE_BINARY_OP_IC_CALL(JSBitwiseXor, Token::BIT_XOR)
REPLACE_BINARY_OP_IC_CALL(JSBitwiseAnd, Token::BIT_AND)
REPLACE_BINARY_OP_IC_CALL(JSShiftLeft, Token::SHL)
REPLACE_BINARY_OP_IC_CALL(JSShiftRight, Token::SAR)
REPLACE_BINARY_OP_IC_CALL(JSShiftRightLogical, Token::SHR)
REPLACE_BINARY_OP_IC_CALL(JSAdd, Token::ADD)
REPLACE_BINARY_OP_IC_CALL(JSSubtract, Token::SUB)
REPLACE_BINARY_OP_IC_CALL(JSMultiply, Token::MUL)
REPLACE_BINARY_OP_IC_CALL(JSDivide, Token::DIV)
REPLACE_BINARY_OP_IC_CALL(JSModulus, Token::MOD)
#undef REPLACE_BINARY_OP_IC_CALL

#define REPLACE_RUNTIME_CALL(op, fun)             \
  void JSGenericLowering::Lower##op(Node* node) { \
    ReplaceWithRuntimeCall(node, fun);            \
  }
REPLACE_RUNTIME_CALL(JSEqual, Runtime::kEqual)
REPLACE_RUNTIME_CALL(JSNotEqual, Runtime::kNotEqual)
REPLACE_RUNTIME_CALL(JSStrictEqual, Runtime::kStrictEqual)
REPLACE_RUNTIME_CALL(JSStrictNotEqual, Runtime::kStrictNotEqual)
REPLACE_RUNTIME_CALL(JSLessThan, Runtime::kLessThan)
REPLACE_RUNTIME_CALL(JSGreaterThan, Runtime::kGreaterThan)
REPLACE_RUNTIME_CALL(JSLessThanOrEqual, Runtime::kLessThanOrEqual)
REPLACE_RUNTIME_CALL(JSGreaterThanOrEqual, Runtime::kGreaterThanOrEqual)
REPLACE_RUNTIME_CALL(JSCreateWithContext, Runtime::kPushWithContext)
REPLACE_RUNTIME_CALL(JSCreateModuleContext, Runtime::kPushModuleContext)
REPLACE_RUNTIME_CALL(JSConvertReceiver, Runtime::kConvertReceiver)
#undef REPLACE_RUNTIME_CALL

void JSGenericLowering::ReplaceWithStubCall(Node* node, Callable callable,
                                            CallDescriptor::Flags flags) {
  Operator::Properties properties = node->op()->properties();
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), 0, flags, properties);
  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  node->InsertInput(zone(), 0, stub_code);
  NodeProperties::ChangeOp(node, common()->Call(desc));
}


void JSGenericLowering::ReplaceWithRuntimeCall(Node* node,
                                               Runtime::FunctionId f,
                                               int nargs_override) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Operator::Properties properties = node->op()->properties();
  const Runtime::Function* fun = Runtime::FunctionForId(f);
  int nargs = (nargs_override < 0) ? fun->nargs : nargs_override;
  CallDescriptor* desc =
      Linkage::GetRuntimeCallDescriptor(zone(), f, nargs, properties, flags);
  Node* ref = jsgraph()->ExternalConstant(ExternalReference(f, isolate()));
  Node* arity = jsgraph()->Int32Constant(nargs);
  node->InsertInput(zone(), 0, jsgraph()->CEntryStubConstant(fun->result_size));
  node->InsertInput(zone(), nargs + 1, ref);
  node->InsertInput(zone(), nargs + 2, arity);
  NodeProperties::ChangeOp(node, common()->Call(desc));
}


void JSGenericLowering::LowerJSTypeOf(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::Typeof(isolate());
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSToBoolean(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::ToBoolean(isolate());
  ReplaceWithStubCall(node, callable,
                      CallDescriptor::kPatchableCallSite | flags);
}


void JSGenericLowering::LowerJSToNumber(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::ToNumber(isolate());
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSToString(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::ToString(isolate());
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSToName(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::ToName(isolate());
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSToObject(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::ToObject(isolate());
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSLoadProperty(Node* node) {
  Node* closure = NodeProperties::GetValueInput(node, 2);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  const PropertyAccess& p = PropertyAccessOf(node->op());
  Callable callable =
      CodeFactory::KeyedLoadICInOptimizedCode(isolate(), UNINITIALIZED);
  // Load the type feedback vector from the closure.
  Node* shared_info = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), closure,
      jsgraph()->IntPtrConstant(JSFunction::kSharedFunctionInfoOffset -
                                kHeapObjectTag),
      effect, control);
  Node* vector = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), shared_info,
      jsgraph()->IntPtrConstant(SharedFunctionInfo::kFeedbackVectorOffset -
                                kHeapObjectTag),
      effect, control);
  node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
  node->ReplaceInput(3, vector);
  node->ReplaceInput(6, effect);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSLoadNamed(Node* node) {
  Node* closure = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  NamedAccess const& p = NamedAccessOf(node->op());
  Callable callable = CodeFactory::LoadICInOptimizedCode(
      isolate(), NOT_INSIDE_TYPEOF, UNINITIALIZED);
  // Load the type feedback vector from the closure.
  Node* shared_info = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), closure,
      jsgraph()->IntPtrConstant(JSFunction::kSharedFunctionInfoOffset -
                                kHeapObjectTag),
      effect, control);
  Node* vector = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), shared_info,
      jsgraph()->IntPtrConstant(SharedFunctionInfo::kFeedbackVectorOffset -
                                kHeapObjectTag),
      effect, control);
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
  node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
  node->ReplaceInput(3, vector);
  node->ReplaceInput(6, effect);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSLoadGlobal(Node* node) {
  Node* closure = NodeProperties::GetValueInput(node, 0);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  const LoadGlobalParameters& p = LoadGlobalParametersOf(node->op());
  Callable callable = CodeFactory::LoadICInOptimizedCode(
      isolate(), p.typeof_mode(), UNINITIALIZED);
  // Load the type feedback vector from the closure.
  Node* shared_info = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), closure,
      jsgraph()->IntPtrConstant(JSFunction::kSharedFunctionInfoOffset -
                                kHeapObjectTag),
      effect, control);
  Node* vector = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), shared_info,
      jsgraph()->IntPtrConstant(SharedFunctionInfo::kFeedbackVectorOffset -
                                kHeapObjectTag),
      effect, control);
  // Load global object from the context.
  Node* native_context = effect =
      graph()->NewNode(machine()->Load(MachineType::AnyTagged()), context,
                       jsgraph()->IntPtrConstant(
                           Context::SlotOffset(Context::NATIVE_CONTEXT_INDEX)),
                       effect, control);
  Node* global = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), native_context,
      jsgraph()->IntPtrConstant(Context::SlotOffset(Context::EXTENSION_INDEX)),
      effect, control);
  node->InsertInput(zone(), 0, global);
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
  node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
  node->ReplaceInput(3, vector);
  node->ReplaceInput(6, effect);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSStoreProperty(Node* node) {
  Node* closure = NodeProperties::GetValueInput(node, 3);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  PropertyAccess const& p = PropertyAccessOf(node->op());
  LanguageMode language_mode = p.language_mode();
  Callable callable = CodeFactory::KeyedStoreICInOptimizedCode(
      isolate(), language_mode, UNINITIALIZED);
  // Load the type feedback vector from the closure.
  Node* shared_info = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), closure,
      jsgraph()->IntPtrConstant(JSFunction::kSharedFunctionInfoOffset -
                                kHeapObjectTag),
      effect, control);
  Node* vector = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), shared_info,
      jsgraph()->IntPtrConstant(SharedFunctionInfo::kFeedbackVectorOffset -
                                kHeapObjectTag),
      effect, control);
  node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.feedback().index()));
  node->ReplaceInput(4, vector);
  node->ReplaceInput(7, effect);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSStoreNamed(Node* node) {
  Node* closure = NodeProperties::GetValueInput(node, 2);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  NamedAccess const& p = NamedAccessOf(node->op());
  Callable callable = CodeFactory::StoreICInOptimizedCode(
      isolate(), p.language_mode(), UNINITIALIZED);
  // Load the type feedback vector from the closure.
  Node* shared_info = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), closure,
      jsgraph()->IntPtrConstant(JSFunction::kSharedFunctionInfoOffset -
                                kHeapObjectTag),
      effect, control);
  Node* vector = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), shared_info,
      jsgraph()->IntPtrConstant(SharedFunctionInfo::kFeedbackVectorOffset -
                                kHeapObjectTag),
      effect, control);
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
  node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.feedback().index()));
  node->ReplaceInput(4, vector);
  node->ReplaceInput(7, effect);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSStoreGlobal(Node* node) {
  Node* closure = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  const StoreGlobalParameters& p = StoreGlobalParametersOf(node->op());
  Callable callable = CodeFactory::StoreICInOptimizedCode(
      isolate(), p.language_mode(), UNINITIALIZED);
  // Load the type feedback vector from the closure.
  Node* shared_info = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), closure,
      jsgraph()->IntPtrConstant(JSFunction::kSharedFunctionInfoOffset -
                                kHeapObjectTag),
      effect, control);
  Node* vector = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), shared_info,
      jsgraph()->IntPtrConstant(SharedFunctionInfo::kFeedbackVectorOffset -
                                kHeapObjectTag),
      effect, control);
  // Load global object from the context.
  Node* native_context = effect =
      graph()->NewNode(machine()->Load(MachineType::AnyTagged()), context,
                       jsgraph()->IntPtrConstant(
                           Context::SlotOffset(Context::NATIVE_CONTEXT_INDEX)),
                       effect, control);
  Node* global = effect = graph()->NewNode(
      machine()->Load(MachineType::AnyTagged()), native_context,
      jsgraph()->IntPtrConstant(Context::SlotOffset(Context::EXTENSION_INDEX)),
      effect, control);
  node->InsertInput(zone(), 0, global);
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
  node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.feedback().index()));
  node->ReplaceInput(4, vector);
  node->ReplaceInput(7, effect);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSDeleteProperty(Node* node) {
  LanguageMode language_mode = OpParameter<LanguageMode>(node);
  ReplaceWithRuntimeCall(node, is_strict(language_mode)
                                   ? Runtime::kDeleteProperty_Strict
                                   : Runtime::kDeleteProperty_Sloppy);
}


void JSGenericLowering::LowerJSHasProperty(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kHasProperty);
}


void JSGenericLowering::LowerJSInstanceOf(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::InstanceOf(isolate());
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSLoadContext(Node* node) {
  const ContextAccess& access = ContextAccessOf(node->op());
  for (size_t i = 0; i < access.depth(); ++i) {
    node->ReplaceInput(
        0, graph()->NewNode(machine()->Load(MachineType::AnyTagged()),
                            NodeProperties::GetValueInput(node, 0),
                            jsgraph()->Int32Constant(
                                Context::SlotOffset(Context::PREVIOUS_INDEX)),
                            NodeProperties::GetEffectInput(node),
                            graph()->start()));
  }
  node->ReplaceInput(1, jsgraph()->Int32Constant(Context::SlotOffset(
                            static_cast<int>(access.index()))));
  node->AppendInput(zone(), graph()->start());
  NodeProperties::ChangeOp(node, machine()->Load(MachineType::AnyTagged()));
}


void JSGenericLowering::LowerJSStoreContext(Node* node) {
  const ContextAccess& access = ContextAccessOf(node->op());
  for (size_t i = 0; i < access.depth(); ++i) {
    node->ReplaceInput(
        0, graph()->NewNode(machine()->Load(MachineType::AnyTagged()),
                            NodeProperties::GetValueInput(node, 0),
                            jsgraph()->Int32Constant(
                                Context::SlotOffset(Context::PREVIOUS_INDEX)),
                            NodeProperties::GetEffectInput(node),
                            graph()->start()));
  }
  node->ReplaceInput(2, NodeProperties::GetValueInput(node, 1));
  node->ReplaceInput(1, jsgraph()->Int32Constant(Context::SlotOffset(
                            static_cast<int>(access.index()))));
  NodeProperties::ChangeOp(
      node, machine()->Store(StoreRepresentation(MachineRepresentation::kTagged,
                                                 kFullWriteBarrier)));
}


void JSGenericLowering::LowerJSCreate(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::FastNewObject(isolate());
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSCreateArguments(Node* node) {
  CreateArgumentsType const type = CreateArgumentsTypeOf(node->op());
  switch (type) {
    case CreateArgumentsType::kMappedArguments:
      ReplaceWithRuntimeCall(node, Runtime::kNewSloppyArguments_Generic);
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
  Handle<AllocationSite> const site = p.site();

  // TODO(turbofan): We embed the AllocationSite from the Operator at this
  // point, which we should not do once we want to both consume the feedback
  // but at the same time shared the optimized code across native contexts,
  // as the AllocationSite is associated with a single native context (it's
  // stored in the type feedback vector after all). Once we go for cross
  // context code generation, we should somehow find a way to get to the
  // allocation site for the actual native context at runtime.
  if (!site.is_null()) {
    // Reduce {node} to the appropriate ArrayConstructorStub backend.
    // Note that these stubs "behave" like JSFunctions, which means they
    // expect a receiver on the stack, which they remove. We just push
    // undefined for the receiver.
    ElementsKind elements_kind = site->GetElementsKind();
    AllocationSiteOverrideMode override_mode =
        (AllocationSite::GetMode(elements_kind) == TRACK_ALLOCATION_SITE)
            ? DISABLE_ALLOCATION_SITES
            : DONT_OVERRIDE;
    if (arity == 0) {
      ArrayNoArgumentConstructorStub stub(isolate(), elements_kind,
                                          override_mode);
      CallDescriptor* desc = Linkage::GetStubCallDescriptor(
          isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(), 1,
          CallDescriptor::kNeedsFrameState);
      node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
      node->InsertInput(graph()->zone(), 2, jsgraph()->HeapConstant(site));
      node->InsertInput(graph()->zone(), 3, jsgraph()->UndefinedConstant());
      NodeProperties::ChangeOp(node, common()->Call(desc));
    } else if (arity == 1) {
      // TODO(bmeurer): Optimize for the 0 length non-holey case?
      ArraySingleArgumentConstructorStub stub(
          isolate(), GetHoleyElementsKind(elements_kind), override_mode);
      CallDescriptor* desc = Linkage::GetStubCallDescriptor(
          isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(), 2,
          CallDescriptor::kNeedsFrameState);
      node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
      node->InsertInput(graph()->zone(), 2, jsgraph()->HeapConstant(site));
      node->InsertInput(graph()->zone(), 3, jsgraph()->Int32Constant(1));
      node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
      NodeProperties::ChangeOp(node, common()->Call(desc));
    } else {
      ArrayNArgumentsConstructorStub stub(isolate(), elements_kind,
                                          override_mode);
      CallDescriptor* desc = Linkage::GetStubCallDescriptor(
          isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(),
          arity + 1, CallDescriptor::kNeedsFrameState);
      node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
      node->InsertInput(graph()->zone(), 2, jsgraph()->HeapConstant(site));
      node->InsertInput(graph()->zone(), 3, jsgraph()->Int32Constant(arity));
      node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
      NodeProperties::ChangeOp(node, common()->Call(desc));
    }
  } else {
    Node* new_target = node->InputAt(1);
    Node* type_info = site.is_null() ? jsgraph()->UndefinedConstant()
                                     : jsgraph()->HeapConstant(site);
    node->RemoveInput(1);
    node->InsertInput(zone(), 1 + arity, new_target);
    node->InsertInput(zone(), 2 + arity, type_info);
    ReplaceWithRuntimeCall(node, Runtime::kNewArray, arity + 3);
  }
}


void JSGenericLowering::LowerJSCreateClosure(Node* node) {
  CreateClosureParameters const& p = CreateClosureParametersOf(node->op());
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Handle<SharedFunctionInfo> const shared_info = p.shared_info();
  node->InsertInput(zone(), 0, jsgraph()->HeapConstant(shared_info));

  // Use the FastNewClosureStub that allocates in new space only for nested
  // functions that don't need literals cloning.
  if (p.pretenure() == NOT_TENURED && shared_info->num_literals() == 0) {
    Callable callable = CodeFactory::FastNewClosure(
        isolate(), shared_info->language_mode(), shared_info->kind());
    ReplaceWithStubCall(node, callable, flags);
  } else {
    ReplaceWithRuntimeCall(node, (p.pretenure() == TENURED)
                                     ? Runtime::kNewClosure_Tenured
                                     : Runtime::kNewClosure);
  }
}


void JSGenericLowering::LowerJSCreateFunctionContext(Node* node) {
  int const slot_count = OpParameter<int>(node->op());
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);

  // Use the FastNewContextStub only for function contexts up maximum size.
  if (slot_count <= FastNewContextStub::kMaximumSlots) {
    Callable callable = CodeFactory::FastNewContext(isolate(), slot_count);
    ReplaceWithStubCall(node, callable, flags);
  } else {
    ReplaceWithRuntimeCall(node, Runtime::kNewFunctionContext);
  }
}


void JSGenericLowering::LowerJSCreateIterResultObject(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kCreateIterResultObject);
}


void JSGenericLowering::LowerJSCreateLiteralArray(Node* node) {
  CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  int const length = Handle<FixedArray>::cast(p.constant())->length();
  node->InsertInput(zone(), 1, jsgraph()->SmiConstant(p.index()));
  node->InsertInput(zone(), 2, jsgraph()->HeapConstant(p.constant()));

  // Use the FastCloneShallowArrayStub only for shallow boilerplates up to the
  // initial length limit for arrays with "fast" elements kind.
  if ((p.flags() & ArrayLiteral::kShallowElements) != 0 &&
      (p.flags() & ArrayLiteral::kIsStrong) == 0 &&
      length < JSArray::kInitialMaxFastElementArray) {
    Callable callable = CodeFactory::FastCloneShallowArray(isolate());
    ReplaceWithStubCall(node, callable, flags);
  } else {
    node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.flags()));
    ReplaceWithRuntimeCall(node, Runtime::kCreateArrayLiteral);
  }
}


void JSGenericLowering::LowerJSCreateLiteralObject(Node* node) {
  CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  int const length = Handle<FixedArray>::cast(p.constant())->length();
  node->InsertInput(zone(), 1, jsgraph()->SmiConstant(p.index()));
  node->InsertInput(zone(), 2, jsgraph()->HeapConstant(p.constant()));
  node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.flags()));

  // Use the FastCloneShallowObjectStub only for shallow boilerplates without
  // elements up to the number of properties that the stubs can handle.
  if ((p.flags() & ObjectLiteral::kShallowProperties) != 0 &&
      length <= FastCloneShallowObjectStub::kMaximumClonedProperties) {
    Callable callable = CodeFactory::FastCloneShallowObject(isolate(), length);
    ReplaceWithStubCall(node, callable, flags);
  } else {
    ReplaceWithRuntimeCall(node, Runtime::kCreateObjectLiteral);
  }
}


void JSGenericLowering::LowerJSCreateLiteralRegExp(Node* node) {
  CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::FastCloneRegExp(isolate());
  Node* literal_index = jsgraph()->SmiConstant(p.index());
  Node* literal_flags = jsgraph()->SmiConstant(p.flags());
  Node* pattern = jsgraph()->HeapConstant(p.constant());
  node->InsertInput(graph()->zone(), 1, literal_index);
  node->InsertInput(graph()->zone(), 2, pattern);
  node->InsertInput(graph()->zone(), 3, literal_flags);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSCreateCatchContext(Node* node) {
  Handle<String> name = OpParameter<Handle<String>>(node);
  node->InsertInput(zone(), 0, jsgraph()->HeapConstant(name));
  ReplaceWithRuntimeCall(node, Runtime::kPushCatchContext);
}


void JSGenericLowering::LowerJSCreateBlockContext(Node* node) {
  Handle<ScopeInfo> scope_info = OpParameter<Handle<ScopeInfo>>(node);
  node->InsertInput(zone(), 0, jsgraph()->HeapConstant(scope_info));
  ReplaceWithRuntimeCall(node, Runtime::kPushBlockContext);
}


void JSGenericLowering::LowerJSCreateScriptContext(Node* node) {
  Handle<ScopeInfo> scope_info = OpParameter<Handle<ScopeInfo>>(node);
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(scope_info));
  ReplaceWithRuntimeCall(node, Runtime::kNewScriptContext);
}


void JSGenericLowering::LowerJSCallConstruct(Node* node) {
  CallConstructParameters const& p = CallConstructParametersOf(node->op());
  int const arg_count = static_cast<int>(p.arity() - 2);
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::Construct(isolate());
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), arg_count + 1, flags);
  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  Node* stub_arity = jsgraph()->Int32Constant(arg_count);
  Node* new_target = node->InputAt(arg_count + 1);
  Node* receiver = jsgraph()->UndefinedConstant();
  node->RemoveInput(arg_count + 1);  // Drop new target.
  node->InsertInput(zone(), 0, stub_code);
  node->InsertInput(zone(), 2, new_target);
  node->InsertInput(zone(), 3, stub_arity);
  node->InsertInput(zone(), 4, receiver);
  NodeProperties::ChangeOp(node, common()->Call(desc));
}


void JSGenericLowering::LowerJSCallFunction(Node* node) {
  CallFunctionParameters const& p = CallFunctionParametersOf(node->op());
  int const arg_count = static_cast<int>(p.arity() - 2);
  ConvertReceiverMode const mode = p.convert_mode();
  Callable callable = CodeFactory::Call(isolate(), mode);
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  if (p.tail_call_mode() == TailCallMode::kAllow) {
    flags |= CallDescriptor::kSupportsTailCalls;
  }
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), arg_count + 1, flags);
  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  Node* stub_arity = jsgraph()->Int32Constant(arg_count);
  node->InsertInput(zone(), 0, stub_code);
  node->InsertInput(zone(), 2, stub_arity);
  NodeProperties::ChangeOp(node, common()->Call(desc));
}


void JSGenericLowering::LowerJSCallRuntime(Node* node) {
  const CallRuntimeParameters& p = CallRuntimeParametersOf(node->op());
  AdjustFrameStatesForCall(node);
  ReplaceWithRuntimeCall(node, p.id(), static_cast<int>(p.arity()));
}


void JSGenericLowering::LowerJSForInDone(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kForInDone);
}


void JSGenericLowering::LowerJSForInNext(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kForInNext);
}


void JSGenericLowering::LowerJSForInPrepare(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kForInPrepare);
}


void JSGenericLowering::LowerJSForInStep(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kForInStep);
}


void JSGenericLowering::LowerJSLoadMessage(Node* node) {
  ExternalReference message_address =
      ExternalReference::address_of_pending_message_obj(isolate());
  node->RemoveInput(NodeProperties::FirstContextIndex(node));
  node->InsertInput(zone(), 0, jsgraph()->ExternalConstant(message_address));
  node->InsertInput(zone(), 1, jsgraph()->IntPtrConstant(0));
  NodeProperties::ChangeOp(node, machine()->Load(MachineType::AnyTagged()));
}


void JSGenericLowering::LowerJSStoreMessage(Node* node) {
  ExternalReference message_address =
      ExternalReference::address_of_pending_message_obj(isolate());
  node->RemoveInput(NodeProperties::FirstContextIndex(node));
  node->InsertInput(zone(), 0, jsgraph()->ExternalConstant(message_address));
  node->InsertInput(zone(), 1, jsgraph()->IntPtrConstant(0));
  StoreRepresentation representation(MachineRepresentation::kTagged,
                                     kNoWriteBarrier);
  NodeProperties::ChangeOp(node, machine()->Store(representation));
}


void JSGenericLowering::LowerJSYield(Node* node) { UNIMPLEMENTED(); }


void JSGenericLowering::LowerJSStackCheck(Node* node) {
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* limit = graph()->NewNode(
      machine()->Load(MachineType::Pointer()),
      jsgraph()->ExternalConstant(
          ExternalReference::address_of_stack_limit(isolate())),
      jsgraph()->IntPtrConstant(0), effect, control);
  Node* pointer = graph()->NewNode(machine()->LoadStackPointer());

  Node* check = graph()->NewNode(machine()->UintLessThan(), limit, pointer);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  NodeProperties::ReplaceControlInput(node, if_false);
  Node* efalse = node;

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* ephi = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, merge);

  // Wire the new diamond into the graph, {node} can still throw.
  NodeProperties::ReplaceUses(node, node, ephi, node, node);
  NodeProperties::ReplaceEffectInput(ephi, efalse, 1);

  // TODO(mstarzinger): This iteration cuts out the IfSuccess projection from
  // the node and places it inside the diamond. Come up with a helper method!
  for (Node* use : node->uses()) {
    if (use->opcode() == IrOpcode::kIfSuccess) {
      use->ReplaceUses(merge);
      merge->ReplaceInput(1, use);
    }
  }

  // Turn the stack check into a runtime call.
  ReplaceWithRuntimeCall(node, Runtime::kStackGuard);
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
