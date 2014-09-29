// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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


// TODO(mstarzinger): This is a temporary workaround for non-hydrogen stubs for
// which we don't have an interface descriptor yet. Use ReplaceWithICStubCall
// once these stub have been made into a HydrogenCodeStub.
template <typename T>
static CodeStubInterfaceDescriptor* GetInterfaceDescriptor(Isolate* isolate,
                                                           T* stub) {
  CodeStub::Major key = static_cast<CodeStub*>(stub)->MajorKey();
  CodeStubInterfaceDescriptor* d = isolate->code_stub_interface_descriptor(key);
  stub->InitializeInterfaceDescriptor(d);
  return d;
}


// TODO(mstarzinger): This is a temporary shim to be able to call an IC stub
// which doesn't have an interface descriptor yet. It mimics a hydrogen code
// stub for the underlying IC stub code.
class LoadICStubShim : public HydrogenCodeStub {
 public:
  LoadICStubShim(Isolate* isolate, ContextualMode contextual_mode)
      : HydrogenCodeStub(isolate), contextual_mode_(contextual_mode) {
    i::compiler::GetInterfaceDescriptor(isolate, this);
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE {
    ExtraICState extra_state = LoadIC::ComputeExtraICState(contextual_mode_);
    return LoadIC::initialize_stub(isolate(), extra_state);
  }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE {
    Register registers[] = { InterfaceDescriptor::ContextRegister(),
                             LoadIC::ReceiverRegister(),
                             LoadIC::NameRegister() };
    descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers);
  }

 private:
  virtual Major MajorKey() const V8_OVERRIDE { return NoCache; }
  virtual int NotMissMinorKey() const V8_OVERRIDE { return 0; }
  virtual bool UseSpecialCache() V8_OVERRIDE { return true; }

  ContextualMode contextual_mode_;
};


// TODO(mstarzinger): This is a temporary shim to be able to call an IC stub
// which doesn't have an interface descriptor yet. It mimics a hydrogen code
// stub for the underlying IC stub code.
class KeyedLoadICStubShim : public HydrogenCodeStub {
 public:
  explicit KeyedLoadICStubShim(Isolate* isolate) : HydrogenCodeStub(isolate) {
    i::compiler::GetInterfaceDescriptor(isolate, this);
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE {
    return isolate()->builtins()->KeyedLoadIC_Initialize();
  }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE {
    Register registers[] = { InterfaceDescriptor::ContextRegister(),
                             KeyedLoadIC::ReceiverRegister(),
                             KeyedLoadIC::NameRegister() };
    descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers);
  }

 private:
  virtual Major MajorKey() const V8_OVERRIDE { return NoCache; }
  virtual int NotMissMinorKey() const V8_OVERRIDE { return 0; }
  virtual bool UseSpecialCache() V8_OVERRIDE { return true; }
};


// TODO(mstarzinger): This is a temporary shim to be able to call an IC stub
// which doesn't have an interface descriptor yet. It mimics a hydrogen code
// stub for the underlying IC stub code.
class StoreICStubShim : public HydrogenCodeStub {
 public:
  StoreICStubShim(Isolate* isolate, StrictMode strict_mode)
      : HydrogenCodeStub(isolate), strict_mode_(strict_mode) {
    i::compiler::GetInterfaceDescriptor(isolate, this);
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE {
    return StoreIC::initialize_stub(isolate(), strict_mode_);
  }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE {
    Register registers[] = { InterfaceDescriptor::ContextRegister(),
                             StoreIC::ReceiverRegister(),
                             StoreIC::NameRegister(),
                             StoreIC::ValueRegister() };
    descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers);
  }

 private:
  virtual Major MajorKey() const V8_OVERRIDE { return NoCache; }
  virtual int NotMissMinorKey() const V8_OVERRIDE { return 0; }
  virtual bool UseSpecialCache() V8_OVERRIDE { return true; }

  StrictMode strict_mode_;
};


// TODO(mstarzinger): This is a temporary shim to be able to call an IC stub
// which doesn't have an interface descriptor yet. It mimics a hydrogen code
// stub for the underlying IC stub code.
class KeyedStoreICStubShim : public HydrogenCodeStub {
 public:
  KeyedStoreICStubShim(Isolate* isolate, StrictMode strict_mode)
      : HydrogenCodeStub(isolate), strict_mode_(strict_mode) {
    i::compiler::GetInterfaceDescriptor(isolate, this);
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE {
    return strict_mode_ == SLOPPY
               ? isolate()->builtins()->KeyedStoreIC_Initialize()
               : isolate()->builtins()->KeyedStoreIC_Initialize_Strict();
  }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE {
    Register registers[] = { InterfaceDescriptor::ContextRegister(),
                             KeyedStoreIC::ReceiverRegister(),
                             KeyedStoreIC::NameRegister(),
                             KeyedStoreIC::ValueRegister() };
    descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers);
  }

 private:
  virtual Major MajorKey() const V8_OVERRIDE { return NoCache; }
  virtual int NotMissMinorKey() const V8_OVERRIDE { return 0; }
  virtual bool UseSpecialCache() V8_OVERRIDE { return true; }

  StrictMode strict_mode_;
};


JSGenericLowering::JSGenericLowering(CompilationInfo* info, JSGraph* jsgraph,
                                     MachineOperatorBuilder* machine,
                                     SourcePositionTable* source_positions)
    : LoweringBuilder(jsgraph->graph(), source_positions),
      info_(info),
      jsgraph_(jsgraph),
      linkage_(new (jsgraph->zone()) Linkage(info)),
      machine_(machine) {}


void JSGenericLowering::PatchOperator(Node* node, Operator* op) {
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


void JSGenericLowering::Lower(Node* node) {
  Node* replacement = NULL;
  // Dispatch according to the opcode.
  switch (node->opcode()) {
#define DECLARE_CASE(x)           \
  case IrOpcode::k##x:            \
    replacement = Lower##x(node); \
    break;
    DECLARE_CASE(Branch)
    JS_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE
    default:
      // Nothing to see.
      return;
  }

  // Nothing to do if lowering was done by patching the existing node.
  if (replacement == node) return;

  // Iterate through uses of the original node and replace uses accordingly.
  UNIMPLEMENTED();
}


#define REPLACE_IC_STUB_CALL(op, StubDeclaration)  \
  Node* JSGenericLowering::Lower##op(Node* node) { \
    StubDeclaration;                               \
    ReplaceWithICStubCall(node, &stub);            \
    return node;                                   \
  }
REPLACE_IC_STUB_CALL(JSBitwiseOr, BinaryOpICStub stub(isolate(), Token::BIT_OR))
REPLACE_IC_STUB_CALL(JSBitwiseXor,
                     BinaryOpICStub stub(isolate(), Token::BIT_XOR))
REPLACE_IC_STUB_CALL(JSBitwiseAnd,
                     BinaryOpICStub stub(isolate(), Token::BIT_AND))
REPLACE_IC_STUB_CALL(JSShiftLeft, BinaryOpICStub stub(isolate(), Token::SHL))
REPLACE_IC_STUB_CALL(JSShiftRight, BinaryOpICStub stub(isolate(), Token::SAR))
REPLACE_IC_STUB_CALL(JSShiftRightLogical,
                     BinaryOpICStub stub(isolate(), Token::SHR))
REPLACE_IC_STUB_CALL(JSAdd, BinaryOpICStub stub(isolate(), Token::ADD))
REPLACE_IC_STUB_CALL(JSSubtract, BinaryOpICStub stub(isolate(), Token::SUB))
REPLACE_IC_STUB_CALL(JSMultiply, BinaryOpICStub stub(isolate(), Token::MUL))
REPLACE_IC_STUB_CALL(JSDivide, BinaryOpICStub stub(isolate(), Token::DIV))
REPLACE_IC_STUB_CALL(JSModulus, BinaryOpICStub stub(isolate(), Token::MOD))
REPLACE_IC_STUB_CALL(JSToNumber, ToNumberStub stub(isolate()))
#undef REPLACE_IC_STUB_CALL


#define REPLACE_COMPARE_IC_CALL(op, token, pure)   \
  Node* JSGenericLowering::Lower##op(Node* node) { \
    ReplaceWithCompareIC(node, token, pure);       \
    return node;                                   \
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


#define REPLACE_RUNTIME_CALL(op, fun)              \
  Node* JSGenericLowering::Lower##op(Node* node) { \
    ReplaceWithRuntimeCall(node, fun);             \
    return node;                                   \
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


#define REPLACE_UNIMPLEMENTED(op)                  \
  Node* JSGenericLowering::Lower##op(Node* node) { \
    UNIMPLEMENTED();                               \
    return node;                                   \
  }
REPLACE_UNIMPLEMENTED(JSToString)
REPLACE_UNIMPLEMENTED(JSToName)
REPLACE_UNIMPLEMENTED(JSYield)
REPLACE_UNIMPLEMENTED(JSDebugger)
#undef REPLACE_UNIMPLEMENTED


static CallDescriptor::DeoptimizationSupport DeoptimizationSupportForNode(
    Node* node) {
  return OperatorProperties::CanLazilyDeoptimize(node->op())
             ? CallDescriptor::kCanDeoptimize
             : CallDescriptor::kCannotDeoptimize;
}


void JSGenericLowering::ReplaceWithCompareIC(Node* node, Token::Value token,
                                             bool pure) {
  BinaryOpICStub stub(isolate(), Token::ADD);  // TODO(mstarzinger): Hack.
  CodeStubInterfaceDescriptor* d = stub.GetInterfaceDescriptor();
  CallDescriptor* desc_compare = linkage()->GetStubCallDescriptor(d);
  Handle<Code> ic = CompareIC::GetUninitialized(isolate(), token);
  Node* compare;
  if (pure) {
    // A pure (strict) comparison doesn't have an effect or control.
    // But for the graph, we need to add these inputs.
    compare = graph()->NewNode(common()->Call(desc_compare), CodeConstant(ic),
                               NodeProperties::GetValueInput(node, 0),
                               NodeProperties::GetValueInput(node, 1),
                               NodeProperties::GetContextInput(node),
                               graph()->start(), graph()->start());
  } else {
    compare = graph()->NewNode(common()->Call(desc_compare), CodeConstant(ic),
                               NodeProperties::GetValueInput(node, 0),
                               NodeProperties::GetValueInput(node, 1),
                               NodeProperties::GetContextInput(node),
                               NodeProperties::GetEffectInput(node),
                               NodeProperties::GetControlInput(node));
  }
  node->ReplaceInput(0, compare);
  node->ReplaceInput(1, SmiConstant(token));
  ReplaceWithRuntimeCall(node, Runtime::kBooleanize);
}


void JSGenericLowering::ReplaceWithICStubCall(Node* node,
                                              HydrogenCodeStub* stub) {
  CodeStubInterfaceDescriptor* d = stub->GetInterfaceDescriptor();
  CallDescriptor* desc = linkage()->GetStubCallDescriptor(
      d, 0, DeoptimizationSupportForNode(node));
  Node* stub_code = CodeConstant(stub->GetCode());
  PatchInsertInput(node, 0, stub_code);
  PatchOperator(node, common()->Call(desc));
}


void JSGenericLowering::ReplaceWithBuiltinCall(Node* node,
                                               Builtins::JavaScript id,
                                               int nargs) {
  CallFunctionStub stub(isolate(), nargs - 1, NO_CALL_FUNCTION_FLAGS);
  CodeStubInterfaceDescriptor* d = GetInterfaceDescriptor(isolate(), &stub);
  CallDescriptor* desc = linkage()->GetStubCallDescriptor(d, nargs);
  // TODO(mstarzinger): Accessing the builtins object this way prevents sharing
  // of code across native contexts. Fix this by loading from given context.
  Handle<JSFunction> function(
      JSFunction::cast(info()->context()->builtins()->javascript_builtin(id)));
  Node* stub_code = CodeConstant(stub.GetCode());
  Node* function_node = FunctionConstant(function);
  PatchInsertInput(node, 0, stub_code);
  PatchInsertInput(node, 1, function_node);
  PatchOperator(node, common()->Call(desc));
}


void JSGenericLowering::ReplaceWithRuntimeCall(Node* node,
                                               Runtime::FunctionId f,
                                               int nargs_override) {
  Operator::Property props = node->op()->properties();
  const Runtime::Function* fun = Runtime::FunctionForId(f);
  int nargs = (nargs_override < 0) ? fun->nargs : nargs_override;
  CallDescriptor* desc = linkage()->GetRuntimeCallDescriptor(
      f, nargs, props, DeoptimizationSupportForNode(node));
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


Node* JSGenericLowering::LowerBranch(Node* node) {
  Node* test = graph()->NewNode(machine()->WordEqual(), node->InputAt(0),
                                jsgraph()->TrueConstant());
  node->ReplaceInput(0, test);
  return node;
}


Node* JSGenericLowering::LowerJSUnaryNot(Node* node) {
  ToBooleanStub stub(isolate(), ToBooleanStub::RESULT_AS_INVERSE_ODDBALL);
  ReplaceWithICStubCall(node, &stub);
  return node;
}


Node* JSGenericLowering::LowerJSToBoolean(Node* node) {
  ToBooleanStub stub(isolate(), ToBooleanStub::RESULT_AS_ODDBALL);
  ReplaceWithICStubCall(node, &stub);
  return node;
}


Node* JSGenericLowering::LowerJSToObject(Node* node) {
  ReplaceWithBuiltinCall(node, Builtins::TO_OBJECT, 1);
  return node;
}


Node* JSGenericLowering::LowerJSLoadProperty(Node* node) {
  KeyedLoadICStubShim stub(isolate());
  ReplaceWithICStubCall(node, &stub);
  return node;
}


Node* JSGenericLowering::LowerJSLoadNamed(Node* node) {
  LoadNamedParameters p = OpParameter<LoadNamedParameters>(node);
  LoadICStubShim stub(isolate(), p.contextual_mode);
  PatchInsertInput(node, 1, jsgraph()->HeapConstant(p.name));
  ReplaceWithICStubCall(node, &stub);
  return node;
}


Node* JSGenericLowering::LowerJSStoreProperty(Node* node) {
  // TODO(mstarzinger): The strict_mode needs to be carried along in the
  // operator so that graphs are fully compositional for inlining.
  StrictMode strict_mode = info()->strict_mode();
  KeyedStoreICStubShim stub(isolate(), strict_mode);
  ReplaceWithICStubCall(node, &stub);
  return node;
}


Node* JSGenericLowering::LowerJSStoreNamed(Node* node) {
  PrintableUnique<Name> key = OpParameter<PrintableUnique<Name> >(node);
  // TODO(mstarzinger): The strict_mode needs to be carried along in the
  // operator so that graphs are fully compositional for inlining.
  StrictMode strict_mode = info()->strict_mode();
  StoreICStubShim stub(isolate(), strict_mode);
  PatchInsertInput(node, 1, jsgraph()->HeapConstant(key));
  ReplaceWithICStubCall(node, &stub);
  return node;
}


Node* JSGenericLowering::LowerJSDeleteProperty(Node* node) {
  StrictMode strict_mode = OpParameter<StrictMode>(node);
  PatchInsertInput(node, 2, SmiConstant(strict_mode));
  ReplaceWithBuiltinCall(node, Builtins::DELETE, 3);
  return node;
}


Node* JSGenericLowering::LowerJSHasProperty(Node* node) {
  ReplaceWithBuiltinCall(node, Builtins::IN, 2);
  return node;
}


Node* JSGenericLowering::LowerJSInstanceOf(Node* node) {
  InstanceofStub::Flags flags = static_cast<InstanceofStub::Flags>(
      InstanceofStub::kReturnTrueFalseObject |
      InstanceofStub::kArgsInRegisters);
  InstanceofStub stub(isolate(), flags);
  CodeStubInterfaceDescriptor* d = GetInterfaceDescriptor(isolate(), &stub);
  CallDescriptor* desc = linkage()->GetStubCallDescriptor(d, 0);
  Node* stub_code = CodeConstant(stub.GetCode());
  PatchInsertInput(node, 0, stub_code);
  PatchOperator(node, common()->Call(desc));
  return node;
}


Node* JSGenericLowering::LowerJSLoadContext(Node* node) {
  ContextAccess access = OpParameter<ContextAccess>(node);
  // TODO(mstarzinger): Use simplified operators instead of machine operators
  // here so that load/store optimization can be applied afterwards.
  for (int i = 0; i < access.depth(); ++i) {
    node->ReplaceInput(
        0, graph()->NewNode(
               machine()->Load(kMachineTagged),
               NodeProperties::GetValueInput(node, 0),
               Int32Constant(Context::SlotOffset(Context::PREVIOUS_INDEX)),
               NodeProperties::GetEffectInput(node)));
  }
  node->ReplaceInput(1, Int32Constant(Context::SlotOffset(access.index())));
  PatchOperator(node, machine()->Load(kMachineTagged));
  return node;
}


Node* JSGenericLowering::LowerJSStoreContext(Node* node) {
  ContextAccess access = OpParameter<ContextAccess>(node);
  // TODO(mstarzinger): Use simplified operators instead of machine operators
  // here so that load/store optimization can be applied afterwards.
  for (int i = 0; i < access.depth(); ++i) {
    node->ReplaceInput(
        0, graph()->NewNode(
               machine()->Load(kMachineTagged),
               NodeProperties::GetValueInput(node, 0),
               Int32Constant(Context::SlotOffset(Context::PREVIOUS_INDEX)),
               NodeProperties::GetEffectInput(node)));
  }
  node->ReplaceInput(2, NodeProperties::GetValueInput(node, 1));
  node->ReplaceInput(1, Int32Constant(Context::SlotOffset(access.index())));
  PatchOperator(node, machine()->Store(kMachineTagged, kFullWriteBarrier));
  return node;
}


Node* JSGenericLowering::LowerJSCallConstruct(Node* node) {
  int arity = OpParameter<int>(node);
  CallConstructStub stub(isolate(), NO_CALL_CONSTRUCTOR_FLAGS);
  CodeStubInterfaceDescriptor* d = GetInterfaceDescriptor(isolate(), &stub);
  CallDescriptor* desc = linkage()->GetStubCallDescriptor(
      d, arity, DeoptimizationSupportForNode(node));
  Node* stub_code = CodeConstant(stub.GetCode());
  Node* construct = NodeProperties::GetValueInput(node, 0);
  PatchInsertInput(node, 0, stub_code);
  PatchInsertInput(node, 1, Int32Constant(arity - 1));
  PatchInsertInput(node, 2, construct);
  PatchInsertInput(node, 3, jsgraph()->UndefinedConstant());
  PatchOperator(node, common()->Call(desc));
  return node;
}


Node* JSGenericLowering::LowerJSCallFunction(Node* node) {
  CallParameters p = OpParameter<CallParameters>(node);
  CallFunctionStub stub(isolate(), p.arity - 2, p.flags);
  CodeStubInterfaceDescriptor* d = GetInterfaceDescriptor(isolate(), &stub);
  CallDescriptor* desc = linkage()->GetStubCallDescriptor(
      d, p.arity - 1, DeoptimizationSupportForNode(node));
  Node* stub_code = CodeConstant(stub.GetCode());
  PatchInsertInput(node, 0, stub_code);
  PatchOperator(node, common()->Call(desc));
  return node;
}


Node* JSGenericLowering::LowerJSCallRuntime(Node* node) {
  Runtime::FunctionId function = OpParameter<Runtime::FunctionId>(node);
  int arity = OperatorProperties::GetValueInputCount(node->op());
  ReplaceWithRuntimeCall(node, function, arity);
  return node;
}
}
}
}  // namespace v8::internal::compiler
