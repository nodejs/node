// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fast-accessor-assembler.h"

#include "src/base/logging.h"
#include "src/code-stub-assembler.h"
#include "src/code-stubs.h"  // For CallApiCallbackStub.
#include "src/handles-inl.h"
#include "src/objects.h"  // For FAA::LoadInternalField impl.

using v8::internal::CodeStubAssembler;
using v8::internal::compiler::Node;

namespace v8 {
namespace internal {

FastAccessorAssembler::FastAccessorAssembler(Isolate* isolate)
    : zone_(isolate->allocator()),
      isolate_(isolate),
      assembler_(new CodeStubAssembler(isolate, zone(), 1,
                                       Code::ComputeFlags(Code::STUB),
                                       "FastAccessorAssembler")),
      state_(kBuilding) {}

FastAccessorAssembler::~FastAccessorAssembler() { Clear(); }

FastAccessorAssembler::ValueId FastAccessorAssembler::IntegerConstant(
    int const_value) {
  CHECK_EQ(kBuilding, state_);
  return FromRaw(assembler_->NumberConstant(const_value));
}

FastAccessorAssembler::ValueId FastAccessorAssembler::GetReceiver() {
  CHECK_EQ(kBuilding, state_);

  // For JS functions, the receiver is parameter 0.
  return FromRaw(assembler_->Parameter(0));
}

FastAccessorAssembler::ValueId FastAccessorAssembler::LoadInternalField(
    ValueId value, int field_no) {
  CHECK_EQ(kBuilding, state_);

  CodeStubAssembler::Variable result(assembler_.get(),
                                     MachineRepresentation::kTagged);
  LabelId is_not_jsobject = MakeLabel();
  CodeStubAssembler::Label merge(assembler_.get(), &result);

  CheckIsJSObjectOrJump(value, is_not_jsobject);

  Node* internal_field = assembler_->LoadObjectField(
      FromId(value), JSObject::kHeaderSize + kPointerSize * field_no,
      MachineType::Pointer());

  result.Bind(internal_field);
  assembler_->Goto(&merge);

  // Return null, mimicking the C++ counterpart.
  SetLabel(is_not_jsobject);
  result.Bind(assembler_->NullConstant());
  assembler_->Goto(&merge);

  // Return.
  assembler_->Bind(&merge);
  return FromRaw(result.value());
}

FastAccessorAssembler::ValueId
FastAccessorAssembler::LoadInternalFieldUnchecked(ValueId value, int field_no) {
  CHECK_EQ(kBuilding, state_);

  // Defensive debug checks.
  if (FLAG_debug_code) {
    LabelId is_jsobject = MakeLabel();
    LabelId is_not_jsobject = MakeLabel();
    CheckIsJSObjectOrJump(value, is_not_jsobject);
    assembler_->Goto(FromId(is_jsobject));

    SetLabel(is_not_jsobject);
    assembler_->DebugBreak();
    assembler_->Goto(FromId(is_jsobject));

    SetLabel(is_jsobject);
  }

  Node* result = assembler_->LoadObjectField(
      FromId(value), JSObject::kHeaderSize + kPointerSize * field_no,
      MachineType::Pointer());

  return FromRaw(result);
}

FastAccessorAssembler::ValueId FastAccessorAssembler::LoadValue(ValueId value,
                                                                int offset) {
  CHECK_EQ(kBuilding, state_);
  return FromRaw(assembler_->LoadBufferObject(FromId(value), offset,
                                              MachineType::IntPtr()));
}

FastAccessorAssembler::ValueId FastAccessorAssembler::LoadObject(ValueId value,
                                                                 int offset) {
  CHECK_EQ(kBuilding, state_);
  return FromRaw(assembler_->LoadBufferObject(
      assembler_->LoadBufferObject(FromId(value), offset,
                                   MachineType::Pointer()),
      0, MachineType::AnyTagged()));
}

FastAccessorAssembler::ValueId FastAccessorAssembler::ToSmi(ValueId value) {
  CHECK_EQ(kBuilding, state_);
  return FromRaw(assembler_->SmiTag(FromId(value)));
}

void FastAccessorAssembler::ReturnValue(ValueId value) {
  CHECK_EQ(kBuilding, state_);
  assembler_->Return(FromId(value));
}

void FastAccessorAssembler::CheckFlagSetOrReturnNull(ValueId value, int mask) {
  CHECK_EQ(kBuilding, state_);
  CodeStubAssembler::Label pass(assembler_.get());
  CodeStubAssembler::Label fail(assembler_.get());
  assembler_->Branch(
      assembler_->Word32Equal(
          assembler_->Word32And(FromId(value), assembler_->Int32Constant(mask)),
          assembler_->Int32Constant(0)),
      &fail, &pass);
  assembler_->Bind(&fail);
  assembler_->Return(assembler_->NullConstant());
  assembler_->Bind(&pass);
}

void FastAccessorAssembler::CheckNotZeroOrReturnNull(ValueId value) {
  CHECK_EQ(kBuilding, state_);
  CodeStubAssembler::Label is_null(assembler_.get());
  CodeStubAssembler::Label not_null(assembler_.get());
  assembler_->Branch(
      assembler_->WordEqual(FromId(value), assembler_->IntPtrConstant(0)),
      &is_null, &not_null);
  assembler_->Bind(&is_null);
  assembler_->Return(assembler_->NullConstant());
  assembler_->Bind(&not_null);
}

FastAccessorAssembler::LabelId FastAccessorAssembler::MakeLabel() {
  CHECK_EQ(kBuilding, state_);
  return FromRaw(new CodeStubAssembler::Label(assembler_.get()));
}

void FastAccessorAssembler::SetLabel(LabelId label_id) {
  CHECK_EQ(kBuilding, state_);
  assembler_->Bind(FromId(label_id));
}

void FastAccessorAssembler::Goto(LabelId label_id) {
  CHECK_EQ(kBuilding, state_);
  assembler_->Goto(FromId(label_id));
}

void FastAccessorAssembler::CheckNotZeroOrJump(ValueId value_id,
                                               LabelId label_id) {
  CHECK_EQ(kBuilding, state_);
  CodeStubAssembler::Label pass(assembler_.get());
  assembler_->Branch(
      assembler_->WordEqual(FromId(value_id), assembler_->IntPtrConstant(0)),
      FromId(label_id), &pass);
  assembler_->Bind(&pass);
}

FastAccessorAssembler::ValueId FastAccessorAssembler::Call(
    FunctionCallback callback_function, ValueId arg) {
  CHECK_EQ(kBuilding, state_);

  // Wrap the FunctionCallback in an ExternalReference.
  ApiFunction callback_api_function(FUNCTION_ADDR(callback_function));
  ExternalReference callback(&callback_api_function,
                             ExternalReference::DIRECT_API_CALL, isolate());

  // Create & call API callback via stub.
  const int kJSParameterCount = 1;
  CallApiCallbackStub stub(isolate(), kJSParameterCount, true, true);
  CallInterfaceDescriptor descriptor = stub.GetCallInterfaceDescriptor();
  DCHECK_EQ(4, descriptor.GetParameterCount());
  DCHECK_EQ(0, descriptor.GetStackParameterCount());
  // TODO(vogelheim): There is currently no clean way to retrieve the context
  //     parameter for a stub and the implementation details are hidden in
  //     compiler/*. The context_paramter is computed as:
  //       Linkage::GetJSCallContextParamIndex(descriptor->JSParameterCount())
  const int kContextParameter = 3;
  Node* context = assembler_->Parameter(kContextParameter);
  Node* target = assembler_->HeapConstant(stub.GetCode());

  int param_count = descriptor.GetParameterCount();
  Node** args = zone()->NewArray<Node*>(param_count + 1 + kJSParameterCount);
  // Stub/register parameters:
  args[0] = assembler_->UndefinedConstant();  // callee (there's no JSFunction)
  args[1] = assembler_->UndefinedConstant();  // call_data (undefined)
  args[2] = assembler_->Parameter(0);  // receiver (same as holder in this case)
  args[3] = assembler_->ExternalConstant(callback);  // API callback function

  // JS arguments, on stack:
  args[4] = FromId(arg);

  // Context.
  args[5] = context;

  Node* call =
      assembler_->CallStubN(descriptor, kJSParameterCount, target, args);

  return FromRaw(call);
}

void FastAccessorAssembler::CheckIsJSObjectOrJump(ValueId value_id,
                                                  LabelId label_id) {
  CHECK_EQ(kBuilding, state_);

  // Determine the 'value' object's instance type.
  Node* object_map = assembler_->LoadObjectField(
      FromId(value_id), Internals::kHeapObjectMapOffset,
      MachineType::Pointer());

  Node* instance_type = assembler_->WordAnd(
      assembler_->LoadObjectField(object_map,
                                  Internals::kMapInstanceTypeAndBitFieldOffset,
                                  MachineType::Uint16()),
      assembler_->IntPtrConstant(0xff));

  CodeStubAssembler::Label is_jsobject(assembler_.get());

  // Check whether we have a proper JSObject.
  assembler_->GotoIf(
      assembler_->WordEqual(
          instance_type, assembler_->IntPtrConstant(Internals::kJSObjectType)),
      &is_jsobject);

  // JSApiObject?.
  assembler_->GotoUnless(
      assembler_->WordEqual(instance_type, assembler_->IntPtrConstant(
                                               Internals::kJSApiObjectType)),
      FromId(label_id));

  // Continue.
  assembler_->Goto(&is_jsobject);
  assembler_->Bind(&is_jsobject);
}

MaybeHandle<Code> FastAccessorAssembler::Build() {
  CHECK_EQ(kBuilding, state_);
  Handle<Code> code = assembler_->GenerateCode();
  state_ = !code.is_null() ? kBuilt : kError;
  Clear();
  return code;
}

FastAccessorAssembler::ValueId FastAccessorAssembler::FromRaw(Node* node) {
  nodes_.push_back(node);
  ValueId value = {nodes_.size() - 1};
  return value;
}

FastAccessorAssembler::LabelId FastAccessorAssembler::FromRaw(
    CodeStubAssembler::Label* label) {
  labels_.push_back(label);
  LabelId label_id = {labels_.size() - 1};
  return label_id;
}

Node* FastAccessorAssembler::FromId(ValueId value) const {
  CHECK_LT(value.value_id, nodes_.size());
  CHECK_NOT_NULL(nodes_.at(value.value_id));
  return nodes_.at(value.value_id);
}

CodeStubAssembler::Label* FastAccessorAssembler::FromId(LabelId label) const {
  CHECK_LT(label.label_id, labels_.size());
  CHECK_NOT_NULL(labels_.at(label.label_id));
  return labels_.at(label.label_id);
}

void FastAccessorAssembler::Clear() {
  for (auto label : labels_) {
    delete label;
  }
  nodes_.clear();
  labels_.clear();
}

}  // namespace internal
}  // namespace v8
