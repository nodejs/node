// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/ic/ic.h"
#include "src/ic/ic-state.h"
#include "src/objects.h"
#include "src/type-feedback-vector-inl.h"

namespace v8 {
namespace internal {

// static
TypeFeedbackVector::VectorICKind TypeFeedbackVector::FromCodeKind(
    Code::Kind kind) {
  switch (kind) {
    case Code::CALL_IC:
      return KindCallIC;
    case Code::LOAD_IC:
      return KindLoadIC;
    case Code::KEYED_LOAD_IC:
      return KindKeyedLoadIC;
    default:
      // Shouldn't get here.
      UNREACHABLE();
  }

  return KindUnused;
}


// static
Code::Kind TypeFeedbackVector::FromVectorICKind(VectorICKind kind) {
  switch (kind) {
    case KindCallIC:
      return Code::CALL_IC;
    case KindLoadIC:
      return Code::LOAD_IC;
    case KindKeyedLoadIC:
      return Code::KEYED_LOAD_IC;
    case KindUnused:
      break;
  }
  // Sentinel for no information.
  return Code::NUMBER_OF_KINDS;
}


Code::Kind TypeFeedbackVector::GetKind(FeedbackVectorICSlot slot) const {
  if (!FLAG_vector_ics) {
    // We only have CALL_ICs
    return Code::CALL_IC;
  }

  int index = VectorICComputer::index(kReservedIndexCount, slot.ToInt());
  int data = Smi::cast(get(index))->value();
  VectorICKind b = VectorICComputer::decode(data, slot.ToInt());
  return FromVectorICKind(b);
}


void TypeFeedbackVector::SetKind(FeedbackVectorICSlot slot, Code::Kind kind) {
  if (!FLAG_vector_ics) {
    // Nothing to do if we only have CALL_ICs
    return;
  }

  VectorICKind b = FromCodeKind(kind);
  int index = VectorICComputer::index(kReservedIndexCount, slot.ToInt());
  int data = Smi::cast(get(index))->value();
  int new_data = VectorICComputer::encode(data, slot.ToInt(), b);
  set(index, Smi::FromInt(new_data));
}


template Handle<TypeFeedbackVector> TypeFeedbackVector::Allocate(
    Isolate* isolate, const FeedbackVectorSpec* spec);
template Handle<TypeFeedbackVector> TypeFeedbackVector::Allocate(
    Isolate* isolate, const ZoneFeedbackVectorSpec* spec);


// static
template <typename Spec>
Handle<TypeFeedbackVector> TypeFeedbackVector::Allocate(Isolate* isolate,
                                                        const Spec* spec) {
  const int slot_count = spec->slots();
  const int ic_slot_count = spec->ic_slots();
  const int index_count =
      FLAG_vector_ics ? VectorICComputer::word_count(ic_slot_count) : 0;
  const int length = slot_count + (ic_slot_count * elements_per_ic_slot()) +
                     index_count + kReservedIndexCount;
  if (length == kReservedIndexCount) {
    return Handle<TypeFeedbackVector>::cast(
        isolate->factory()->empty_fixed_array());
  }

  Handle<FixedArray> array = isolate->factory()->NewFixedArray(length, TENURED);
  if (ic_slot_count > 0) {
    array->set(kFirstICSlotIndex,
               Smi::FromInt(slot_count + index_count + kReservedIndexCount));
  } else {
    array->set(kFirstICSlotIndex, Smi::FromInt(length));
  }
  array->set(kWithTypesIndex, Smi::FromInt(0));
  array->set(kGenericCountIndex, Smi::FromInt(0));
  // Fill the indexes with zeros.
  for (int i = 0; i < index_count; i++) {
    array->set(kReservedIndexCount + i, Smi::FromInt(0));
  }

  // Ensure we can skip the write barrier
  Handle<Object> uninitialized_sentinel = UninitializedSentinel(isolate);
  DCHECK_EQ(isolate->heap()->uninitialized_symbol(), *uninitialized_sentinel);
  for (int i = kReservedIndexCount + index_count; i < length; i++) {
    array->set(i, *uninitialized_sentinel, SKIP_WRITE_BARRIER);
  }

  Handle<TypeFeedbackVector> vector = Handle<TypeFeedbackVector>::cast(array);
  if (FLAG_vector_ics) {
    for (int i = 0; i < ic_slot_count; i++) {
      vector->SetKind(FeedbackVectorICSlot(i), spec->GetKind(i));
    }
  }
  return vector;
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
static bool ClearLogic(Heap* heap, int ic_age) {
  return FLAG_cleanup_code_caches_at_gc &&
         heap->isolate()->serializer_enabled();
}


void TypeFeedbackVector::ClearSlots(SharedFunctionInfo* shared) {
  int slots = Slots();
  Isolate* isolate = GetIsolate();
  Object* uninitialized_sentinel =
      TypeFeedbackVector::RawUninitializedSentinel(isolate->heap());

  for (int i = 0; i < slots; i++) {
    FeedbackVectorSlot slot(i);
    Object* obj = Get(slot);
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
  }
}


void TypeFeedbackVector::ClearICSlotsImpl(SharedFunctionInfo* shared,
                                          bool force_clear) {
  Heap* heap = GetIsolate()->heap();

  // I'm not sure yet if this ic age is the correct one.
  int ic_age = shared->ic_age();

  if (!force_clear && !ClearLogic(heap, ic_age)) return;

  int slots = ICSlots();
  Code* host = shared->code();
  Object* uninitialized_sentinel =
      TypeFeedbackVector::RawUninitializedSentinel(heap);
  for (int i = 0; i < slots; i++) {
    FeedbackVectorICSlot slot(i);
    Object* obj = Get(slot);
    if (obj != uninitialized_sentinel) {
      Code::Kind kind = GetKind(slot);
      if (kind == Code::CALL_IC) {
        CallICNexus nexus(this, slot);
        nexus.Clear(host);
      } else if (kind == Code::LOAD_IC) {
        LoadICNexus nexus(this, slot);
        nexus.Clear(host);
      } else if (kind == Code::KEYED_LOAD_IC) {
        KeyedLoadICNexus nexus(this, slot);
        nexus.Clear(host);
      }
    }
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


InlineCacheState LoadICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *vector()->UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *vector()->MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  } else if (feedback == *vector()->PremonomorphicSentinel(isolate)) {
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

  if (feedback == *vector()->UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *vector()->PremonomorphicSentinel(isolate)) {
    return PREMONOMORPHIC;
  } else if (feedback == *vector()->MegamorphicSentinel(isolate)) {
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
  DCHECK(!FLAG_vector_ics ||
         GetFeedbackExtra() == *vector()->UninitializedSentinel(isolate));

  if (feedback == *vector()->MegamorphicSentinel(isolate)) {
    return GENERIC;
  } else if (feedback->IsAllocationSite() || feedback->IsWeakCell()) {
    return MONOMORPHIC;
  }

  CHECK(feedback == *vector()->UninitializedSentinel(isolate));
  return UNINITIALIZED;
}


void CallICNexus::Clear(Code* host) { CallIC::Clear(GetIsolate(), host, this); }


void CallICNexus::ConfigureGeneric() {
  SetFeedback(*vector()->MegamorphicSentinel(GetIsolate()), SKIP_WRITE_BARRIER);
}


void CallICNexus::ConfigureMonomorphicArray() {
  Object* feedback = GetFeedback();
  if (!feedback->IsAllocationSite()) {
    Handle<AllocationSite> new_site =
        GetIsolate()->factory()->NewAllocationSite();
    SetFeedback(*new_site);
  }
}


void CallICNexus::ConfigureUninitialized() {
  SetFeedback(*vector()->UninitializedSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
}


void CallICNexus::ConfigureMonomorphic(Handle<JSFunction> function) {
  Handle<WeakCell> new_cell = GetIsolate()->factory()->NewWeakCell(function);
  SetFeedback(*new_cell);
}


void KeyedLoadICNexus::ConfigureMegamorphic() {
  Isolate* isolate = GetIsolate();
  SetFeedback(*vector()->MegamorphicSentinel(isolate), SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*vector()->UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}


void LoadICNexus::ConfigureMegamorphic() {
  SetFeedback(*vector()->MegamorphicSentinel(GetIsolate()), SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*vector()->UninitializedSentinel(GetIsolate()),
                   SKIP_WRITE_BARRIER);
}


void LoadICNexus::ConfigurePremonomorphic() {
  SetFeedback(*vector()->PremonomorphicSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*vector()->UninitializedSentinel(GetIsolate()),
                   SKIP_WRITE_BARRIER);
}


void KeyedLoadICNexus::ConfigurePremonomorphic() {
  Isolate* isolate = GetIsolate();
  SetFeedback(*vector()->PremonomorphicSentinel(isolate), SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*vector()->UninitializedSentinel(isolate),
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
    SetFeedback(*name);
    Handle<FixedArray> array = EnsureExtraArrayOfSize(2);
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
  SetFeedbackExtra(*vector()->UninitializedSentinel(isolate),
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
    SetFeedbackExtra(*vector()->UninitializedSentinel(GetIsolate()),
                     SKIP_WRITE_BARRIER);
  } else {
    SetFeedback(*name);
    array = EnsureExtraArrayOfSize(receiver_count * 2);
  }

  InstallHandlers(array, maps, handlers);
}


int FeedbackNexus::ExtractMaps(MapHandleList* maps) const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();
  if (feedback->IsFixedArray() || feedback->IsString()) {
    int found = 0;
    if (feedback->IsString()) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    // The array should be of the form [<optional name>], then
    // [map, handler, map, handler, ... ]
    DCHECK(array->length() >= 2);
    for (int i = 0; i < array->length(); i += 2) {
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
  if (feedback->IsFixedArray() || feedback->IsString()) {
    if (feedback->IsString()) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    for (int i = 0; i < array->length(); i += 2) {
      DCHECK(array->get(i)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(array->get(i));
      if (!cell->cleared()) {
        Map* array_map = Map::cast(cell->value());
        if (array_map == *map) {
          Code* code = Code::cast(array->get(i + 1));
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
  if (feedback->IsFixedArray() || feedback->IsString()) {
    if (feedback->IsString()) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    // The array should be of the form [map, handler, map, handler, ... ].
    // Be sure to skip handlers whose maps have been cleared.
    DCHECK(array->length() >= 2);
    for (int i = 0; i < array->length(); i += 2) {
      DCHECK(array->get(i)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(array->get(i));
      if (!cell->cleared()) {
        Code* code = Code::cast(array->get(i + 1));
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
  if (feedback->IsString()) {
    return Name::cast(feedback);
  }
  return NULL;
}
}
}  // namespace v8::internal
