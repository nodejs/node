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

  // Determine the 'value' object's instance type.
  Node* object_map = assembler_->LoadObjectField(
      FromId(value), Internals::kHeapObjectMapOffset, MachineType::Pointer());
  Node* instance_type = assembler_->WordAnd(
      assembler_->LoadObjectField(object_map,
                                  Internals::kMapInstanceTypeAndBitFieldOffset,
                                  MachineType::Uint16()),
      assembler_->IntPtrConstant(0xff));

  // Check whether we have a proper JSObject.
  CodeStubAssembler::Variable result(assembler_.get(),
                                     MachineRepresentation::kTagged);
  CodeStubAssembler::Label is_jsobject(assembler_.get());
  CodeStubAssembler::Label maybe_api_object(assembler_.get());
  CodeStubAssembler::Label is_not_jsobject(assembler_.get());
  CodeStubAssembler::Label merge(assembler_.get(), &result);
  assembler_->Branch(
      assembler_->WordEqual(
          instance_type, assembler_->IntPtrConstant(Internals::kJSObjectType)),
      &is_jsobject, &maybe_api_object);

  // JSObject? Then load the internal field field_no.
  assembler_->Bind(&is_jsobject);
  Node* internal_field = assembler_->LoadObjectField(
      FromId(value), JSObject::kHeaderSize + kPointerSize * field_no,
      MachineType::Pointer());
  result.Bind(internal_field);
  assembler_->Goto(&merge);

  assembler_->Bind(&maybe_api_object);
  assembler_->Branch(
      assembler_->WordEqual(instance_type, assembler_->IntPtrConstant(
                                               Internals::kJSApiObjectType)),
      &is_jsobject, &is_not_jsobject);

  // No JSObject? Return undefined.
  // TODO(vogelheim): Check whether this is the appropriate action, or whether
  //                  the method should take a label instead.
  assembler_->Bind(&is_not_jsobject);
  Node* fail_value = assembler_->UndefinedConstant();
  result.Bind(fail_value);
  assembler_->Goto(&merge);

  // Return.
  assembler_->Bind(&merge);
  return FromRaw(result.value());
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
      &pass, &fail);
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

void FastAccessorAssembler::CheckNotZeroOrJump(ValueId value_id,
                                               LabelId label_id) {
  CHECK_EQ(kBuilding, state_);
  CodeStubAssembler::Label pass(assembler_.get());
  assembler_->Branch(
      assembler_->WordEqual(FromId(value_id), assembler_->IntPtrConstant(0)),
      &pass, FromId(label_id));
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
  CallApiCallbackStub stub(isolate(), 1, true);
  DCHECK_EQ(5, stub.GetCallInterfaceDescriptor().GetParameterCount());
  DCHECK_EQ(1, stub.GetCallInterfaceDescriptor().GetStackParameterCount());
  // TODO(vogelheim): There is currently no clean way to retrieve the context
  //     parameter for a stub and the implementation details are hidden in
  //     compiler/*. The context_paramter is computed as:
  //       Linkage::GetJSCallContextParamIndex(descriptor->JSParameterCount())
  const int context_parameter = 2;
  Node* call = assembler_->CallStub(
      stub.GetCallInterfaceDescriptor(),
      assembler_->HeapConstant(stub.GetCode()),
      assembler_->Parameter(context_parameter),

      // Stub/register parameters:
      assembler_->Parameter(0),               /* receiver (use accessor's) */
      assembler_->UndefinedConstant(),        /* call_data (undefined) */
      assembler_->NullConstant(),             /* holder (null) */
      assembler_->ExternalConstant(callback), /* API callback function */

      // JS arguments, on stack:
      FromId(arg));

  return FromRaw(call);
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
