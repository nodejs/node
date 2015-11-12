// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPE_FEEDBACK_VECTOR_INL_H_
#define V8_TYPE_FEEDBACK_VECTOR_INL_H_

#include "src/type-feedback-vector.h"

namespace v8 {
namespace internal {

// static
TypeFeedbackVector* TypeFeedbackVector::cast(Object* obj) {
  DCHECK(obj->IsTypeFeedbackVector());
  return reinterpret_cast<TypeFeedbackVector*>(obj);
}


int TypeFeedbackVector::first_ic_slot_index() const {
  DCHECK(length() >= kReservedIndexCount);
  return Smi::cast(get(kFirstICSlotIndex))->value();
}


int TypeFeedbackVector::ic_with_type_info_count() {
  return length() > 0 ? Smi::cast(get(kWithTypesIndex))->value() : 0;
}


void TypeFeedbackVector::change_ic_with_type_info_count(int delta) {
  if (delta == 0) return;
  int value = ic_with_type_info_count() + delta;
  // Could go negative because of the debugger.
  if (value >= 0) {
    set(kWithTypesIndex, Smi::FromInt(value));
  }
}


int TypeFeedbackVector::ic_generic_count() {
  return length() > 0 ? Smi::cast(get(kGenericCountIndex))->value() : 0;
}


void TypeFeedbackVector::change_ic_generic_count(int delta) {
  if (delta == 0) return;
  int value = ic_generic_count() + delta;
  if (value >= 0) {
    set(kGenericCountIndex, Smi::FromInt(value));
  }
}


int TypeFeedbackVector::Slots() const {
  if (length() == 0) return 0;
  return Max(
      0, first_ic_slot_index() - ic_metadata_length() - kReservedIndexCount);
}


int TypeFeedbackVector::ICSlots() const {
  if (length() == 0) return 0;
  return (length() - first_ic_slot_index()) / elements_per_ic_slot();
}


int TypeFeedbackVector::ic_metadata_length() const {
  return VectorICComputer::word_count(ICSlots());
}


// Conversion from a slot or ic slot to an integer index to the underlying
// array.
int TypeFeedbackVector::GetIndex(FeedbackVectorSlot slot) const {
  DCHECK(slot.ToInt() < first_ic_slot_index());
  return kReservedIndexCount + ic_metadata_length() + slot.ToInt();
}


int TypeFeedbackVector::GetIndex(FeedbackVectorICSlot slot) const {
  int first_ic_slot = first_ic_slot_index();
  DCHECK(slot.ToInt() < ICSlots());
  return first_ic_slot + slot.ToInt() * elements_per_ic_slot();
}


// Conversion from an integer index to either a slot or an ic slot. The caller
// should know what kind she expects.
FeedbackVectorSlot TypeFeedbackVector::ToSlot(int index) const {
  DCHECK(index >= kReservedIndexCount && index < first_ic_slot_index());
  return FeedbackVectorSlot(index - ic_metadata_length() - kReservedIndexCount);
}


FeedbackVectorICSlot TypeFeedbackVector::ToICSlot(int index) const {
  DCHECK(index >= first_ic_slot_index() && index < length());
  int ic_slot = (index - first_ic_slot_index()) / elements_per_ic_slot();
  return FeedbackVectorICSlot(ic_slot);
}


Object* TypeFeedbackVector::Get(FeedbackVectorSlot slot) const {
  return get(GetIndex(slot));
}


void TypeFeedbackVector::Set(FeedbackVectorSlot slot, Object* value,
                             WriteBarrierMode mode) {
  set(GetIndex(slot), value, mode);
}


Object* TypeFeedbackVector::Get(FeedbackVectorICSlot slot) const {
  return get(GetIndex(slot));
}


void TypeFeedbackVector::Set(FeedbackVectorICSlot slot, Object* value,
                             WriteBarrierMode mode) {
  set(GetIndex(slot), value, mode);
}


Handle<Object> TypeFeedbackVector::UninitializedSentinel(Isolate* isolate) {
  return isolate->factory()->uninitialized_symbol();
}


Handle<Object> TypeFeedbackVector::MegamorphicSentinel(Isolate* isolate) {
  return isolate->factory()->megamorphic_symbol();
}


Handle<Object> TypeFeedbackVector::PremonomorphicSentinel(Isolate* isolate) {
  return isolate->factory()->premonomorphic_symbol();
}


Object* TypeFeedbackVector::RawUninitializedSentinel(Heap* heap) {
  return heap->uninitialized_symbol();
}


Object* FeedbackNexus::GetFeedback() const { return vector()->Get(slot()); }


Object* FeedbackNexus::GetFeedbackExtra() const {
  DCHECK(TypeFeedbackVector::elements_per_ic_slot() > 1);
  int extra_index = vector()->GetIndex(slot()) + 1;
  return vector()->get(extra_index);
}


void FeedbackNexus::SetFeedback(Object* feedback, WriteBarrierMode mode) {
  vector()->Set(slot(), feedback, mode);
}


void FeedbackNexus::SetFeedbackExtra(Object* feedback_extra,
                                     WriteBarrierMode mode) {
  DCHECK(TypeFeedbackVector::elements_per_ic_slot() > 1);
  int index = vector()->GetIndex(slot()) + 1;
  vector()->set(index, feedback_extra, mode);
}


Isolate* FeedbackNexus::GetIsolate() const { return vector()->GetIsolate(); }
}
}  // namespace v8::internal

#endif  // V8_TYPE_FEEDBACK_VECTOR_INL_H_
