// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FEEDBACK_VECTOR_INL_H_
#define V8_FEEDBACK_VECTOR_INL_H_

#include "src/factory.h"
#include "src/feedback-vector.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

template <typename Derived>
FeedbackVectorSlot FeedbackVectorSpecBase<Derived>::AddSlot(
    FeedbackVectorSlotKind kind) {
  int slot = This()->slots();
  int entries_per_slot = FeedbackMetadata::GetSlotSize(kind);
  This()->append(kind);
  for (int i = 1; i < entries_per_slot; i++) {
    This()->append(FeedbackVectorSlotKind::INVALID);
  }
  return FeedbackVectorSlot(slot);
}

// static
FeedbackMetadata* FeedbackMetadata::cast(Object* obj) {
  DCHECK(obj->IsFeedbackMetadata());
  return reinterpret_cast<FeedbackMetadata*>(obj);
}

bool FeedbackMetadata::is_empty() const {
  if (length() == 0) return true;
  return false;
}

int FeedbackMetadata::slot_count() const {
  if (length() == 0) return 0;
  DCHECK(length() > kReservedIndexCount);
  return Smi::cast(get(kSlotsCountIndex))->value();
}

// static
FeedbackVector* FeedbackVector::cast(Object* obj) {
  DCHECK(obj->IsFeedbackVector());
  return reinterpret_cast<FeedbackVector*>(obj);
}


int FeedbackMetadata::GetSlotSize(FeedbackVectorSlotKind kind) {
  DCHECK_NE(FeedbackVectorSlotKind::INVALID, kind);
  DCHECK_NE(FeedbackVectorSlotKind::KINDS_NUMBER, kind);
  if (kind == FeedbackVectorSlotKind::GENERAL ||
      kind == FeedbackVectorSlotKind::INTERPRETER_BINARYOP_IC ||
      kind == FeedbackVectorSlotKind::INTERPRETER_COMPARE_IC ||
      kind == FeedbackVectorSlotKind::CREATE_CLOSURE) {
    return 1;
  }

  return 2;
}

bool FeedbackMetadata::SlotRequiresParameter(FeedbackVectorSlotKind kind) {
  switch (kind) {
    case FeedbackVectorSlotKind::CREATE_CLOSURE:
      return true;

    case FeedbackVectorSlotKind::CALL_IC:
    case FeedbackVectorSlotKind::LOAD_IC:
    case FeedbackVectorSlotKind::LOAD_GLOBAL_IC:
    case FeedbackVectorSlotKind::KEYED_LOAD_IC:
    case FeedbackVectorSlotKind::STORE_IC:
    case FeedbackVectorSlotKind::KEYED_STORE_IC:
    case FeedbackVectorSlotKind::INTERPRETER_BINARYOP_IC:
    case FeedbackVectorSlotKind::INTERPRETER_COMPARE_IC:
    case FeedbackVectorSlotKind::STORE_DATA_PROPERTY_IN_LITERAL_IC:
    case FeedbackVectorSlotKind::GENERAL:
    case FeedbackVectorSlotKind::INVALID:
      return false;

    case FeedbackVectorSlotKind::KINDS_NUMBER:
      break;
  }
  UNREACHABLE();
  return false;
}

bool FeedbackVector::is_empty() const {
  return length() == kReservedIndexCount;
}

int FeedbackVector::slot_count() const {
  return length() - kReservedIndexCount;
}

FeedbackMetadata* FeedbackVector::metadata() const {
  return FeedbackMetadata::cast(get(kMetadataIndex));
}

int FeedbackVector::invocation_count() const {
  return Smi::cast(get(kInvocationCountIndex))->value();
}

// Conversion from an integer index to either a slot or an ic slot.
// static
FeedbackVectorSlot FeedbackVector::ToSlot(int index) {
  DCHECK_GE(index, kReservedIndexCount);
  return FeedbackVectorSlot(index - kReservedIndexCount);
}

Object* FeedbackVector::Get(FeedbackVectorSlot slot) const {
  return get(GetIndex(slot));
}

void FeedbackVector::Set(FeedbackVectorSlot slot, Object* value,
                         WriteBarrierMode mode) {
  set(GetIndex(slot), value, mode);
}

// Helper function to transform the feedback to BinaryOperationHint.
BinaryOperationHint BinaryOperationHintFromFeedback(int type_feedback) {
  switch (type_feedback) {
    case BinaryOperationFeedback::kNone:
      return BinaryOperationHint::kNone;
    case BinaryOperationFeedback::kSignedSmall:
      return BinaryOperationHint::kSignedSmall;
    case BinaryOperationFeedback::kNumber:
    case BinaryOperationFeedback::kNumberOrOddball:
      return BinaryOperationHint::kNumberOrOddball;
    case BinaryOperationFeedback::kString:
      return BinaryOperationHint::kString;
    case BinaryOperationFeedback::kAny:
    default:
      return BinaryOperationHint::kAny;
  }
  UNREACHABLE();
  return BinaryOperationHint::kNone;
}

// Helper function to transform the feedback to CompareOperationHint.
CompareOperationHint CompareOperationHintFromFeedback(int type_feedback) {
  switch (type_feedback) {
    case CompareOperationFeedback::kNone:
      return CompareOperationHint::kNone;
    case CompareOperationFeedback::kSignedSmall:
      return CompareOperationHint::kSignedSmall;
    case CompareOperationFeedback::kNumber:
      return CompareOperationHint::kNumber;
    case CompareOperationFeedback::kNumberOrOddball:
      return CompareOperationHint::kNumberOrOddball;
    case CompareOperationFeedback::kInternalizedString:
      return CompareOperationHint::kInternalizedString;
    case CompareOperationFeedback::kString:
      return CompareOperationHint::kString;
    default:
      return CompareOperationHint::kAny;
  }
  UNREACHABLE();
  return CompareOperationHint::kNone;
}

void FeedbackVector::ComputeCounts(int* with_type_info, int* generic,
                                   int* vector_ic_count,
                                   bool code_is_interpreted) {
  Object* megamorphic_sentinel =
      *FeedbackVector::MegamorphicSentinel(GetIsolate());
  int with = 0;
  int gen = 0;
  int total = 0;
  FeedbackMetadataIterator iter(metadata());
  while (iter.HasNext()) {
    FeedbackVectorSlot slot = iter.Next();
    FeedbackVectorSlotKind kind = iter.kind();

    Object* const obj = Get(slot);
    switch (kind) {
      case FeedbackVectorSlotKind::CALL_IC:
      case FeedbackVectorSlotKind::LOAD_IC:
      case FeedbackVectorSlotKind::LOAD_GLOBAL_IC:
      case FeedbackVectorSlotKind::KEYED_LOAD_IC:
      case FeedbackVectorSlotKind::STORE_IC:
      case FeedbackVectorSlotKind::KEYED_STORE_IC:
      case FeedbackVectorSlotKind::STORE_DATA_PROPERTY_IN_LITERAL_IC: {
        if (obj->IsWeakCell() || obj->IsFixedArray() || obj->IsString()) {
          with++;
        } else if (obj == megamorphic_sentinel) {
          gen++;
        }
        total++;
        break;
      }
      case FeedbackVectorSlotKind::INTERPRETER_BINARYOP_IC:
      case FeedbackVectorSlotKind::INTERPRETER_COMPARE_IC: {
        // If we are not running interpreted code, we need to ignore the special
        // IC slots for binaryop/compare used by the interpreter.
        // TODO(mvstanton): Remove code_is_interpreted when full code is retired
        // from service.
        if (code_is_interpreted) {
          int const feedback = Smi::cast(obj)->value();
          if (kind == FeedbackVectorSlotKind::INTERPRETER_COMPARE_IC) {
            CompareOperationHint hint =
                CompareOperationHintFromFeedback(feedback);
            if (hint == CompareOperationHint::kAny) {
              gen++;
            } else if (hint != CompareOperationHint::kNone) {
              with++;
            }
          } else {
            DCHECK_EQ(FeedbackVectorSlotKind::INTERPRETER_BINARYOP_IC, kind);
            BinaryOperationHint hint =
                BinaryOperationHintFromFeedback(feedback);
            if (hint == BinaryOperationHint::kAny) {
              gen++;
            } else if (hint != BinaryOperationHint::kNone) {
              with++;
            }
          }
          total++;
        }
        break;
      }
      case FeedbackVectorSlotKind::CREATE_CLOSURE:
      case FeedbackVectorSlotKind::GENERAL:
        break;
      case FeedbackVectorSlotKind::INVALID:
      case FeedbackVectorSlotKind::KINDS_NUMBER:
        UNREACHABLE();
        break;
    }
  }

  *with_type_info = with;
  *generic = gen;
  *vector_ic_count = total;
}

Handle<Symbol> FeedbackVector::UninitializedSentinel(Isolate* isolate) {
  return isolate->factory()->uninitialized_symbol();
}

Handle<Symbol> FeedbackVector::MegamorphicSentinel(Isolate* isolate) {
  return isolate->factory()->megamorphic_symbol();
}

Handle<Symbol> FeedbackVector::PremonomorphicSentinel(Isolate* isolate) {
  return isolate->factory()->premonomorphic_symbol();
}

Symbol* FeedbackVector::RawUninitializedSentinel(Isolate* isolate) {
  return isolate->heap()->uninitialized_symbol();
}

bool FeedbackMetadataIterator::HasNext() const {
  return next_slot_.ToInt() < metadata()->slot_count();
}

FeedbackVectorSlot FeedbackMetadataIterator::Next() {
  DCHECK(HasNext());
  cur_slot_ = next_slot_;
  slot_kind_ = metadata()->GetKind(cur_slot_);
  next_slot_ = FeedbackVectorSlot(next_slot_.ToInt() + entry_size());
  return cur_slot_;
}

int FeedbackMetadataIterator::entry_size() const {
  return FeedbackMetadata::GetSlotSize(kind());
}

Object* FeedbackNexus::GetFeedback() const { return vector()->Get(slot()); }

Object* FeedbackNexus::GetFeedbackExtra() const {
#ifdef DEBUG
  FeedbackVectorSlotKind kind = vector()->GetKind(slot());
  DCHECK_LT(1, FeedbackMetadata::GetSlotSize(kind));
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
  DCHECK_LT(1, FeedbackMetadata::GetSlotSize(kind));
#endif
  int index = vector()->GetIndex(slot()) + 1;
  vector()->set(index, feedback_extra, mode);
}

Isolate* FeedbackNexus::GetIsolate() const { return vector()->GetIsolate(); }
}  // namespace internal
}  // namespace v8

#endif  // V8_FEEDBACK_VECTOR_INL_H_
