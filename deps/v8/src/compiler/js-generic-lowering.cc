// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-generic-lowering.h"

#include "src/ast/ast.h"
#include "src/builtins/builtins-constructor.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"

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

JSGenericLowering::JSGenericLowering(JSGraph* jsgraph) : jsgraph_(jsgraph) {}

JSGenericLowering::~JSGenericLowering() {}


Reduction JSGenericLowering::Reduce(Node* node) {
  switch (node->opcode()) {
#define DECLARE_CASE(x)  \
    case IrOpcode::k##x: \
      Lower##x(node);    \
      break;
    JS_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE
    default:
      // Nothing to see.
      return NoChange();
  }
  return Changed(node);
}

#define REPLACE_STUB_CALL(Name)                                \
  void JSGenericLowering::LowerJS##Name(Node* node) {          \
    CallDescriptor::Flags flags = FrameStateFlagForCall(node); \
    Callable callable = CodeFactory::Name(isolate());          \
    ReplaceWithStubCall(node, callable, flags);                \
  }
REPLACE_STUB_CALL(Add)
REPLACE_STUB_CALL(Subtract)
REPLACE_STUB_CALL(Multiply)
REPLACE_STUB_CALL(Divide)
REPLACE_STUB_CALL(Modulus)
REPLACE_STUB_CALL(BitwiseAnd)
REPLACE_STUB_CALL(BitwiseOr)
REPLACE_STUB_CALL(BitwiseXor)
REPLACE_STUB_CALL(ShiftLeft)
REPLACE_STUB_CALL(ShiftRight)
REPLACE_STUB_CALL(ShiftRightLogical)
REPLACE_STUB_CALL(LessThan)
REPLACE_STUB_CALL(LessThanOrEqual)
REPLACE_STUB_CALL(GreaterThan)
REPLACE_STUB_CALL(GreaterThanOrEqual)
REPLACE_STUB_CALL(HasProperty)
REPLACE_STUB_CALL(Equal)
REPLACE_STUB_CALL(NotEqual)
REPLACE_STUB_CALL(ToInteger)
REPLACE_STUB_CALL(ToLength)
REPLACE_STUB_CALL(ToNumber)
REPLACE_STUB_CALL(ToName)
REPLACE_STUB_CALL(ToObject)
REPLACE_STUB_CALL(ToString)
#undef REPLACE_STUB_CALL

void JSGenericLowering::ReplaceWithStubCall(Node* node, Callable callable,
                                            CallDescriptor::Flags flags) {
  ReplaceWithStubCall(node, callable, flags, node->op()->properties());
}

void JSGenericLowering::ReplaceWithStubCall(Node* node, Callable callable,
                                            CallDescriptor::Flags flags,
                                            Operator::Properties properties) {
  const CallInterfaceDescriptor& descriptor = callable.descriptor();
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(), flags,
      properties);
  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  node->InsertInput(zone(), 0, stub_code);
  NodeProperties::ChangeOp(node, common()->Call(desc));
}


void JSGenericLowering::ReplaceWithRuntimeCall(Node* node,
                                               Runtime::FunctionId f,
                                               int nargs_override) {
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
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

void JSGenericLowering::LowerJSStrictEqual(Node* node) {
  // The === operator doesn't need the current context.
  NodeProperties::ReplaceContextInput(node, jsgraph()->NoContextConstant());
  Callable callable = CodeFactory::StrictEqual(isolate());
  node->RemoveInput(4);  // control
  ReplaceWithStubCall(node, callable, CallDescriptor::kNoFlags,
                      Operator::kEliminatable);
}

void JSGenericLowering::LowerJSStrictNotEqual(Node* node) {
  // The !== operator doesn't need the current context.
  NodeProperties::ReplaceContextInput(node, jsgraph()->NoContextConstant());
  Callable callable = CodeFactory::StrictNotEqual(isolate());
  node->RemoveInput(4);  // control
  ReplaceWithStubCall(node, callable, CallDescriptor::kNoFlags,
                      Operator::kEliminatable);
}

void JSGenericLowering::LowerJSToBoolean(Node* node) {
  // The ToBoolean conversion doesn't need the current context.
  NodeProperties::ReplaceContextInput(node, jsgraph()->NoContextConstant());
  Callable callable = CodeFactory::ToBoolean(isolate());
  node->AppendInput(zone(), graph()->start());
  ReplaceWithStubCall(node, callable, CallDescriptor::kNoAllocate,
                      Operator::kEliminatable);
}

void JSGenericLowering::LowerJSTypeOf(Node* node) {
  // The typeof operator doesn't need the current context.
  NodeProperties::ReplaceContextInput(node, jsgraph()->NoContextConstant());
  Callable callable = CodeFactory::Typeof(isolate());
  node->AppendInput(zone(), graph()->start());
  ReplaceWithStubCall(node, callable, CallDescriptor::kNoAllocate,
                      Operator::kEliminatable);
}


void JSGenericLowering::LowerJSLoadProperty(Node* node) {
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  const PropertyAccess& p = PropertyAccessOf(node->op());
  Callable callable = CodeFactory::KeyedLoadICInOptimizedCode(isolate());
  Node* vector = jsgraph()->HeapConstant(p.feedback().vector());
  node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
  node->InsertInput(zone(), 3, vector);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSLoadNamed(Node* node) {
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  NamedAccess const& p = NamedAccessOf(node->op());
  Callable callable = CodeFactory::LoadICInOptimizedCode(isolate());
  Node* vector = jsgraph()->HeapConstant(p.feedback().vector());
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
  node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
  node->InsertInput(zone(), 3, vector);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSLoadGlobal(Node* node) {
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  const LoadGlobalParameters& p = LoadGlobalParametersOf(node->op());
  Callable callable =
      CodeFactory::LoadGlobalICInOptimizedCode(isolate(), p.typeof_mode());
  Node* vector = jsgraph()->HeapConstant(p.feedback().vector());
  node->InsertInput(zone(), 0, jsgraph()->HeapConstant(p.name()));
  node->InsertInput(zone(), 1, jsgraph()->SmiConstant(p.feedback().index()));
  node->InsertInput(zone(), 2, vector);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSStoreProperty(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* value = NodeProperties::GetValueInput(node, 2);
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  PropertyAccess const& p = PropertyAccessOf(node->op());
  LanguageMode language_mode = p.language_mode();
  Callable callable =
      CodeFactory::KeyedStoreICInOptimizedCode(isolate(), language_mode);
  Node* vector = jsgraph()->HeapConstant(p.feedback().vector());
  typedef StoreWithVectorDescriptor Descriptor;
  node->InsertInputs(zone(), 0, 2);
  node->ReplaceInput(Descriptor::kReceiver, receiver);
  node->ReplaceInput(Descriptor::kName, key);
  node->ReplaceInput(Descriptor::kValue, value);
  node->ReplaceInput(Descriptor::kSlot,
                     jsgraph()->SmiConstant(p.feedback().index()));
  node->ReplaceInput(Descriptor::kVector, vector);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSStoreNamed(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* value = NodeProperties::GetValueInput(node, 1);
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  NamedAccess const& p = NamedAccessOf(node->op());
  Callable callable =
      CodeFactory::StoreICInOptimizedCode(isolate(), p.language_mode());
  Node* vector = jsgraph()->HeapConstant(p.feedback().vector());
  typedef StoreWithVectorDescriptor Descriptor;
  node->InsertInputs(zone(), 0, 3);
  node->ReplaceInput(Descriptor::kReceiver, receiver);
  node->ReplaceInput(Descriptor::kName, jsgraph()->HeapConstant(p.name()));
  node->ReplaceInput(Descriptor::kValue, value);
  node->ReplaceInput(Descriptor::kSlot,
                     jsgraph()->SmiConstant(p.feedback().index()));
  node->ReplaceInput(Descriptor::kVector, vector);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSStoreGlobal(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  const StoreGlobalParameters& p = StoreGlobalParametersOf(node->op());
  Callable callable =
      CodeFactory::StoreICInOptimizedCode(isolate(), p.language_mode());
  Node* vector = jsgraph()->HeapConstant(p.feedback().vector());
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
  typedef StoreWithVectorDescriptor Descriptor;
  node->InsertInputs(zone(), 0, 4);
  node->ReplaceInput(Descriptor::kReceiver, global);
  node->ReplaceInput(Descriptor::kName, jsgraph()->HeapConstant(p.name()));
  node->ReplaceInput(Descriptor::kValue, value);
  node->ReplaceInput(Descriptor::kSlot,
                     jsgraph()->SmiConstant(p.feedback().index()));
  node->ReplaceInput(Descriptor::kVector, vector);
  node->ReplaceInput(7, effect);
  ReplaceWithStubCall(node, callable, flags);
}

void JSGenericLowering::LowerJSStoreDataPropertyInLiteral(Node* node) {
  DataPropertyParameters const& p = DataPropertyParametersOf(node->op());
  node->InsertInputs(zone(), 4, 2);
  node->ReplaceInput(4, jsgraph()->HeapConstant(p.feedback().vector()));
  node->ReplaceInput(5, jsgraph()->SmiConstant(p.feedback().index()));
  ReplaceWithRuntimeCall(node, Runtime::kDefineDataPropertyInLiteral);
}

void JSGenericLowering::LowerJSDeleteProperty(Node* node) {
  LanguageMode language_mode = OpParameter<LanguageMode>(node);
  ReplaceWithRuntimeCall(node, is_strict(language_mode)
                                   ? Runtime::kDeleteProperty_Strict
                                   : Runtime::kDeleteProperty_Sloppy);
}

void JSGenericLowering::LowerJSGetSuperConstructor(Node* node) {
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  Callable callable = CodeFactory::GetSuperConstructor(isolate());
  ReplaceWithStubCall(node, callable, flags);
}

void JSGenericLowering::LowerJSInstanceOf(Node* node) {
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  Callable callable = CodeFactory::InstanceOf(isolate());
  ReplaceWithStubCall(node, callable, flags);
}

void JSGenericLowering::LowerJSOrdinaryHasInstance(Node* node) {
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  Callable callable = CodeFactory::OrdinaryHasInstance(isolate());
  ReplaceWithStubCall(node, callable, flags);
}

void JSGenericLowering::LowerJSLoadContext(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}


void JSGenericLowering::LowerJSStoreContext(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}


void JSGenericLowering::LowerJSCreate(Node* node) {
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
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
  Node* new_target = node->InputAt(1);
  Node* type_info = site.is_null() ? jsgraph()->UndefinedConstant()
                                   : jsgraph()->HeapConstant(site);
  node->RemoveInput(1);
  node->InsertInput(zone(), 1 + arity, new_target);
  node->InsertInput(zone(), 2 + arity, type_info);
  ReplaceWithRuntimeCall(node, Runtime::kNewArray, arity + 3);
}


void JSGenericLowering::LowerJSCreateClosure(Node* node) {
  CreateClosureParameters const& p = CreateClosureParametersOf(node->op());
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  Handle<SharedFunctionInfo> const shared_info = p.shared_info();
  node->InsertInput(zone(), 0, jsgraph()->HeapConstant(shared_info));

  // Use the FastNewClosurebuiltin only for functions allocated in new
  // space.
  if (p.pretenure() == NOT_TENURED) {
    Callable callable = CodeFactory::FastNewClosure(isolate());
    node->InsertInput(zone(), 1,
                      jsgraph()->HeapConstant(p.feedback().vector()));
    node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
    ReplaceWithStubCall(node, callable, flags);
  } else {
    node->InsertInput(zone(), 1,
                      jsgraph()->HeapConstant(p.feedback().vector()));
    node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
    ReplaceWithRuntimeCall(node, (p.pretenure() == TENURED)
                                     ? Runtime::kNewClosure_Tenured
                                     : Runtime::kNewClosure);
  }
}


void JSGenericLowering::LowerJSCreateFunctionContext(Node* node) {
  const CreateFunctionContextParameters& parameters =
      CreateFunctionContextParametersOf(node->op());
  int slot_count = parameters.slot_count();
  ScopeType scope_type = parameters.scope_type();
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);

  if (slot_count <=
      ConstructorBuiltinsAssembler::MaximumFunctionContextSlots()) {
    Callable callable =
        CodeFactory::FastNewFunctionContext(isolate(), scope_type);
    node->InsertInput(zone(), 1, jsgraph()->Int32Constant(slot_count));
    ReplaceWithStubCall(node, callable, flags);
  } else {
    node->InsertInput(zone(), 1, jsgraph()->SmiConstant(scope_type));
    ReplaceWithRuntimeCall(node, Runtime::kNewFunctionContext);
  }
}


void JSGenericLowering::LowerJSCreateIterResultObject(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kCreateIterResultObject);
}

void JSGenericLowering::LowerJSCreateKeyValueArray(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kCreateKeyValueArray);
}

void JSGenericLowering::LowerJSCreateLiteralArray(Node* node) {
  CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  node->InsertInput(zone(), 1, jsgraph()->SmiConstant(p.index()));
  node->InsertInput(zone(), 2, jsgraph()->HeapConstant(p.constant()));

  // Use the FastCloneShallowArray builtin only for shallow boilerplates without
  // properties up to the number of elements that the stubs can handle.
  if ((p.flags() & ArrayLiteral::kShallowElements) != 0 &&
      p.length() <
          ConstructorBuiltinsAssembler::kMaximumClonedShallowArrayElements) {
    Callable callable = CodeFactory::FastCloneShallowArray(
        isolate(), DONT_TRACK_ALLOCATION_SITE);
    ReplaceWithStubCall(node, callable, flags);
  } else {
    node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.flags()));
    ReplaceWithRuntimeCall(node, Runtime::kCreateArrayLiteral);
  }
}


void JSGenericLowering::LowerJSCreateLiteralObject(Node* node) {
  CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
  node->InsertInput(zone(), 1, jsgraph()->SmiConstant(p.index()));
  node->InsertInput(zone(), 2, jsgraph()->HeapConstant(p.constant()));
  node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.flags()));

  // Use the FastCloneShallowObject builtin only for shallow boilerplates
  // without elements up to the number of properties that the stubs can handle.
  if ((p.flags() & ObjectLiteral::kShallowProperties) != 0 &&
      p.length() <=
          ConstructorBuiltinsAssembler::kMaximumClonedShallowObjectProperties) {
    Callable callable =
        CodeFactory::FastCloneShallowObject(isolate(), p.length());
    ReplaceWithStubCall(node, callable, flags);
  } else {
    ReplaceWithRuntimeCall(node, Runtime::kCreateObjectLiteral);
  }
}


void JSGenericLowering::LowerJSCreateLiteralRegExp(Node* node) {
  CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
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
  const CreateCatchContextParameters& parameters =
      CreateCatchContextParametersOf(node->op());
  node->InsertInput(zone(), 0,
                    jsgraph()->HeapConstant(parameters.catch_name()));
  node->InsertInput(zone(), 2,
                    jsgraph()->HeapConstant(parameters.scope_info()));
  ReplaceWithRuntimeCall(node, Runtime::kPushCatchContext);
}

void JSGenericLowering::LowerJSCreateWithContext(Node* node) {
  Handle<ScopeInfo> scope_info = OpParameter<Handle<ScopeInfo>>(node);
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(scope_info));
  ReplaceWithRuntimeCall(node, Runtime::kPushWithContext);
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
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
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

void JSGenericLowering::LowerJSCallConstructWithSpread(Node* node) {
  CallConstructWithSpreadParameters const& p =
      CallConstructWithSpreadParametersOf(node->op());
  ReplaceWithRuntimeCall(node, Runtime::kNewWithSpread,
                         static_cast<int>(p.arity()));
}

void JSGenericLowering::LowerJSCallFunction(Node* node) {
  CallFunctionParameters const& p = CallFunctionParametersOf(node->op());
  int const arg_count = static_cast<int>(p.arity() - 2);
  ConvertReceiverMode const mode = p.convert_mode();
  Callable callable = CodeFactory::Call(isolate(), mode);
  CallDescriptor::Flags flags = FrameStateFlagForCall(node);
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
  ReplaceWithRuntimeCall(node, p.id(), static_cast<int>(p.arity()));
}

void JSGenericLowering::LowerJSConvertReceiver(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kConvertReceiver);
}

void JSGenericLowering::LowerJSForInNext(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kForInNext);
}


void JSGenericLowering::LowerJSForInPrepare(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kForInPrepare);
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

void JSGenericLowering::LowerJSGeneratorStore(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSGeneratorRestoreContinuation(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

void JSGenericLowering::LowerJSGeneratorRestoreRegister(Node* node) {
  UNREACHABLE();  // Eliminated in typed lowering.
}

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
