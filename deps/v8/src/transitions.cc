// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/transitions.h"

#include "src/objects-inl.h"
#include "src/transitions-inl.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

void TransitionsAccessor::Initialize() {
  raw_transitions_ = map_->raw_transitions();
  if (raw_transitions_->IsSmi()) {
    encoding_ = kUninitialized;
  } else if (HeapObject::cast(raw_transitions_)->IsWeakCell()) {
    encoding_ = kWeakCell;
  } else if (HeapObject::cast(raw_transitions_)->IsStoreHandler()) {
    encoding_ = kHandler;
  } else if (HeapObject::cast(raw_transitions_)->IsTransitionArray()) {
    encoding_ = kFullTransitionArray;
  } else {
    DCHECK(HeapObject::cast(raw_transitions_)->IsPrototypeInfo());
    encoding_ = kPrototypeInfo;
  }
  target_cell_ = nullptr;
#if DEBUG
  needs_reload_ = false;
#endif
}

Map* TransitionsAccessor::GetSimpleTransition() {
  switch (encoding()) {
    case kWeakCell:
      return Map::cast(GetTargetCell<kWeakCell>()->value());
    case kHandler:
      return Map::cast(GetTargetCell<kHandler>()->value());
    default:
      return nullptr;
  }
}

bool TransitionsAccessor::HasSimpleTransitionTo(WeakCell* cell) {
  DCHECK(cell->value()->IsMap());
  switch (encoding()) {
    case kWeakCell:
      return raw_transitions_ == cell;
    case kHandler:
      return StoreHandler::GetTransitionCell(raw_transitions_) == cell;
    case kPrototypeInfo:
    case kUninitialized:
    case kFullTransitionArray:
      return false;
  }
  UNREACHABLE();
  return false;  // Make GCC happy.
}

void TransitionsAccessor::Insert(Handle<Name> name, Handle<Map> target,
                                 SimpleTransitionFlag flag) {
  DCHECK(!map_handle_.is_null());
  Isolate* isolate = map_->GetIsolate();
  Handle<WeakCell> weak_cell_with_target = Map::WeakCellForMap(target);
  Reload();
  target->SetBackPointer(map_);

  // If the map doesn't have any transitions at all yet, install the new one.
  if (encoding() == kUninitialized) {
    if (flag == SIMPLE_PROPERTY_TRANSITION) {
      ReplaceTransitions(*weak_cell_with_target);
      return;
    }
    // If the flag requires a full TransitionArray, allocate one.
    Handle<TransitionArray> result = TransitionArray::Allocate(isolate, 0, 1);
    ReplaceTransitions(*result);
    Reload();
  }

  bool is_special_transition = flag == SPECIAL_TRANSITION;
  // If the map has a simple transition, check if it should be overwritten.
  Map* simple_transition = GetSimpleTransition();
  if (simple_transition != nullptr) {
    Name* key = GetSimpleTransitionKey(simple_transition);
    PropertyDetails old_details = GetSimpleTargetDetails(simple_transition);
    PropertyDetails new_details = is_special_transition
                                      ? PropertyDetails::Empty()
                                      : GetTargetDetails(*name, *target);
    if (flag == SIMPLE_PROPERTY_TRANSITION && key->Equals(*name) &&
        old_details.kind() == new_details.kind() &&
        old_details.attributes() == new_details.attributes()) {
      ReplaceTransitions(*weak_cell_with_target);
      return;
    }
    // Otherwise allocate a full TransitionArray with slack for a new entry.
    Handle<TransitionArray> result = TransitionArray::Allocate(isolate, 1, 1);
    // Reload state; the allocation might have caused it to be cleared.
    Reload();
    simple_transition = GetSimpleTransition();
    if (simple_transition != nullptr) {
      result->Set(0, GetSimpleTransitionKey(simple_transition),
                  raw_transitions_);
    } else {
      result->SetNumberOfTransitions(0);
    }
    ReplaceTransitions(*result);
    Reload();
  }

  // At this point, we know that the map has a full TransitionArray.
  DCHECK_EQ(kFullTransitionArray, encoding());

  int number_of_transitions = 0;
  int new_nof = 0;
  int insertion_index = kNotFound;
  DCHECK_EQ(is_special_transition, IsSpecialTransition(*name));
  PropertyDetails details = is_special_transition
                                ? PropertyDetails::Empty()
                                : GetTargetDetails(*name, *target);

  {
    DisallowHeapAllocation no_gc;
    TransitionArray* array = transitions();
    number_of_transitions = array->number_of_transitions();
    new_nof = number_of_transitions;

    int index =
        is_special_transition
            ? array->SearchSpecial(Symbol::cast(*name), &insertion_index)
            : array->Search(details.kind(), *name, details.attributes(),
                            &insertion_index);
    // If an existing entry was found, overwrite it and return.
    if (index != kNotFound) {
      array->SetTarget(index, *weak_cell_with_target);
      return;
    }

    ++new_nof;
    CHECK_LE(new_nof, kMaxNumberOfTransitions);
    DCHECK(insertion_index >= 0 && insertion_index <= number_of_transitions);

    // If there is enough capacity, insert new entry into the existing array.
    if (new_nof <= array->Capacity()) {
      array->SetNumberOfTransitions(new_nof);
      for (index = number_of_transitions; index > insertion_index; --index) {
        array->SetKey(index, array->GetKey(index - 1));
        array->SetTarget(index, array->GetRawTarget(index - 1));
      }
      array->SetKey(index, *name);
      array->SetTarget(index, *weak_cell_with_target);
      SLOW_DCHECK(array->IsSortedNoDuplicates());
      return;
    }
  }

  // We're gonna need a bigger TransitionArray.
  Handle<TransitionArray> result = TransitionArray::Allocate(
      isolate, new_nof,
      Map::SlackForArraySize(number_of_transitions, kMaxNumberOfTransitions));

  // The map's transition array may have shrunk during the allocation above as
  // it was weakly traversed, though it is guaranteed not to disappear. Trim the
  // result copy if needed, and recompute variables.
  Reload();
  DisallowHeapAllocation no_gc;
  TransitionArray* array = transitions();
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

    result->Shrink(TransitionArray::ToKeyIndex(new_nof));
    result->SetNumberOfTransitions(new_nof);
  }

  if (array->HasPrototypeTransitions()) {
    result->SetPrototypeTransitions(array->GetPrototypeTransitions());
  }

  DCHECK_NE(kNotFound, insertion_index);
  for (int i = 0; i < insertion_index; ++i) {
    result->Set(i, array->GetKey(i), array->GetRawTarget(i));
  }
  result->Set(insertion_index, *name, *weak_cell_with_target);
  for (int i = insertion_index; i < number_of_transitions; ++i) {
    result->Set(i + 1, array->GetKey(i), array->GetRawTarget(i));
  }

  SLOW_DCHECK(result->IsSortedNoDuplicates());
  ReplaceTransitions(*result);
}

void TransitionsAccessor::UpdateHandler(Name* name, Object* handler) {
  if (map_->is_dictionary_map()) return;
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
      UNREACHABLE();
      return;
    case kWeakCell:
    case kHandler:
      DCHECK_EQ(GetSimpleTransition(), GetTargetFromRaw(handler));
      ReplaceTransitions(handler);
      return;
    case kFullTransitionArray: {
      PropertyAttributes attributes = name->IsPrivate() ? DONT_ENUM : NONE;
      int transition = transitions()->Search(kData, name, attributes);
      DCHECK_NE(kNotFound, transition);
      DCHECK_EQ(transitions()->GetTarget(transition),
                GetTargetFromRaw(handler));
      transitions()->SetTarget(transition, handler);
      return;
    }
  }
}

Object* TransitionsAccessor::SearchHandler(Name* name,
                                           Handle<Map>* out_transition) {
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
    case kWeakCell:
      return nullptr;
    case kHandler: {
      Object* raw_handler = StoreHandler::ValidHandlerOrNull(
          raw_transitions_, name, out_transition);
      if (raw_handler == nullptr) return raw_handler;
      // Check transition key.
      WeakCell* target_cell = StoreHandler::GetTransitionCell(raw_handler);
      if (!IsMatchingMap(target_cell, name, kData, NONE)) return nullptr;
      return raw_handler;
    }

    case kFullTransitionArray: {
      int transition = transitions()->Search(kData, name, NONE);
      if (transition == kNotFound) return nullptr;
      Object* raw_handler = transitions()->GetRawTarget(transition);
      if (raw_handler->IsStoreHandler()) {
        return StoreHandler::ValidHandlerOrNull(raw_handler, name,
                                                out_transition);
      }
      return nullptr;
    }
  }
  UNREACHABLE();
  return nullptr;  // Make GCC happy.
}

Map* TransitionsAccessor::SearchTransition(Name* name, PropertyKind kind,
                                           PropertyAttributes attributes) {
  DCHECK(name->IsUniqueName());
  WeakCell* cell = nullptr;
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
      return nullptr;
    case kWeakCell:
      cell = GetTargetCell<kWeakCell>();
      break;
    case kHandler:
      cell = GetTargetCell<kHandler>();
      break;
    case kFullTransitionArray: {
      int transition = transitions()->Search(kind, name, attributes);
      if (transition == kNotFound) return nullptr;
      return transitions()->GetTarget(transition);
    }
  }
  DCHECK(!cell->cleared());
  if (!IsMatchingMap(cell, name, kind, attributes)) return nullptr;
  return Map::cast(cell->value());
}

Map* TransitionsAccessor::SearchSpecial(Symbol* name) {
  if (encoding() != kFullTransitionArray) return nullptr;
  int transition = transitions()->SearchSpecial(name);
  if (transition == kNotFound) return nullptr;
  return transitions()->GetTarget(transition);
}

// static
bool TransitionsAccessor::IsSpecialTransition(Name* name) {
  if (!name->IsSymbol()) return false;
  Heap* heap = name->GetHeap();
  return name == heap->nonextensible_symbol() ||
         name == heap->sealed_symbol() || name == heap->frozen_symbol() ||
         name == heap->elements_transition_symbol() ||
         name == heap->strict_function_transition_symbol();
}

Handle<Map> TransitionsAccessor::FindTransitionToField(Handle<Name> name) {
  DCHECK(name->IsUniqueName());
  DisallowHeapAllocation no_gc;
  Map* target = SearchTransition(*name, kData, NONE);
  if (target == nullptr) return Handle<Map>::null();
  PropertyDetails details = target->GetLastDescriptorDetails();
  DCHECK_EQ(NONE, details.attributes());
  if (details.location() != kField) return Handle<Map>::null();
  DCHECK_EQ(kData, details.kind());
  return Handle<Map>(target);
}

Handle<String> TransitionsAccessor::ExpectedTransitionKey() {
  DisallowHeapAllocation no_gc;
  WeakCell* cell = nullptr;
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
    case kFullTransitionArray:
      return Handle<String>::null();
    case kWeakCell:
      cell = GetTargetCell<kWeakCell>();
      break;
    case kHandler:
      cell = GetTargetCell<kHandler>();
      break;
  }
  DCHECK(!cell->cleared());
  Map* target = Map::cast(cell->value());
  PropertyDetails details = GetSimpleTargetDetails(target);
  if (details.location() != kField) return Handle<String>::null();
  DCHECK_EQ(kData, details.kind());
  if (details.attributes() != NONE) return Handle<String>::null();
  Name* name = GetSimpleTransitionKey(target);
  if (!name->IsString()) return Handle<String>::null();
  return handle(String::cast(name));
}

Handle<Map> TransitionsAccessor::ExpectedTransitionTarget() {
  DCHECK(!ExpectedTransitionKey().is_null());
  return handle(GetTarget(0));
}

bool TransitionsAccessor::CanHaveMoreTransitions() {
  if (map_->is_dictionary_map()) return false;
  if (encoding() == kFullTransitionArray) {
    return transitions()->number_of_transitions() < kMaxNumberOfTransitions;
  }
  return true;
}

// static
bool TransitionsAccessor::IsMatchingMap(WeakCell* target_cell, Name* name,
                                        PropertyKind kind,
                                        PropertyAttributes attributes) {
  Map* target = Map::cast(target_cell->value());
  int descriptor = target->LastAdded();
  DescriptorArray* descriptors = target->instance_descriptors();
  Name* key = descriptors->GetKey(descriptor);
  if (key != name) return false;
  PropertyDetails details = descriptors->GetDetails(descriptor);
  return (details.kind() == kind && details.attributes() == attributes);
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

void TransitionsAccessor::PutPrototypeTransition(Handle<Object> prototype,
                                                 Handle<Map> target_map) {
  DCHECK(HeapObject::cast(*prototype)->map()->IsMap());
  // Don't cache prototype transition if this map is either shared, or a map of
  // a prototype.
  if (map_->is_prototype_map()) return;
  if (map_->is_dictionary_map() || !FLAG_cache_prototype_transitions) return;

  const int header = TransitionArray::kProtoTransitionHeaderSize;

  Handle<FixedArray> cache(GetPrototypeTransitions());
  int capacity = cache->length() - header;
  int transitions = TransitionArray::NumberOfPrototypeTransitions(*cache) + 1;

  if (transitions > capacity) {
    // Grow the array if compacting it doesn't free space.
    if (!TransitionArray::CompactPrototypeTransitionArray(*cache)) {
      if (capacity == TransitionArray::kMaxCachedPrototypeTransitions) return;
      cache = TransitionArray::GrowPrototypeTransitionArray(
          cache, 2 * transitions, target_map->GetIsolate());
      Reload();
      SetPrototypeTransitions(cache);
    }
  }

  // Reload number of transitions as they might have been compacted.
  int last = TransitionArray::NumberOfPrototypeTransitions(*cache);
  int entry = header + last;

  Handle<WeakCell> target_cell = Map::WeakCellForMap(target_map);
  Reload();  // Reload after possible GC.
  cache->set(entry, *target_cell);
  TransitionArray::SetNumberOfPrototypeTransitions(*cache, last + 1);
}

Handle<Map> TransitionsAccessor::GetPrototypeTransition(
    Handle<Object> prototype) {
  DisallowHeapAllocation no_gc;
  FixedArray* cache = GetPrototypeTransitions();
  int length = TransitionArray::NumberOfPrototypeTransitions(cache);
  for (int i = 0; i < length; i++) {
    WeakCell* target_cell = WeakCell::cast(
        cache->get(TransitionArray::kProtoTransitionHeaderSize + i));
    if (!target_cell->cleared() &&
        Map::cast(target_cell->value())->prototype() == *prototype) {
      return handle(Map::cast(target_cell->value()));
    }
  }
  return Handle<Map>();
}

FixedArray* TransitionsAccessor::GetPrototypeTransitions() {
  if (encoding() != kFullTransitionArray ||
      !transitions()->HasPrototypeTransitions()) {
    return map_->GetHeap()->empty_fixed_array();
  }
  return transitions()->GetPrototypeTransitions();
}

// static
void TransitionArray::SetNumberOfPrototypeTransitions(
    FixedArray* proto_transitions, int value) {
  DCHECK_NE(proto_transitions->length(), 0);
  proto_transitions->set(kProtoTransitionNumberOfEntriesOffset,
                         Smi::FromInt(value));
}

int TransitionsAccessor::NumberOfTransitions() {
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
      return 0;
    case kWeakCell:
    case kHandler:
      return 1;
    case kFullTransitionArray:
      return transitions()->number_of_transitions();
  }
  UNREACHABLE();
  return 0;  // Make GCC happy.
}

Handle<TransitionArray> TransitionArray::Allocate(Isolate* isolate,
                                                  int number_of_transitions,
                                                  int slack) {
  Handle<FixedArray> array = isolate->factory()->NewTransitionArray(
      LengthFor(number_of_transitions + slack));
  array->set(kPrototypeTransitionsIndex, Smi::kZero);
  array->set(kTransitionLengthIndex, Smi::FromInt(number_of_transitions));
  return Handle<TransitionArray>::cast(array);
}

void TransitionArray::Zap() {
  MemsetPointer(data_start() + kPrototypeTransitionsIndex,
                GetHeap()->the_hole_value(),
                length() - kPrototypeTransitionsIndex);
  SetNumberOfTransitions(0);
}

void TransitionsAccessor::ReplaceTransitions(Object* new_transitions) {
  if (encoding() == kFullTransitionArray) {
    TransitionArray* old_transitions = transitions();
#if DEBUG
    CheckNewTransitionsAreConsistent(old_transitions, new_transitions);
    DCHECK(old_transitions != new_transitions);
#endif
    // Transition arrays are not shared. When one is replaced, it should not
    // keep referenced objects alive, so we zap it.
    // When there is another reference to the array somewhere (e.g. a handle),
    // not zapping turns from a waste of memory into a source of crashes.
    old_transitions->Zap();
  }
  map_->set_raw_transitions(new_transitions);
  MarkNeedsReload();
}

void TransitionsAccessor::SetPrototypeTransitions(
    Handle<FixedArray> proto_transitions) {
  EnsureHasFullTransitionArray();
  transitions()->SetPrototypeTransitions(*proto_transitions);
}

void TransitionsAccessor::EnsureHasFullTransitionArray() {
  if (encoding() == kFullTransitionArray) return;
  int nof = encoding() == kUninitialized ? 0 : 1;
  Handle<TransitionArray> result =
      TransitionArray::Allocate(map_->GetIsolate(), nof);
  Reload();  // Reload after possible GC.
  if (nof == 1) {
    if (encoding() == kUninitialized) {
      // If allocation caused GC and cleared the target, trim the new array.
      result->Shrink(TransitionArray::ToKeyIndex(0));
      result->SetNumberOfTransitions(0);
    } else {
      // Otherwise populate the new array.
      Handle<Map> target(GetSimpleTransition());
      Handle<WeakCell> weak_cell_with_target = Map::WeakCellForMap(target);
      Reload();  // Reload after possible GC.
      Name* key = GetSimpleTransitionKey(*target);
      result->Set(0, key, *weak_cell_with_target);
    }
  }
  ReplaceTransitions(*result);
  Reload();  // Reload after replacing transitions.
}

void TransitionsAccessor::TraverseTransitionTreeInternal(
    TraverseCallback callback, void* data, DisallowHeapAllocation* no_gc) {
  Map* simple_target = nullptr;
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
      break;
    case kWeakCell:
      simple_target = Map::cast(GetTargetCell<kWeakCell>()->value());
      break;
    case kHandler:
      simple_target = Map::cast(GetTargetCell<kHandler>()->value());
      break;
    case kFullTransitionArray: {
      if (transitions()->HasPrototypeTransitions()) {
        FixedArray* proto_trans = transitions()->GetPrototypeTransitions();
        int length = TransitionArray::NumberOfPrototypeTransitions(proto_trans);
        for (int i = 0; i < length; ++i) {
          int index = TransitionArray::kProtoTransitionHeaderSize + i;
          WeakCell* cell = WeakCell::cast(proto_trans->get(index));
          if (cell->cleared()) continue;
          TransitionsAccessor(Map::cast(cell->value()), no_gc)
              .TraverseTransitionTreeInternal(callback, data, no_gc);
        }
      }
      for (int i = 0; i < transitions()->number_of_transitions(); ++i) {
        TransitionsAccessor(transitions()->GetTarget(i), no_gc)
            .TraverseTransitionTreeInternal(callback, data, no_gc);
      }
      break;
    }
  }
  if (simple_target != nullptr) {
    TransitionsAccessor(simple_target, no_gc)
        .TraverseTransitionTreeInternal(callback, data, no_gc);
  }
  callback(map_, data);
}

#ifdef DEBUG
void TransitionsAccessor::CheckNewTransitionsAreConsistent(
    TransitionArray* old_transitions, Object* transitions) {
  // This function only handles full transition arrays.
  DCHECK_EQ(kFullTransitionArray, encoding());
  TransitionArray* new_transitions = TransitionArray::cast(transitions);
  for (int i = 0; i < old_transitions->number_of_transitions(); i++) {
    Map* target = old_transitions->GetTarget(i);
    if (target->instance_descriptors() == map_->instance_descriptors()) {
      Name* key = old_transitions->GetKey(i);
      int new_target_index;
      if (IsSpecialTransition(key)) {
        new_target_index = new_transitions->SearchSpecial(Symbol::cast(key));
      } else {
        PropertyDetails details = GetTargetDetails(key, target);
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
    PropertyDetails target_details =
        TransitionsAccessor::GetTargetDetails(key, target);

    int cmp = CompareDetails(kind, attributes, target_details.kind(),
                             target_details.attributes());
    if (cmp == 0) {
      return transition;
    } else if (cmp < 0) {
      break;
    }
  }
  if (out_insertion_index != nullptr) *out_insertion_index = transition;
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
    Object* target = GetRawTarget(i);
    PropertyKind kind = kData;
    PropertyAttributes attributes = NONE;
    if (!TransitionsAccessor::IsSpecialTransition(key)) {
      Map* target_map = TransitionsAccessor::GetTargetFromRaw(target);
      PropertyDetails details =
          TransitionsAccessor::GetTargetDetails(key, target_map);
      kind = details.kind();
      attributes = details.attributes();
    }
    int j;
    for (j = i - 1; j >= 0; j--) {
      Name* temp_key = GetKey(j);
      Object* temp_target = GetRawTarget(j);
      PropertyKind temp_kind = kData;
      PropertyAttributes temp_attributes = NONE;
      if (!TransitionsAccessor::IsSpecialTransition(temp_key)) {
        Map* temp_target_map =
            TransitionsAccessor::GetTargetFromRaw(temp_target);
        PropertyDetails details =
            TransitionsAccessor::GetTargetDetails(temp_key, temp_target_map);
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
