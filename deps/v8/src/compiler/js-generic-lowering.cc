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
#include "src/unique.h"

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


#define REPLACE_BINARY_OP_IC_CALL(op, token)                                  \
  void JSGenericLowering::Lower##op(Node* node) {                             \
    CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);             \
    ReplaceWithStubCall(node, CodeFactory::BinaryOpIC(                        \
                                  isolate(), token,                           \
                                  strength(OpParameter<LanguageMode>(node))), \
                        CallDescriptor::kPatchableCallSiteWithNop | flags);   \
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


#define REPLACE_COMPARE_IC_CALL(op, token)        \
  void JSGenericLowering::Lower##op(Node* node) { \
    ReplaceWithCompareIC(node, token);            \
  }
REPLACE_COMPARE_IC_CALL(JSEqual, Token::EQ)
REPLACE_COMPARE_IC_CALL(JSNotEqual, Token::NE)
REPLACE_COMPARE_IC_CALL(JSStrictEqual, Token::EQ_STRICT)
REPLACE_COMPARE_IC_CALL(JSStrictNotEqual, Token::NE_STRICT)
REPLACE_COMPARE_IC_CALL(JSLessThan, Token::LT)
REPLACE_COMPARE_IC_CALL(JSGreaterThan, Token::GT)
REPLACE_COMPARE_IC_CALL(JSLessThanOrEqual, Token::LTE)
REPLACE_COMPARE_IC_CALL(JSGreaterThanOrEqual, Token::GTE)
#undef REPLACE_COMPARE_IC_CALL


#define REPLACE_RUNTIME_CALL(op, fun)             \
  void JSGenericLowering::Lower##op(Node* node) { \
    ReplaceWithRuntimeCall(node, fun);            \
  }
REPLACE_RUNTIME_CALL(JSCreate, Runtime::kAbort)
REPLACE_RUNTIME_CALL(JSCreateFunctionContext, Runtime::kNewFunctionContext)
REPLACE_RUNTIME_CALL(JSCreateWithContext, Runtime::kPushWithContext)
REPLACE_RUNTIME_CALL(JSCreateBlockContext, Runtime::kPushBlockContext)
REPLACE_RUNTIME_CALL(JSCreateModuleContext, Runtime::kPushModuleContext)
REPLACE_RUNTIME_CALL(JSCreateScriptContext, Runtime::kNewScriptContext)
#undef REPLACE_RUNTIME


#define REPLACE_UNIMPLEMENTED(op) \
  void JSGenericLowering::Lower##op(Node* node) { UNIMPLEMENTED(); }
REPLACE_UNIMPLEMENTED(JSYield)
#undef REPLACE_UNIMPLEMENTED


static CallDescriptor::Flags FlagsForNode(Node* node) {
  CallDescriptor::Flags result = CallDescriptor::kNoFlags;
  if (OperatorProperties::GetFrameStateInputCount(node->op()) > 0) {
    result |= CallDescriptor::kNeedsFrameState;
  }
  return result;
}


void JSGenericLowering::ReplaceWithCompareIC(Node* node, Token::Value token) {
  Callable callable = CodeFactory::CompareIC(
      isolate(), token, strength(OpParameter<LanguageMode>(node)));

  // Create a new call node asking a CompareIC for help.
  NodeVector inputs(zone());
  inputs.reserve(node->InputCount() + 1);
  inputs.push_back(jsgraph()->HeapConstant(callable.code()));
  inputs.push_back(NodeProperties::GetValueInput(node, 0));
  inputs.push_back(NodeProperties::GetValueInput(node, 1));
  inputs.push_back(NodeProperties::GetContextInput(node));
  if (node->op()->HasProperty(Operator::kPure)) {
    // A pure (strict) comparison doesn't have an effect, control or frame
    // state.  But for the graph, we need to add control and effect inputs.
    DCHECK(OperatorProperties::GetFrameStateInputCount(node->op()) == 0);
    inputs.push_back(graph()->start());
    inputs.push_back(graph()->start());
  } else {
    inputs.push_back(NodeProperties::GetFrameStateInput(node, 0));
    inputs.push_back(NodeProperties::GetEffectInput(node));
    inputs.push_back(NodeProperties::GetControlInput(node));
  }
  CallDescriptor* desc_compare = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), 0,
      CallDescriptor::kPatchableCallSiteWithNop | FlagsForNode(node),
      Operator::kNoProperties, kMachIntPtr);
  Node* compare =
      graph()->NewNode(common()->Call(desc_compare),
                       static_cast<int>(inputs.size()), &inputs.front());

  // Decide how the return value from the above CompareIC can be converted into
  // a JavaScript boolean oddball depending on the given token.
  Node* false_value = jsgraph()->FalseConstant();
  Node* true_value = jsgraph()->TrueConstant();
  const Operator* op = nullptr;
  switch (token) {
    case Token::EQ:  // a == 0
    case Token::EQ_STRICT:
      op = machine()->WordEqual();
      break;
    case Token::NE:  // a != 0 becomes !(a == 0)
    case Token::NE_STRICT:
      op = machine()->WordEqual();
      std::swap(true_value, false_value);
      break;
    case Token::LT:  // a < 0
      op = machine()->IntLessThan();
      break;
    case Token::GT:  // a > 0 becomes !(a <= 0)
      op = machine()->IntLessThanOrEqual();
      std::swap(true_value, false_value);
      break;
    case Token::LTE:  // a <= 0
      op = machine()->IntLessThanOrEqual();
      break;
    case Token::GTE:  // a >= 0 becomes !(a < 0)
      op = machine()->IntLessThan();
      std::swap(true_value, false_value);
      break;
    default:
      UNREACHABLE();
  }
  Node* booleanize = graph()->NewNode(op, compare, jsgraph()->ZeroConstant());

  // Finally patch the original node to select a boolean.
  NodeProperties::ReplaceUses(node, node, compare, compare, compare);
  node->TrimInputCount(3);
  node->ReplaceInput(0, booleanize);
  node->ReplaceInput(1, true_value);
  node->ReplaceInput(2, false_value);
  node->set_op(common()->Select(kMachAnyTagged));
}


void JSGenericLowering::ReplaceWithStubCall(Node* node, Callable callable,
                                            CallDescriptor::Flags flags) {
  Operator::Properties properties = node->op()->properties();
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), 0, flags, properties);
  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  node->InsertInput(zone(), 0, stub_code);
  node->set_op(common()->Call(desc));
}


void JSGenericLowering::ReplaceWithBuiltinCall(Node* node,
                                               Builtins::JavaScript id,
                                               int nargs) {
  Node* context_input = NodeProperties::GetContextInput(node);
  Node* effect_input = NodeProperties::GetEffectInput(node);

  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Operator::Properties properties = node->op()->properties();
  Callable callable =
      CodeFactory::CallFunction(isolate(), nargs - 1, NO_CALL_FUNCTION_FLAGS);
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), nargs, flags, properties);
  Node* global_object =
      graph()->NewNode(machine()->Load(kMachAnyTagged), context_input,
                       jsgraph()->IntPtrConstant(
                           Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)),
                       effect_input, graph()->start());
  Node* builtins_object = graph()->NewNode(
      machine()->Load(kMachAnyTagged), global_object,
      jsgraph()->IntPtrConstant(GlobalObject::kBuiltinsOffset - kHeapObjectTag),
      effect_input, graph()->start());
  Node* function = graph()->NewNode(
      machine()->Load(kMachAnyTagged), builtins_object,
      jsgraph()->IntPtrConstant(JSBuiltinsObject::OffsetOfFunctionWithId(id) -
                                kHeapObjectTag),
      effect_input, graph()->start());
  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  node->InsertInput(zone(), 0, stub_code);
  node->InsertInput(zone(), 1, function);
  node->set_op(common()->Call(desc));
}


void JSGenericLowering::ReplaceWithRuntimeCall(Node* node,
                                               Runtime::FunctionId f,
                                               int nargs_override) {
  Operator::Properties properties = node->op()->properties();
  const Runtime::Function* fun = Runtime::FunctionForId(f);
  int nargs = (nargs_override < 0) ? fun->nargs : nargs_override;
  CallDescriptor* desc =
      Linkage::GetRuntimeCallDescriptor(zone(), f, nargs, properties);
  Node* ref = jsgraph()->ExternalConstant(ExternalReference(f, isolate()));
  Node* arity = jsgraph()->Int32Constant(nargs);
  node->InsertInput(zone(), 0, jsgraph()->CEntryStubConstant(fun->result_size));
  node->InsertInput(zone(), nargs + 1, ref);
  node->InsertInput(zone(), nargs + 2, arity);
  node->set_op(common()->Call(desc));
}


void JSGenericLowering::LowerJSUnaryNot(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::ToBoolean(
      isolate(), ToBooleanStub::RESULT_AS_INVERSE_ODDBALL);
  ReplaceWithStubCall(node, callable,
                      CallDescriptor::kPatchableCallSite | flags);
}


void JSGenericLowering::LowerJSTypeOf(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::Typeof(isolate());
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSToBoolean(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable =
      CodeFactory::ToBoolean(isolate(), ToBooleanStub::RESULT_AS_ODDBALL);
  ReplaceWithStubCall(node, callable,
                      CallDescriptor::kPatchableCallSite | flags);
}


void JSGenericLowering::LowerJSToNumber(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  Callable callable = CodeFactory::ToNumber(isolate());
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSToString(Node* node) {
  ReplaceWithBuiltinCall(node, Builtins::TO_STRING, 1);
}


void JSGenericLowering::LowerJSToName(Node* node) {
  ReplaceWithBuiltinCall(node, Builtins::TO_NAME, 1);
}


void JSGenericLowering::LowerJSToObject(Node* node) {
  ReplaceWithBuiltinCall(node, Builtins::TO_OBJECT, 1);
}


void JSGenericLowering::LowerJSLoadProperty(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  const LoadPropertyParameters& p = LoadPropertyParametersOf(node->op());
  Callable callable = CodeFactory::KeyedLoadICInOptimizedCode(
      isolate(), p.language_mode(), UNINITIALIZED);
  node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSLoadNamed(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  const LoadNamedParameters& p = LoadNamedParametersOf(node->op());
  Callable callable = CodeFactory::LoadICInOptimizedCode(
      isolate(), p.contextual_mode(), p.language_mode(), UNINITIALIZED);
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
  node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSLoadGlobal(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  const LoadNamedParameters& p = LoadGlobalParametersOf(node->op());
  Callable callable = CodeFactory::LoadICInOptimizedCode(
      isolate(), p.contextual_mode(), SLOPPY, UNINITIALIZED);
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
  node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSStoreProperty(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  const StorePropertyParameters& p = StorePropertyParametersOf(node->op());
  LanguageMode language_mode = OpParameter<LanguageMode>(node);
  Callable callable = CodeFactory::KeyedStoreICInOptimizedCode(
      isolate(), language_mode, UNINITIALIZED);
  if (FLAG_vector_stores) {
    DCHECK(p.feedback().index() != -1);
    node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.feedback().index()));
  } else {
    node->RemoveInput(3);
  }
  ReplaceWithStubCall(node, callable,
                      CallDescriptor::kPatchableCallSite | flags);
}


void JSGenericLowering::LowerJSStoreNamed(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  const StoreNamedParameters& p = StoreNamedParametersOf(node->op());
  Callable callable = CodeFactory::StoreICInOptimizedCode(
      isolate(), p.language_mode(), UNINITIALIZED);
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
  if (FLAG_vector_stores) {
    DCHECK(p.feedback().index() != -1);
    node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.feedback().index()));
  } else {
    node->RemoveInput(3);
  }
  ReplaceWithStubCall(node, callable,
                      CallDescriptor::kPatchableCallSite | flags);
}


void JSGenericLowering::LowerJSStoreGlobal(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  const StoreNamedParameters& p = StoreGlobalParametersOf(node->op());
  Callable callable = CodeFactory::StoreIC(isolate(), p.language_mode());
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
  if (FLAG_vector_stores) {
    DCHECK(p.feedback().index() != -1);
    node->InsertInput(zone(), 3, jsgraph()->SmiConstant(p.feedback().index()));
  } else {
    node->RemoveInput(3);
  }
  ReplaceWithStubCall(node, callable,
                      CallDescriptor::kPatchableCallSite | flags);
}


void JSGenericLowering::LowerJSDeleteProperty(Node* node) {
  LanguageMode language_mode = OpParameter<LanguageMode>(node);
  ReplaceWithBuiltinCall(node, Builtins::DELETE, 3);
  node->InsertInput(zone(), 4, jsgraph()->SmiConstant(language_mode));
}


void JSGenericLowering::LowerJSHasProperty(Node* node) {
  ReplaceWithBuiltinCall(node, Builtins::IN, 2);
}


void JSGenericLowering::LowerJSInstanceOf(Node* node) {
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  InstanceofStub::Flags stub_flags = static_cast<InstanceofStub::Flags>(
      InstanceofStub::kReturnTrueFalseObject |
      InstanceofStub::kArgsInRegisters);
  Callable callable = CodeFactory::Instanceof(isolate(), stub_flags);
  ReplaceWithStubCall(node, callable, flags);
}


void JSGenericLowering::LowerJSLoadContext(Node* node) {
  const ContextAccess& access = ContextAccessOf(node->op());
  for (size_t i = 0; i < access.depth(); ++i) {
    node->ReplaceInput(
        0, graph()->NewNode(machine()->Load(kMachAnyTagged),
                            NodeProperties::GetValueInput(node, 0),
                            jsgraph()->Int32Constant(
                                Context::SlotOffset(Context::PREVIOUS_INDEX)),
                            NodeProperties::GetEffectInput(node),
                            graph()->start()));
  }
  node->ReplaceInput(1, jsgraph()->Int32Constant(Context::SlotOffset(
                            static_cast<int>(access.index()))));
  node->AppendInput(zone(), graph()->start());
  node->set_op(machine()->Load(kMachAnyTagged));
}


void JSGenericLowering::LowerJSStoreContext(Node* node) {
  const ContextAccess& access = ContextAccessOf(node->op());
  for (size_t i = 0; i < access.depth(); ++i) {
    node->ReplaceInput(
        0, graph()->NewNode(machine()->Load(kMachAnyTagged),
                            NodeProperties::GetValueInput(node, 0),
                            jsgraph()->Int32Constant(
                                Context::SlotOffset(Context::PREVIOUS_INDEX)),
                            NodeProperties::GetEffectInput(node),
                            graph()->start()));
  }
  node->ReplaceInput(2, NodeProperties::GetValueInput(node, 1));
  node->ReplaceInput(1, jsgraph()->Int32Constant(Context::SlotOffset(
                            static_cast<int>(access.index()))));
  node->set_op(
      machine()->Store(StoreRepresentation(kMachAnyTagged, kFullWriteBarrier)));
}


void JSGenericLowering::LowerJSLoadDynamicGlobal(Node* node) {
  const DynamicGlobalAccess& access = DynamicGlobalAccessOf(node->op());
  Runtime::FunctionId function_id =
      (access.mode() == CONTEXTUAL) ? Runtime::kLoadLookupSlot
                                    : Runtime::kLoadLookupSlotNoReferenceError;
  Node* projection = graph()->NewNode(common()->Projection(0), node);
  NodeProperties::ReplaceUses(node, projection, node, node, node);
  node->RemoveInput(NodeProperties::FirstFrameStateIndex(node) + 1);
  node->RemoveInput(NodeProperties::FirstValueIndex(node));
  node->InsertInput(zone(), 1, jsgraph()->Constant(access.name()));
  ReplaceWithRuntimeCall(node, function_id);
  projection->ReplaceInput(0, node);
}


void JSGenericLowering::LowerJSLoadDynamicContext(Node* node) {
  const DynamicContextAccess& access = DynamicContextAccessOf(node->op());
  Node* projection = graph()->NewNode(common()->Projection(0), node);
  NodeProperties::ReplaceUses(node, projection, node, node, node);
  node->InsertInput(zone(), 1, jsgraph()->Constant(access.name()));
  ReplaceWithRuntimeCall(node, Runtime::kLoadLookupSlot);
  projection->ReplaceInput(0, node);
}


void JSGenericLowering::LowerJSCreateClosure(Node* node) {
  CreateClosureParameters p = CreateClosureParametersOf(node->op());
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.shared_info()));
  node->InsertInput(zone(), 2, jsgraph()->BooleanConstant(p.pretenure()));
  ReplaceWithRuntimeCall(node, Runtime::kNewClosure);
}


void JSGenericLowering::LowerJSCreateLiteralArray(Node* node) {
  int literal_flags = OpParameter<int>(node->op());
  node->InsertInput(zone(), 3, jsgraph()->SmiConstant(literal_flags));
  ReplaceWithRuntimeCall(node, Runtime::kCreateArrayLiteral);
}


void JSGenericLowering::LowerJSCreateLiteralObject(Node* node) {
  int literal_flags = OpParameter<int>(node->op());
  node->InsertInput(zone(), 3, jsgraph()->SmiConstant(literal_flags));
  ReplaceWithRuntimeCall(node, Runtime::kCreateObjectLiteral);
}


void JSGenericLowering::LowerJSCreateCatchContext(Node* node) {
  Unique<String> name = OpParameter<Unique<String>>(node);
  node->InsertInput(zone(), 0, jsgraph()->HeapConstant(name));
  ReplaceWithRuntimeCall(node, Runtime::kPushCatchContext);
}


void JSGenericLowering::LowerJSCallConstruct(Node* node) {
  int arity = OpParameter<int>(node);
  CallConstructStub stub(isolate(), NO_CALL_CONSTRUCTOR_FLAGS);
  CallInterfaceDescriptor d = stub.GetCallInterfaceDescriptor();
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  CallDescriptor* desc =
      Linkage::GetStubCallDescriptor(isolate(), zone(), d, arity, flags);
  Node* stub_code = jsgraph()->HeapConstant(stub.GetCode());
  Node* construct = NodeProperties::GetValueInput(node, 0);
  node->InsertInput(zone(), 0, stub_code);
  node->InsertInput(zone(), 1, jsgraph()->Int32Constant(arity - 1));
  node->InsertInput(zone(), 2, construct);
  node->InsertInput(zone(), 3, jsgraph()->UndefinedConstant());
  node->set_op(common()->Call(desc));
}


void JSGenericLowering::LowerJSCallFunction(Node* node) {
  const CallFunctionParameters& p = CallFunctionParametersOf(node->op());
  int arg_count = static_cast<int>(p.arity() - 2);
  CallFunctionStub stub(isolate(), arg_count, p.flags());
  CallInterfaceDescriptor d = stub.GetCallInterfaceDescriptor();
  CallDescriptor::Flags flags = AdjustFrameStatesForCall(node);
  if (p.AllowTailCalls()) {
    flags |= CallDescriptor::kSupportsTailCalls;
  }
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), d, static_cast<int>(p.arity() - 1), flags);
  Node* stub_code = jsgraph()->HeapConstant(stub.GetCode());
  node->InsertInput(zone(), 0, stub_code);
  node->set_op(common()->Call(desc));
}


void JSGenericLowering::LowerJSCallRuntime(Node* node) {
  const CallRuntimeParameters& p = CallRuntimeParametersOf(node->op());
  ReplaceWithRuntimeCall(node, p.id(), static_cast<int>(p.arity()));
}


void JSGenericLowering::LowerJSForInDone(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kForInDone);
}


void JSGenericLowering::LowerJSForInNext(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kForInNext);
}


void JSGenericLowering::LowerJSForInPrepare(Node* node) {
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 0);

  // Get the set of properties to enumerate.
  Runtime::Function const* function =
      Runtime::FunctionForId(Runtime::kGetPropertyNamesFast);
  CallDescriptor const* descriptor = Linkage::GetRuntimeCallDescriptor(
      zone(), function->function_id, 1, Operator::kNoProperties);
  Node* cache_type = effect = graph()->NewNode(
      common()->Call(descriptor),
      jsgraph()->CEntryStubConstant(function->result_size), object,
      jsgraph()->ExternalConstant(function->function_id),
      jsgraph()->Int32Constant(1), context, frame_state, effect, control);
  control = graph()->NewNode(common()->IfSuccess(), cache_type);

  Node* object_map = effect = graph()->NewNode(
      machine()->Load(kMachAnyTagged), object,
      jsgraph()->IntPtrConstant(HeapObject::kMapOffset - kHeapObjectTag),
      effect, control);
  Node* cache_type_map = effect = graph()->NewNode(
      machine()->Load(kMachAnyTagged), cache_type,
      jsgraph()->IntPtrConstant(HeapObject::kMapOffset - kHeapObjectTag),
      effect, control);
  Node* meta_map = jsgraph()->HeapConstant(isolate()->factory()->meta_map());

  // If we got a map from the GetPropertyNamesFast runtime call, we can do a
  // fast modification check. Otherwise, we got a fixed array, and we have to
  // perform a slow check on every iteration.
  Node* check0 =
      graph()->NewNode(machine()->WordEqual(), cache_type_map, meta_map);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* cache_array_true0;
  Node* cache_length_true0;
  Node* cache_type_true0;
  Node* etrue0;
  {
    // Enum cache case.
    Node* cache_type_enum_length = etrue0 = graph()->NewNode(
        machine()->Load(kMachUint32), cache_type,
        jsgraph()->IntPtrConstant(Map::kBitField3Offset - kHeapObjectTag),
        effect, if_true0);
    cache_type_enum_length =
        graph()->NewNode(machine()->Word32And(), cache_type_enum_length,
                         jsgraph()->Uint32Constant(Map::EnumLengthBits::kMask));

    Node* check1 =
        graph()->NewNode(machine()->Word32Equal(), cache_type_enum_length,
                         jsgraph()->Int32Constant(0));
    Node* branch1 =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* cache_array_true1;
    Node* etrue1;
    {
      // No properties to enumerate.
      cache_array_true1 =
          jsgraph()->HeapConstant(isolate()->factory()->empty_fixed_array());
      etrue1 = etrue0;
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* cache_array_false1;
    Node* efalse1;
    {
      // Load the enumeration cache from the instance descriptors of {object}.
      Node* object_map_descriptors = efalse1 = graph()->NewNode(
          machine()->Load(kMachAnyTagged), object_map,
          jsgraph()->IntPtrConstant(Map::kDescriptorsOffset - kHeapObjectTag),
          etrue0, if_false1);
      Node* object_map_enum_cache = efalse1 = graph()->NewNode(
          machine()->Load(kMachAnyTagged), object_map_descriptors,
          jsgraph()->IntPtrConstant(DescriptorArray::kEnumCacheOffset -
                                    kHeapObjectTag),
          efalse1, if_false1);
      cache_array_false1 = efalse1 = graph()->NewNode(
          machine()->Load(kMachAnyTagged), object_map_enum_cache,
          jsgraph()->IntPtrConstant(
              DescriptorArray::kEnumCacheBridgeCacheOffset - kHeapObjectTag),
          efalse1, if_false1);
    }

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    etrue0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_true0);
    cache_array_true0 =
        graph()->NewNode(common()->Phi(kMachAnyTagged, 2), cache_array_true1,
                         cache_array_false1, if_true0);

    cache_length_true0 = graph()->NewNode(
        machine()->WordShl(),
        machine()->Is64()
            ? graph()->NewNode(machine()->ChangeUint32ToUint64(),
                               cache_type_enum_length)
            : cache_type_enum_length,
        jsgraph()->Int32Constant(kSmiShiftSize + kSmiTagSize));
    cache_type_true0 = cache_type;
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* cache_array_false0;
  Node* cache_length_false0;
  Node* cache_type_false0;
  Node* efalse0;
  {
    // FixedArray case.
    Node* object_instance_type = efalse0 = graph()->NewNode(
        machine()->Load(kMachUint8), object_map,
        jsgraph()->IntPtrConstant(Map::kInstanceTypeOffset - kHeapObjectTag),
        effect, if_false0);

    STATIC_ASSERT(FIRST_JS_PROXY_TYPE == FIRST_SPEC_OBJECT_TYPE);
    Node* check1 = graph()->NewNode(
        machine()->Uint32LessThanOrEqual(), object_instance_type,
        jsgraph()->Uint32Constant(LAST_JS_PROXY_TYPE));
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* cache_type_true1 = jsgraph()->ZeroConstant();  // Zero indicates proxy

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* cache_type_false1 = jsgraph()->OneConstant();  // One means slow check

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    cache_type_false0 =
        graph()->NewNode(common()->Phi(kMachAnyTagged, 2), cache_type_true1,
                         cache_type_false1, if_false0);

    cache_array_false0 = cache_type;
    cache_length_false0 = efalse0 = graph()->NewNode(
        machine()->Load(kMachAnyTagged), cache_array_false0,
        jsgraph()->IntPtrConstant(FixedArray::kLengthOffset - kHeapObjectTag),
        efalse0, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  Node* cache_array =
      graph()->NewNode(common()->Phi(kMachAnyTagged, 2), cache_array_true0,
                       cache_array_false0, control);
  Node* cache_length =
      graph()->NewNode(common()->Phi(kMachAnyTagged, 2), cache_length_true0,
                       cache_length_false0, control);
  cache_type = graph()->NewNode(common()->Phi(kMachAnyTagged, 2),
                                cache_type_true0, cache_type_false0, control);

  for (auto edge : node->use_edges()) {
    if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
    } else if (NodeProperties::IsControlEdge(edge)) {
      Node* const use = edge.from();
      if (use->opcode() == IrOpcode::kIfSuccess) {
        use->ReplaceUses(control);
        use->Kill();
      } else if (use->opcode() == IrOpcode::kIfException) {
        edge.UpdateTo(cache_type_true0);
      } else {
        UNREACHABLE();
      }
    } else {
      Node* const use = edge.from();
      DCHECK(NodeProperties::IsValueEdge(edge));
      DCHECK_EQ(IrOpcode::kProjection, use->opcode());
      switch (ProjectionIndexOf(use->op())) {
        case 0:
          use->ReplaceUses(cache_type);
          break;
        case 1:
          use->ReplaceUses(cache_array);
          break;
        case 2:
          use->ReplaceUses(cache_length);
          break;
        default:
          UNREACHABLE();
          break;
      }
      use->Kill();
    }
  }
}


void JSGenericLowering::LowerJSForInStep(Node* node) {
  ReplaceWithRuntimeCall(node, Runtime::kForInStep);
}


void JSGenericLowering::LowerJSStackCheck(Node* node) {
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* limit = graph()->NewNode(
      machine()->Load(kMachPtr),
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
