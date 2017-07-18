// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/transitions.h"

#include "src/objects-inl.h"
#include "src/transitions-inl.h"
#include "src/utils.h"

namespace v8 {
namespace internal {


// static
void TransitionArray::Insert(Handle<Map> map, Handle<Name> name,
                             Handle<Map> target, SimpleTransitionFlag flag) {
  Isolate* isolate = map->GetIsolate();
  target->SetBackPointer(*map);

  // If the map doesn't have any transitions at all yet, install the new one.
  if (CanStoreSimpleTransition(map->raw_transitions())) {
    if (flag == SIMPLE_PROPERTY_TRANSITION) {
      Handle<WeakCell> cell = Map::WeakCellForMap(target);
      ReplaceTransitions(map, *cell);
      return;
    }
    // If the flag requires a full TransitionArray, allocate one.
    Handle<TransitionArray> result = Allocate(isolate, 0, 1);
    ReplaceTransitions(map, *result);
  }

  bool is_special_transition = flag == SPECIAL_TRANSITION;
  // If the map has a simple transition, check if it should be overwritten.
  if (IsSimpleTransition(map->raw_transitions())) {
    Map* old_target = GetSimpleTransition(map->raw_transitions());
    Name* key = GetSimpleTransitionKey(old_target);
    PropertyDetails old_details = GetSimpleTargetDetails(old_target);
    PropertyDetails new_details = is_special_transition
                                      ? PropertyDetails::Empty()
                                      : GetTargetDetails(*name, *target);
    if (flag == SIMPLE_PROPERTY_TRANSITION && key->Equals(*name) &&
        old_details.kind() == new_details.kind() &&
        old_details.attributes() == new_details.attributes()) {
      Handle<WeakCell> cell = Map::WeakCellForMap(target);
      ReplaceTransitions(map, *cell);
      return;
    }
    // Otherwise allocate a full TransitionArray with slack for a new entry.
    Handle<TransitionArray> result = Allocate(isolate, 1, 1);
    // Re-read existing data; the allocation might have caused it to be cleared.
    if (IsSimpleTransition(map->raw_transitions())) {
      old_target = GetSimpleTransition(map->raw_transitions());
      result->Set(0, GetSimpleTransitionKey(old_target), old_target);
    } else {
      result->SetNumberOfTransitions(0);
    }
    ReplaceTransitions(map, *result);
  }

  // At this point, we know that the map has a full TransitionArray.
  DCHECK(IsFullTransitionArray(map->raw_transitions()));

  int number_of_transitions = 0;
  int new_nof = 0;
  int insertion_index = kNotFound;
  DCHECK_EQ(is_special_transition, IsSpecialTransition(*name));
  PropertyDetails details = is_special_transition
                                ? PropertyDetails::Empty()
                                : GetTargetDetails(*name, *target);

  {
    DisallowHeapAllocation no_gc;
    TransitionArray* array = TransitionArray::cast(map->raw_transitions());
    number_of_transitions = array->number_of_transitions();
    new_nof = number_of_transitions;

    int index =
        is_special_transition
            ? array->SearchSpecial(Symbol::cast(*name), &insertion_index)
            : array->Search(details.kind(), *name, details.attributes(),
                            &insertion_index);
    // If an existing entry was found, overwrite it and return.
    if (index != kNotFound) {
      array->SetTarget(index, *target);
      return;
    }

    ++new_nof;
    CHECK(new_nof <= kMaxNumberOfTransitions);
    DCHECK(insertion_index >= 0 && insertion_index <= number_of_transitions);

    // If there is enough capacity, insert new entry into the existing array.
    if (new_nof <= Capacity(array)) {
      array->SetNumberOfTransitions(new_nof);
      for (index = number_of_transitions; index > insertion_index; --index) {
        array->SetKey(index, array->GetKey(index - 1));
        array->SetTarget(index, array->GetTarget(index - 1));
      }
      array->SetKey(index, *name);
      array->SetTarget(index, *target);
      SLOW_DCHECK(array->IsSortedNoDuplicates());
      return;
    }
  }

  // We're gonna need a bigger TransitionArray.
  Handle<TransitionArray> result = Allocate(
      map->GetIsolate(), new_nof,
      Map::SlackForArraySize(number_of_transitions, kMaxNumberOfTransitions));

  // The map's transition array may have shrunk during the allocation above as
  // it was weakly traversed, though it is guaranteed not to disappear. Trim the
  // result copy if needed, and recompute variables.
  DCHECK(IsFullTransitionArray(map->raw_transitions()));
  DisallowHeapAllocation no_gc;
  TransitionArray* array = TransitionArray::cast(map->raw_transitions());
  if (array->number_of_transitions() != number_of_transitions) {
    DCHECK(array->number_of_transitions() < number_of_transitions);

    number_of_transitions = array->number_of_transitions();
    new_nof = number_of_transitions;

    insertion_index = kNotFound;
    int index =
        is_special_transition
            ? array->SearchSpecial(Symbol::cast(*name), &insertion_index)
            : array->Search(details.kind(), *name, details.attributes(),
                            &insertion_index);
    if (index == kNotFound) {
      ++new_nof;
    } else {
      insertion_index = index;
    }
    DCHECK(insertion_index >= 0 && insertion_index <= number_of_transitions);

    result->Shrink(ToKeyIndex(new_nof));
    result->SetNumberOfTransitions(new_nof);
  }

  if (array->HasPrototypeTransitions()) {
    result->SetPrototypeTransitions(array->GetPrototypeTransitions());
  }

  DCHECK_NE(kNotFound, insertion_index);
  for (int i = 0; i < insertion_index; ++i) {
    result->Set(i, array->GetKey(i), array->GetTarget(i));
  }
  result->Set(insertion_index, *name, *target);
  for (int i = insertion_index; i < number_of_transitions; ++i) {
    result->Set(i + 1, array->GetKey(i), array->GetTarget(i));
  }

  SLOW_DCHECK(result->IsSortedNoDuplicates());
  ReplaceTransitions(map, *result);
}


// static
Map* TransitionArray::SearchTransition(Map* map, PropertyKind kind, Name* name,
                                       PropertyAttributes attributes) {
  DCHECK(name->IsUniqueName());
  Object* raw_transitions = map->raw_transitions();
  if (IsSimpleTransition(raw_transitions)) {
    Map* target = GetSimpleTransition(raw_transitions);
    Name* key = GetSimpleTransitionKey(target);
    if (key != name) return nullptr;
    PropertyDetails details = GetSimpleTargetDetails(target);
    if (details.attributes() != attributes) return nullptr;
    if (details.kind() != kind) return nullptr;
    return target;
  }
  if (IsFullTransitionArray(raw_transitions)) {
    TransitionArray* transitions = TransitionArray::cast(raw_transitions);
    int transition = transitions->Search(kind, name, attributes);
    if (transition == kNotFound) return nullptr;
    return transitions->GetTarget(transition);
  }
  return NULL;
}


// static
Map* TransitionArray::SearchSpecial(Map* map, Symbol* name) {
  Object* raw_transitions = map->raw_transitions();
  if (IsFullTransitionArray(raw_transitions)) {
    TransitionArray* transitions = TransitionArray::cast(raw_transitions);
    int transition = transitions->SearchSpecial(name);
    if (transition == kNotFound) return NULL;
    return transitions->GetTarget(transition);
  }
  return NULL;
}


// static
Handle<Map> TransitionArray::FindTransitionToField(Handle<Map> map,
                                                   Handle<Name> name) {
  DCHECK(name->IsUniqueName());
  DisallowHeapAllocation no_gc;
  Map* target = SearchTransition(*map, kData, *name, NONE);
  if (target == NULL) return Handle<Map>::null();
  PropertyDetails details = target->GetLastDescriptorDetails();
  DCHECK_EQ(NONE, details.attributes());
  if (details.location() != kField) return Handle<Map>::null();
  DCHECK_EQ(kData, details.kind());
  return Handle<Map>(target);
}


// static
Handle<String> TransitionArray::ExpectedTransitionKey(Handle<Map> map) {
  DisallowHeapAllocation no_gc;
  Object* raw_transition = map->raw_transitions();
  if (!IsSimpleTransition(raw_transition)) return Handle<String>::null();
  Map* target = GetSimpleTransition(raw_transition);
  PropertyDetails details = GetSimpleTargetDetails(target);
  if (details.location() != kField) return Handle<String>::null();
  DCHECK_EQ(kData, details.kind());
  if (details.attributes() != NONE) return Handle<String>::null();
  Name* name = GetSimpleTransitionKey(target);
  if (!name->IsString()) return Handle<String>::null();
  return Handle<String>(String::cast(name));
}


// static
bool TransitionArray::CanHaveMoreTransitions(Handle<Map> map) {
  if (map->is_dictionary_map()) return false;
  Object* raw_transitions = map->raw_transitions();
  if (IsFullTransitionArray(raw_transitions)) {
    TransitionArray* transitions = TransitionArray::cast(raw_transitions);
    return transitions->number_of_transitions() < kMaxNumberOfTransitions;
  }
  return true;
}


// static
bool TransitionArray::CompactPrototypeTransitionArray(FixedArray* array) {
  const int header = kProtoTransitionHeaderSize;
  int number_of_transitions = NumberOfPrototypeTransitions(array);
  if (number_of_transitions == 0) {
    // Empty array cannot be compacted.
    return false;
  }
  int new_number_of_transitions = 0;
  for (int i = 0; i < number_of_transitions; i++) {
    Object* cell = array->get(header + i);
    if (!WeakCell::cast(cell)->cleared()) {
      if (new_number_of_transitions != i) {
        array->set(header + new_number_of_transitions, cell);
      }
      new_number_of_transitions++;
    }
  }
  // Fill slots that became free with undefined value.
  for (int i = new_number_of_transitions; i < number_of_transitions; i++) {
    array->set_undefined(header + i);
  }
  if (number_of_transitions != new_number_of_transitions) {
    SetNumberOfPrototypeTransitions(array, new_number_of_transitions);
  }
  return new_number_of_transitions < number_of_transitions;
}


// static
Handle<FixedArray> TransitionArray::GrowPrototypeTransitionArray(
    Handle<FixedArray> array, int new_capacity, Isolate* isolate) {
  // Grow array by factor 2 up to MaxCachedPrototypeTransitions.
  int capacity = array->length() - kProtoTransitionHeaderSize;
  new_capacity = Min(kMaxCachedPrototypeTransitions, new_capacity);
  DCHECK_GT(new_capacity, capacity);
  int grow_by = new_capacity - capacity;
  array = isolate->factory()->CopyFixedArrayAndGrow(array, grow_by, TENURED);
  if (capacity < 0) {
    // There was no prototype transitions array before, so the size
    // couldn't be copied. Initialize it explicitly.
    SetNumberOfPrototypeTransitions(*array, 0);
  }
  return array;
}


// static
int TransitionArray::NumberOfPrototypeTransitionsForTest(Map* map) {
  FixedArray* transitions = GetPrototypeTransitions(map);
  CompactPrototypeTransitionArray(transitions);
  return TransitionArray::NumberOfPrototypeTransitions(transitions);
}


// static
void TransitionArray::PutPrototypeTransition(Handle<Map> map,
                                             Handle<Object> prototype,
                                             Handle<Map> target_map) {
  DCHECK(HeapObject::cast(*prototype)->map()->IsMap());
  // Don't cache prototype transition if this map is either shared, or a map of
  // a prototype.
  if (map->is_prototype_map()) return;
  if (map->is_dictionary_map() || !FLAG_cache_prototype_transitions) return;

  const int header = kProtoTransitionHeaderSize;

  Handle<WeakCell> target_cell = Map::WeakCellForMap(target_map);

  Handle<FixedArray> cache(GetPrototypeTransitions(*map));
  int capacity = cache->length() - header;
  int transitions = NumberOfPrototypeTransitions(*cache) + 1;

  if (transitions > capacity) {
    // Grow the array if compacting it doesn't free space.
    if (!CompactPrototypeTransitionArray(*cache)) {
      if (capacity == kMaxCachedPrototypeTransitions) return;
      cache = GrowPrototypeTransitionArray(cache, 2 * transitions,
                                           map->GetIsolate());
      SetPrototypeTransitions(map, cache);
    }
  }

  // Reload number of transitions as they might have been compacted.
  int last = NumberOfPrototypeTransitions(*cache);
  int entry = header + last;

  cache->set(entry, *target_cell);
  SetNumberOfPrototypeTransitions(*cache, last + 1);
}


// static
Handle<Map> TransitionArray::GetPrototypeTransition(Handle<Map> map,
                                                    Handle<Object> prototype) {
  DisallowHeapAllocation no_gc;
  FixedArray* cache = GetPrototypeTransitions(*map);
  int number_of_transitions = NumberOfPrototypeTransitions(cache);
  for (int i = 0; i < number_of_transitions; i++) {
    WeakCell* target_cell =
        WeakCell::cast(cache->get(kProtoTransitionHeaderSize + i));
    if (!target_cell->cleared() &&
        Map::cast(target_cell->value())->prototype() == *prototype) {
      return handle(Map::cast(target_cell->value()));
    }
  }
  return Handle<Map>();
}


// static
FixedArray* TransitionArray::GetPrototypeTransitions(Map* map) {
  Object* raw_transitions = map->raw_transitions();
  Heap* heap = map->GetHeap();
  if (!IsFullTransitionArray(raw_transitions)) {
    return heap->empty_fixed_array();
  }
  TransitionArray* transitions = TransitionArray::cast(raw_transitions);
  if (!transitions->HasPrototypeTransitions()) {
    return heap->empty_fixed_array();
  }
  return transitions->GetPrototypeTransitions();
}


// static
void TransitionArray::SetNumberOfPrototypeTransitions(
    FixedArray* proto_transitions, int value) {
  DCHECK(proto_transitions->length() != 0);
  proto_transitions->set(kProtoTransitionNumberOfEntriesOffset,
                         Smi::FromInt(value));
}


// static
int TransitionArray::NumberOfTransitions(Object* raw_transitions) {
  if (CanStoreSimpleTransition(raw_transitions)) return 0;
  if (IsSimpleTransition(raw_transitions)) return 1;
  // Prototype maps don't have transitions.
  if (raw_transitions->IsPrototypeInfo()) return 0;
  DCHECK(IsFullTransitionArray(raw_transitions));
  return TransitionArray::cast(raw_transitions)->number_of_transitions();
}


// static
int TransitionArray::Capacity(Object* raw_transitions) {
  if (!IsFullTransitionArray(raw_transitions)) return 1;
  TransitionArray* t = TransitionArray::cast(raw_transitions);
  if (t->length() <= kFirstIndex) return 0;
  return (t->length() - kFirstIndex) / kTransitionSize;
}


// Private static helper functions.

Handle<TransitionArray> TransitionArray::Allocate(Isolate* isolate,
                                                  int number_of_transitions,
                                                  int slack) {
  Handle<FixedArray> array = isolate->factory()->NewTransitionArray(
      LengthFor(number_of_transitions + slack));
  array->set(kPrototypeTransitionsIndex, Smi::kZero);
  array->set(kTransitionLengthIndex, Smi::FromInt(number_of_transitions));
  return Handle<TransitionArray>::cast(array);
}


// static
void TransitionArray::ZapTransitionArray(TransitionArray* transitions) {
  // Do not zap the next link that is used by GC.
  STATIC_ASSERT(kNextLinkIndex + 1 == kPrototypeTransitionsIndex);
  MemsetPointer(transitions->data_start() + kPrototypeTransitionsIndex,
                transitions->GetHeap()->the_hole_value(),
                transitions->length() - kPrototypeTransitionsIndex);
  transitions->SetNumberOfTransitions(0);
}


void TransitionArray::ReplaceTransitions(Handle<Map> map,
                                         Object* new_transitions) {
  Object* raw_transitions = map->raw_transitions();
  if (IsFullTransitionArray(raw_transitions)) {
    TransitionArray* old_transitions = TransitionArray::cast(raw_transitions);
#ifdef DEBUG
    CheckNewTransitionsAreConsistent(map, old_transitions, new_transitions);
    DCHECK(old_transitions != new_transitions);
#endif
    // Transition arrays are not shared. When one is replaced, it should not
    // keep referenced objects alive, so we zap it.
    // When there is another reference to the array somewhere (e.g. a handle),
    // not zapping turns from a waste of memory into a source of crashes.
    ZapTransitionArray(old_transitions);
  }
  map->set_raw_transitions(new_transitions);
}


void TransitionArray::SetPrototypeTransitions(
    Handle<Map> map, Handle<FixedArray> proto_transitions) {
  EnsureHasFullTransitionArray(map);
  TransitionArray* transitions = TransitionArray::cast(map->raw_transitions());
  transitions->SetPrototypeTransitions(*proto_transitions);
}


void TransitionArray::EnsureHasFullTransitionArray(Handle<Map> map) {
  Object* raw_transitions = map->raw_transitions();
  if (IsFullTransitionArray(raw_transitions)) return;
  int nof = IsSimpleTransition(raw_transitions) ? 1 : 0;
  Handle<TransitionArray> result = Allocate(map->GetIsolate(), nof);
  DisallowHeapAllocation no_gc;
  // Reload pointer after the allocation that just happened.
  raw_transitions = map->raw_transitions();
  int new_nof = IsSimpleTransition(raw_transitions) ? 1 : 0;
  if (new_nof != nof) {
    DCHECK(new_nof == 0);
    result->Shrink(ToKeyIndex(0));
    result->SetNumberOfTransitions(0);
  } else if (nof == 1) {
    Map* target = GetSimpleTransition(raw_transitions);
    Name* key = GetSimpleTransitionKey(target);
    result->Set(0, key, target);
  }
  ReplaceTransitions(map, *result);
}


void TransitionArray::TraverseTransitionTreeInternal(Map* map,
                                                     TraverseCallback callback,
                                                     void* data) {
  Object* raw_transitions = map->raw_transitions();
  if (IsFullTransitionArray(raw_transitions)) {
    TransitionArray* transitions = TransitionArray::cast(raw_transitions);
    if (transitions->HasPrototypeTransitions()) {
      FixedArray* proto_trans = transitions->GetPrototypeTransitions();
      for (int i = 0; i < NumberOfPrototypeTransitions(proto_trans); ++i) {
        int index = TransitionArray::kProtoTransitionHeaderSize + i;
        WeakCell* cell = WeakCell::cast(proto_trans->get(index));
        if (!cell->cleared()) {
          TraverseTransitionTreeInternal(Map::cast(cell->value()), callback,
                                         data);
        }
      }
    }
    for (int i = 0; i < transitions->number_of_transitions(); ++i) {
      TraverseTransitionTreeInternal(transitions->GetTarget(i), callback, data);
    }
  } else if (IsSimpleTransition(raw_transitions)) {
    TraverseTransitionTreeInternal(GetSimpleTransition(raw_transitions),
                                   callback, data);
  }
  callback(map, data);
}


#ifdef DEBUG
void TransitionArray::CheckNewTransitionsAreConsistent(
    Handle<Map> map, TransitionArray* old_transitions, Object* transitions) {
  // This function only handles full transition arrays.
  DCHECK(IsFullTransitionArray(transitions));
  TransitionArray* new_transitions = TransitionArray::cast(transitions);
  for (int i = 0; i < old_transitions->number_of_transitions(); i++) {
    Map* target = old_transitions->GetTarget(i);
    if (target->instance_descriptors() == map->instance_descriptors()) {
      Name* key = old_transitions->GetKey(i);
      int new_target_index;
      if (TransitionArray::IsSpecialTransition(key)) {
        new_target_index = new_transitions->SearchSpecial(Symbol::cast(key));
      } else {
        PropertyDetails details =
            TransitionArray::GetTargetDetails(key, target);
        new_target_index =
            new_transitions->Search(details.kind(), key, details.attributes());
      }
      DCHECK_NE(TransitionArray::kNotFound, new_target_index);
      DCHECK_EQ(target, new_transitions->GetTarget(new_target_index));
    }
  }
}
#endif


// Private non-static helper functions (operating on full transition arrays).

int TransitionArray::SearchDetails(int transition, PropertyKind kind,
                                   PropertyAttributes attributes,
                                   int* out_insertion_index) {
  int nof_transitions = number_of_transitions();
  DCHECK(transition < nof_transitions);
  Name* key = GetKey(transition);
  for (; transition < nof_transitions && GetKey(transition) == key;
       transition++) {
    Map* target = GetTarget(transition);
    PropertyDetails target_details = GetTargetDetails(key, target);

    int cmp = CompareDetails(kind, attributes, target_details.kind(),
                             target_details.attributes());
    if (cmp == 0) {
      return transition;
    } else if (cmp < 0) {
      break;
    }
  }
  if (out_insertion_index != NULL) *out_insertion_index = transition;
  return kNotFound;
}


int TransitionArray::Search(PropertyKind kind, Name* name,
                            PropertyAttributes attributes,
                            int* out_insertion_index) {
  int transition = SearchName(name, out_insertion_index);
  if (transition == kNotFound) return kNotFound;
  return SearchDetails(transition, kind, attributes, out_insertion_index);
}

void TransitionArray::Sort() {
  DisallowHeapAllocation no_gc;
  // In-place insertion sort.
  int length = number_of_transitions();
  for (int i = 1; i < length; i++) {
    Name* key = GetKey(i);
    Map* target = GetTarget(i);
    PropertyKind kind = kData;
    PropertyAttributes attributes = NONE;
    if (!IsSpecialTransition(key)) {
      PropertyDetails details = GetTargetDetails(key, target);
      kind = details.kind();
      attributes = details.attributes();
    }
    int j;
    for (j = i - 1; j >= 0; j--) {
      Name* temp_key = GetKey(j);
      Map* temp_target = GetTarget(j);
      PropertyKind temp_kind = kData;
      PropertyAttributes temp_attributes = NONE;
      if (!IsSpecialTransition(temp_key)) {
        PropertyDetails details = GetTargetDetails(temp_key, temp_target);
        temp_kind = details.kind();
        temp_attributes = details.attributes();
      }
      int cmp =
          CompareKeys(temp_key, temp_key->Hash(), temp_kind, temp_attributes,
                      key, key->Hash(), kind, attributes);
      if (cmp > 0) {
        SetKey(j + 1, temp_key);
        SetTarget(j + 1, temp_target);
      } else {
        break;
      }
    }
    SetKey(j + 1, key);
    SetTarget(j + 1, target);
  }
  DCHECK(IsSortedNoDuplicates());
}

}  // namespace internal
}  // namespace v8
