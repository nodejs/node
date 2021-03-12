// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FEEDBACK_VECTOR_INL_H_
#define V8_OBJECTS_FEEDBACK_VECTOR_INL_H_

#include "src/common/globals.h"
#include "src/heap/factory-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/smi.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/feedback-vector-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(FeedbackVector)
OBJECT_CONSTRUCTORS_IMPL(FeedbackMetadata, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(ClosureFeedbackCellArray, FixedArray)

NEVER_READ_ONLY_SPACE_IMPL(FeedbackVector)
NEVER_READ_ONLY_SPACE_IMPL(ClosureFeedbackCellArray)

CAST_ACCESSOR(FeedbackMetadata)
CAST_ACCESSOR(ClosureFeedbackCellArray)

INT32_ACCESSORS(FeedbackMetadata, slot_count, kSlotCountOffset)

INT32_ACCESSORS(FeedbackMetadata, create_closure_slot_count,
                kCreateClosureSlotCountOffset)

RELEASE_ACQUIRE_WEAK_ACCESSORS(FeedbackVector, maybe_optimized_code,
                               kMaybeOptimizedCodeOffset)

int32_t FeedbackMetadata::synchronized_slot_count() const {
  return base::Acquire_Load(
      reinterpret_cast<const base::Atomic32*>(field_address(kSlotCountOffset)));
}

int32_t FeedbackMetadata::get(int index) const {
  DCHECK(index >= 0 && index < length());
  int offset = kHeaderSize + index * kInt32Size;
  return ReadField<int32_t>(offset);
}

void FeedbackMetadata::set(int index, int32_t value) {
  DCHECK(index >= 0 && index < length());
  int offset = kHeaderSize + index * kInt32Size;
  WriteField<int32_t>(offset, value);
}

bool FeedbackMetadata::is_empty() const { return slot_count() == 0; }

int FeedbackMetadata::length() const {
  return FeedbackMetadata::length(slot_count());
}

int FeedbackMetadata::GetSlotSize(FeedbackSlotKind kind) {
  switch (kind) {
    case FeedbackSlotKind::kForIn:
    case FeedbackSlotKind::kInstanceOf:
    case FeedbackSlotKind::kCompareOp:
    case FeedbackSlotKind::kBinaryOp:
    case FeedbackSlotKind::kLiteral:
    case FeedbackSlotKind::kTypeProfile:
      return 1;

    case FeedbackSlotKind::kCall:
    case FeedbackSlotKind::kCloneObject:
    case FeedbackSlotKind::kLoadProperty:
    case FeedbackSlotKind::kLoadGlobalInsideTypeof:
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
    case FeedbackSlotKind::kLoadKeyed:
    case FeedbackSlotKind::kHasKeyed:
    case FeedbackSlotKind::kStoreNamedSloppy:
    case FeedbackSlotKind::kStoreNamedStrict:
    case FeedbackSlotKind::kStoreOwnNamed:
    case FeedbackSlotKind::kStoreGlobalSloppy:
    case FeedbackSlotKind::kStoreGlobalStrict:
    case FeedbackSlotKind::kStoreKeyedSloppy:
    case FeedbackSlotKind::kStoreKeyedStrict:
    case FeedbackSlotKind::kStoreInArrayLiteral:
    case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      return 2;

    case FeedbackSlotKind::kInvalid:
    case FeedbackSlotKind::kKindsNumber:
      UNREACHABLE();
  }
  return 1;
}

Handle<FeedbackCell> ClosureFeedbackCellArray::GetFeedbackCell(int index) {
  return handle(FeedbackCell::cast(get(index)), GetIsolate());
}

FeedbackCell ClosureFeedbackCellArray::cell(int index) {
  return FeedbackCell::cast(get(index));
}

bool FeedbackVector::is_empty() const { return length() == 0; }

FeedbackMetadata FeedbackVector::metadata() const {
  return shared_function_info().feedback_metadata();
}

void FeedbackVector::clear_invocation_count() { set_invocation_count(0); }

Code FeedbackVector::optimized_code() const {
  MaybeObject slot = maybe_optimized_code(kAcquireLoad);
  DCHECK(slot->IsWeakOrCleared());
  HeapObject heap_object;
  Code code =
      slot->GetHeapObject(&heap_object) ? Code::cast(heap_object) : Code();
  // It is possible that the maybe_optimized_code slot is cleared but the
  // optimization tier hasn't been updated yet. We update the tier when we
  // execute the function next time / when we create new closure.
  DCHECK_IMPLIES(!code.is_null(), OptimizationTierBits::decode(flags()) ==
                                      GetTierForCodeKind(code.kind()));
  return code;
}

OptimizationMarker FeedbackVector::optimization_marker() const {
  return OptimizationMarkerBits::decode(flags());
}

int FeedbackVector::global_ticks_at_last_runtime_profiler_interrupt() const {
  return GlobalTicksAtLastRuntimeProfilerInterruptBits::decode(flags());
}

void FeedbackVector::set_global_ticks_at_last_runtime_profiler_interrupt(
    int ticks) {
  set_flags(
      GlobalTicksAtLastRuntimeProfilerInterruptBits::update(flags(), ticks));
}

OptimizationTier FeedbackVector::optimization_tier() const {
  OptimizationTier tier = OptimizationTierBits::decode(flags());
  // It is possible that the optimization tier bits aren't updated when the code
  // was cleared due to a GC.
  DCHECK_IMPLIES(tier == OptimizationTier::kNone,
                 maybe_optimized_code(kAcquireLoad)->IsCleared());
  return tier;
}

bool FeedbackVector::has_optimized_code() const {
  return !optimized_code().is_null();
}

bool FeedbackVector::has_optimization_marker() const {
  return optimization_marker() != OptimizationMarker::kLogFirstExecution &&
         optimization_marker() != OptimizationMarker::kNone;
}

// Conversion from an integer index to either a slot or an ic slot.
// static
FeedbackSlot FeedbackVector::ToSlot(intptr_t index) {
  DCHECK_LE(static_cast<uintptr_t>(index),
            static_cast<uintptr_t>(std::numeric_limits<int>::max()));
  return FeedbackSlot(static_cast<int>(index));
}

#ifdef DEBUG
// Instead of FixedArray, the Feedback and the Extra should contain
// WeakFixedArrays. The only allowed FixedArray subtype is HashTable.
bool FeedbackVector::IsOfLegacyType(MaybeObject value) {
  HeapObject heap_object;
  if (value->GetHeapObject(&heap_object)) {
    return heap_object.IsFixedArray() && !heap_object.IsHashTable();
  }
  return false;
}
#endif  // DEBUG

MaybeObject FeedbackVector::Get(FeedbackSlot slot) const {
  MaybeObject value = raw_feedback_slots(GetIndex(slot));
  DCHECK(!IsOfLegacyType(value));
  return value;
}

MaybeObject FeedbackVector::Get(IsolateRoot isolate, FeedbackSlot slot) const {
  MaybeObject value = raw_feedback_slots(isolate, GetIndex(slot));
  DCHECK(!IsOfLegacyType(value));
  return value;
}

Handle<FeedbackCell> FeedbackVector::GetClosureFeedbackCell(int index) const {
  DCHECK_GE(index, 0);
  return closure_feedback_cell_array().GetFeedbackCell(index);
}

FeedbackCell FeedbackVector::closure_feedback_cell(int index) const {
  DCHECK_GE(index, 0);
  return closure_feedback_cell_array().cell(index);
}

MaybeObject FeedbackVector::SynchronizedGet(FeedbackSlot slot) const {
  const int i = slot.ToInt();
  DCHECK_LT(static_cast<unsigned>(i), static_cast<unsigned>(this->length()));
  const int offset = kRawFeedbackSlotsOffset + i * kTaggedSize;
  MaybeObject value = TaggedField<MaybeObject>::Acquire_Load(*this, offset);
  DCHECK(!IsOfLegacyType(value));
  return value;
}

void FeedbackVector::SynchronizedSet(FeedbackSlot slot, MaybeObject value,
                                     WriteBarrierMode mode) {
  DCHECK(!IsOfLegacyType(value));
  const int i = slot.ToInt();
  DCHECK_LT(static_cast<unsigned>(i), static_cast<unsigned>(this->length()));
  const int offset = kRawFeedbackSlotsOffset + i * kTaggedSize;
  TaggedField<MaybeObject>::Release_Store(*this, offset, value);
  CONDITIONAL_WEAK_WRITE_BARRIER(*this, offset, value, mode);
}

void FeedbackVector::SynchronizedSet(FeedbackSlot slot, Object value,
                                     WriteBarrierMode mode) {
  SynchronizedSet(slot, MaybeObject::FromObject(value), mode);
}

void FeedbackVector::Set(FeedbackSlot slot, MaybeObject value,
                         WriteBarrierMode mode) {
  DCHECK(!IsOfLegacyType(value));
  set_raw_feedback_slots(GetIndex(slot), value, mode);
}

void FeedbackVector::Set(FeedbackSlot slot, Object value,
                         WriteBarrierMode mode) {
  MaybeObject maybe_value = MaybeObject::FromObject(value);
  DCHECK(!IsOfLegacyType(maybe_value));
  set_raw_feedback_slots(GetIndex(slot), maybe_value, mode);
}

inline MaybeObjectSlot FeedbackVector::slots_start() {
  return RawMaybeWeakField(OffsetOfElementAt(0));
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
      return BinaryOperationHint::kBigInt;
    default:
      return BinaryOperationHint::kAny;
  }
  UNREACHABLE();
}

// Helper function to transform the feedback to CompareOperationHint.
template <CompareOperationFeedback::Type Feedback>
bool Is(int type_feedback) {
  return !(type_feedback & ~Feedback);
}

CompareOperationHint CompareOperationHintFromFeedback(int type_feedback) {
  if (Is<CompareOperationFeedback::kNone>(type_feedback)) {
    return CompareOperationHint::kNone;
  }

  if (Is<CompareOperationFeedback::kSignedSmall>(type_feedback)) {
    return CompareOperationHint::kSignedSmall;
  } else if (Is<CompareOperationFeedback::kNumber>(type_feedback)) {
    return CompareOperationHint::kNumber;
  } else if (Is<CompareOperationFeedback::kNumberOrBoolean>(type_feedback)) {
    return CompareOperationHint::kNumberOrBoolean;
  }

  if (Is<CompareOperationFeedback::kInternalizedString>(type_feedback)) {
    return CompareOperationHint::kInternalizedString;
  } else if (Is<CompareOperationFeedback::kString>(type_feedback)) {
    return CompareOperationHint::kString;
  }

  if (Is<CompareOperationFeedback::kReceiver>(type_feedback)) {
    return CompareOperationHint::kReceiver;
  } else if (Is<CompareOperationFeedback::kReceiverOrNullOrUndefined>(
                 type_feedback)) {
    return CompareOperationHint::kReceiverOrNullOrUndefined;
  }

  if (Is<CompareOperationFeedback::kBigInt>(type_feedback)) {
    return CompareOperationHint::kBigInt;
  }

  if (Is<CompareOperationFeedback::kSymbol>(type_feedback)) {
    return CompareOperationHint::kSymbol;
  }

  DCHECK(Is<CompareOperationFeedback::kAny>(type_feedback));
  return CompareOperationHint::kAny;
}

// Helper function to transform the feedback to ForInHint.
ForInHint ForInHintFromFeedback(ForInFeedback type_feedback) {
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

Handle<Symbol> FeedbackVector::UninitializedSentinel(Isolate* isolate) {
  return isolate->factory()->uninitialized_symbol();
}

Handle<Symbol> FeedbackVector::MegamorphicSentinel(Isolate* isolate) {
  return isolate->factory()->megamorphic_symbol();
}

Symbol FeedbackVector::RawUninitializedSentinel(Isolate* isolate) {
  return ReadOnlyRoots(isolate).uninitialized_symbol();
}

bool FeedbackMetadataIterator::HasNext() const {
  return next_slot_.ToInt() < metadata().slot_count();
}

FeedbackSlot FeedbackMetadataIterator::Next() {
  DCHECK(HasNext());
  cur_slot_ = next_slot_;
  slot_kind_ = metadata().GetKind(cur_slot_);
  next_slot_ = FeedbackSlot(next_slot_.ToInt() + entry_size());
  return cur_slot_;
}

int FeedbackMetadataIterator::entry_size() const {
  return FeedbackMetadata::GetSlotSize(kind());
}

MaybeObject NexusConfig::GetFeedback(FeedbackVector vector,
                                     FeedbackSlot slot) const {
  return vector.SynchronizedGet(slot);
}

void NexusConfig::SetFeedback(FeedbackVector vector, FeedbackSlot slot,
                              MaybeObject feedback,
                              WriteBarrierMode mode) const {
  DCHECK(can_write());
  vector.SynchronizedSet(slot, feedback, mode);
}

MaybeObject FeedbackNexus::UninitializedSentinel() const {
  return MaybeObject::FromObject(
      *FeedbackVector::UninitializedSentinel(GetIsolate()));
}

MaybeObject FeedbackNexus::MegamorphicSentinel() const {
  return MaybeObject::FromObject(
      *FeedbackVector::MegamorphicSentinel(GetIsolate()));
}

MaybeObject FeedbackNexus::FromHandle(MaybeObjectHandle slot) const {
  return slot.is_null() ? HeapObjectReference::ClearedValue(config()->isolate())
                        : *slot;
}

MaybeObjectHandle FeedbackNexus::ToHandle(MaybeObject value) const {
  return value.IsCleared() ? MaybeObjectHandle()
                           : MaybeObjectHandle(config()->NewHandle(value));
}

MaybeObject FeedbackNexus::GetFeedback() const {
  auto pair = GetFeedbackPair();
  return pair.first;
}

MaybeObject FeedbackNexus::GetFeedbackExtra() const {
  auto pair = GetFeedbackPair();
  return pair.second;
}

std::pair<MaybeObject, MaybeObject> FeedbackNexus::GetFeedbackPair() const {
  if (config()->mode() == NexusConfig::BackgroundThread &&
      feedback_cache_.has_value()) {
    return std::make_pair(FromHandle(feedback_cache_->first),
                          FromHandle(feedback_cache_->second));
  }
  auto pair = FeedbackMetadata::GetSlotSize(kind()) == 2
                  ? config()->GetFeedbackPair(vector(), slot())
                  : std::make_pair(config()->GetFeedback(vector(), slot()),
                                   MaybeObject());
  if (config()->mode() == NexusConfig::BackgroundThread &&
      !feedback_cache_.has_value()) {
    feedback_cache_ =
        std::make_pair(ToHandle(pair.first), ToHandle(pair.second));
  }
  return pair;
}

template <typename T>
struct IsValidFeedbackType
    : public std::integral_constant<bool,
                                    std::is_base_of<MaybeObject, T>::value ||
                                        std::is_base_of<Object, T>::value> {};

template <typename FeedbackType>
void FeedbackNexus::SetFeedback(FeedbackType feedback, WriteBarrierMode mode) {
  static_assert(IsValidFeedbackType<FeedbackType>(),
                "feedbacks need to be Smi, Object or MaybeObject");
  MaybeObject fmo = MaybeObject::Create(feedback);
  config()->SetFeedback(vector(), slot(), fmo, mode);
}

template <typename FeedbackType, typename FeedbackExtraType>
void FeedbackNexus::SetFeedback(FeedbackType feedback, WriteBarrierMode mode,
                                FeedbackExtraType feedback_extra,
                                WriteBarrierMode mode_extra) {
  static_assert(IsValidFeedbackType<FeedbackType>(),
                "feedbacks need to be Smi, Object or MaybeObject");
  static_assert(IsValidFeedbackType<FeedbackExtraType>(),
                "feedbacks need to be Smi, Object or MaybeObject");
  MaybeObject fmo = MaybeObject::Create(feedback);
  MaybeObject fmo_extra = MaybeObject::Create(feedback_extra);
  config()->SetFeedbackPair(vector(), slot(), fmo, mode, fmo_extra, mode_extra);
}

Isolate* FeedbackNexus::GetIsolate() const { return vector().GetIsolate(); }
}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FEEDBACK_VECTOR_INL_H_
