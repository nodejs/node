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


// static
Handle<TypeFeedbackVector> TypeFeedbackVector::Allocate(
    Isolate* isolate, const FeedbackVectorSpec& spec) {
  const int slot_count = spec.slots();
  const int ic_slot_count = spec.ic_slots();
  const int index_count =
      FLAG_vector_ics ? VectorICComputer::word_count(ic_slot_count) : 0;
  const int length =
      slot_count + ic_slot_count + index_count + kReservedIndexCount;
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
      vector->SetKind(FeedbackVectorICSlot(i), spec.GetKind(i));
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


void FeedbackNexus::InstallHandlers(int start_index, MapHandleList* maps,
                                    CodeHandleList* handlers) {
  Isolate* isolate = GetIsolate();
  Handle<FixedArray> array = handle(FixedArray::cast(GetFeedback()), isolate);
  int receiver_count = maps->length();
  for (int current = 0; current < receiver_count; ++current) {
    Handle<Map> map = maps->at(current);
    Handle<WeakCell> cell = Map::WeakCellForMap(map);
    array->set(start_index + (current * 2), *cell);
    array->set(start_index + (current * 2 + 1), *handlers->at(current));
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
    FixedArray* array = FixedArray::cast(feedback);
    int length = array->length();
    DCHECK(length >= 2);
    return length == 2 ? MONOMORPHIC : POLYMORPHIC;
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
    FixedArray* array = FixedArray::cast(feedback);
    int length = array->length();
    DCHECK(length >= 3);
    return length == 3 ? MONOMORPHIC : POLYMORPHIC;
  }

  return UNINITIALIZED;
}


InlineCacheState CallICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

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
  SetFeedback(*vector()->MegamorphicSentinel(GetIsolate()), SKIP_WRITE_BARRIER);
}


void LoadICNexus::ConfigureMegamorphic() {
  SetFeedback(*vector()->MegamorphicSentinel(GetIsolate()), SKIP_WRITE_BARRIER);
}


void LoadICNexus::ConfigurePremonomorphic() {
  SetFeedback(*vector()->PremonomorphicSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
}


void KeyedLoadICNexus::ConfigurePremonomorphic() {
  SetFeedback(*vector()->PremonomorphicSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
}


void LoadICNexus::ConfigureMonomorphic(Handle<Map> receiver_map,
                                       Handle<Code> handler) {
  Handle<FixedArray> array = EnsureArrayOfSize(2);
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
  array->set(0, *cell);
  array->set(1, *handler);
}


void KeyedLoadICNexus::ConfigureMonomorphic(Handle<Name> name,
                                            Handle<Map> receiver_map,
                                            Handle<Code> handler) {
  Handle<FixedArray> array = EnsureArrayOfSize(3);
  if (name.is_null()) {
    array->set(0, Smi::FromInt(0));
  } else {
    array->set(0, *name);
  }
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
  array->set(1, *cell);
  array->set(2, *handler);
}


void LoadICNexus::ConfigurePolymorphic(MapHandleList* maps,
                                       CodeHandleList* handlers) {
  int receiver_count = maps->length();
  EnsureArrayOfSize(receiver_count * 2);
  InstallHandlers(0, maps, handlers);
}


void KeyedLoadICNexus::ConfigurePolymorphic(Handle<Name> name,
                                            MapHandleList* maps,
                                            CodeHandleList* handlers) {
  int receiver_count = maps->length();
  Handle<FixedArray> array = EnsureArrayOfSize(1 + receiver_count * 2);
  if (name.is_null()) {
    array->set(0, Smi::FromInt(0));
  } else {
    array->set(0, *name);
  }
  InstallHandlers(1, maps, handlers);
}


int FeedbackNexus::ExtractMaps(int start_index, MapHandleList* maps) const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();
  if (feedback->IsFixedArray()) {
    int found = 0;
    FixedArray* array = FixedArray::cast(feedback);
    // The array should be of the form [<optional name>], then
    // [map, handler, map, handler, ... ]
    DCHECK(array->length() >= (2 + start_index));
    for (int i = start_index; i < array->length(); i += 2) {
      WeakCell* cell = WeakCell::cast(array->get(i));
      if (!cell->cleared()) {
        Map* map = Map::cast(cell->value());
        maps->Add(handle(map, isolate));
        found++;
      }
    }
    return found;
  }

  return 0;
}


MaybeHandle<Code> FeedbackNexus::FindHandlerForMap(int start_index,
                                                   Handle<Map> map) const {
  Object* feedback = GetFeedback();
  if (feedback->IsFixedArray()) {
    FixedArray* array = FixedArray::cast(feedback);
    for (int i = start_index; i < array->length(); i += 2) {
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
  }

  return MaybeHandle<Code>();
}


bool FeedbackNexus::FindHandlers(int start_index, CodeHandleList* code_list,
                                 int length) const {
  Object* feedback = GetFeedback();
  int count = 0;
  if (feedback->IsFixedArray()) {
    FixedArray* array = FixedArray::cast(feedback);
    // The array should be of the form [<optional name>], then
    // [map, handler, map, handler, ... ]. Be sure to skip handlers whose maps
    // have been cleared.
    DCHECK(array->length() >= (2 + start_index));
    for (int i = start_index; i < array->length(); i += 2) {
      WeakCell* cell = WeakCell::cast(array->get(i));
      if (!cell->cleared()) {
        Code* code = Code::cast(array->get(i + 1));
        DCHECK(code->kind() == Code::HANDLER);
        code_list->Add(handle(code));
        count++;
      }
    }
  }
  return count == length;
}


int LoadICNexus::ExtractMaps(MapHandleList* maps) const {
  return FeedbackNexus::ExtractMaps(0, maps);
}


void LoadICNexus::Clear(Code* host) { LoadIC::Clear(GetIsolate(), host, this); }


void KeyedLoadICNexus::Clear(Code* host) {
  KeyedLoadIC::Clear(GetIsolate(), host, this);
}


int KeyedLoadICNexus::ExtractMaps(MapHandleList* maps) const {
  return FeedbackNexus::ExtractMaps(1, maps);
}


MaybeHandle<Code> LoadICNexus::FindHandlerForMap(Handle<Map> map) const {
  return FeedbackNexus::FindHandlerForMap(0, map);
}


MaybeHandle<Code> KeyedLoadICNexus::FindHandlerForMap(Handle<Map> map) const {
  return FeedbackNexus::FindHandlerForMap(1, map);
}


bool LoadICNexus::FindHandlers(CodeHandleList* code_list, int length) const {
  return FeedbackNexus::FindHandlers(0, code_list, length);
}


bool KeyedLoadICNexus::FindHandlers(CodeHandleList* code_list,
                                    int length) const {
  return FeedbackNexus::FindHandlers(1, code_list, length);
}


Name* KeyedLoadICNexus::FindFirstName() const {
  Object* feedback = GetFeedback();
  if (feedback->IsFixedArray()) {
    FixedArray* array = FixedArray::cast(feedback);
    DCHECK(array->length() >= 3);
    Object* name = array->get(0);
    if (name->IsName()) return Name::cast(name);
  }
  return NULL;
}
}
}  // namespace v8::internal
