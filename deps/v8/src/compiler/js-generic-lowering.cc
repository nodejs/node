// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/js-generic-lowering.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-aux-data-inl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"
#include "src/unique.h"

namespace v8 {
namespace internal {
namespace compiler {

JSGenericLowering::JSGenericLowering(CompilationInfo* info, JSGraph* jsgraph)
    : info_(info),
      jsgraph_(jsgraph),
      linkage_(new (jsgraph->zone()) Linkage(jsgraph->zone(), info)) {}


void JSGenericLowering::PatchOperator(Node* node, const Operator* op) {
  node->set_op(op);
}


void JSGenericLowering::PatchInsertInput(Node* node, int index, Node* input) {
  node->InsertInput(zone(), index, input);
}


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
      if (!info()->is_typing_enabled()) {
        Node* test = graph()->NewNode(machine()->WordEqual(), node->InputAt(0),
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
    ReplaceWithStubCall(node, CodeFactory::BinaryOpIC(isolate(), token), \
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
REPLACE_RUNTIME_CALL(JSTypeOf, Runtime::kTypeof)
REPLACE_RUNTIME_CALL(JSCreate, Runtime::kAbort)
REPLACE_RUNTIME_CALL(JSCreateFunctionContext, Runtime::kNewFunctionContext)
REPLACE_RUNTIME_CALL(JSCreateCatchContext, Runtime::kPushCatchContext)
REPLACE_RUNTIME_CALL(JSCreateWithContext, Runtime::kPushWithContext)
REPLACE_RUNTIME_CALL(JSCreateBlockContext, Runtime::kPushBlockContext)
REPLACE_RUNTIME_CALL(JSCreateModuleContext, Runtime::kPushModuleContext)
REPLACE_RUNTIME_CALL(JSCreateScriptContext, Runtime::kAbort)
#undef REPLACE_RUNTIME


#define REPLACE_UNIMPLEMENTED(op) \
  void JSGenericLowering::Lower##op(Node* node) { UNIMPLEMENTED(); }
REPLACE_UNIMPLEMENTED(JSToName)
REPLACE_UNIMPLEMENTED(JSYield)
REPLACE_UNIMPLEMENTED(JSDebugger)
#undef REPLACE_UNIMPLEMENTED


static CallDescriptor::Flags FlagsForNode(Node* node) {
  CallDescriptor::Flags result = CallDescriptor::kNoFlags;
  if (OperatorProperties::HasFrameStateInput(node->op())) {
    result |= CallDescriptor::kNeedsFrameState;
  }
  return result;
}


void JSGenericLowering::ReplaceWithCompareIC(Node* node, Token::Value token) {
  Callable callable = CodeFactory::CompareIC(isolate(), token);
  bool has_frame_state = OperatorProperties::HasFrameStateInput(node->op());
  CallDescriptor* desc_compare = linkage()->GetStubCallDescriptor(
      callable.descriptor(), 0,
      CallDescriptor::kPatchableCallSiteWithNop | FlagsForNode(node));
  NodeVector inputs(zone());
  inputs.reserve(node->InputCount() + 1);
  inputs.push_back(jsgraph()->HeapConstant(callable.code()));
  inputs.push_back(NodeProperties::GetValueInput(node, 0));
  inputs.push_back(NodeProperties::GetValueInput(node, 1));
  inputs.push_back(NodeProperties::GetContextInput(node));
  if (node->op()->HasProperty(Operator::kPure)) {
    // A pure (strict) comparison doesn't have an effect, control or frame
    // state.  But for the graph, we need to add control and effect inputs.
    DCHECK(!has_frame_state);
    inputs.push_back(graph()->start());
    inputs.push_back(graph()->start());
  } else {
    DCHECK(has_frame_state == FLAG_turbo_deoptimization);
    if (FLAG_turbo_deoptimization) {
      inputs.push_back(NodeProperties::GetFrameStateInput(node));
    }
    inputs.push_back(NodeProperties::GetEffectInput(node));
    inputs.push_back(NodeProperties::GetControlInput(node));
  }
  Node* compare =
      graph()->NewNode(common()->Call(desc_compare),
                       static_cast<int>(inputs.size()), &inputs.front());

  node->ReplaceInput(0, compare);
  node->ReplaceInput(1, jsgraph()->SmiConstant(token));

  if (has_frame_state) {
    // Remove the frame state from inputs.
    node->RemoveInput(NodeProperties::FirstFrameStateIndex(node));
  }

  ReplaceWithRuntimeCall(node, Runtime::kBooleanize);
}


void JSGenericLowering::ReplaceWithStubCall(Node* node, Callable callable,
                                            CallDescriptor::Flags flags) {
  Operator::Properties properties = node->op()->properties();
  CallDescriptor* desc = linkage()->GetStubCallDescriptor(
      callable.descriptor(), 0, flags | FlagsForNode(node), properties);
  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  PatchInsertInput(node, 0, stub_code);
  PatchOperator(node, common()->Call(desc));
}


void JSGenericLowering::ReplaceWithBuiltinCall(Node* node,
                                               Builtins::JavaScript id,
                                               int nargs) {
  Operator::Properties properties = node->op()->properties();
  Callable callable =
      CodeFactory::CallFunction(isolate(), nargs - 1, NO_CALL_FUNCTION_FLAGS);
  CallDescriptor* desc = linkage()->GetStubCallDescriptor(
      callable.descriptor(), nargs, FlagsForNode(node), properties);
  // TODO(mstarzinger): Accessing the builtins object this way prevents sharing
  // of code across native contexts. Fix this by loading from given context.
  Handle<JSFunction> function(
      JSFunction::cast(info()->context()->builtins()->javascript_builtin(id)));
  Node* stub_code = jsgraph()->HeapConstant(callable.code());
  Node* function_node = jsgraph()->HeapConstant(function);
  PatchInsertInput(node, 0, stub_code);
  PatchInsertInput(node, 1, function_node);
  PatchOperator(node, common()->Call(desc));
}


void JSGenericLowering::ReplaceWithRuntimeCall(Node* node,
                                               Runtime::FunctionId f,
                                               int nargs_override) {
  Operator::Properties properties = node->op()->properties();
  const Runtime::Function* fun = Runtime::FunctionForId(f);
  int nargs = (nargs_override < 0) ? fun->nargs : nargs_override;
  CallDescriptor* desc =
      linkage()->GetRuntimeCallDescriptor(f, nargs, properties);
  Node* ref = jsgraph()->ExternalConstant(ExternalReference(f, isolate()));
  Node* arity = jsgraph()->Int32Constant(nargs);
  PatchInsertInput(node, 0, jsgraph()->CEntryStubConstant(fun->result_size));
  PatchInsertInput(node, nargs + 1, ref);
  PatchInsertInput(node, nargs + 2, arity);
  PatchOperator(node, common()->Call(desc));
}


void JSGenericLowering::LowerJSUnaryNot(Node* node) {
  Callable callable = CodeFactory::ToBoolean(
      isolate(), ToBooleanStub::RESULT_AS_INVERSE_ODDBALL);
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
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


void JSGenericLowering::LowerJSToObject(Node* node) {
  ReplaceWithBuiltinCall(node, Builtins::TO_OBJECT, 1);
}


void JSGenericLowering::LowerJSLoadProperty(Node* node) {
  const LoadPropertyParameters& p = LoadPropertyParametersOf(node->op());
  Callable callable = CodeFactory::KeyedLoadICInOptimizedCode(isolate());
  if (FLAG_vector_ics) {
    PatchInsertInput(node, 2, jsgraph()->SmiConstant(p.feedback().index()));
    PatchInsertInput(node, 3, jsgraph()->HeapConstant(p.feedback().vector()));
  }
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSLoadNamed(Node* node) {
  const LoadNamedParameters& p = LoadNamedParametersOf(node->op());
  Callable callable =
      CodeFactory::LoadICInOptimizedCode(isolate(), p.contextual_mode());
  PatchInsertInput(node, 1, jsgraph()->HeapConstant(p.name()));
  if (FLAG_vector_ics) {
    PatchInsertInput(node, 2, jsgraph()->SmiConstant(p.feedback().index()));
    PatchInsertInput(node, 3, jsgraph()->HeapConstant(p.feedback().vector()));
  }
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSStoreProperty(Node* node) {
  StrictMode strict_mode = OpParameter<StrictMode>(node);
  Callable callable = CodeFactory::KeyedStoreIC(isolate(), strict_mode);
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSStoreNamed(Node* node) {
  const StoreNamedParameters& p = StoreNamedParametersOf(node->op());
  Callable callable = CodeFactory::StoreIC(isolate(), p.strict_mode());
  PatchInsertInput(node, 1, jsgraph()->HeapConstant(p.name()));
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSDeleteProperty(Node* node) {
  StrictMode strict_mode = OpParameter<StrictMode>(node);
  PatchInsertInput(node, 2, jsgraph()->SmiConstant(strict_mode));
  ReplaceWithBuiltinCall(node, Builtins::DELETE, 3);
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
  CallDescriptor* desc =
      linkage()->GetStubCallDescriptor(d, 0, FlagsForNode(node));
  Node* stub_code = jsgraph()->HeapConstant(stub.GetCode());
  PatchInsertInput(node, 0, stub_code);
  PatchOperator(node, common()->Call(desc));
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
  PatchOperator(node, machine()->Load(kMachAnyTagged));
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
  PatchOperator(node, machine()->Store(StoreRepresentation(kMachAnyTagged,
                                                           kFullWriteBarrier)));
}


void JSGenericLowering::LowerJSCallConstruct(Node* node) {
  int arity = OpParameter<int>(node);
  CallConstructStub stub(isolate(), NO_CALL_CONSTRUCTOR_FLAGS);
  CallInterfaceDescriptor d = stub.GetCallInterfaceDescriptor();
  CallDescriptor* desc =
      linkage()->GetStubCallDescriptor(d, arity, FlagsForNode(node));
  Node* stub_code = jsgraph()->HeapConstant(stub.GetCode());
  Node* construct = NodeProperties::GetValueInput(node, 0);
  PatchInsertInput(node, 0, stub_code);
  PatchInsertInput(node, 1, jsgraph()->Int32Constant(arity - 1));
  PatchInsertInput(node, 2, construct);
  PatchInsertInput(node, 3, jsgraph()->UndefinedConstant());
  PatchOperator(node, common()->Call(desc));
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
  if (arg_count != function->shared()->formal_parameter_count()) return false;

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
  CallDescriptor* desc = linkage()->GetJSCallDescriptor(
      1 + arg_count, jsgraph()->zone(), FlagsForNode(node));
  PatchOperator(node, common()->Call(desc));
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
  CallDescriptor* desc = linkage()->GetStubCallDescriptor(
      d, static_cast<int>(p.arity() - 1), FlagsForNode(node));
  Node* stub_code = jsgraph()->HeapConstant(stub.GetCode());
  PatchInsertInput(node, 0, stub_code);
  PatchOperator(node, common()->Call(desc));
}


void JSGenericLowering::LowerJSCallRuntime(Node* node) {
  const CallRuntimeParameters& p = CallRuntimeParametersOf(node->op());
  ReplaceWithRuntimeCall(node, p.id(), static_cast<int>(p.arity()));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
