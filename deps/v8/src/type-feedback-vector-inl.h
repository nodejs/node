// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPE_FEEDBACK_VECTOR_INL_H_
#define V8_TYPE_FEEDBACK_VECTOR_INL_H_

#include "src/type-feedback-vector.h"

namespace v8 {
namespace internal {


template <typename Derived>
FeedbackVectorSlot FeedbackVectorSpecBase<Derived>::AddSlot(
    FeedbackVectorSlotKind kind) {
  Derived* derived = static_cast<Derived*>(this);

  int slot = derived->slots();
  int entries_per_slot = TypeFeedbackMetadata::GetSlotSize(kind);
  derived->append(kind);
  for (int i = 1; i < entries_per_slot; i++) {
    derived->append(FeedbackVectorSlotKind::INVALID);
  }
  return FeedbackVectorSlot(slot);
}


// static
TypeFeedbackMetadata* TypeFeedbackMetadata::cast(Object* obj) {
  DCHECK(obj->IsTypeFeedbackVector());
  return reinterpret_cast<TypeFeedbackMetadata*>(obj);
}


int TypeFeedbackMetadata::slot_count() const {
  if (length() == 0) return 0;
  DCHECK(length() > kReservedIndexCount);
  return Smi::cast(get(kSlotsCountIndex))->value();
}


// static
TypeFeedbackVector* TypeFeedbackVector::cast(Object* obj) {
  DCHECK(obj->IsTypeFeedbackVector());
  return reinterpret_cast<TypeFeedbackVector*>(obj);
}


int TypeFeedbackMetadata::GetSlotSize(FeedbackVectorSlotKind kind) {
  DCHECK_NE(FeedbackVectorSlotKind::INVALID, kind);
  DCHECK_NE(FeedbackVectorSlotKind::KINDS_NUMBER, kind);
  return kind == FeedbackVectorSlotKind::GENERAL ? 1 : 2;
}


bool TypeFeedbackVector::is_empty() const {
  if (length() == 0) return true;
  DCHECK(length() > kReservedIndexCount);
  return false;
}


int TypeFeedbackVector::slot_count() const {
  if (length() == 0) return 0;
  DCHECK(length() > kReservedIndexCount);
  return length() - kReservedIndexCount;
}


TypeFeedbackMetadata* TypeFeedbackVector::metadata() const {
  return is_empty() ? TypeFeedbackMetadata::cast(GetHeap()->empty_fixed_array())
                    : TypeFeedbackMetadata::cast(get(kMetadataIndex));
}


FeedbackVectorSlotKind TypeFeedbackVector::GetKind(
    FeedbackVectorSlot slot) const {
  DCHECK(!is_empty());
  return metadata()->GetKind(slot);
}


int TypeFeedbackVector::GetIndex(FeedbackVectorSlot slot) const {
  DCHECK(slot.ToInt() < slot_count());
  return kReservedIndexCount + slot.ToInt();
}


// Conversion from an integer index to either a slot or an ic slot. The caller
// should know what kind she expects.
FeedbackVectorSlot TypeFeedbackVector::ToSlot(int index) const {
  DCHECK(index >= kReservedIndexCount && index < length());
  return FeedbackVectorSlot(index - kReservedIndexCount);
}


Object* TypeFeedbackVector::Get(FeedbackVectorSlot slot) const {
  return get(GetIndex(slot));
}


void TypeFeedbackVector::Set(FeedbackVectorSlot slot, Object* value,
                             WriteBarrierMode mode) {
  set(GetIndex(slot), value, mode);
}


void TypeFeedbackVector::ComputeCounts(int* with_type_info, int* generic) {
  Object* uninitialized_sentinel =
      TypeFeedbackVector::RawUninitializedSentinel(GetIsolate());
  Object* megamorphic_sentinel =
      *TypeFeedbackVector::MegamorphicSentinel(GetIsolate());
  int with = 0;
  int gen = 0;
  TypeFeedbackMetadataIterator iter(metadata());
  while (iter.HasNext()) {
    FeedbackVectorSlot slot = iter.Next();
    FeedbackVectorSlotKind kind = iter.kind();

    Object* obj = Get(slot);
    if (obj != uninitialized_sentinel &&
        kind != FeedbackVectorSlotKind::GENERAL) {
      if (obj->IsWeakCell() || obj->IsFixedArray() || obj->IsString()) {
        with++;
      } else if (obj == megamorphic_sentinel) {
        gen++;
      }
    }
  }

  *with_type_info = with;
  *generic = gen;
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


Object* TypeFeedbackVector::RawUninitializedSentinel(Isolate* isolate) {
  return isolate->heap()->uninitialized_symbol();
}


Object* FeedbackNexus::GetFeedback() const { return vector()->Get(slot()); }


Object* FeedbackNexus::GetFeedbackExtra() const {
#ifdef DEBUG
  FeedbackVectorSlotKind kind = vector()->GetKind(slot());
  DCHECK_LT(1, TypeFeedbackMetadata::GetSlotSize(kind));
#endif
  int extra_index = vector()->GetIndex(slot()) + 1;
  return vector()->get(extra_index);
}


void FeedbackNexus::SetFeedback(Object* feedback, WriteBarrierMode mode) {
  vector()->Set(slot(), feedback, mode);
}


void FeedbackNexus::SetFeedbackExtra(Object* feedback_extra,
                                     WriteBarrierMode mode) {
#ifdef DEBUG
  FeedbackVectorSlotKind kind = vector()->GetKind(slot());
  DCHECK_LT(1, TypeFeedbackMetadata::GetSlotSize(kind));
#endif
  int index = vector()->GetIndex(slot()) + 1;
  vector()->set(index, feedback_extra, mode);
}


Isolate* FeedbackNexus::GetIsolate() const { return vector()->GetIsolate(); }
}  // namespace internal
}  // namespace v8

#endif  // V8_TYPE_FEEDBACK_VECTOR_INL_H_
