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
FeedbackSlot FeedbackVectorSpecBase<Derived>::AddSlot(FeedbackSlotKind kind) {
  int slot = This()->slots();
  int entries_per_slot = FeedbackMetadata::GetSlotSize(kind);
  This()->append(kind);
  for (int i = 1; i < entries_per_slot; i++) {
    This()->append(FeedbackSlotKind::kInvalid);
  }
  return FeedbackSlot(slot);
}

template <typename Derived>
FeedbackSlot FeedbackVectorSpecBase<Derived>::AddTypeProfileSlot() {
  DCHECK(FLAG_type_profile);
  FeedbackSlot slot = AddSlot(FeedbackSlotKind::kTypeProfile);
  CHECK_EQ(FeedbackVectorSpec::kTypeProfileSlotIndex,
           FeedbackVector::GetIndex(slot));
  return slot;
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

int FeedbackMetadata::GetSlotSize(FeedbackSlotKind kind) {
  switch (kind) {
    case FeedbackSlotKind::kGeneral:
    case FeedbackSlotKind::kCompareOp:
    case FeedbackSlotKind::kBinaryOp:
    case FeedbackSlotKind::kToBoolean:
    case FeedbackSlotKind::kLiteral:
    case FeedbackSlotKind::kCreateClosure:
    case FeedbackSlotKind::kTypeProfile:
      return 1;

    case FeedbackSlotKind::kCall:
    case FeedbackSlotKind::kLoadProperty:
    case FeedbackSlotKind::kLoadGlobalInsideTypeof:
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
    case FeedbackSlotKind::kLoadKeyed:
    case FeedbackSlotKind::kStoreNamedSloppy:
    case FeedbackSlotKind::kStoreNamedStrict:
    case FeedbackSlotKind::kStoreOwnNamed:
    case FeedbackSlotKind::kStoreGlobalSloppy:
    case FeedbackSlotKind::kStoreGlobalStrict:
    case FeedbackSlotKind::kStoreKeyedSloppy:
    case FeedbackSlotKind::kStoreKeyedStrict:
    case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      return 2;

    case FeedbackSlotKind::kInvalid:
    case FeedbackSlotKind::kKindsNumber:
      UNREACHABLE();
      break;
  }
  return 1;
}

bool FeedbackVector::is_empty() const {
  return length() == kReservedIndexCount;
}

int FeedbackVector::slot_count() const {
  return length() - kReservedIndexCount;
}

FeedbackMetadata* FeedbackVector::metadata() const {
  return shared_function_info()->feedback_metadata();
}

SharedFunctionInfo* FeedbackVector::shared_function_info() const {
  return SharedFunctionInfo::cast(get(kSharedFunctionInfoIndex));
}

int FeedbackVector::invocation_count() const {
  return Smi::cast(get(kInvocationCountIndex))->value();
}

void FeedbackVector::clear_invocation_count() {
  set(kInvocationCountIndex, Smi::kZero);
}

Code* FeedbackVector::optimized_code() const {
  WeakCell* cell = WeakCell::cast(get(kOptimizedCodeIndex));
  return cell->cleared() ? nullptr : Code::cast(cell->value());
}

bool FeedbackVector::has_optimized_code() const {
  return !WeakCell::cast(get(kOptimizedCodeIndex))->cleared();
}

// Conversion from an integer index to either a slot or an ic slot.
// static
FeedbackSlot FeedbackVector::ToSlot(int index) {
  DCHECK_GE(index, kReservedIndexCount);
  return FeedbackSlot(index - kReservedIndexCount);
}

Object* FeedbackVector::Get(FeedbackSlot slot) const {
  return get(GetIndex(slot));
}

void FeedbackVector::Set(FeedbackSlot slot, Object* value,
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
    case CompareOperationFeedback::kReceiver:
      return CompareOperationHint::kReceiver;
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
    FeedbackSlot slot = iter.Next();
    FeedbackSlotKind kind = iter.kind();

    Object* const obj = Get(slot);
    switch (kind) {
      case FeedbackSlotKind::kCall:
      case FeedbackSlotKind::kLoadProperty:
      case FeedbackSlotKind::kLoadGlobalInsideTypeof:
      case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
      case FeedbackSlotKind::kLoadKeyed:
      case FeedbackSlotKind::kStoreNamedSloppy:
      case FeedbackSlotKind::kStoreNamedStrict:
      case FeedbackSlotKind::kStoreOwnNamed:
      case FeedbackSlotKind::kStoreGlobalSloppy:
      case FeedbackSlotKind::kStoreGlobalStrict:
      case FeedbackSlotKind::kStoreKeyedSloppy:
      case FeedbackSlotKind::kStoreKeyedStrict:
      case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      case FeedbackSlotKind::kTypeProfile: {
        if (obj->IsWeakCell() || obj->IsFixedArray() || obj->IsString()) {
          with++;
        } else if (obj == megamorphic_sentinel) {
          gen++;
          if (code_is_interpreted) with++;
        }
        total++;
        break;
      }
      case FeedbackSlotKind::kBinaryOp:
        // If we are not running interpreted code, we need to ignore the special
        // IC slots for binaryop/compare used by the interpreter.
        // TODO(mvstanton): Remove code_is_interpreted when full code is retired
        // from service.
        if (code_is_interpreted) {
          int const feedback = Smi::cast(obj)->value();
          BinaryOperationHint hint = BinaryOperationHintFromFeedback(feedback);
          if (hint == BinaryOperationHint::kAny) {
            gen++;
          }
          if (hint != BinaryOperationHint::kNone) {
            with++;
          }
          total++;
        }
        break;
      case FeedbackSlotKind::kCompareOp: {
        // If we are not running interpreted code, we need to ignore the special
        // IC slots for binaryop/compare used by the interpreter.
        // TODO(mvstanton): Remove code_is_interpreted when full code is retired
        // from service.
        if (code_is_interpreted) {
          int const feedback = Smi::cast(obj)->value();
          CompareOperationHint hint =
              CompareOperationHintFromFeedback(feedback);
          if (hint == CompareOperationHint::kAny) {
            gen++;
          }
          if (hint != CompareOperationHint::kNone) {
            with++;
          }
          total++;
        }
        break;
      }
      case FeedbackSlotKind::kToBoolean:
      case FeedbackSlotKind::kCreateClosure:
      case FeedbackSlotKind::kGeneral:
      case FeedbackSlotKind::kLiteral:
        break;
      case FeedbackSlotKind::kInvalid:
      case FeedbackSlotKind::kKindsNumber:
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

FeedbackSlot FeedbackMetadataIterator::Next() {
  DCHECK(HasNext());
  cur_slot_ = next_slot_;
  slot_kind_ = metadata()->GetKind(cur_slot_);
  next_slot_ = FeedbackSlot(next_slot_.ToInt() + entry_size());
  return cur_slot_;
}

int FeedbackMetadataIterator::entry_size() const {
  return FeedbackMetadata::GetSlotSize(kind());
}

Object* FeedbackNexus::GetFeedback() const { return vector()->Get(slot()); }

Object* FeedbackNexus::GetFeedbackExtra() const {
#ifdef DEBUG
  FeedbackSlotKind kind = vector()->GetKind(slot());
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
  FeedbackSlotKind kind = vector()->GetKind(slot());
  DCHECK_LT(1, FeedbackMetadata::GetSlotSize(kind));
#endif
  int index = vector()->GetIndex(slot()) + 1;
  vector()->set(index, feedback_extra, mode);
}

Isolate* FeedbackNexus::GetIsolate() const { return vector()->GetIsolate(); }
}  // namespace internal
}  // namespace v8

#endif  // V8_FEEDBACK_VECTOR_INL_H_
