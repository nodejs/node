// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/fast-accessor-assembler.h"

#include "src/base/logging.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/schedule.h"
#include "src/compiler/verifier.h"
#include "src/handles-inl.h"
#include "src/objects.h"  // For FAA::GetInternalField impl.

namespace v8 {
namespace internal {
namespace compiler {

FastAccessorAssembler::FastAccessorAssembler(Isolate* isolate)
    : zone_(),
      assembler_(new RawMachineAssembler(
          isolate, new (zone()) Graph(zone()),
          Linkage::GetJSCallDescriptor(&zone_, false, 1,
                                       CallDescriptor::kNoFlags))),
      state_(kBuilding) {}


FastAccessorAssembler::~FastAccessorAssembler() {}


FastAccessorAssembler::ValueId FastAccessorAssembler::IntegerConstant(
    int const_value) {
  CHECK_EQ(kBuilding, state_);
  return FromRaw(assembler_->NumberConstant(const_value));
}


FastAccessorAssembler::ValueId FastAccessorAssembler::GetReceiver() {
  CHECK_EQ(kBuilding, state_);

  // For JS call descriptor, the receiver is parameter 0. If we use other
  // call descriptors, this may or may not hold. So let's check.
  CHECK(assembler_->call_descriptor()->IsJSFunctionCall());
  return FromRaw(assembler_->Parameter(0));
}


FastAccessorAssembler::ValueId FastAccessorAssembler::LoadInternalField(
    ValueId value, int field_no) {
  CHECK_EQ(kBuilding, state_);
  // Determine the 'value' object's instance type.
  Node* object_map =
      assembler_->Load(MachineType::Pointer(), FromId(value),
                       assembler_->IntPtrConstant(
                           Internals::kHeapObjectMapOffset - kHeapObjectTag));
  Node* instance_type = assembler_->WordAnd(
      assembler_->Load(
          MachineType::Uint16(), object_map,
          assembler_->IntPtrConstant(
              Internals::kMapInstanceTypeAndBitFieldOffset - kHeapObjectTag)),
      assembler_->IntPtrConstant(0xff));

  // Check whether we have a proper JSObject.
  RawMachineLabel is_jsobject, is_not_jsobject, merge;
  assembler_->Branch(
      assembler_->WordEqual(
          instance_type, assembler_->IntPtrConstant(Internals::kJSObjectType)),
      &is_jsobject, &is_not_jsobject);

  // JSObject? Then load the internal field field_no.
  assembler_->Bind(&is_jsobject);
  Node* internal_field = assembler_->Load(
      MachineType::Pointer(), FromId(value),
      assembler_->IntPtrConstant(JSObject::kHeaderSize - kHeapObjectTag +
                                 kPointerSize * field_no));
  assembler_->Goto(&merge);

  // No JSObject? Return undefined.
  // TODO(vogelheim): Check whether this is the appropriate action, or whether
  //                  the method should take a label instead.
  assembler_->Bind(&is_not_jsobject);
  Node* fail_value = assembler_->UndefinedConstant();
  assembler_->Goto(&merge);

  // Return.
  assembler_->Bind(&merge);
  Node* phi = assembler_->Phi(MachineRepresentation::kTagged, internal_field,
                              fail_value);
  return FromRaw(phi);
}


FastAccessorAssembler::ValueId FastAccessorAssembler::LoadValue(ValueId value,
                                                                int offset) {
  CHECK_EQ(kBuilding, state_);
  return FromRaw(assembler_->Load(MachineType::IntPtr(), FromId(value),
                                  assembler_->IntPtrConstant(offset)));
}


FastAccessorAssembler::ValueId FastAccessorAssembler::LoadObject(ValueId value,
                                                                 int offset) {
  CHECK_EQ(kBuilding, state_);
  return FromRaw(
      assembler_->Load(MachineType::AnyTagged(),
                       assembler_->Load(MachineType::Pointer(), FromId(value),
                                        assembler_->IntPtrConstant(offset))));
}


void FastAccessorAssembler::ReturnValue(ValueId value) {
  CHECK_EQ(kBuilding, state_);
  assembler_->Return(FromId(value));
}


void FastAccessorAssembler::CheckFlagSetOrReturnNull(ValueId value, int mask) {
  CHECK_EQ(kBuilding, state_);
  RawMachineLabel pass, fail;
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
  RawMachineLabel is_null, not_null;
  assembler_->Branch(
      assembler_->IntPtrEqual(FromId(value), assembler_->IntPtrConstant(0)),
      &is_null, &not_null);
  assembler_->Bind(&is_null);
  assembler_->Return(assembler_->NullConstant());
  assembler_->Bind(&not_null);
}


FastAccessorAssembler::LabelId FastAccessorAssembler::MakeLabel() {
  CHECK_EQ(kBuilding, state_);
  RawMachineLabel* label =
      new (zone()->New(sizeof(RawMachineLabel))) RawMachineLabel;
  return FromRaw(label);
}


void FastAccessorAssembler::SetLabel(LabelId label_id) {
  CHECK_EQ(kBuilding, state_);
  assembler_->Bind(FromId(label_id));
}


void FastAccessorAssembler::CheckNotZeroOrJump(ValueId value_id,
                                               LabelId label_id) {
  CHECK_EQ(kBuilding, state_);
  RawMachineLabel pass;
  assembler_->Branch(
      assembler_->IntPtrEqual(FromId(value_id), assembler_->IntPtrConstant(0)),
      &pass, FromId(label_id));
  assembler_->Bind(&pass);
}


MaybeHandle<Code> FastAccessorAssembler::Build() {
  CHECK_EQ(kBuilding, state_);

  // Cleanup: We no longer need this.
  nodes_.clear();
  labels_.clear();

  // Export the schedule and call the compiler.
  Schedule* schedule = assembler_->Export();
  MaybeHandle<Code> code = Pipeline::GenerateCodeForCodeStub(
      assembler_->isolate(), assembler_->call_descriptor(), assembler_->graph(),
      schedule, Code::STUB, "FastAccessorAssembler");

  // Update state & return.
  state_ = !code.is_null() ? kBuilt : kError;
  return code;
}


FastAccessorAssembler::ValueId FastAccessorAssembler::FromRaw(Node* node) {
  nodes_.push_back(node);
  ValueId value = {nodes_.size() - 1};
  return value;
}


FastAccessorAssembler::LabelId FastAccessorAssembler::FromRaw(
    RawMachineLabel* label) {
  labels_.push_back(label);
  LabelId label_id = {labels_.size() - 1};
  return label_id;
}


Node* FastAccessorAssembler::FromId(ValueId value) const {
  CHECK_LT(value.value_id, nodes_.size());
  CHECK_NOT_NULL(nodes_.at(value.value_id));
  return nodes_.at(value.value_id);
}


RawMachineLabel* FastAccessorAssembler::FromId(LabelId label) const {
  CHECK_LT(label.label_id, labels_.size());
  CHECK_NOT_NULL(labels_.at(label.label_id));
  return labels_.at(label.label_id);
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
