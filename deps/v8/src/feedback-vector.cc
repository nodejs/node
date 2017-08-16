// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/feedback-vector.h"
#include "src/code-stubs.h"
#include "src/feedback-vector-inl.h"
#include "src/ic/ic-inl.h"
#include "src/ic/ic-state.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

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
  int data = Smi::cast(get(index))->value();
  return VectorICComputer::decode(data, slot.ToInt());
}

void FeedbackMetadata::SetKind(FeedbackSlot slot, FeedbackSlotKind kind) {
  int index = VectorICComputer::index(kReservedIndexCount, slot.ToInt());
  int data = Smi::cast(get(index))->value();
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
    case FeedbackSlotKind::kToBoolean:
      return "ToBoolean";
    case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      return "StoreDataPropertyInLiteral";
    case FeedbackSlotKind::kCreateClosure:
      return "kCreateClosure";
    case FeedbackSlotKind::kLiteral:
      return "Literal";
    case FeedbackSlotKind::kTypeProfile:
      return "TypeProfile";
    case FeedbackSlotKind::kGeneral:
      return "General";
    case FeedbackSlotKind::kKindsNumber:
      break;
  }
  UNREACHABLE();
  return "?";
}

bool FeedbackMetadata::HasTypeProfileSlot() const {
  FeedbackSlot slot =
      FeedbackVector::ToSlot(FeedbackVectorSpec::kTypeProfileSlotIndex);
  return GetKind(slot) == FeedbackSlotKind::kTypeProfile;
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
  const int length = slot_count + kReservedIndexCount;

  Handle<FixedArray> array = factory->NewFixedArray(length, TENURED);
  array->set_map_no_write_barrier(isolate->heap()->feedback_vector_map());
  array->set(kSharedFunctionInfoIndex, *shared);
  array->set(kOptimizedCodeIndex, *factory->empty_weak_cell());
  array->set(kInvocationCountIndex, Smi::kZero);

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
        array->set(index, isolate->heap()->empty_weak_cell(),
                   SKIP_WRITE_BARRIER);
        break;
      case FeedbackSlotKind::kCompareOp:
      case FeedbackSlotKind::kBinaryOp:
      case FeedbackSlotKind::kToBoolean:
        array->set(index, Smi::kZero, SKIP_WRITE_BARRIER);
        break;
      case FeedbackSlotKind::kCreateClosure: {
        Handle<Cell> cell = factory->NewNoClosuresCell(undefined_value);
        array->set(index, *cell);
        break;
      }
      case FeedbackSlotKind::kLiteral:
        array->set(index, *undefined_value, SKIP_WRITE_BARRIER);
        break;
      case FeedbackSlotKind::kCall:
        array->set(index, *uninitialized_sentinel, SKIP_WRITE_BARRIER);
        extra_value = Smi::kZero;
        break;
      case FeedbackSlotKind::kLoadProperty:
      case FeedbackSlotKind::kLoadKeyed:
      case FeedbackSlotKind::kStoreNamedSloppy:
      case FeedbackSlotKind::kStoreNamedStrict:
      case FeedbackSlotKind::kStoreOwnNamed:
      case FeedbackSlotKind::kStoreGlobalSloppy:
      case FeedbackSlotKind::kStoreGlobalStrict:
      case FeedbackSlotKind::kStoreKeyedSloppy:
      case FeedbackSlotKind::kStoreKeyedStrict:
      case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      case FeedbackSlotKind::kGeneral:
      case FeedbackSlotKind::kTypeProfile:
        array->set(index, *uninitialized_sentinel, SKIP_WRITE_BARRIER);
        break;

      case FeedbackSlotKind::kInvalid:
      case FeedbackSlotKind::kKindsNumber:
        UNREACHABLE();
        array->set(index, Smi::kZero, SKIP_WRITE_BARRIER);
        break;
    }
    for (int j = 1; j < entry_size; j++) {
      array->set(index + j, extra_value, SKIP_WRITE_BARRIER);
    }
    i += entry_size;
  }

  Handle<FeedbackVector> result = Handle<FeedbackVector>::cast(array);
  if (!isolate->is_best_effort_code_coverage()) {
    AddToCodeCoverageList(isolate, result);
  }
  return result;
}

// static
Handle<FeedbackVector> FeedbackVector::Copy(Isolate* isolate,
                                            Handle<FeedbackVector> vector) {
  Handle<FeedbackVector> result;
  result = Handle<FeedbackVector>::cast(
      isolate->factory()->CopyFixedArray(Handle<FixedArray>::cast(vector)));
  if (!isolate->is_best_effort_code_coverage()) {
    AddToCodeCoverageList(isolate, result);
  }
  return result;
}

// static
void FeedbackVector::AddToCodeCoverageList(Isolate* isolate,
                                           Handle<FeedbackVector> vector) {
  DCHECK(!isolate->is_best_effort_code_coverage());
  if (!vector->shared_function_info()->IsSubjectToDebugging()) return;
  Handle<ArrayList> list =
      Handle<ArrayList>::cast(isolate->factory()->code_coverage_list());
  list = ArrayList::Add(list, vector);
  isolate->SetCodeCoverageList(*list);
}

// static
void FeedbackVector::SetOptimizedCode(Handle<FeedbackVector> vector,
                                      Handle<Code> code) {
  DCHECK_EQ(code->kind(), Code::OPTIMIZED_FUNCTION);
  Factory* factory = vector->GetIsolate()->factory();
  Handle<WeakCell> cell = factory->NewWeakCell(code);
  vector->set(kOptimizedCodeIndex, *cell);
}

void FeedbackVector::ClearOptimizedCode() {
  set(kOptimizedCodeIndex, GetIsolate()->heap()->empty_weak_cell());
}

void FeedbackVector::EvictOptimizedCodeMarkedForDeoptimization(
    SharedFunctionInfo* shared, const char* reason) {
  WeakCell* cell = WeakCell::cast(get(kOptimizedCodeIndex));
  if (!cell->cleared()) {
    Code* code = Code::cast(cell->value());
    if (code->marked_for_deoptimization()) {
      if (FLAG_trace_deopt) {
        PrintF("[evicting optimizing code marked for deoptimization (%s) for ",
               reason);
        shared->ShortPrint();
        PrintF("]\n");
      }
      if (!code->deopt_already_counted()) {
        shared->increment_deopt_count();
        code->set_deopt_already_counted(true);
      }
      ClearOptimizedCode();
    }
  }
}

void FeedbackVector::ClearSlots(JSFunction* host_function) {
  Isolate* isolate = GetIsolate();

  Object* uninitialized_sentinel =
      FeedbackVector::RawUninitializedSentinel(isolate);
  Oddball* undefined_value = isolate->heap()->undefined_value();

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
        case FeedbackSlotKind::kStoreOwnNamed:
        case FeedbackSlotKind::kStoreGlobalSloppy:
        case FeedbackSlotKind::kStoreGlobalStrict: {
          StoreICNexus nexus(this, slot);
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
        case FeedbackSlotKind::kBinaryOp:
        case FeedbackSlotKind::kCompareOp: {
          DCHECK(Get(slot)->IsSmi());
          // don't clear these smi slots.
          // Set(slot, Smi::kZero);
          break;
        }
        case FeedbackSlotKind::kCreateClosure: {
          case FeedbackSlotKind::kTypeProfile:
            break;
        }
        case FeedbackSlotKind::kGeneral: {
          if (obj->IsHeapObject()) {
            InstanceType instance_type =
                HeapObject::cast(obj)->map()->instance_type();
            // AllocationSites are exempt from clearing. They don't store Maps
            // or Code pointers which can cause memory leaks if not cleared
            // regularly.
            if (instance_type != ALLOCATION_SITE_TYPE) {
              Set(slot, uninitialized_sentinel, SKIP_WRITE_BARRIER);
              feedback_updated = true;
            }
          }
          break;
        }
        case FeedbackSlotKind::kLiteral: {
          Set(slot, undefined_value, SKIP_WRITE_BARRIER);
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
        case FeedbackSlotKind::kToBoolean:
        case FeedbackSlotKind::kInvalid:
        case FeedbackSlotKind::kKindsNumber:
          UNREACHABLE();
          break;
      }
    }
  }
  if (feedback_updated) {
    IC::OnFeedbackChanged(isolate, host_function);
  }
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

void FeedbackNexus::ConfigureMegamorphic(IcCheckType property_type) {
  Isolate* isolate = GetIsolate();
  SetFeedback(*FeedbackVector::MegamorphicSentinel(isolate),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(Smi::FromInt(static_cast<int>(property_type)),
                   SKIP_WRITE_BARRIER);
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

InlineCacheState LoadGlobalICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  Object* extra = GetFeedbackExtra();
  if (!WeakCell::cast(feedback)->cleared() ||
      extra != *FeedbackVector::UninitializedSentinel(isolate)) {
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

int CallICNexus::ExtractCallCount() {
  Object* call_count = GetFeedbackExtra();
  CHECK(call_count->IsSmi());
  int value = Smi::cast(call_count)->value();
  return value;
}

float CallICNexus::ComputeCallFrequency() {
  double const invocation_count = vector()->invocation_count();
  double const call_count = ExtractCallCount();
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

void LoadGlobalICNexus::ConfigureUninitialized() {
  Isolate* isolate = GetIsolate();
  SetFeedback(isolate->heap()->empty_weak_cell(), SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}

void LoadGlobalICNexus::ConfigurePropertyCellMode(Handle<PropertyCell> cell) {
  Isolate* isolate = GetIsolate();
  SetFeedback(*isolate->factory()->NewWeakCell(cell));
  SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}

void LoadGlobalICNexus::ConfigureHandlerMode(Handle<Object> handler) {
  SetFeedback(GetIsolate()->heap()->empty_weak_cell());
  SetFeedbackExtra(*handler);
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
                                         List<Handle<Object>>* handlers) {
  int receiver_count = static_cast<int>(maps.size());
  DCHECK(receiver_count > 1);
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

bool FeedbackNexus::FindHandlers(List<Handle<Object>>* code_list,
                                 int length) const {
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
        code_list->Add(handle(code, isolate));
        count++;
      }
    }
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Object* code = GetFeedbackExtra();
      DCHECK(IC::IsHandler(code));
      code_list->Add(handle(code, isolate));
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
  return NULL;
}

Name* KeyedStoreICNexus::FindFirstName() const {
  Object* feedback = GetFeedback();
  if (IsPropertyNameFeedback(feedback)) {
    return Name::cast(feedback);
  }
  return NULL;
}

KeyedAccessStoreMode KeyedStoreICNexus::GetKeyedAccessStoreMode() const {
  KeyedAccessStoreMode mode = STANDARD_STORE;
  MapHandles maps;
  List<Handle<Object>> handlers;

  if (GetKeyType() == PROPERTY) return mode;

  ExtractMaps(&maps);
  FindHandlers(&handlers, static_cast<int>(maps.size()));
  for (int i = 0; i < handlers.length(); i++) {
    // The first handler that isn't the slow handler will have the bits we need.
    Handle<Object> maybe_code_handler = handlers.at(i);
    Handle<Code> handler;
    if (maybe_code_handler->IsTuple3()) {
      // Elements transition.
      Handle<Tuple3> data_handler = Handle<Tuple3>::cast(maybe_code_handler);
      handler = handle(Code::cast(data_handler->value2()));
    } else if (maybe_code_handler->IsTuple2()) {
      // Element store with prototype chain check.
      Handle<Tuple2> data_handler = Handle<Tuple2>::cast(maybe_code_handler);
      handler = handle(Code::cast(data_handler->value2()));
    } else {
      // Element store without prototype chain check.
      handler = Handle<Code>::cast(maybe_code_handler);
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
    return static_cast<IcCheckType>(Smi::cast(GetFeedbackExtra())->value());
  }
  return IsPropertyNameFeedback(feedback) ? PROPERTY : ELEMENT;
}

IcCheckType KeyedStoreICNexus::GetKeyType() const {
  Object* feedback = GetFeedback();
  if (feedback == *FeedbackVector::MegamorphicSentinel(GetIsolate())) {
    return static_cast<IcCheckType>(Smi::cast(GetFeedbackExtra())->value());
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
  int feedback = Smi::cast(GetFeedback())->value();
  return BinaryOperationHintFromFeedback(feedback);
}

CompareOperationHint CompareICNexus::GetCompareOperationFeedback() const {
  int feedback = Smi::cast(GetFeedback())->value();
  return CompareOperationHintFromFeedback(feedback);
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

void CollectTypeProfileNexus::Collect(Handle<String> type, int position) {
  DCHECK_GE(position, 0);
  Isolate* isolate = GetIsolate();

  Object* const feedback = GetFeedback();

  // Map source position to collection of types
  Handle<UnseededNumberDictionary> types;

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    types = UnseededNumberDictionary::NewEmpty(isolate);
  } else {
    types = handle(UnseededNumberDictionary::cast(feedback));
  }

  Handle<ArrayList> position_specific_types;

  if (types->Has(position)) {
    int entry = types->FindEntry(position);
    DCHECK(types->ValueAt(entry)->IsArrayList());
    position_specific_types = handle(ArrayList::cast(types->ValueAt(entry)));
  } else {
    position_specific_types = ArrayList::New(isolate, 1);
  }

  types = UnseededNumberDictionary::Set(
      types, position, ArrayList::Add(position_specific_types, type));
  SetFeedback(*types);
}

namespace {

Handle<JSObject> ConvertToJSObject(Isolate* isolate,
                                   Handle<UnseededNumberDictionary> feedback) {
  Handle<JSObject> type_profile =
      isolate->factory()->NewJSObject(isolate->object_function());

  for (int index = UnseededNumberDictionary::kElementsStartIndex;
       index < feedback->length();
       index += UnseededNumberDictionary::kEntrySize) {
    int key_index = index + UnseededNumberDictionary::kEntryKeyIndex;
    Object* key = feedback->get(key_index);
    if (key->IsSmi()) {
      int value_index = index + UnseededNumberDictionary::kEntryValueIndex;

      Handle<ArrayList> position_specific_types(
          ArrayList::cast(feedback->get(value_index)));

      int position = Smi::cast(key)->value();
      JSObject::AddDataElement(type_profile, position,
                               isolate->factory()->NewJSArrayWithElements(
                                   position_specific_types->Elements()),
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

  return *ConvertToJSObject(isolate,
                            handle(UnseededNumberDictionary::cast(feedback)));
}

}  // namespace internal
}  // namespace v8
