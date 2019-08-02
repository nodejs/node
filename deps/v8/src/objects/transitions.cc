// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/transitions.h"

#include "src/objects/objects-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

Map TransitionsAccessor::GetSimpleTransition() {
  switch (encoding()) {
    case kWeakRef:
      return Map::cast(raw_transitions_->GetHeapObjectAssumeWeak());
    default:
      return Map();
  }
}

bool TransitionsAccessor::HasSimpleTransitionTo(Map map) {
  switch (encoding()) {
    case kWeakRef:
      return raw_transitions_->GetHeapObjectAssumeWeak() == map;
    case kPrototypeInfo:
    case kUninitialized:
    case kMigrationTarget:
    case kFullTransitionArray:
      return false;
  }
  UNREACHABLE();
}

void TransitionsAccessor::Insert(Handle<Name> name, Handle<Map> target,
                                 SimpleTransitionFlag flag) {
  DCHECK(!map_handle_.is_null());
  target->SetBackPointer(map_);

  // If the map doesn't have any transitions at all yet, install the new one.
  if (encoding() == kUninitialized || encoding() == kMigrationTarget) {
    if (flag == SIMPLE_PROPERTY_TRANSITION) {
      ReplaceTransitions(HeapObjectReference::Weak(*target));
      return;
    }
    // If the flag requires a full TransitionArray, allocate one.
    Handle<TransitionArray> result =
        isolate_->factory()->NewTransitionArray(0, 1);
    ReplaceTransitions(MaybeObject::FromObject(*result));
    Reload();
  }

  bool is_special_transition = flag == SPECIAL_TRANSITION;
  // If the map has a simple transition, check if it should be overwritten.
  Map simple_transition = GetSimpleTransition();
  if (!simple_transition.is_null()) {
    Name key = GetSimpleTransitionKey(simple_transition);
    PropertyDetails old_details = GetSimpleTargetDetails(simple_transition);
    PropertyDetails new_details = is_special_transition
                                      ? PropertyDetails::Empty()
                                      : GetTargetDetails(*name, *target);
    if (flag == SIMPLE_PROPERTY_TRANSITION && key.Equals(*name) &&
        old_details.kind() == new_details.kind() &&
        old_details.attributes() == new_details.attributes()) {
      ReplaceTransitions(HeapObjectReference::Weak(*target));
      return;
    }
    // Otherwise allocate a full TransitionArray with slack for a new entry.
    Handle<Map> map(simple_transition, isolate_);
    Handle<TransitionArray> result =
        isolate_->factory()->NewTransitionArray(1, 1);
    // Reload state; allocations might have caused it to be cleared.
    Reload();
    simple_transition = GetSimpleTransition();
    if (!simple_transition.is_null()) {
      DCHECK_EQ(*map, simple_transition);
      if (encoding_ == kWeakRef) {
        result->Set(0, GetSimpleTransitionKey(simple_transition),
                    HeapObjectReference::Weak(simple_transition));
      } else {
        UNREACHABLE();
      }
    } else {
      result->SetNumberOfTransitions(0);
    }
    ReplaceTransitions(MaybeObject::FromObject(*result));
    Reload();
  }

  // At this point, we know that the map has a full TransitionArray.
  DCHECK_EQ(kFullTransitionArray, encoding());

  int number_of_transitions = 0;
  int new_nof = 0;
  int insertion_index = kNotFound;
  DCHECK_EQ(is_special_transition,
            IsSpecialTransition(ReadOnlyRoots(isolate_), *name));
  PropertyDetails details = is_special_transition
                                ? PropertyDetails::Empty()
                                : GetTargetDetails(*name, *target);

  {
    DisallowHeapAllocation no_gc;
    TransitionArray array = transitions();
    number_of_transitions = array.number_of_transitions();
    new_nof = number_of_transitions;

    int index = is_special_transition
                    ? array.SearchSpecial(Symbol::cast(*name), &insertion_index)
                    : array.Search(details.kind(), *name, details.attributes(),
                                   &insertion_index);
    // If an existing entry was found, overwrite it and return.
    if (index != kNotFound) {
      array.SetRawTarget(index, HeapObjectReference::Weak(*target));
      return;
    }

    ++new_nof;
    CHECK_LE(new_nof, kMaxNumberOfTransitions);
    DCHECK(insertion_index >= 0 && insertion_index <= number_of_transitions);

    // If there is enough capacity, insert new entry into the existing array.
    if (new_nof <= array.Capacity()) {
      array.SetNumberOfTransitions(new_nof);
      for (index = number_of_transitions; index > insertion_index; --index) {
        array.SetKey(index, array.GetKey(index - 1));
        array.SetRawTarget(index, array.GetRawTarget(index - 1));
      }
      array.SetKey(index, *name);
      array.SetRawTarget(index, HeapObjectReference::Weak(*target));
      SLOW_DCHECK(array.IsSortedNoDuplicates());
      return;
    }
  }

  // We're gonna need a bigger TransitionArray.
  Handle<TransitionArray> result = isolate_->factory()->NewTransitionArray(
      new_nof,
      Map::SlackForArraySize(number_of_transitions, kMaxNumberOfTransitions));

  // The map's transition array may have shrunk during the allocation above as
  // it was weakly traversed, though it is guaranteed not to disappear. Trim the
  // result copy if needed, and recompute variables.
  Reload();
  DisallowHeapAllocation no_gc;
  TransitionArray array = transitions();
  if (array.number_of_transitions() != number_of_transitions) {
    DCHECK(array.number_of_transitions() < number_of_transitions);

    number_of_transitions = array.number_of_transitions();
    new_nof = number_of_transitions;

    insertion_index = kNotFound;
    int index = is_special_transition
                    ? array.SearchSpecial(Symbol::cast(*name), &insertion_index)
                    : array.Search(details.kind(), *name, details.attributes(),
                                   &insertion_index);
    if (index == kNotFound) {
      ++new_nof;
    } else {
      insertion_index = index;
    }
    DCHECK(insertion_index >= 0 && insertion_index <= number_of_transitions);

    result->SetNumberOfTransitions(new_nof);
  }

  if (array.HasPrototypeTransitions()) {
    result->SetPrototypeTransitions(array.GetPrototypeTransitions());
  }

  DCHECK_NE(kNotFound, insertion_index);
  for (int i = 0; i < insertion_index; ++i) {
    result->Set(i, array.GetKey(i), array.GetRawTarget(i));
  }
  result->Set(insertion_index, *name, HeapObjectReference::Weak(*target));
  for (int i = insertion_index; i < number_of_transitions; ++i) {
    result->Set(i + 1, array.GetKey(i), array.GetRawTarget(i));
  }

  SLOW_DCHECK(result->IsSortedNoDuplicates());
  ReplaceTransitions(MaybeObject::FromObject(*result));
}

Map TransitionsAccessor::SearchTransition(Name name, PropertyKind kind,
                                          PropertyAttributes attributes) {
  DCHECK(name.IsUniqueName());
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
    case kMigrationTarget:
      return Map();
    case kWeakRef: {
      Map map = Map::cast(raw_transitions_->GetHeapObjectAssumeWeak());
      if (!IsMatchingMap(map, name, kind, attributes)) return Map();
      return map;
    }
    case kFullTransitionArray: {
      return transitions().SearchAndGetTarget(kind, name, attributes);
    }
  }
  UNREACHABLE();
}

Map TransitionsAccessor::SearchSpecial(Symbol name) {
  if (encoding() != kFullTransitionArray) return Map();
  int transition = transitions().SearchSpecial(name);
  if (transition == kNotFound) return Map();
  return transitions().GetTarget(transition);
}

// static
bool TransitionsAccessor::IsSpecialTransition(ReadOnlyRoots roots, Name name) {
  if (!name.IsSymbol()) return false;
  return name == roots.nonextensible_symbol() ||
         name == roots.sealed_symbol() || name == roots.frozen_symbol() ||
         name == roots.elements_transition_symbol() ||
         name == roots.strict_function_transition_symbol();
}

MaybeHandle<Map> TransitionsAccessor::FindTransitionToDataProperty(
    Handle<Name> name, RequestedLocation requested_location) {
  DCHECK(name->IsUniqueName());
  DisallowHeapAllocation no_gc;
  PropertyAttributes attributes = name->IsPrivate() ? DONT_ENUM : NONE;
  Map target = SearchTransition(*name, kData, attributes);
  if (target.is_null()) return MaybeHandle<Map>();
  PropertyDetails details = target.GetLastDescriptorDetails();
  DCHECK_EQ(attributes, details.attributes());
  DCHECK_EQ(kData, details.kind());
  if (requested_location == kFieldOnly && details.location() != kField) {
    return MaybeHandle<Map>();
  }
  return Handle<Map>(target, isolate_);
}

bool TransitionsAccessor::CanHaveMoreTransitions() {
  if (map_.is_dictionary_map()) return false;
  if (encoding() == kFullTransitionArray) {
    return transitions().number_of_transitions() < kMaxNumberOfTransitions;
  }
  return true;
}

// static
bool TransitionsAccessor::IsMatchingMap(Map target, Name name,
                                        PropertyKind kind,
                                        PropertyAttributes attributes) {
  int descriptor = target.LastAdded();
  DescriptorArray descriptors = target.instance_descriptors();
  Name key = descriptors.GetKey(descriptor);
  if (key != name) return false;
  return descriptors.GetDetails(descriptor)
      .HasKindAndAttributes(kind, attributes);
}

// static
bool TransitionArray::CompactPrototypeTransitionArray(Isolate* isolate,
                                                      WeakFixedArray array) {
  const int header = kProtoTransitionHeaderSize;
  int number_of_transitions = NumberOfPrototypeTransitions(array);
  if (number_of_transitions == 0) {
    // Empty array cannot be compacted.
    return false;
  }
  int new_number_of_transitions = 0;
  for (int i = 0; i < number_of_transitions; i++) {
    MaybeObject target = array.Get(header + i);
    DCHECK(target->IsCleared() ||
           (target->IsWeak() && target->GetHeapObject().IsMap()));
    if (!target->IsCleared()) {
      if (new_number_of_transitions != i) {
        array.Set(header + new_number_of_transitions, target);
      }
      new_number_of_transitions++;
    }
  }
  // Fill slots that became free with undefined value.
  MaybeObject undefined =
      MaybeObject::FromObject(*isolate->factory()->undefined_value());
  for (int i = new_number_of_transitions; i < number_of_transitions; i++) {
    array.Set(header + i, undefined);
  }
  if (number_of_transitions != new_number_of_transitions) {
    SetNumberOfPrototypeTransitions(array, new_number_of_transitions);
  }
  return new_number_of_transitions < number_of_transitions;
}

// static
Handle<WeakFixedArray> TransitionArray::GrowPrototypeTransitionArray(
    Handle<WeakFixedArray> array, int new_capacity, Isolate* isolate) {
  // Grow array by factor 2 up to MaxCachedPrototypeTransitions.
  int capacity = array->length() - kProtoTransitionHeaderSize;
  new_capacity = Min(kMaxCachedPrototypeTransitions, new_capacity);
  DCHECK_GT(new_capacity, capacity);
  int grow_by = new_capacity - capacity;
  array = isolate->factory()->CopyWeakFixedArrayAndGrow(array, grow_by,
                                                        AllocationType::kOld);
  if (capacity < 0) {
    // There was no prototype transitions array before, so the size
    // couldn't be copied. Initialize it explicitly.
    SetNumberOfPrototypeTransitions(*array, 0);
  }
  return array;
}

void TransitionsAccessor::PutPrototypeTransition(Handle<Object> prototype,
                                                 Handle<Map> target_map) {
  DCHECK(HeapObject::cast(*prototype).map().IsMap());
  // Don't cache prototype transition if this map is either shared, or a map of
  // a prototype.
  if (map_.is_prototype_map()) return;
  if (map_.is_dictionary_map() || !FLAG_cache_prototype_transitions) return;

  const int header = TransitionArray::kProtoTransitionHeaderSize;

  Handle<WeakFixedArray> cache(GetPrototypeTransitions(), isolate_);
  int capacity = cache->length() - header;
  int transitions = TransitionArray::NumberOfPrototypeTransitions(*cache) + 1;

  if (transitions > capacity) {
    // Grow the array if compacting it doesn't free space.
    if (!TransitionArray::CompactPrototypeTransitionArray(isolate_, *cache)) {
      if (capacity == TransitionArray::kMaxCachedPrototypeTransitions) return;
      cache = TransitionArray::GrowPrototypeTransitionArray(
          cache, 2 * transitions, isolate_);
      Reload();
      SetPrototypeTransitions(cache);
    }
  }

  // Reload number of transitions as they might have been compacted.
  int last = TransitionArray::NumberOfPrototypeTransitions(*cache);
  int entry = header + last;

  cache->Set(entry, HeapObjectReference::Weak(*target_map));
  TransitionArray::SetNumberOfPrototypeTransitions(*cache, last + 1);
}

Handle<Map> TransitionsAccessor::GetPrototypeTransition(
    Handle<Object> prototype) {
  DisallowHeapAllocation no_gc;
  WeakFixedArray cache = GetPrototypeTransitions();
  int length = TransitionArray::NumberOfPrototypeTransitions(cache);
  for (int i = 0; i < length; i++) {
    MaybeObject target =
        cache.Get(TransitionArray::kProtoTransitionHeaderSize + i);
    DCHECK(target->IsWeakOrCleared());
    HeapObject heap_object;
    if (target->GetHeapObjectIfWeak(&heap_object)) {
      Map map = Map::cast(heap_object);
      if (map.prototype() == *prototype) {
        return handle(map, isolate_);
      }
    }
  }
  return Handle<Map>();
}

WeakFixedArray TransitionsAccessor::GetPrototypeTransitions() {
  if (encoding() != kFullTransitionArray ||
      !transitions().HasPrototypeTransitions()) {
    return ReadOnlyRoots(isolate_).empty_weak_fixed_array();
  }
  return transitions().GetPrototypeTransitions();
}

// static
void TransitionArray::SetNumberOfPrototypeTransitions(
    WeakFixedArray proto_transitions, int value) {
  DCHECK_NE(proto_transitions.length(), 0);
  proto_transitions.Set(kProtoTransitionNumberOfEntriesOffset,
                        MaybeObject::FromSmi(Smi::FromInt(value)));
}

int TransitionsAccessor::NumberOfTransitions() {
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
    case kMigrationTarget:
      return 0;
    case kWeakRef:
      return 1;
    case kFullTransitionArray:
      return transitions().number_of_transitions();
  }
  UNREACHABLE();
  return 0;  // Make GCC happy.
}

void TransitionsAccessor::SetMigrationTarget(Map migration_target) {
  // We only cache the migration target for maps with empty transitions for GC's
  // sake.
  if (encoding() != kUninitialized) return;
  DCHECK(map_.is_deprecated());
  map_.set_raw_transitions(MaybeObject::FromObject(migration_target));
  MarkNeedsReload();
}

Map TransitionsAccessor::GetMigrationTarget() {
  if (encoding() == kMigrationTarget) {
    return map_.raw_transitions()->cast<Map>();
  }
  return Map();
}

void TransitionArray::Zap(Isolate* isolate) {
  MemsetTagged(ObjectSlot(RawFieldOfElementAt(kPrototypeTransitionsIndex)),
               ReadOnlyRoots(isolate).the_hole_value(),
               length() - kPrototypeTransitionsIndex);
  SetNumberOfTransitions(0);
}

void TransitionsAccessor::ReplaceTransitions(MaybeObject new_transitions) {
  if (encoding() == kFullTransitionArray) {
    TransitionArray old_transitions = transitions();
#if DEBUG
    CheckNewTransitionsAreConsistent(
        old_transitions, new_transitions->GetHeapObjectAssumeStrong());
    DCHECK(old_transitions != new_transitions->GetHeapObjectAssumeStrong());
#endif
    // Transition arrays are not shared. When one is replaced, it should not
    // keep referenced objects alive, so we zap it.
    // When there is another reference to the array somewhere (e.g. a handle),
    // not zapping turns from a waste of memory into a source of crashes.
    old_transitions.Zap(isolate_);
  }
  map_.set_raw_transitions(new_transitions);
  MarkNeedsReload();
}

void TransitionsAccessor::SetPrototypeTransitions(
    Handle<WeakFixedArray> proto_transitions) {
  EnsureHasFullTransitionArray();
  transitions().SetPrototypeTransitions(*proto_transitions);
}

void TransitionsAccessor::EnsureHasFullTransitionArray() {
  if (encoding() == kFullTransitionArray) return;
  int nof =
      (encoding() == kUninitialized || encoding() == kMigrationTarget) ? 0 : 1;
  Handle<TransitionArray> result = isolate_->factory()->NewTransitionArray(nof);
  Reload();  // Reload after possible GC.
  if (nof == 1) {
    if (encoding() == kUninitialized) {
      // If allocation caused GC and cleared the target, trim the new array.
      result->SetNumberOfTransitions(0);
    } else {
      // Otherwise populate the new array.
      Handle<Map> target(GetSimpleTransition(), isolate_);
      Name key = GetSimpleTransitionKey(*target);
      result->Set(0, key, HeapObjectReference::Weak(*target));
    }
  }
  ReplaceTransitions(MaybeObject::FromObject(*result));
  Reload();  // Reload after replacing transitions.
}

void TransitionsAccessor::TraverseTransitionTreeInternal(
    TraverseCallback callback, void* data, DisallowHeapAllocation* no_gc) {
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
    case kMigrationTarget:
      break;
    case kWeakRef: {
      Map simple_target =
          Map::cast(raw_transitions_->GetHeapObjectAssumeWeak());
      TransitionsAccessor(isolate_, simple_target, no_gc)
          .TraverseTransitionTreeInternal(callback, data, no_gc);
      break;
    }
    case kFullTransitionArray: {
      if (transitions().HasPrototypeTransitions()) {
        WeakFixedArray proto_trans = transitions().GetPrototypeTransitions();
        int length = TransitionArray::NumberOfPrototypeTransitions(proto_trans);
        for (int i = 0; i < length; ++i) {
          int index = TransitionArray::kProtoTransitionHeaderSize + i;
          MaybeObject target = proto_trans.Get(index);
          HeapObject heap_object;
          if (target->GetHeapObjectIfWeak(&heap_object)) {
            TransitionsAccessor(isolate_, Map::cast(heap_object), no_gc)
                .TraverseTransitionTreeInternal(callback, data, no_gc);
          } else {
            DCHECK(target->IsCleared());
          }
        }
      }
      for (int i = 0; i < transitions().number_of_transitions(); ++i) {
        TransitionsAccessor(isolate_, transitions().GetTarget(i), no_gc)
            .TraverseTransitionTreeInternal(callback, data, no_gc);
      }
      break;
    }
  }
  callback(map_, data);
}

#ifdef DEBUG
void TransitionsAccessor::CheckNewTransitionsAreConsistent(
    TransitionArray old_transitions, Object transitions) {
  // This function only handles full transition arrays.
  DCHECK_EQ(kFullTransitionArray, encoding());
  TransitionArray new_transitions = TransitionArray::cast(transitions);
  for (int i = 0; i < old_transitions.number_of_transitions(); i++) {
    Map target = old_transitions.GetTarget(i);
    if (target.instance_descriptors() == map_.instance_descriptors()) {
      Name key = old_transitions.GetKey(i);
      int new_target_index;
      if (IsSpecialTransition(ReadOnlyRoots(isolate_), key)) {
        new_target_index = new_transitions.SearchSpecial(Symbol::cast(key));
      } else {
        PropertyDetails details = GetTargetDetails(key, target);
        new_target_index =
            new_transitions.Search(details.kind(), key, details.attributes());
      }
      DCHECK_NE(TransitionArray::kNotFound, new_target_index);
      DCHECK_EQ(target, new_transitions.GetTarget(new_target_index));
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
  Name key = GetKey(transition);
  for (; transition < nof_transitions && GetKey(transition) == key;
       transition++) {
    Map target = GetTarget(transition);
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

Map TransitionArray::SearchDetailsAndGetTarget(int transition,
                                               PropertyKind kind,
                                               PropertyAttributes attributes) {
  int nof_transitions = number_of_transitions();
  DCHECK(transition < nof_transitions);
  Name key = GetKey(transition);
  for (; transition < nof_transitions && GetKey(transition) == key;
       transition++) {
    Map target = GetTarget(transition);
    PropertyDetails target_details =
        TransitionsAccessor::GetTargetDetails(key, target);

    int cmp = CompareDetails(kind, attributes, target_details.kind(),
                             target_details.attributes());
    if (cmp == 0) {
      return target;
    } else if (cmp < 0) {
      break;
    }
  }
  return Map();
}

int TransitionArray::Search(PropertyKind kind, Name name,
                            PropertyAttributes attributes,
                            int* out_insertion_index) {
  int transition = SearchName(name, out_insertion_index);
  if (transition == kNotFound) return kNotFound;
  return SearchDetails(transition, kind, attributes, out_insertion_index);
}

Map TransitionArray::SearchAndGetTarget(PropertyKind kind, Name name,
                                        PropertyAttributes attributes) {
  int transition = SearchName(name, nullptr);
  if (transition == kNotFound) {
    return Map();
  }
  return SearchDetailsAndGetTarget(transition, kind, attributes);
}

void TransitionArray::Sort() {
  DisallowHeapAllocation no_gc;
  // In-place insertion sort.
  int length = number_of_transitions();
  ReadOnlyRoots roots = GetReadOnlyRoots();
  for (int i = 1; i < length; i++) {
    Name key = GetKey(i);
    MaybeObject target = GetRawTarget(i);
    PropertyKind kind = kData;
    PropertyAttributes attributes = NONE;
    if (!TransitionsAccessor::IsSpecialTransition(roots, key)) {
      Map target_map = TransitionsAccessor::GetTargetFromRaw(target);
      PropertyDetails details =
          TransitionsAccessor::GetTargetDetails(key, target_map);
      kind = details.kind();
      attributes = details.attributes();
    }
    int j;
    for (j = i - 1; j >= 0; j--) {
      Name temp_key = GetKey(j);
      MaybeObject temp_target = GetRawTarget(j);
      PropertyKind temp_kind = kData;
      PropertyAttributes temp_attributes = NONE;
      if (!TransitionsAccessor::IsSpecialTransition(roots, temp_key)) {
        Map temp_target_map =
            TransitionsAccessor::GetTargetFromRaw(temp_target);
        PropertyDetails details =
            TransitionsAccessor::GetTargetDetails(temp_key, temp_target_map);
        temp_kind = details.kind();
        temp_attributes = details.attributes();
      }
      int cmp = CompareKeys(temp_key, temp_key.Hash(), temp_kind,
                            temp_attributes, key, key.Hash(), kind, attributes);
      if (cmp > 0) {
        SetKey(j + 1, temp_key);
        SetRawTarget(j + 1, temp_target);
      } else {
        break;
      }
    }
    SetKey(j + 1, key);
    SetRawTarget(j + 1, target);
  }
  DCHECK(IsSortedNoDuplicates());
}

bool TransitionsAccessor::HasIntegrityLevelTransitionTo(
    Map to, Symbol* out_symbol, PropertyAttributes* out_integrity_level) {
  ReadOnlyRoots roots(isolate_);
  if (SearchSpecial(roots.frozen_symbol()) == to) {
    if (out_integrity_level) *out_integrity_level = FROZEN;
    if (out_symbol) *out_symbol = roots.frozen_symbol();
  } else if (SearchSpecial(roots.sealed_symbol()) == to) {
    if (out_integrity_level) *out_integrity_level = SEALED;
    if (out_symbol) *out_symbol = roots.sealed_symbol();
  } else if (SearchSpecial(roots.nonextensible_symbol()) == to) {
    if (out_integrity_level) *out_integrity_level = NONE;
    if (out_symbol) *out_symbol = roots.nonextensible_symbol();
  } else {
    return false;
  }
  return true;
}

}  // namespace internal
}  // namespace v8
