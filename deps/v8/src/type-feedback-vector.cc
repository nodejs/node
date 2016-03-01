// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/type-feedback-vector.h"

#include "src/code-stubs.h"
#include "src/ic/ic.h"
#include "src/ic/ic-state.h"
#include "src/objects.h"
#include "src/type-feedback-vector-inl.h"

namespace v8 {
namespace internal {


static bool IsPropertyNameFeedback(Object* feedback) {
  return feedback->IsString() ||
         (feedback->IsSymbol() && !Symbol::cast(feedback)->is_private());
}


std::ostream& operator<<(std::ostream& os, FeedbackVectorSlotKind kind) {
  return os << TypeFeedbackMetadata::Kind2String(kind);
}


FeedbackVectorSlotKind TypeFeedbackMetadata::GetKind(
    FeedbackVectorSlot slot) const {
  int index = VectorICComputer::index(kReservedIndexCount, slot.ToInt());
  int data = Smi::cast(get(index))->value();
  return VectorICComputer::decode(data, slot.ToInt());
}


void TypeFeedbackMetadata::SetKind(FeedbackVectorSlot slot,
                                   FeedbackVectorSlotKind kind) {
  int index = VectorICComputer::index(kReservedIndexCount, slot.ToInt());
  int data = Smi::cast(get(index))->value();
  int new_data = VectorICComputer::encode(data, slot.ToInt(), kind);
  set(index, Smi::FromInt(new_data));
}


template Handle<TypeFeedbackMetadata> TypeFeedbackMetadata::New(
    Isolate* isolate, const StaticFeedbackVectorSpec* spec);
template Handle<TypeFeedbackMetadata> TypeFeedbackMetadata::New(
    Isolate* isolate, const FeedbackVectorSpec* spec);


// static
template <typename Spec>
Handle<TypeFeedbackMetadata> TypeFeedbackMetadata::New(Isolate* isolate,
                                                       const Spec* spec) {
  const int slot_count = spec->slots();
  const int slot_kinds_length = VectorICComputer::word_count(slot_count);
  const int length = slot_kinds_length + kReservedIndexCount;
  if (length == kReservedIndexCount) {
    return Handle<TypeFeedbackMetadata>::cast(
        isolate->factory()->empty_fixed_array());
  }
#ifdef DEBUG
  for (int i = 0; i < slot_count;) {
    FeedbackVectorSlotKind kind = spec->GetKind(i);
    int entry_size = TypeFeedbackMetadata::GetSlotSize(kind);
    for (int j = 1; j < entry_size; j++) {
      FeedbackVectorSlotKind kind = spec->GetKind(i + j);
      DCHECK_EQ(FeedbackVectorSlotKind::INVALID, kind);
    }
    i += entry_size;
  }
#endif

  Handle<FixedArray> array = isolate->factory()->NewFixedArray(length, TENURED);
  array->set(kSlotsCountIndex, Smi::FromInt(slot_count));
  // Fill the bit-vector part with zeros.
  for (int i = 0; i < slot_kinds_length; i++) {
    array->set(kReservedIndexCount + i, Smi::FromInt(0));
  }

  Handle<TypeFeedbackMetadata> metadata =
      Handle<TypeFeedbackMetadata>::cast(array);
  for (int i = 0; i < slot_count; i++) {
    metadata->SetKind(FeedbackVectorSlot(i), spec->GetKind(i));
  }
  return metadata;
}


bool TypeFeedbackMetadata::SpecDiffersFrom(
    const FeedbackVectorSpec* other_spec) const {
  if (other_spec->slots() != slot_count()) {
    return true;
  }

  int slots = slot_count();
  for (int i = 0; i < slots; i++) {
    if (GetKind(FeedbackVectorSlot(i)) != other_spec->GetKind(i)) {
      return true;
    }
  }
  return false;
}


const char* TypeFeedbackMetadata::Kind2String(FeedbackVectorSlotKind kind) {
  switch (kind) {
    case FeedbackVectorSlotKind::INVALID:
      return "INVALID";
    case FeedbackVectorSlotKind::CALL_IC:
      return "CALL_IC";
    case FeedbackVectorSlotKind::LOAD_IC:
      return "LOAD_IC";
    case FeedbackVectorSlotKind::KEYED_LOAD_IC:
      return "KEYED_LOAD_IC";
    case FeedbackVectorSlotKind::STORE_IC:
      return "STORE_IC";
    case FeedbackVectorSlotKind::KEYED_STORE_IC:
      return "KEYED_STORE_IC";
    case FeedbackVectorSlotKind::GENERAL:
      return "STUB";
    case FeedbackVectorSlotKind::KINDS_NUMBER:
      break;
  }
  UNREACHABLE();
  return "?";
}


// static
Handle<TypeFeedbackVector> TypeFeedbackVector::New(
    Isolate* isolate, Handle<TypeFeedbackMetadata> metadata) {
  Factory* factory = isolate->factory();

  const int slot_count = metadata->slot_count();
  const int length = slot_count + kReservedIndexCount;
  if (length == kReservedIndexCount) {
    return Handle<TypeFeedbackVector>::cast(factory->empty_fixed_array());
  }

  Handle<FixedArray> array = factory->NewFixedArray(length, TENURED);
  array->set(kMetadataIndex, *metadata);

  // Ensure we can skip the write barrier
  Handle<Object> uninitialized_sentinel = UninitializedSentinel(isolate);
  DCHECK_EQ(*factory->uninitialized_symbol(), *uninitialized_sentinel);
  for (int i = kReservedIndexCount; i < length; i++) {
    array->set(i, *uninitialized_sentinel, SKIP_WRITE_BARRIER);
  }

  return Handle<TypeFeedbackVector>::cast(array);
}


// static
int TypeFeedbackVector::GetIndexFromSpec(const FeedbackVectorSpec* spec,
                                         FeedbackVectorSlot slot) {
  return kReservedIndexCount + slot.ToInt();
}


// static
Handle<TypeFeedbackVector> TypeFeedbackVector::Copy(
    Isolate* isolate, Handle<TypeFeedbackVector> vector) {
  Handle<TypeFeedbackVector> result;
  result = Handle<TypeFeedbackVector>::cast(
      isolate->factory()->CopyFixedArray(Handle<FixedArray>::cast(vector)));
  return result;
}


// This logic is copied from
// StaticMarkingVisitor<StaticVisitor>::VisitCodeTarget.
static bool ClearLogic(Isolate* isolate) {
  return FLAG_cleanup_code_caches_at_gc && isolate->serializer_enabled();
}


void TypeFeedbackVector::ClearSlotsImpl(SharedFunctionInfo* shared,
                                        bool force_clear) {
  Isolate* isolate = GetIsolate();

  if (!force_clear && !ClearLogic(isolate)) return;

  Object* uninitialized_sentinel =
      TypeFeedbackVector::RawUninitializedSentinel(isolate);

  TypeFeedbackMetadataIterator iter(metadata());
  while (iter.HasNext()) {
    FeedbackVectorSlot slot = iter.Next();
    FeedbackVectorSlotKind kind = iter.kind();

    Object* obj = Get(slot);
    if (obj != uninitialized_sentinel) {
      switch (kind) {
        case FeedbackVectorSlotKind::CALL_IC: {
          CallICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::LOAD_IC: {
          LoadICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::KEYED_LOAD_IC: {
          KeyedLoadICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::STORE_IC: {
          StoreICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::KEYED_STORE_IC: {
          KeyedStoreICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::GENERAL: {
          if (obj->IsHeapObject()) {
            InstanceType instance_type =
                HeapObject::cast(obj)->map()->instance_type();
            // AllocationSites are exempt from clearing. They don't store Maps
            // or Code pointers which can cause memory leaks if not cleared
            // regularly.
            if (instance_type != ALLOCATION_SITE_TYPE) {
              Set(slot, uninitialized_sentinel, SKIP_WRITE_BARRIER);
            }
          }
          break;
        }
        case FeedbackVectorSlotKind::INVALID:
        case FeedbackVectorSlotKind::KINDS_NUMBER:
          UNREACHABLE();
          break;
      }
    }
  }
}


// static
void TypeFeedbackVector::ClearAllKeyedStoreICs(Isolate* isolate) {
  SharedFunctionInfo::Iterator iterator(isolate);
  SharedFunctionInfo* shared;
  while ((shared = iterator.Next())) {
    TypeFeedbackVector* vector = shared->feedback_vector();
    vector->ClearKeyedStoreICs(shared);
  }
}


void TypeFeedbackVector::ClearKeyedStoreICs(SharedFunctionInfo* shared) {
  Isolate* isolate = GetIsolate();

  Code* host = shared->code();
  Object* uninitialized_sentinel =
      TypeFeedbackVector::RawUninitializedSentinel(isolate);

  TypeFeedbackMetadataIterator iter(metadata());
  while (iter.HasNext()) {
    FeedbackVectorSlot slot = iter.Next();
    FeedbackVectorSlotKind kind = iter.kind();
    if (kind != FeedbackVectorSlotKind::KEYED_STORE_IC) continue;
    Object* obj = Get(slot);
    if (obj != uninitialized_sentinel) {
      KeyedStoreICNexus nexus(this, slot);
      nexus.Clear(host);
    }
  }
}


// static
Handle<TypeFeedbackVector> TypeFeedbackVector::DummyVector(Isolate* isolate) {
  return isolate->factory()->dummy_vector();
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


void FeedbackNexus::InstallHandlers(Handle<FixedArray> array,
                                    MapHandleList* maps,
                                    CodeHandleList* handlers) {
  int receiver_count = maps->length();
  for (int current = 0; current < receiver_count; ++current) {
    Handle<Map> map = maps->at(current);
    Handle<WeakCell> cell = Map::WeakCellForMap(map);
    array->set(current * 2, *cell);
    array->set(current * 2 + 1, *handlers->at(current));
  }
}


void FeedbackNexus::ConfigureUninitialized() {
  SetFeedback(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
                   SKIP_WRITE_BARRIER);
}


void FeedbackNexus::ConfigurePremonomorphic() {
  SetFeedback(*TypeFeedbackVector::PremonomorphicSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
                   SKIP_WRITE_BARRIER);
}


void FeedbackNexus::ConfigureMegamorphic() {
  Isolate* isolate = GetIsolate();
  SetFeedback(*TypeFeedbackVector::MegamorphicSentinel(isolate),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}


InlineCacheState LoadICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *TypeFeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *TypeFeedbackVector::MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  } else if (feedback == *TypeFeedbackVector::PremonomorphicSentinel(isolate)) {
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

  if (feedback == *TypeFeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *TypeFeedbackVector::PremonomorphicSentinel(isolate)) {
    return PREMONOMORPHIC;
  } else if (feedback == *TypeFeedbackVector::MegamorphicSentinel(isolate)) {
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

  if (feedback == *TypeFeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *TypeFeedbackVector::MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  } else if (feedback == *TypeFeedbackVector::PremonomorphicSentinel(isolate)) {
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

  if (feedback == *TypeFeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *TypeFeedbackVector::PremonomorphicSentinel(isolate)) {
    return PREMONOMORPHIC;
  } else if (feedback == *TypeFeedbackVector::MegamorphicSentinel(isolate)) {
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
             *TypeFeedbackVector::UninitializedSentinel(isolate) ||
         GetFeedbackExtra()->IsSmi());

  if (feedback == *TypeFeedbackVector::MegamorphicSentinel(isolate)) {
    return GENERIC;
  } else if (feedback->IsAllocationSite() || feedback->IsWeakCell()) {
    return MONOMORPHIC;
  }

  CHECK(feedback == *TypeFeedbackVector::UninitializedSentinel(isolate));
  return UNINITIALIZED;
}


int CallICNexus::ExtractCallCount() {
  Object* call_count = GetFeedbackExtra();
  if (call_count->IsSmi()) {
    int value = Smi::cast(call_count)->value() / 2;
    return value;
  }
  return -1;
}


void CallICNexus::Clear(Code* host) { CallIC::Clear(GetIsolate(), host, this); }


void CallICNexus::ConfigureMonomorphicArray() {
  Object* feedback = GetFeedback();
  if (!feedback->IsAllocationSite()) {
    Handle<AllocationSite> new_site =
        GetIsolate()->factory()->NewAllocationSite();
    SetFeedback(*new_site);
  }
  SetFeedbackExtra(Smi::FromInt(kCallCountIncrement), SKIP_WRITE_BARRIER);
}


void CallICNexus::ConfigureMonomorphic(Handle<JSFunction> function) {
  Handle<WeakCell> new_cell = GetIsolate()->factory()->NewWeakCell(function);
  SetFeedback(*new_cell);
  SetFeedbackExtra(Smi::FromInt(kCallCountIncrement), SKIP_WRITE_BARRIER);
}


void CallICNexus::ConfigureMegamorphic() {
  FeedbackNexus::ConfigureMegamorphic();
}


void CallICNexus::ConfigureMegamorphic(int call_count) {
  SetFeedback(*TypeFeedbackVector::MegamorphicSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(Smi::FromInt(call_count * kCallCountIncrement),
                   SKIP_WRITE_BARRIER);
}


void LoadICNexus::ConfigureMonomorphic(Handle<Map> receiver_map,
                                       Handle<Code> handler) {
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
  SetFeedback(*cell);
  SetFeedbackExtra(*handler);
}


void KeyedLoadICNexus::ConfigureMonomorphic(Handle<Name> name,
                                            Handle<Map> receiver_map,
                                            Handle<Code> handler) {
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


void StoreICNexus::ConfigureMonomorphic(Handle<Map> receiver_map,
                                        Handle<Code> handler) {
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
  SetFeedback(*cell);
  SetFeedbackExtra(*handler);
}


void KeyedStoreICNexus::ConfigureMonomorphic(Handle<Name> name,
                                             Handle<Map> receiver_map,
                                             Handle<Code> handler) {
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


void LoadICNexus::ConfigurePolymorphic(MapHandleList* maps,
                                       CodeHandleList* handlers) {
  Isolate* isolate = GetIsolate();
  int receiver_count = maps->length();
  Handle<FixedArray> array = EnsureArrayOfSize(receiver_count * 2);
  InstallHandlers(array, maps, handlers);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}


void KeyedLoadICNexus::ConfigurePolymorphic(Handle<Name> name,
                                            MapHandleList* maps,
                                            CodeHandleList* handlers) {
  int receiver_count = maps->length();
  DCHECK(receiver_count > 1);
  Handle<FixedArray> array;
  if (name.is_null()) {
    array = EnsureArrayOfSize(receiver_count * 2);
    SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
                     SKIP_WRITE_BARRIER);
  } else {
    array = EnsureExtraArrayOfSize(receiver_count * 2);
    SetFeedback(*name);
  }

  InstallHandlers(array, maps, handlers);
}


void StoreICNexus::ConfigurePolymorphic(MapHandleList* maps,
                                        CodeHandleList* handlers) {
  Isolate* isolate = GetIsolate();
  int receiver_count = maps->length();
  Handle<FixedArray> array = EnsureArrayOfSize(receiver_count * 2);
  InstallHandlers(array, maps, handlers);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}


void KeyedStoreICNexus::ConfigurePolymorphic(Handle<Name> name,
                                             MapHandleList* maps,
                                             CodeHandleList* handlers) {
  int receiver_count = maps->length();
  DCHECK(receiver_count > 1);
  Handle<FixedArray> array;
  if (name.is_null()) {
    array = EnsureArrayOfSize(receiver_count * 2);
    SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
                     SKIP_WRITE_BARRIER);
  } else {
    array = EnsureExtraArrayOfSize(receiver_count * 2);
    SetFeedback(*name);
  }

  InstallHandlers(array, maps, handlers);
}


void KeyedStoreICNexus::ConfigurePolymorphic(MapHandleList* maps,
                                             MapHandleList* transitioned_maps,
                                             CodeHandleList* handlers) {
  int receiver_count = maps->length();
  DCHECK(receiver_count > 1);
  Handle<FixedArray> array = EnsureArrayOfSize(receiver_count * 3);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
                   SKIP_WRITE_BARRIER);

  Handle<Oddball> undefined_value = GetIsolate()->factory()->undefined_value();
  for (int i = 0; i < receiver_count; ++i) {
    Handle<Map> map = maps->at(i);
    Handle<WeakCell> cell = Map::WeakCellForMap(map);
    array->set(i * 3, *cell);
    if (!transitioned_maps->at(i).is_null()) {
      Handle<Map> transitioned_map = transitioned_maps->at(i);
      cell = Map::WeakCellForMap(transitioned_map);
      array->set((i * 3) + 1, *cell);
    } else {
      array->set((i * 3) + 1, *undefined_value);
    }
    array->set((i * 3) + 2, *handlers->at(i));
  }
}


int FeedbackNexus::ExtractMaps(MapHandleList* maps) const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsFixedArray() || is_named_feedback) {
    int found = 0;
    if (is_named_feedback) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    // The array should be of the form
    // [map, handler, map, handler, ...]
    // or
    // [map, map, handler, map, map, handler, ...]
    DCHECK(array->length() >= 2);
    int increment = array->get(1)->IsCode() ? 2 : 3;
    for (int i = 0; i < array->length(); i += increment) {
      DCHECK(array->get(i)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(array->get(i));
      if (!cell->cleared()) {
        Map* map = Map::cast(cell->value());
        maps->Add(handle(map, isolate));
        found++;
      }
    }
    return found;
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Map* map = Map::cast(cell->value());
      maps->Add(handle(map, isolate));
      return 1;
    }
  }

  return 0;
}


MaybeHandle<Code> FeedbackNexus::FindHandlerForMap(Handle<Map> map) const {
  Object* feedback = GetFeedback();
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsFixedArray() || is_named_feedback) {
    if (is_named_feedback) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    DCHECK(array->length() >= 2);
    int increment = array->get(1)->IsCode() ? 2 : 3;
    for (int i = 0; i < array->length(); i += increment) {
      DCHECK(array->get(i)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(array->get(i));
      if (!cell->cleared()) {
        Map* array_map = Map::cast(cell->value());
        if (array_map == *map) {
          Code* code = Code::cast(array->get(i + increment - 1));
          DCHECK(code->kind() == Code::HANDLER);
          return handle(code);
        }
      }
    }
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Map* cell_map = Map::cast(cell->value());
      if (cell_map == *map) {
        Code* code = Code::cast(GetFeedbackExtra());
        DCHECK(code->kind() == Code::HANDLER);
        return handle(code);
      }
    }
  }

  return MaybeHandle<Code>();
}


bool FeedbackNexus::FindHandlers(CodeHandleList* code_list, int length) const {
  Object* feedback = GetFeedback();
  int count = 0;
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsFixedArray() || is_named_feedback) {
    if (is_named_feedback) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    // The array should be of the form
    // [map, handler, map, handler, ...]
    // or
    // [map, map, handler, map, map, handler, ...]
    // Be sure to skip handlers whose maps have been cleared.
    DCHECK(array->length() >= 2);
    int increment = array->get(1)->IsCode() ? 2 : 3;
    for (int i = 0; i < array->length(); i += increment) {
      DCHECK(array->get(i)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(array->get(i));
      if (!cell->cleared()) {
        Code* code = Code::cast(array->get(i + increment - 1));
        DCHECK(code->kind() == Code::HANDLER);
        code_list->Add(handle(code));
        count++;
      }
    }
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Code* code = Code::cast(GetFeedbackExtra());
      DCHECK(code->kind() == Code::HANDLER);
      code_list->Add(handle(code));
      count++;
    }
  }
  return count == length;
}


void LoadICNexus::Clear(Code* host) { LoadIC::Clear(GetIsolate(), host, this); }


void KeyedLoadICNexus::Clear(Code* host) {
  KeyedLoadIC::Clear(GetIsolate(), host, this);
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


void StoreICNexus::Clear(Code* host) {
  StoreIC::Clear(GetIsolate(), host, this);
}


void KeyedStoreICNexus::Clear(Code* host) {
  KeyedStoreIC::Clear(GetIsolate(), host, this);
}


KeyedAccessStoreMode KeyedStoreICNexus::GetKeyedAccessStoreMode() const {
  KeyedAccessStoreMode mode = STANDARD_STORE;
  MapHandleList maps;
  CodeHandleList handlers;

  if (GetKeyType() == PROPERTY) return mode;

  ExtractMaps(&maps);
  FindHandlers(&handlers, maps.length());
  for (int i = 0; i < handlers.length(); i++) {
    // The first handler that isn't the slow handler will have the bits we need.
    Handle<Code> handler = handlers.at(i);
    CodeStub::Major major_key = CodeStub::MajorKeyFromKey(handler->stub_key());
    uint32_t minor_key = CodeStub::MinorKeyFromKey(handler->stub_key());
    CHECK(major_key == CodeStub::KeyedStoreSloppyArguments ||
          major_key == CodeStub::StoreFastElement ||
          major_key == CodeStub::StoreElement ||
          major_key == CodeStub::ElementsTransitionAndStore ||
          major_key == CodeStub::NoCache);
    if (major_key != CodeStub::NoCache) {
      mode = CommonStoreModeBits::decode(minor_key);
      break;
    }
  }

  return mode;
}


IcCheckType KeyedStoreICNexus::GetKeyType() const {
  // The structure of the vector slots tells us the type.
  return GetFeedback()->IsName() ? PROPERTY : ELEMENT;
}
}  // namespace internal
}  // namespace v8
