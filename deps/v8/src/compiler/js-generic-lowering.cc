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
#include "src/compiler/node-properties-inl.h"
#include "src/unique.h"

namespace v8 {
namespace internal {
namespace compiler {

JSGenericLowering::JSGenericLowering(CompilationInfo* info, JSGraph* jsgraph)
    : info_(info),
      jsgraph_(jsgraph),
      linkage_(new (jsgraph->zone()) Linkage(info)) {}


void JSGenericLowering::PatchOperator(Node* node, const Operator* op) {
  node->set_op(op);
}


void JSGenericLowering::PatchInsertInput(Node* node, int index, Node* input) {
  node->InsertInput(zone(), index, input);
}


Node* JSGenericLowering::SmiConstant(int32_t immediate) {
  return jsgraph()->SmiConstant(immediate);
}


Node* JSGenericLowering::Int32Constant(int immediate) {
  return jsgraph()->Int32Constant(immediate);
}


Node* JSGenericLowering::CodeConstant(Handle<Code> code) {
  return jsgraph()->HeapConstant(code);
}


Node* JSGenericLowering::FunctionConstant(Handle<JSFunction> function) {
  return jsgraph()->HeapConstant(function);
}


Node* JSGenericLowering::ExternalConstant(ExternalReference ref) {
  return jsgraph()->ExternalConstant(ref);
}


Reduction JSGenericLowering::Reduce(Node* node) {
  switch (node->opcode()) {
#define DECLARE_CASE(x) \
  case IrOpcode::k##x:  \
    Lower##x(node);     \
    break;
    DECLARE_CASE(Branch)
    JS_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE
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


#define REPLACE_COMPARE_IC_CALL(op, token, pure)  \
  void JSGenericLowering::Lower##op(Node* node) { \
    ReplaceWithCompareIC(node, token, pure);      \
  }
REPLACE_COMPARE_IC_CALL(JSEqual, Token::EQ, false)
REPLACE_COMPARE_IC_CALL(JSNotEqual, Token::NE, false)
REPLACE_COMPARE_IC_CALL(JSStrictEqual, Token::EQ_STRICT, true)
REPLACE_COMPARE_IC_CALL(JSStrictNotEqual, Token::NE_STRICT, true)
REPLACE_COMPARE_IC_CALL(JSLessThan, Token::LT, false)
REPLACE_COMPARE_IC_CALL(JSGreaterThan, Token::GT, false)
REPLACE_COMPARE_IC_CALL(JSLessThanOrEqual, Token::LTE, false)
REPLACE_COMPARE_IC_CALL(JSGreaterThanOrEqual, Token::GTE, false)
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
REPLACE_RUNTIME_CALL(JSCreateGlobalContext, Runtime::kAbort)
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


void JSGenericLowering::ReplaceWithCompareIC(Node* node, Token::Value token,
                                             bool pure) {
  Callable callable = CodeFactory::CompareIC(isolate(), token);
  bool has_frame_state = OperatorProperties::HasFrameStateInput(node->op());
  CallDescriptor* desc_compare = linkage()->GetStubCallDescriptor(
      callable.descriptor(), 0,
      CallDescriptor::kPatchableCallSiteWithNop | FlagsForNode(node));
  NodeVector inputs(zone());
  inputs.reserve(node->InputCount() + 1);
  inputs.push_back(CodeConstant(callable.code()));
  inputs.push_back(NodeProperties::GetValueInput(node, 0));
  inputs.push_back(NodeProperties::GetValueInput(node, 1));
  inputs.push_back(NodeProperties::GetContextInput(node));
  if (pure) {
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
  node->ReplaceInput(1, SmiConstant(token));

  if (has_frame_state) {
    // Remove the frame state from inputs.
    node->RemoveInput(NodeProperties::FirstFrameStateIndex(node));
  }

  ReplaceWithRuntimeCall(node, Runtime::kBooleanize);
}


void JSGenericLowering::ReplaceWithStubCall(Node* node, Callable callable,
                                            CallDescriptor::Flags flags) {
  CallDescriptor* desc = linkage()->GetStubCallDescriptor(
      callable.descriptor(), 0, flags | FlagsForNode(node));
  Node* stub_code = CodeConstant(callable.code());
  PatchInsertInput(node, 0, stub_code);
  PatchOperator(node, common()->Call(desc));
}


void JSGenericLowering::ReplaceWithBuiltinCall(Node* node,
                                               Builtins::JavaScript id,
                                               int nargs) {
  Callable callable =
      CodeFactory::CallFunction(isolate(), nargs - 1, NO_CALL_FUNCTION_FLAGS);
  CallDescriptor* desc =
      linkage()->GetStubCallDescriptor(callable.descriptor(), nargs);
  // TODO(mstarzinger): Accessing the builtins object this way prevents sharing
  // of code across native contexts. Fix this by loading from given context.
  Handle<JSFunction> function(
      JSFunction::cast(info()->context()->builtins()->javascript_builtin(id)));
  Node* stub_code = CodeConstant(callable.code());
  Node* function_node = FunctionConstant(function);
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
  Node* ref = ExternalConstant(ExternalReference(f, isolate()));
  Node* arity = Int32Constant(nargs);
  if (!centrystub_constant_.is_set()) {
    centrystub_constant_.set(CodeConstant(CEntryStub(isolate(), 1).GetCode()));
  }
  PatchInsertInput(node, 0, centrystub_constant_.get());
  PatchInsertInput(node, nargs + 1, ref);
  PatchInsertInput(node, nargs + 2, arity);
  PatchOperator(node, common()->Call(desc));
}


void JSGenericLowering::LowerBranch(Node* node) {
  if (!info()->is_typing_enabled()) {
    // TODO(mstarzinger): If typing is enabled then simplified lowering will
    // have inserted the correct ChangeBoolToBit, otherwise we need to perform
    // poor-man's representation inference here and insert manual change.
    Node* test = graph()->NewNode(machine()->WordEqual(), node->InputAt(0),
                                  jsgraph()->TrueConstant());
    node->ReplaceInput(0, test);
  }
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
  ReplaceWithStubCall(node, callable, CallDescriptor::kNoFlags);
}


void JSGenericLowering::LowerJSToString(Node* node) {
  ReplaceWithBuiltinCall(node, Builtins::TO_STRING, 1);
}


void JSGenericLowering::LowerJSToObject(Node* node) {
  ReplaceWithBuiltinCall(node, Builtins::TO_OBJECT, 1);
}


void JSGenericLowering::LowerJSLoadProperty(Node* node) {
  Callable callable = CodeFactory::KeyedLoadIC(isolate());
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSLoadNamed(Node* node) {
  LoadNamedParameters p = OpParameter<LoadNamedParameters>(node);
  Callable callable = CodeFactory::LoadIC(isolate(), p.contextual_mode);
  PatchInsertInput(node, 1, jsgraph()->HeapConstant(p.name));
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSStoreProperty(Node* node) {
  StrictMode strict_mode = OpParameter<StrictMode>(node);
  Callable callable = CodeFactory::KeyedStoreIC(isolate(), strict_mode);
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSStoreNamed(Node* node) {
  StoreNamedParameters params = OpParameter<StoreNamedParameters>(node);
  Callable callable = CodeFactory::StoreIC(isolate(), params.strict_mode);
  PatchInsertInput(node, 1, jsgraph()->HeapConstant(params.name));
  ReplaceWithStubCall(node, callable, CallDescriptor::kPatchableCallSite);
}


void JSGenericLowering::LowerJSDeleteProperty(Node* node) {
  StrictMode strict_mode = OpParameter<StrictMode>(node);
  PatchInsertInput(node, 2, SmiConstant(strict_mode));
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
  CallDescriptor* desc = linkage()->GetStubCallDescriptor(d, 0);
  Node* stub_code = CodeConstant(stub.GetCode());
  PatchInsertInput(node, 0, stub_code);
  PatchOperator(node, common()->Call(desc));
}


void JSGenericLowering::LowerJSLoadContext(Node* node) {
  ContextAccess access = OpParameter<ContextAccess>(node);
  // TODO(mstarzinger): Use simplified operators instead of machine operators
  // here so that load/store optimization can be applied afterwards.
  for (int i = 0; i < access.depth(); ++i) {
    node->ReplaceInput(
        0, graph()->NewNode(
               machine()->Load(kMachAnyTagged),
               NodeProperties::GetValueInput(node, 0),
               Int32Constant(Context::SlotOffset(Context::PREVIOUS_INDEX)),
               NodeProperties::GetEffectInput(node)));
  }
  node->ReplaceInput(1, Int32Constant(Context::SlotOffset(access.index())));
  PatchOperator(node, machine()->Load(kMachAnyTagged));
}


void JSGenericLowering::LowerJSStoreContext(Node* node) {
  ContextAccess access = OpParameter<ContextAccess>(node);
  // TODO(mstarzinger): Use simplified operators instead of machine operators
  // here so that load/store optimization can be applied afterwards.
  for (int i = 0; i < access.depth(); ++i) {
    node->ReplaceInput(
        0, graph()->NewNode(
               machine()->Load(kMachAnyTagged),
               NodeProperties::GetValueInput(node, 0),
               Int32Constant(Context::SlotOffset(Context::PREVIOUS_INDEX)),
               NodeProperties::GetEffectInput(node)));
  }
  node->ReplaceInput(2, NodeProperties::GetValueInput(node, 1));
  node->ReplaceInput(1, Int32Constant(Context::SlotOffset(access.index())));
  PatchOperator(node, machine()->Store(StoreRepresentation(kMachAnyTagged,
                                                           kFullWriteBarrier)));
}


void JSGenericLowering::LowerJSCallConstruct(Node* node) {
  int arity = OpParameter<int>(node);
  CallConstructStub stub(isolate(), NO_CALL_CONSTRUCTOR_FLAGS);
  CallInterfaceDescriptor d = stub.GetCallInterfaceDescriptor();
  CallDescriptor* desc =
      linkage()->GetStubCallDescriptor(d, arity, FlagsForNode(node));
  Node* stub_code = CodeConstant(stub.GetCode());
  Node* construct = NodeProperties::GetValueInput(node, 0);
  PatchInsertInput(node, 0, stub_code);
  PatchInsertInput(node, 1, Int32Constant(arity - 1));
  PatchInsertInput(node, 2, construct);
  PatchInsertInput(node, 3, jsgraph()->UndefinedConstant());
  PatchOperator(node, common()->Call(desc));
}


void JSGenericLowering::LowerJSCallFunction(Node* node) {
  CallParameters p = OpParameter<CallParameters>(node);
  CallFunctionStub stub(isolate(), p.arity - 2, p.flags);
  CallInterfaceDescriptor d = stub.GetCallInterfaceDescriptor();
  CallDescriptor* desc =
      linkage()->GetStubCallDescriptor(d, p.arity - 1, FlagsForNode(node));
  Node* stub_code = CodeConstant(stub.GetCode());
  PatchInsertInput(node, 0, stub_code);
  PatchOperator(node, common()->Call(desc));
}


void JSGenericLowering::LowerJSCallRuntime(Node* node) {
  Runtime::FunctionId function = OpParameter<Runtime::FunctionId>(node);
  int arity = OperatorProperties::GetValueInputCount(node->op());
  ReplaceWithRuntimeCall(node, function, arity);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
