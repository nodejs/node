// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FEEDBACK_VECTOR_INL_H_
#define V8_FEEDBACK_VECTOR_INL_H_

#include "src/factory-inl.h"
#include "src/feedback-vector.h"
#include "src/globals.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/objects/shared-function-info.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

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
  DCHECK_GT(length(), kReservedIndexCount);
  return Smi::ToInt(get(kSlotsCountIndex));
}

// static
FeedbackVector* FeedbackVector::cast(Object* obj) {
  DCHECK(obj->IsFeedbackVector());
  return reinterpret_cast<FeedbackVector*>(obj);
}

int FeedbackMetadata::GetSlotSize(FeedbackSlotKind kind) {
  switch (kind) {
    case FeedbackSlotKind::kForIn:
    case FeedbackSlotKind::kInstanceOf:
    case FeedbackSlotKind::kCompareOp:
    case FeedbackSlotKind::kBinaryOp:
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

ACCESSORS(FeedbackVector, shared_function_info, SharedFunctionInfo,
          kSharedFunctionInfoOffset)
ACCESSORS(FeedbackVector, optimized_code_cell, Object, kOptimizedCodeOffset)
INT32_ACCESSORS(FeedbackVector, length, kLengthOffset)
INT32_ACCESSORS(FeedbackVector, invocation_count, kInvocationCountOffset)
INT32_ACCESSORS(FeedbackVector, profiler_ticks, kProfilerTicksOffset)
INT32_ACCESSORS(FeedbackVector, deopt_count, kDeoptCountOffset)

bool FeedbackVector::is_empty() const { return length() == 0; }

FeedbackMetadata* FeedbackVector::metadata() const {
  return shared_function_info()->feedback_metadata();
}

void FeedbackVector::clear_invocation_count() { set_invocation_count(0); }

void FeedbackVector::increment_deopt_count() {
  int count = deopt_count();
  if (count < std::numeric_limits<int32_t>::max()) {
    set_deopt_count(count + 1);
  }
}

Code* FeedbackVector::optimized_code() const {
  Object* slot = optimized_code_cell();
  if (slot->IsSmi()) return nullptr;
  WeakCell* cell = WeakCell::cast(slot);
  return cell->cleared() ? nullptr : Code::cast(cell->value());
}

OptimizationMarker FeedbackVector::optimization_marker() const {
  Object* slot = optimized_code_cell();
  if (!slot->IsSmi()) return OptimizationMarker::kNone;
  Smi* value = Smi::cast(slot);
  return static_cast<OptimizationMarker>(value->value());
}

bool FeedbackVector::has_optimized_code() const {
  return optimized_code() != nullptr;
}

bool FeedbackVector::has_optimization_marker() const {
  return optimization_marker() != OptimizationMarker::kNone;
}

// Conversion from an integer index to either a slot or an ic slot.
// static
FeedbackSlot FeedbackVector::ToSlot(int index) {
  DCHECK_GE(index, 0);
  return FeedbackSlot(index);
}

Object* FeedbackVector::Get(FeedbackSlot slot) const {
  return get(GetIndex(slot));
}

Object* FeedbackVector::get(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kFeedbackSlotsOffset + index * kPointerSize;
  return RELAXED_READ_FIELD(this, offset);
}

void FeedbackVector::Set(FeedbackSlot slot, Object* value,
                         WriteBarrierMode mode) {
  set(GetIndex(slot), value, mode);
}

void FeedbackVector::set(int index, Object* value, WriteBarrierMode mode) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kFeedbackSlotsOffset + index * kPointerSize;
  RELAXED_WRITE_FIELD(this, offset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode);
}

inline Object** FeedbackVector::slots_start() {
  return HeapObject::RawField(this, kFeedbackSlotsOffset);
}

// Helper function to transform the feedback to BinaryOperationHint.
BinaryOperationHint BinaryOperationHintFromFeedback(int type_feedback) {
  switch (type_feedback) {
    case BinaryOperationFeedback::kNone:
      return BinaryOperationHint::kNone;
    case BinaryOperationFeedback::kSignedSmall:
      return BinaryOperationHint::kSignedSmall;
    case BinaryOperationFeedback::kSignedSmallInputs:
      return BinaryOperationHint::kSignedSmallInputs;
    case BinaryOperationFeedback::kNumber:
      return BinaryOperationHint::kNumber;
    case BinaryOperationFeedback::kNumberOrOddball:
      return BinaryOperationHint::kNumberOrOddball;
    case BinaryOperationFeedback::kString:
      return BinaryOperationHint::kString;
    case BinaryOperationFeedback::kBigInt:
    // TODO(jarin/jkummerow/neis): Support BigInts in TF.
    // Fall through for now.
    case BinaryOperationFeedback::kAny:
    default:
      return BinaryOperationHint::kAny;
  }
  UNREACHABLE();
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
    case CompareOperationFeedback::kSymbol:
      return CompareOperationHint::kSymbol;
    case CompareOperationFeedback::kReceiver:
      return CompareOperationHint::kReceiver;
    default:
      return CompareOperationHint::kAny;
  }
  UNREACHABLE();
}

// Helper function to transform the feedback to ForInHint.
ForInHint ForInHintFromFeedback(int type_feedback) {
  switch (type_feedback) {
    case ForInFeedback::kNone:
      return ForInHint::kNone;
    case ForInFeedback::kEnumCacheKeys:
      return ForInHint::kEnumCacheKeys;
    case ForInFeedback::kEnumCacheKeysAndIndices:
      return ForInHint::kEnumCacheKeysAndIndices;
    default:
      return ForInHint::kAny;
  }
  UNREACHABLE();
}

void FeedbackVector::ComputeCounts(int* with_type_info, int* generic,
                                   int* vector_ic_count) {
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
          with++;
        }
        total++;
        break;
      }
      case FeedbackSlotKind::kBinaryOp: {
        int const feedback = Smi::ToInt(obj);
        BinaryOperationHint hint = BinaryOperationHintFromFeedback(feedback);
        if (hint == BinaryOperationHint::kAny) {
          gen++;
        }
        if (hint != BinaryOperationHint::kNone) {
          with++;
        }
        total++;
        break;
      }
      case FeedbackSlotKind::kCompareOp: {
          int const feedback = Smi::ToInt(obj);
          CompareOperationHint hint =
              CompareOperationHintFromFeedback(feedback);
          if (hint == CompareOperationHint::kAny) {
            gen++;
          }
          if (hint != CompareOperationHint::kNone) {
            with++;
          }
          total++;
        break;
      }
      case FeedbackSlotKind::kForIn: {
        int const feedback = Smi::ToInt(obj);
        ForInHint hint = ForInHintFromFeedback(feedback);
        if (hint == ForInHint::kAny) {
          gen++;
        }
        if (hint != ForInHint::kNone) {
          with++;
        }
        total++;
        break;
      }
      case FeedbackSlotKind::kInstanceOf: {
        if (obj->IsWeakCell()) {
          with++;
        } else if (obj == megamorphic_sentinel) {
          gen++;
          with++;
        }
        total++;
        break;
      }
      case FeedbackSlotKind::kCreateClosure:
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

Handle<Symbol> FeedbackVector::GenericSentinel(Isolate* isolate) {
  return isolate->factory()->generic_symbol();
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

#include "src/objects/object-macros-undef.h"

#endif  // V8_FEEDBACK_VECTOR_INL_H_
