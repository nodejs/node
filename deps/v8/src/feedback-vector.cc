// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/feedback-vector.h"
#include "src/code-stubs.h"
#include "src/feedback-vector-inl.h"
#include "src/ic/ic-inl.h"
#include "src/objects.h"
#include "src/objects/object-macros.h"

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

template FeedbackSlot FeedbackVectorSpecBase<FeedbackVectorSpec>::AddSlot(
    FeedbackSlotKind kind);
template FeedbackSlot FeedbackVectorSpecBase<StaticFeedbackVectorSpec>::AddSlot(
    FeedbackSlotKind kind);

template <typename Derived>
FeedbackSlot FeedbackVectorSpecBase<Derived>::AddTypeProfileSlot() {
  FeedbackSlot slot = AddSlot(FeedbackSlotKind::kTypeProfile);
  CHECK_EQ(FeedbackVectorSpec::kTypeProfileSlotIndex,
           FeedbackVector::GetIndex(slot));
  return slot;
}

template FeedbackSlot
FeedbackVectorSpecBase<FeedbackVectorSpec>::AddTypeProfileSlot();
template FeedbackSlot
FeedbackVectorSpecBase<StaticFeedbackVectorSpec>::AddTypeProfileSlot();

bool FeedbackVectorSpec::HasTypeProfileSlot() const {
  FeedbackSlot slot =
      FeedbackVector::ToSlot(FeedbackVectorSpec::kTypeProfileSlotIndex);
  if (slots() <= slot.ToInt()) {
    return false;
  }
  return GetKind(slot) == FeedbackSlotKind::kTypeProfile;
}

static bool IsPropertyNameFeedback(Object* feedback) {
  if (feedback->IsString()) return true;
  if (!feedback->IsSymbol()) return false;
  Symbol* symbol = Symbol::cast(feedback);
  Heap* heap = symbol->GetHeap();
  return symbol != heap->uninitialized_symbol() &&
         symbol != heap->premonomorphic_symbol() &&
         symbol != heap->megamorphic_symbol();
}

std::ostream& operator<<(std::ostream& os, FeedbackSlotKind kind) {
  return os << FeedbackMetadata::Kind2String(kind);
}

FeedbackSlotKind FeedbackMetadata::GetKind(FeedbackSlot slot) const {
  int index = VectorICComputer::index(kReservedIndexCount, slot.ToInt());
  int data = Smi::ToInt(get(index));
  return VectorICComputer::decode(data, slot.ToInt());
}

void FeedbackMetadata::SetKind(FeedbackSlot slot, FeedbackSlotKind kind) {
  int index = VectorICComputer::index(kReservedIndexCount, slot.ToInt());
  int data = Smi::ToInt(get(index));
  int new_data = VectorICComputer::encode(data, slot.ToInt(), kind);
  set(index, Smi::FromInt(new_data));
}

template Handle<FeedbackMetadata> FeedbackMetadata::New(
    Isolate* isolate, const StaticFeedbackVectorSpec* spec);
template Handle<FeedbackMetadata> FeedbackMetadata::New(
    Isolate* isolate, const FeedbackVectorSpec* spec);

// static
template <typename Spec>
Handle<FeedbackMetadata> FeedbackMetadata::New(Isolate* isolate,
                                               const Spec* spec) {
  Factory* factory = isolate->factory();

  const int slot_count = spec->slots();
  const int slot_kinds_length = VectorICComputer::word_count(slot_count);
  const int length = slot_kinds_length + kReservedIndexCount;
  if (length == kReservedIndexCount) {
    return Handle<FeedbackMetadata>::cast(factory->empty_fixed_array());
  }
#ifdef DEBUG
  for (int i = 0; i < slot_count;) {
    FeedbackSlotKind kind = spec->GetKind(FeedbackSlot(i));
    int entry_size = FeedbackMetadata::GetSlotSize(kind);
    for (int j = 1; j < entry_size; j++) {
      FeedbackSlotKind kind = spec->GetKind(FeedbackSlot(i + j));
      DCHECK_EQ(FeedbackSlotKind::kInvalid, kind);
    }
    i += entry_size;
  }
#endif

  Handle<FixedArray> array = factory->NewFixedArray(length, TENURED);
  array->set(kSlotsCountIndex, Smi::FromInt(slot_count));
  // Fill the bit-vector part with zeros.
  for (int i = 0; i < slot_kinds_length; i++) {
    array->set(kReservedIndexCount + i, Smi::kZero);
  }

  Handle<FeedbackMetadata> metadata = Handle<FeedbackMetadata>::cast(array);

  for (int i = 0; i < slot_count; i++) {
    FeedbackSlot slot(i);
    FeedbackSlotKind kind = spec->GetKind(slot);
    metadata->SetKind(slot, kind);
  }

  // It's important that the FeedbackMetadata have a COW map, since it's
  // pointed to by both a SharedFunctionInfo and indirectly by closures through
  // the FeedbackVector. The serializer uses the COW map type to decide
  // this object belongs in the startup snapshot and not the partial
  // snapshot(s).
  metadata->set_map(isolate->heap()->fixed_cow_array_map());

  return metadata;
}

bool FeedbackMetadata::SpecDiffersFrom(
    const FeedbackVectorSpec* other_spec) const {
  if (other_spec->slots() != slot_count()) {
    return true;
  }

  int slots = slot_count();
  for (int i = 0; i < slots;) {
    FeedbackSlot slot(i);
    FeedbackSlotKind kind = GetKind(slot);
    int entry_size = FeedbackMetadata::GetSlotSize(kind);

    if (kind != other_spec->GetKind(slot)) {
      return true;
    }
    i += entry_size;
  }
  return false;
}

const char* FeedbackMetadata::Kind2String(FeedbackSlotKind kind) {
  switch (kind) {
    case FeedbackSlotKind::kInvalid:
      return "Invalid";
    case FeedbackSlotKind::kCall:
      return "Call";
    case FeedbackSlotKind::kLoadProperty:
      return "LoadProperty";
    case FeedbackSlotKind::kLoadGlobalInsideTypeof:
      return "LoadGlobalInsideTypeof";
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
      return "LoadGlobalNotInsideTypeof";
    case FeedbackSlotKind::kLoadKeyed:
      return "LoadKeyed";
    case FeedbackSlotKind::kStoreNamedSloppy:
      return "StoreNamedSloppy";
    case FeedbackSlotKind::kStoreNamedStrict:
      return "StoreNamedStrict";
    case FeedbackSlotKind::kStoreOwnNamed:
      return "StoreOwnNamed";
    case FeedbackSlotKind::kStoreGlobalSloppy:
      return "StoreGlobalSloppy";
    case FeedbackSlotKind::kStoreGlobalStrict:
      return "StoreGlobalStrict";
    case FeedbackSlotKind::kStoreKeyedSloppy:
      return "StoreKeyedSloppy";
    case FeedbackSlotKind::kStoreKeyedStrict:
      return "StoreKeyedStrict";
    case FeedbackSlotKind::kBinaryOp:
      return "BinaryOp";
    case FeedbackSlotKind::kCompareOp:
      return "CompareOp";
    case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      return "StoreDataPropertyInLiteral";
    case FeedbackSlotKind::kCreateClosure:
      return "kCreateClosure";
    case FeedbackSlotKind::kLiteral:
      return "Literal";
    case FeedbackSlotKind::kTypeProfile:
      return "TypeProfile";
    case FeedbackSlotKind::kForIn:
      return "ForIn";
    case FeedbackSlotKind::kInstanceOf:
      return "InstanceOf";
    case FeedbackSlotKind::kKindsNumber:
      break;
  }
  UNREACHABLE();
}

bool FeedbackMetadata::HasTypeProfileSlot() const {
  FeedbackSlot slot =
      FeedbackVector::ToSlot(FeedbackVectorSpec::kTypeProfileSlotIndex);
  return slot.ToInt() < this->length() &&
         GetKind(slot) == FeedbackSlotKind::kTypeProfile;
}

FeedbackSlotKind FeedbackVector::GetKind(FeedbackSlot slot) const {
  DCHECK(!is_empty());
  return metadata()->GetKind(slot);
}

FeedbackSlot FeedbackVector::GetTypeProfileSlot() const {
  DCHECK(metadata()->HasTypeProfileSlot());
  FeedbackSlot slot =
      FeedbackVector::ToSlot(FeedbackVectorSpec::kTypeProfileSlotIndex);
  DCHECK_EQ(FeedbackSlotKind::kTypeProfile, GetKind(slot));
  return slot;
}

// static
Handle<FeedbackVector> FeedbackVector::New(Isolate* isolate,
                                           Handle<SharedFunctionInfo> shared) {
  Factory* factory = isolate->factory();

  const int slot_count = shared->feedback_metadata()->slot_count();

  Handle<FeedbackVector> vector = factory->NewFeedbackVector(shared, TENURED);

  DCHECK_EQ(vector->length(), slot_count);

  DCHECK_EQ(vector->shared_function_info(), *shared);
  DCHECK_EQ(vector->optimized_code_cell(),
            Smi::FromEnum(FLAG_log_function_events
                              ? OptimizationMarker::kLogFirstExecution
                              : OptimizationMarker::kNone));
  DCHECK_EQ(vector->invocation_count(), 0);
  DCHECK_EQ(vector->profiler_ticks(), 0);
  DCHECK_EQ(vector->deopt_count(), 0);

  // Ensure we can skip the write barrier
  Handle<Object> uninitialized_sentinel = UninitializedSentinel(isolate);
  DCHECK_EQ(isolate->heap()->uninitialized_symbol(), *uninitialized_sentinel);
  Handle<Oddball> undefined_value = factory->undefined_value();
  for (int i = 0; i < slot_count;) {
    FeedbackSlot slot(i);
    FeedbackSlotKind kind = shared->feedback_metadata()->GetKind(slot);
    int index = FeedbackVector::GetIndex(slot);
    int entry_size = FeedbackMetadata::GetSlotSize(kind);

    Object* extra_value = *uninitialized_sentinel;
    switch (kind) {
      case FeedbackSlotKind::kLoadGlobalInsideTypeof:
      case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
      case FeedbackSlotKind::kStoreGlobalSloppy:
      case FeedbackSlotKind::kStoreGlobalStrict:
        vector->set(index, isolate->heap()->empty_weak_cell(),
                    SKIP_WRITE_BARRIER);
        break;
      case FeedbackSlotKind::kForIn:
      case FeedbackSlotKind::kCompareOp:
      case FeedbackSlotKind::kBinaryOp:
        vector->set(index, Smi::kZero, SKIP_WRITE_BARRIER);
        break;
      case FeedbackSlotKind::kCreateClosure: {
        Handle<Cell> cell = factory->NewNoClosuresCell(undefined_value);
        vector->set(index, *cell);
        break;
      }
      case FeedbackSlotKind::kLiteral:
        vector->set(index, Smi::kZero, SKIP_WRITE_BARRIER);
        break;
      case FeedbackSlotKind::kCall:
        vector->set(index, *uninitialized_sentinel, SKIP_WRITE_BARRIER);
        extra_value = Smi::kZero;
        break;
      case FeedbackSlotKind::kLoadProperty:
      case FeedbackSlotKind::kLoadKeyed:
      case FeedbackSlotKind::kStoreNamedSloppy:
      case FeedbackSlotKind::kStoreNamedStrict:
      case FeedbackSlotKind::kStoreOwnNamed:
      case FeedbackSlotKind::kStoreKeyedSloppy:
      case FeedbackSlotKind::kStoreKeyedStrict:
      case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      case FeedbackSlotKind::kTypeProfile:
      case FeedbackSlotKind::kInstanceOf:
        vector->set(index, *uninitialized_sentinel, SKIP_WRITE_BARRIER);
        break;

      case FeedbackSlotKind::kInvalid:
      case FeedbackSlotKind::kKindsNumber:
        UNREACHABLE();
        break;
    }
    for (int j = 1; j < entry_size; j++) {
      vector->set(index + j, extra_value, SKIP_WRITE_BARRIER);
    }
    i += entry_size;
  }

  Handle<FeedbackVector> result = Handle<FeedbackVector>::cast(vector);
  if (!isolate->is_best_effort_code_coverage() ||
      isolate->is_collecting_type_profile()) {
    AddToVectorsForProfilingTools(isolate, result);
  }
  return result;
}

// static
Handle<FeedbackVector> FeedbackVector::Copy(Isolate* isolate,
                                            Handle<FeedbackVector> vector) {
  Handle<FeedbackVector> result;
  result = Handle<FeedbackVector>::cast(
      isolate->factory()->CopyFixedArray(Handle<FixedArray>::cast(vector)));
  if (!isolate->is_best_effort_code_coverage() ||
      isolate->is_collecting_type_profile()) {
    AddToVectorsForProfilingTools(isolate, result);
  }
  return result;
}

// static
void FeedbackVector::AddToVectorsForProfilingTools(
    Isolate* isolate, Handle<FeedbackVector> vector) {
  DCHECK(!isolate->is_best_effort_code_coverage() ||
         isolate->is_collecting_type_profile());
  if (!vector->shared_function_info()->IsSubjectToDebugging()) return;
  Handle<ArrayList> list = Handle<ArrayList>::cast(
      isolate->factory()->feedback_vectors_for_profiling_tools());
  list = ArrayList::Add(list, vector);
  isolate->SetFeedbackVectorsForProfilingTools(*list);
}

// static
void FeedbackVector::SetOptimizedCode(Handle<FeedbackVector> vector,
                                      Handle<Code> code) {
  DCHECK_EQ(code->kind(), Code::OPTIMIZED_FUNCTION);
  Factory* factory = vector->GetIsolate()->factory();
  Handle<WeakCell> cell = factory->NewWeakCell(code);
  vector->set_optimized_code_cell(*cell);
}

void FeedbackVector::ClearOptimizedCode() {
  DCHECK(has_optimized_code());
  SetOptimizationMarker(OptimizationMarker::kNone);
}

void FeedbackVector::ClearOptimizationMarker() {
  DCHECK(!has_optimized_code());
  SetOptimizationMarker(OptimizationMarker::kNone);
}

void FeedbackVector::SetOptimizationMarker(OptimizationMarker marker) {
  set_optimized_code_cell(Smi::FromEnum(marker));
}

void FeedbackVector::EvictOptimizedCodeMarkedForDeoptimization(
    SharedFunctionInfo* shared, const char* reason) {
  Object* slot = optimized_code_cell();
  if (slot->IsSmi()) return;

  WeakCell* cell = WeakCell::cast(slot);
  if (cell->cleared()) {
    ClearOptimizationMarker();
    return;
  }

  Code* code = Code::cast(cell->value());
  if (code->marked_for_deoptimization()) {
    if (FLAG_trace_deopt) {
      PrintF("[evicting optimizing code marked for deoptimization (%s) for ",
             reason);
      shared->ShortPrint();
      PrintF("]\n");
    }
    if (!code->deopt_already_counted()) {
      increment_deopt_count();
      code->set_deopt_already_counted(true);
    }
    ClearOptimizedCode();
  }
}

bool FeedbackVector::ClearSlots(Isolate* isolate) {
  Object* uninitialized_sentinel =
      FeedbackVector::RawUninitializedSentinel(isolate);

  bool feedback_updated = false;
  FeedbackMetadataIterator iter(metadata());
  while (iter.HasNext()) {
    FeedbackSlot slot = iter.Next();
    FeedbackSlotKind kind = iter.kind();

    Object* obj = Get(slot);
    if (obj != uninitialized_sentinel) {
      switch (kind) {
        case FeedbackSlotKind::kCall: {
          CallICNexus nexus(this, slot);
          if (!nexus.IsCleared()) {
            nexus.Clear();
            feedback_updated = true;
          }
          break;
        }
        case FeedbackSlotKind::kLoadProperty: {
          LoadICNexus nexus(this, slot);
          if (!nexus.IsCleared()) {
            nexus.Clear();
            feedback_updated = true;
          }
          break;
        }
        case FeedbackSlotKind::kLoadGlobalInsideTypeof:
        case FeedbackSlotKind::kLoadGlobalNotInsideTypeof: {
          LoadGlobalICNexus nexus(this, slot);
          if (!nexus.IsCleared()) {
            nexus.Clear();
            feedback_updated = true;
          }
          break;
        }
        case FeedbackSlotKind::kLoadKeyed: {
          KeyedLoadICNexus nexus(this, slot);
          if (!nexus.IsCleared()) {
            nexus.Clear();
            feedback_updated = true;
          }
          break;
        }
        case FeedbackSlotKind::kStoreNamedSloppy:
        case FeedbackSlotKind::kStoreNamedStrict:
        case FeedbackSlotKind::kStoreOwnNamed: {
          StoreICNexus nexus(this, slot);
          if (!nexus.IsCleared()) {
            nexus.Clear();
            feedback_updated = true;
          }
          break;
        }
        case FeedbackSlotKind::kStoreGlobalSloppy:
        case FeedbackSlotKind::kStoreGlobalStrict: {
          StoreGlobalICNexus nexus(this, slot);
          if (!nexus.IsCleared()) {
            nexus.Clear();
            feedback_updated = true;
          }
          break;
        }
        case FeedbackSlotKind::kStoreKeyedSloppy:
        case FeedbackSlotKind::kStoreKeyedStrict: {
          KeyedStoreICNexus nexus(this, slot);
          if (!nexus.IsCleared()) {
            nexus.Clear();
            feedback_updated = true;
          }
          break;
        }
        case FeedbackSlotKind::kForIn:
        case FeedbackSlotKind::kBinaryOp:
        case FeedbackSlotKind::kCompareOp: {
          DCHECK(Get(slot)->IsSmi());
          // don't clear these smi slots.
          // Set(slot, Smi::kZero);
          break;
        }
        case FeedbackSlotKind::kInstanceOf: {
          InstanceOfICNexus nexus(this, slot);
          if (!nexus.IsCleared()) {
            nexus.Clear();
            feedback_updated = true;
          }
          break;
        }
        case FeedbackSlotKind::kCreateClosure:
        case FeedbackSlotKind::kTypeProfile: {
          break;
        }
        case FeedbackSlotKind::kLiteral: {
          Set(slot, Smi::kZero, SKIP_WRITE_BARRIER);
          feedback_updated = true;
          break;
        }
        case FeedbackSlotKind::kStoreDataPropertyInLiteral: {
          StoreDataPropertyInLiteralICNexus nexus(this, slot);
          if (!nexus.IsCleared()) {
            nexus.Clear();
            feedback_updated = true;
          }
          break;
        }
        case FeedbackSlotKind::kInvalid:
        case FeedbackSlotKind::kKindsNumber:
          UNREACHABLE();
          break;
      }
    }
  }
  return feedback_updated;
}

Handle<FixedArray> FeedbackNexus::EnsureArrayOfSize(int length) {
  Isolate* isolate = GetIsolate();
  Handle<Object> feedback = handle(GetFeedback(), isolate);
  if (!feedback->IsFixedArray() ||
      FixedArray::cast(*feedback)->length() != length) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(length);
    SetFeedback(*array);
    return array;
  }
  return Handle<FixedArray>::cast(feedback);
}

Handle<FixedArray> FeedbackNexus::EnsureExtraArrayOfSize(int length) {
  Isolate* isolate = GetIsolate();
  Handle<Object> feedback_extra = handle(GetFeedbackExtra(), isolate);
  if (!feedback_extra->IsFixedArray() ||
      FixedArray::cast(*feedback_extra)->length() != length) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(length);
    SetFeedbackExtra(*array);
    return array;
  }
  return Handle<FixedArray>::cast(feedback_extra);
}

void FeedbackNexus::ConfigureUninitialized() {
  SetFeedback(*FeedbackVector::UninitializedSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(GetIsolate()),
                   SKIP_WRITE_BARRIER);
}

void FeedbackNexus::ConfigurePremonomorphic() {
  SetFeedback(*FeedbackVector::PremonomorphicSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(GetIsolate()),
                   SKIP_WRITE_BARRIER);
}

bool FeedbackNexus::ConfigureMegamorphic(IcCheckType property_type) {
  DisallowHeapAllocation no_gc;
  Isolate* isolate = GetIsolate();
  bool changed = false;
  Symbol* sentinel = *FeedbackVector::MegamorphicSentinel(isolate);
  if (GetFeedback() != sentinel) {
    SetFeedback(sentinel, SKIP_WRITE_BARRIER);
    changed = true;
  }

  Smi* extra = Smi::FromInt(static_cast<int>(property_type));
  if (changed || GetFeedbackExtra() != extra) {
    SetFeedbackExtra(extra, SKIP_WRITE_BARRIER);
    changed = true;
  }
  return changed;
}

InlineCacheState LoadICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *FeedbackVector::MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  } else if (feedback == *FeedbackVector::PremonomorphicSentinel(isolate)) {
    return PREMONOMORPHIC;
  } else if (feedback->IsFixedArray()) {
    // Determine state purely by our structure, don't check if the maps are
    // cleared.
    return POLYMORPHIC;
  } else if (feedback->IsWeakCell()) {
    // Don't check if the map is cleared.
    return MONOMORPHIC;
  }

  return UNINITIALIZED;
}

InlineCacheState KeyedLoadICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *FeedbackVector::PremonomorphicSentinel(isolate)) {
    return PREMONOMORPHIC;
  } else if (feedback == *FeedbackVector::MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  } else if (feedback->IsFixedArray()) {
    // Determine state purely by our structure, don't check if the maps are
    // cleared.
    return POLYMORPHIC;
  } else if (feedback->IsWeakCell()) {
    // Don't check if the map is cleared.
    return MONOMORPHIC;
  } else if (feedback->IsName()) {
    Object* extra = GetFeedbackExtra();
    FixedArray* extra_array = FixedArray::cast(extra);
    return extra_array->length() > 2 ? POLYMORPHIC : MONOMORPHIC;
  }

  return UNINITIALIZED;
}

void GlobalICNexus::ConfigureUninitialized() {
  Isolate* isolate = GetIsolate();
  SetFeedback(isolate->heap()->empty_weak_cell(), SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}

void GlobalICNexus::ConfigurePropertyCellMode(Handle<PropertyCell> cell) {
  Isolate* isolate = GetIsolate();
  SetFeedback(*isolate->factory()->NewWeakCell(cell));
  SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}

bool GlobalICNexus::ConfigureLexicalVarMode(int script_context_index,
                                            int context_slot_index) {
  DCHECK_LE(0, script_context_index);
  DCHECK_LE(0, context_slot_index);
  if (!ContextIndexBits::is_valid(script_context_index) ||
      !SlotIndexBits::is_valid(context_slot_index)) {
    return false;
  }
  int config = ContextIndexBits::encode(script_context_index) |
               SlotIndexBits::encode(context_slot_index);

  SetFeedback(Smi::FromInt(config));
  Isolate* isolate = GetIsolate();
  SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
  return true;
}

void GlobalICNexus::ConfigureHandlerMode(Handle<Object> handler) {
  SetFeedback(GetIsolate()->heap()->empty_weak_cell());
  SetFeedbackExtra(*handler);
}

InlineCacheState GlobalICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();
  if (feedback->IsSmi()) return MONOMORPHIC;

  Object* extra = GetFeedbackExtra();
  if (!WeakCell::cast(feedback)->cleared() ||
      extra != *FeedbackVector::UninitializedSentinel(isolate)) {
    return MONOMORPHIC;
  }
  return UNINITIALIZED;
}

InlineCacheState StoreICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *FeedbackVector::MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  } else if (feedback == *FeedbackVector::PremonomorphicSentinel(isolate)) {
    return PREMONOMORPHIC;
  } else if (feedback->IsFixedArray()) {
    // Determine state purely by our structure, don't check if the maps are
    // cleared.
    return POLYMORPHIC;
  } else if (feedback->IsWeakCell()) {
    // Don't check if the map is cleared.
    return MONOMORPHIC;
  }

  return UNINITIALIZED;
}

InlineCacheState KeyedStoreICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *FeedbackVector::PremonomorphicSentinel(isolate)) {
    return PREMONOMORPHIC;
  } else if (feedback == *FeedbackVector::MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  } else if (feedback->IsFixedArray()) {
    // Determine state purely by our structure, don't check if the maps are
    // cleared.
    return POLYMORPHIC;
  } else if (feedback->IsWeakCell()) {
    // Don't check if the map is cleared.
    return MONOMORPHIC;
  } else if (feedback->IsName()) {
    Object* extra = GetFeedbackExtra();
    FixedArray* extra_array = FixedArray::cast(extra);
    return extra_array->length() > 2 ? POLYMORPHIC : MONOMORPHIC;
  }

  return UNINITIALIZED;
}

InlineCacheState CallICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();
  DCHECK(GetFeedbackExtra() ==
             *FeedbackVector::UninitializedSentinel(isolate) ||
         GetFeedbackExtra()->IsSmi());

  if (feedback == *FeedbackVector::MegamorphicSentinel(isolate)) {
    return GENERIC;
  } else if (feedback->IsAllocationSite() || feedback->IsWeakCell()) {
    return MONOMORPHIC;
  }

  CHECK(feedback == *FeedbackVector::UninitializedSentinel(isolate));
  return UNINITIALIZED;
}

int CallICNexus::GetCallCount() {
  Object* call_count = GetFeedbackExtra();
  CHECK(call_count->IsSmi());
  uint32_t value = static_cast<uint32_t>(Smi::ToInt(call_count));
  return CallCountField::decode(value);
}

void CallICNexus::SetSpeculationMode(SpeculationMode mode) {
  Object* call_count = GetFeedbackExtra();
  CHECK(call_count->IsSmi());
  uint32_t value = static_cast<uint32_t>(Smi::ToInt(call_count));
  int result = static_cast<int>(CallCountField::decode(value) |
                                SpeculationModeField::encode(mode));
  SetFeedbackExtra(Smi::FromInt(result), SKIP_WRITE_BARRIER);
}

SpeculationMode CallICNexus::GetSpeculationMode() {
  Object* call_count = GetFeedbackExtra();
  CHECK(call_count->IsSmi());
  uint32_t value = static_cast<uint32_t>(Smi::ToInt(call_count));
  return SpeculationModeField::decode(value);
}
float CallICNexus::ComputeCallFrequency() {
  double const invocation_count = vector()->invocation_count();
  double const call_count = GetCallCount();
  if (invocation_count == 0) {
    // Prevent division by 0.
    return 0.0f;
  }
  return static_cast<float>(call_count / invocation_count);
}

void CallICNexus::ConfigureUninitialized() {
  Isolate* isolate = GetIsolate();
  SetFeedback(*FeedbackVector::UninitializedSentinel(isolate),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(Smi::kZero, SKIP_WRITE_BARRIER);
}

void FeedbackNexus::ConfigureMonomorphic(Handle<Name> name,
                                         Handle<Map> receiver_map,
                                         Handle<Object> handler) {
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
  if (name.is_null()) {
    SetFeedback(*cell);
    SetFeedbackExtra(*handler);
  } else {
    Handle<FixedArray> array = EnsureExtraArrayOfSize(2);
    SetFeedback(*name);
    array->set(0, *cell);
    array->set(1, *handler);
  }
}

void FeedbackNexus::ConfigurePolymorphic(Handle<Name> name,
                                         MapHandles const& maps,
                                         ObjectHandles* handlers) {
  int receiver_count = static_cast<int>(maps.size());
  DCHECK_GT(receiver_count, 1);
  Handle<FixedArray> array;
  if (name.is_null()) {
    array = EnsureArrayOfSize(receiver_count * 2);
    SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(GetIsolate()),
                     SKIP_WRITE_BARRIER);
  } else {
    array = EnsureExtraArrayOfSize(receiver_count * 2);
    SetFeedback(*name);
  }

  for (int current = 0; current < receiver_count; ++current) {
    Handle<Map> map = maps[current];
    Handle<WeakCell> cell = Map::WeakCellForMap(map);
    array->set(current * 2, *cell);
    array->set(current * 2 + 1, *handlers->at(current));
  }
}

int FeedbackNexus::ExtractMaps(MapHandles* maps) const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsFixedArray() || is_named_feedback) {
    int found = 0;
    if (is_named_feedback) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    const int increment = 2;
    for (int i = 0; i < array->length(); i += increment) {
      DCHECK(array->get(i)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(array->get(i));
      if (!cell->cleared()) {
        Map* map = Map::cast(cell->value());
        maps->push_back(handle(map, isolate));
        found++;
      }
    }
    return found;
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Map* map = Map::cast(cell->value());
      maps->push_back(handle(map, isolate));
      return 1;
    }
  }

  return 0;
}

MaybeHandle<Object> FeedbackNexus::FindHandlerForMap(Handle<Map> map) const {
  Object* feedback = GetFeedback();
  Isolate* isolate = GetIsolate();
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsFixedArray() || is_named_feedback) {
    if (is_named_feedback) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    const int increment = 2;
    for (int i = 0; i < array->length(); i += increment) {
      DCHECK(array->get(i)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(array->get(i));
      if (!cell->cleared()) {
        Map* array_map = Map::cast(cell->value());
        if (array_map == *map) {
          Object* code = array->get(i + increment - 1);
          DCHECK(IC::IsHandler(code));
          return handle(code, isolate);
        }
      }
    }
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Map* cell_map = Map::cast(cell->value());
      if (cell_map == *map) {
        Object* code = GetFeedbackExtra();
        DCHECK(IC::IsHandler(code));
        return handle(code, isolate);
      }
    }
  }

  return MaybeHandle<Code>();
}

bool FeedbackNexus::FindHandlers(ObjectHandles* code_list, int length) const {
  Object* feedback = GetFeedback();
  Isolate* isolate = GetIsolate();
  int count = 0;
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsFixedArray() || is_named_feedback) {
    if (is_named_feedback) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    const int increment = 2;
    for (int i = 0; i < array->length(); i += increment) {
      DCHECK(array->get(i)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(array->get(i));
      // Be sure to skip handlers whose maps have been cleared.
      if (!cell->cleared()) {
        Object* code = array->get(i + increment - 1);
        DCHECK(IC::IsHandler(code));
        code_list->push_back(handle(code, isolate));
        count++;
      }
    }
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Object* code = GetFeedbackExtra();
      DCHECK(IC::IsHandler(code));
      code_list->push_back(handle(code, isolate));
      count++;
    }
  }
  return count == length;
}

Name* KeyedLoadICNexus::FindFirstName() const {
  Object* feedback = GetFeedback();
  if (IsPropertyNameFeedback(feedback)) {
    return Name::cast(feedback);
  }
  return nullptr;
}

Name* KeyedStoreICNexus::FindFirstName() const {
  Object* feedback = GetFeedback();
  if (IsPropertyNameFeedback(feedback)) {
    return Name::cast(feedback);
  }
  return nullptr;
}

KeyedAccessLoadMode KeyedLoadICNexus::GetKeyedAccessLoadMode() const {
  MapHandles maps;
  ObjectHandles handlers;

  if (GetKeyType() == PROPERTY) return STANDARD_LOAD;

  ExtractMaps(&maps);
  FindHandlers(&handlers, static_cast<int>(maps.size()));
  for (Handle<Object> const& handler : handlers) {
    KeyedAccessLoadMode mode = LoadHandler::GetKeyedAccessLoadMode(*handler);
    if (mode != STANDARD_LOAD) return mode;
  }

  return STANDARD_LOAD;
}

KeyedAccessStoreMode KeyedStoreICNexus::GetKeyedAccessStoreMode() const {
  KeyedAccessStoreMode mode = STANDARD_STORE;
  MapHandles maps;
  ObjectHandles handlers;

  if (GetKeyType() == PROPERTY) return mode;

  ExtractMaps(&maps);
  FindHandlers(&handlers, static_cast<int>(maps.size()));
  for (const Handle<Object>& maybe_code_handler : handlers) {
    // The first handler that isn't the slow handler will have the bits we need.
    Handle<Code> handler;
    if (maybe_code_handler->IsStoreHandler()) {
      Handle<StoreHandler> data_handler =
          Handle<StoreHandler>::cast(maybe_code_handler);
      handler = handle(Code::cast(data_handler->smi_handler()));
    } else if (maybe_code_handler->IsSmi()) {
      // Skip proxy handlers.
      DCHECK_EQ(*maybe_code_handler, *StoreHandler::StoreProxy(GetIsolate()));
      continue;
    } else {
      // Element store without prototype chain check.
      handler = Handle<Code>::cast(maybe_code_handler);
      if (handler->is_builtin()) continue;
    }
    CodeStub::Major major_key = CodeStub::MajorKeyFromKey(handler->stub_key());
    uint32_t minor_key = CodeStub::MinorKeyFromKey(handler->stub_key());
    CHECK(major_key == CodeStub::KeyedStoreSloppyArguments ||
          major_key == CodeStub::StoreFastElement ||
          major_key == CodeStub::StoreSlowElement ||
          major_key == CodeStub::ElementsTransitionAndStore ||
          major_key == CodeStub::NoCache);
    if (major_key != CodeStub::NoCache) {
      mode = CommonStoreModeBits::decode(minor_key);
      break;
    }
  }

  return mode;
}

IcCheckType KeyedLoadICNexus::GetKeyType() const {
  Object* feedback = GetFeedback();
  if (feedback == *FeedbackVector::MegamorphicSentinel(GetIsolate())) {
    return static_cast<IcCheckType>(Smi::ToInt(GetFeedbackExtra()));
  }
  return IsPropertyNameFeedback(feedback) ? PROPERTY : ELEMENT;
}

IcCheckType KeyedStoreICNexus::GetKeyType() const {
  Object* feedback = GetFeedback();
  if (feedback == *FeedbackVector::MegamorphicSentinel(GetIsolate())) {
    return static_cast<IcCheckType>(Smi::ToInt(GetFeedbackExtra()));
  }
  return IsPropertyNameFeedback(feedback) ? PROPERTY : ELEMENT;
}

InlineCacheState BinaryOpICNexus::StateFromFeedback() const {
  BinaryOperationHint hint = GetBinaryOperationFeedback();
  if (hint == BinaryOperationHint::kNone) {
    return UNINITIALIZED;
  } else if (hint == BinaryOperationHint::kAny) {
    return GENERIC;
  }

  return MONOMORPHIC;
}

InlineCacheState CompareICNexus::StateFromFeedback() const {
  CompareOperationHint hint = GetCompareOperationFeedback();
  if (hint == CompareOperationHint::kNone) {
    return UNINITIALIZED;
  } else if (hint == CompareOperationHint::kAny) {
    return GENERIC;
  }

  return MONOMORPHIC;
}

BinaryOperationHint BinaryOpICNexus::GetBinaryOperationFeedback() const {
  int feedback = Smi::ToInt(GetFeedback());
  return BinaryOperationHintFromFeedback(feedback);
}

CompareOperationHint CompareICNexus::GetCompareOperationFeedback() const {
  int feedback = Smi::ToInt(GetFeedback());
  return CompareOperationHintFromFeedback(feedback);
}

InlineCacheState ForInICNexus::StateFromFeedback() const {
  ForInHint hint = GetForInFeedback();
  if (hint == ForInHint::kNone) {
    return UNINITIALIZED;
  } else if (hint == ForInHint::kAny) {
    return GENERIC;
  }
  return MONOMORPHIC;
}

ForInHint ForInICNexus::GetForInFeedback() const {
  int feedback = Smi::ToInt(GetFeedback());
  return ForInHintFromFeedback(feedback);
}

void InstanceOfICNexus::ConfigureUninitialized() {
  SetFeedback(*FeedbackVector::UninitializedSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
}

InlineCacheState InstanceOfICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *FeedbackVector::MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  }
  return MONOMORPHIC;
}

MaybeHandle<JSObject> InstanceOfICNexus::GetConstructorFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();
  if (feedback->IsWeakCell() && !WeakCell::cast(feedback)->cleared()) {
    return handle(JSObject::cast(WeakCell::cast(feedback)->value()), isolate);
  }
  return MaybeHandle<JSObject>();
}

InlineCacheState StoreDataPropertyInLiteralICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback->IsWeakCell()) {
    // Don't check if the map is cleared.
    return MONOMORPHIC;
  }

  return MEGAMORPHIC;
}

void StoreDataPropertyInLiteralICNexus::ConfigureMonomorphic(
    Handle<Name> name, Handle<Map> receiver_map) {
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);

  SetFeedback(*cell);
  SetFeedbackExtra(*name);
}

InlineCacheState CollectTypeProfileNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* const feedback = GetFeedback();

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  }
  return MONOMORPHIC;
}

namespace {

bool InList(Handle<ArrayList> types, Handle<String> type) {
  for (int i = 0; i < types->Length(); i++) {
    Object* obj = types->Get(i);
    if (String::cast(obj)->Equals(*type)) {
      return true;
    }
  }
  return false;
}
}  // anonymous namespace

void CollectTypeProfileNexus::Collect(Handle<String> type, int position) {
  DCHECK_GE(position, 0);
  Isolate* isolate = GetIsolate();

  Object* const feedback = GetFeedback();

  // Map source position to collection of types
  Handle<NumberDictionary> types;

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    types = NumberDictionary::New(isolate, 1);
  } else {
    types = handle(NumberDictionary::cast(feedback));
  }

  Handle<ArrayList> position_specific_types;

  int entry = types->FindEntry(position);
  if (entry == NumberDictionary::kNotFound) {
    position_specific_types = ArrayList::New(isolate, 1);
    types = NumberDictionary::Set(
        types, position, ArrayList::Add(position_specific_types, type));
  } else {
    DCHECK(types->ValueAt(entry)->IsArrayList());
    position_specific_types = handle(ArrayList::cast(types->ValueAt(entry)));
    if (!InList(position_specific_types, type)) {  // Add type
      types = NumberDictionary::Set(
          types, position, ArrayList::Add(position_specific_types, type));
    }
  }
  SetFeedback(*types);
}

void CollectTypeProfileNexus::Clear() {
  SetFeedback(*FeedbackVector::UninitializedSentinel(GetIsolate()));
}

std::vector<int> CollectTypeProfileNexus::GetSourcePositions() const {
  std::vector<int> source_positions;
  Isolate* isolate = GetIsolate();

  Object* const feedback = GetFeedback();

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return source_positions;
  }

  Handle<NumberDictionary> types =
      Handle<NumberDictionary>(NumberDictionary::cast(feedback), isolate);

  for (int index = NumberDictionary::kElementsStartIndex;
       index < types->length(); index += NumberDictionary::kEntrySize) {
    int key_index = index + NumberDictionary::kEntryKeyIndex;
    Object* key = types->get(key_index);
    if (key->IsSmi()) {
      int position = Smi::cast(key)->value();
      source_positions.push_back(position);
    }
  }
  return source_positions;
}

std::vector<Handle<String>> CollectTypeProfileNexus::GetTypesForSourcePositions(
    uint32_t position) const {
  Isolate* isolate = GetIsolate();

  Object* const feedback = GetFeedback();
  std::vector<Handle<String>> types_for_position;
  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return types_for_position;
  }

  Handle<NumberDictionary> types =
      Handle<NumberDictionary>(NumberDictionary::cast(feedback), isolate);

  int entry = types->FindEntry(position);
  if (entry == NumberDictionary::kNotFound) {
    return types_for_position;
  }
  DCHECK(types->ValueAt(entry)->IsArrayList());
  Handle<ArrayList> position_specific_types =
      Handle<ArrayList>(ArrayList::cast(types->ValueAt(entry)));
  for (int i = 0; i < position_specific_types->Length(); i++) {
    Object* t = position_specific_types->Get(i);
    types_for_position.push_back(Handle<String>(String::cast(t), isolate));
  }

  return types_for_position;
}

namespace {

Handle<JSObject> ConvertToJSObject(Isolate* isolate,
                                   Handle<NumberDictionary> feedback) {
  Handle<JSObject> type_profile =
      isolate->factory()->NewJSObject(isolate->object_function());

  for (int index = NumberDictionary::kElementsStartIndex;
       index < feedback->length(); index += NumberDictionary::kEntrySize) {
    int key_index = index + NumberDictionary::kEntryKeyIndex;
    Object* key = feedback->get(key_index);
    if (key->IsSmi()) {
      int value_index = index + NumberDictionary::kEntryValueIndex;

      Handle<ArrayList> position_specific_types(
          ArrayList::cast(feedback->get(value_index)));

      int position = Smi::ToInt(key);
      JSObject::AddDataElement(
          type_profile, position,
          isolate->factory()->NewJSArrayWithElements(
              ArrayList::Elements(position_specific_types)),
          PropertyAttributes::NONE)
          .ToHandleChecked();
    }
  }
  return type_profile;
}
}  // namespace

JSObject* CollectTypeProfileNexus::GetTypeProfile() const {
  Isolate* isolate = GetIsolate();

  Object* const feedback = GetFeedback();

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return *isolate->factory()->NewJSObject(isolate->object_function());
  }

  return *ConvertToJSObject(isolate, handle(NumberDictionary::cast(feedback)));
}

}  // namespace internal
}  // namespace v8
