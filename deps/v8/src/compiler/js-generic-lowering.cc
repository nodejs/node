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
        break;
      }
      // Fall-through.
    default:
      // Nothing to see.
      return NoChange();
  }
  return Changed(node);
}


#define REPLACE_BINARY_OP_IC_CALL(op, token)                             \
  void JSGenericLowering::Lower##op(Node* node) {                        \
    ReplaceWithStubCall(node, CodeFactory::BinaryOpIC(isolate(), token,  \
                        OpParameter<LanguageMode>(node)),                \
                        CallDescriptor::kPatchableCallSiteWithNop);      \
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
  Callable callable = CodeFactory::CompareIC(isolate(), token);
  CallDescriptor* desc_compare = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), 0,
      CallDescriptor::kPatchableCallSiteWithNop | FlagsForNode(node),
      Operator::kNoProperties, kMachIntPtr);

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
  NodeProperties::ReplaceWithValue(node, node, compare);
  // TODO(mstarzinger): Just a work-around because SelectLowering might
  // otherwise introduce a Phi without any uses, making Scheduler unhappy.
  if (node->UseCount() == 0) return;
  node->TrimInputCount(3);
  node->ReplaceInput(0, booleanize);
  node->ReplaceInput(1, true_value);
  node->ReplaceInput(2, false_value);
  node->set_op(common()->Select(kMachAnyTagged));
}


void JSGenericLowering::ReplaceWithStubCall(Node* node, Callable callable,
                                            CallDescriptor::Flags flags) {
  Operator::Properties properties = node->op()->properties();
  flags |= FlagsForNode(node);
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), 0, flags, properties);
  const Operator* new_op = common()->Call(desc);

  // Take care of frame states.
  int old_frame_state_count =
      OperatorProperties::GetFrameStateInputCount(node->op());
  int new_frame_state_count =
      (flags & CallDescriptor::kNeedsFrameState) ? 1 : 0;
  DCHECK_GE(old_frame_state_count, new_frame_state_count);
  // If there are extra frame states, get rid of them.
  for (int i = new_frame_state_count; i < old_frame_state_count; i++) {
    node->RemoveInput(NodeProperties::FirstFrameStateIndex(node) +
                      new_frame_state_count);
  }

  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  node->InsertInput(zone(), 0, stub_code);
  node->set_op(new_op);
}


void JSGenericLowering::ReplaceWithBuiltinCall(Node* node,
                                               Builtins::JavaScript id,
                                               int nargs) {
  Operator::Properties properties = node->op()->properties();
  Callable callable =
      CodeFactory::CallFunction(isolate(), nargs - 1, NO_CALL_FUNCTION_FLAGS);
  CallDescriptor* desc =
      Linkage::GetStubCallDescriptor(isolate(), zone(), callable.descriptor(),
                                     nargs, FlagsForNode(node), properties);
  Node* global_object = graph()->NewNode(
      machine()->Load(kMachAnyTagged), NodeProperties::GetContextInput(node),
      jsgraph()->IntPtrConstant(
          Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)),
      NodeProperties::GetEffectInput(node), graph()->start());
  Node* builtins_object = graph()->NewNode(
      machine()->Load(kMachAnyTagged), global_object,
      jsgraph()->IntPtrConstant(GlobalObject::kBuiltinsOffset - kHeapObjectTag),
      NodeProperties::GetEffectInput(node), graph()->start());
  Node* function = graph()->NewNode(
      machine()->Load(kMachAnyTagged), builtins_object,
      jsgraph()->IntPtrConstant(JSBuiltinsObject::OffsetOfFunctionWithId(id) -
                                kHeapObjectTag),
      NodeProperties::GetEffectInput(node), graph()->start());
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
  Callable callable = CodeFactory::ToBoolean(
      isolate(), ToBooleanStub::RESULT_AS_INVERSE_ODDBALL);
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSTypeOf(Node* node) {
  Callable callable = CodeFactory::Typeof(isolate());
  ReplaceWithStubCall(node, callable, CallDescriptor::kNoFlags);
}


void JSGenericLowering::LowerJSToBoolean(Node* node) {
  Callable callable =
      CodeFactory::ToBoolean(isolate(), ToBooleanStub::RESULT_AS_ODDBALL);
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSToNumber(Node* node) {
  Callable callable = CodeFactory::ToNumber(isolate());
  ReplaceWithStubCall(node, callable, FlagsForNode(node));
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
  const LoadPropertyParameters& p = LoadPropertyParametersOf(node->op());
  Callable callable =
      CodeFactory::KeyedLoadICInOptimizedCode(isolate(), UNINITIALIZED);
  if (FLAG_vector_ics) {
    node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
    node->InsertInput(zone(), 3,
                      jsgraph()->HeapConstant(p.feedback().vector()));
  }
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSLoadNamed(Node* node) {
  const LoadNamedParameters& p = LoadNamedParametersOf(node->op());
  Callable callable =
      p.load_ic() == NAMED
          ? CodeFactory::LoadICInOptimizedCode(isolate(), p.contextual_mode(),
                                               UNINITIALIZED)
          : CodeFactory::KeyedLoadICInOptimizedCode(isolate(), UNINITIALIZED);
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
  if (FLAG_vector_ics) {
    node->InsertInput(zone(), 2, jsgraph()->SmiConstant(p.feedback().index()));
    node->InsertInput(zone(), 3,
                      jsgraph()->HeapConstant(p.feedback().vector()));
  }
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSStoreProperty(Node* node) {
  LanguageMode language_mode = OpParameter<LanguageMode>(node);
  Callable callable = CodeFactory::KeyedStoreICInOptimizedCode(
      isolate(), language_mode, UNINITIALIZED);
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSStoreNamed(Node* node) {
  const StoreNamedParameters& p = StoreNamedParametersOf(node->op());
  Callable callable = p.store_ic() == NAMED
                          ? CodeFactory::StoreIC(isolate(), p.language_mode())
                          : CodeFactory::KeyedStoreICInOptimizedCode(
                                isolate(), p.language_mode(), UNINITIALIZED);
  node->InsertInput(zone(), 1, jsgraph()->HeapConstant(p.name()));
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
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
  InstanceofStub::Flags flags = static_cast<InstanceofStub::Flags>(
      InstanceofStub::kReturnTrueFalseObject |
      InstanceofStub::kArgsInRegisters);
  InstanceofStub stub(isolate(), flags);
  CallInterfaceDescriptor d = stub.GetCallInterfaceDescriptor();
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(isolate(), zone(), d, 0,
                                                        FlagsForNode(node));
  Node* stub_code = jsgraph()->HeapConstant(stub.GetCode());
  node->InsertInput(zone(), 0, stub_code);
  node->set_op(common()->Call(desc));
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
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), d, arity, FlagsForNode(node));
  Node* stub_code = jsgraph()->HeapConstant(stub.GetCode());
  Node* construct = NodeProperties::GetValueInput(node, 0);
  node->InsertInput(zone(), 0, stub_code);
  node->InsertInput(zone(), 1, jsgraph()->Int32Constant(arity - 1));
  node->InsertInput(zone(), 2, construct);
  node->InsertInput(zone(), 3, jsgraph()->UndefinedConstant());
  node->set_op(common()->Call(desc));
}


bool JSGenericLowering::TryLowerDirectJSCall(Node* node) {
  // Lower to a direct call to a constant JSFunction if legal.
  const CallFunctionParameters& p = CallFunctionParametersOf(node->op());
  int arg_count = static_cast<int>(p.arity() - 2);

  // Check the function is a constant and is really a JSFunction.
  HeapObjectMatcher<Object> function_const(node->InputAt(0));
  if (!function_const.HasValue()) return false;  // not a constant.
  Handle<Object> func = function_const.Value().handle();
  if (!func->IsJSFunction()) return false;  // not a function.
  Handle<JSFunction> function = Handle<JSFunction>::cast(func);
  if (arg_count != function->shared()->internal_formal_parameter_count()) {
    return false;
  }

  // Check the receiver doesn't need to be wrapped.
  Node* receiver = node->InputAt(1);
  if (!NodeProperties::IsTyped(receiver)) return false;
  Type* ok_receiver = Type::Union(Type::Undefined(), Type::Receiver(), zone());
  if (!NodeProperties::GetBounds(receiver).upper->Is(ok_receiver)) return false;

  int index = NodeProperties::FirstContextIndex(node);

  // TODO(titzer): total hack to share function context constants.
  // Remove this when the JSGraph canonicalizes heap constants.
  Node* context = node->InputAt(index);
  HeapObjectMatcher<Context> context_const(context);
  if (!context_const.HasValue() ||
      *(context_const.Value().handle()) != function->context()) {
    context = jsgraph()->HeapConstant(Handle<Context>(function->context()));
  }
  node->ReplaceInput(index, context);
  CallDescriptor::Flags flags = FlagsForNode(node);
  if (is_strict(p.language_mode())) flags |= CallDescriptor::kSupportsTailCalls;
  CallDescriptor* desc =
      Linkage::GetJSCallDescriptor(zone(), false, 1 + arg_count, flags);
  node->set_op(common()->Call(desc));
  return true;
}


void JSGenericLowering::LowerJSCallFunction(Node* node) {
  // Fast case: call function directly.
  if (TryLowerDirectJSCall(node)) return;

  // General case: CallFunctionStub.
  const CallFunctionParameters& p = CallFunctionParametersOf(node->op());
  int arg_count = static_cast<int>(p.arity() - 2);
  CallFunctionStub stub(isolate(), arg_count, p.flags());
  CallInterfaceDescriptor d = stub.GetCallInterfaceDescriptor();
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), d, static_cast<int>(p.arity() - 1),
      FlagsForNode(node));
  Node* stub_code = jsgraph()->HeapConstant(stub.GetCode());
  node->InsertInput(zone(), 0, stub_code);
  node->set_op(common()->Call(desc));
}


void JSGenericLowering::LowerJSCallRuntime(Node* node) {
  const CallRuntimeParameters& p = CallRuntimeParametersOf(node->op());
  ReplaceWithRuntimeCall(node, p.id(), static_cast<int>(p.arity()));
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

  // Relax controls of {node}, i.e. make it free floating.
  NodeProperties::ReplaceWithValue(node, node, ephi, merge);
  NodeProperties::ReplaceEffectInput(ephi, efalse, 1);

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
