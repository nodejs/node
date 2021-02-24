// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TRANSITIONS_INL_H_
#define V8_OBJECTS_TRANSITIONS_INL_H_

#include "src/ic/handler-configuration-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/objects/transitions.h"
#include "src/snapshot/deserializer.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TransitionArray TransitionsAccessor::transitions() {
  DCHECK_EQ(kFullTransitionArray, encoding());
  return TransitionArray::cast(raw_transitions_->GetHeapObjectAssumeStrong());
}

OBJECT_CONSTRUCTORS_IMPL(TransitionArray, WeakFixedArray)

CAST_ACCESSOR(TransitionArray)

bool TransitionArray::HasPrototypeTransitions() {
  return Get(kPrototypeTransitionsIndex) != MaybeObject::FromSmi(Smi::zero());
}

WeakFixedArray TransitionArray::GetPrototypeTransitions() {
  DCHECK(HasPrototypeTransitions());  // Callers must check first.
  Object prototype_transitions =
      Get(kPrototypeTransitionsIndex)->GetHeapObjectAssumeStrong();
  return WeakFixedArray::cast(prototype_transitions);
}

HeapObjectSlot TransitionArray::GetKeySlot(int transition_number) {
  DCHECK(transition_number < number_of_transitions());
  return HeapObjectSlot(RawFieldOfElementAt(ToKeyIndex(transition_number)));
}

void TransitionArray::SetPrototypeTransitions(WeakFixedArray transitions) {
  DCHECK(transitions.IsWeakFixedArray());
  WeakFixedArray::Set(kPrototypeTransitionsIndex,
                      HeapObjectReference::Strong(transitions));
}

int TransitionArray::NumberOfPrototypeTransitions(
    WeakFixedArray proto_transitions) {
  if (proto_transitions.length() == 0) return 0;
  MaybeObject raw =
      proto_transitions.Get(kProtoTransitionNumberOfEntriesOffset);
  return raw.ToSmi().value();
}

Name TransitionArray::GetKey(int transition_number) {
  DCHECK(transition_number < number_of_transitions());
  return Name::cast(
      Get(ToKeyIndex(transition_number))->GetHeapObjectAssumeStrong());
}

Name TransitionArray::GetKey(InternalIndex index) {
  return GetKey(index.as_int());
}

Name TransitionsAccessor::GetKey(int transition_number) {
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
    case kMigrationTarget:
      UNREACHABLE();
      return Name();
    case kWeakRef: {
      Map map = Map::cast(raw_transitions_->GetHeapObjectAssumeWeak());
      return GetSimpleTransitionKey(map);
    }
    case kFullTransitionArray:
      return transitions().GetKey(transition_number);
  }
  UNREACHABLE();
}

void TransitionArray::SetKey(int transition_number, Name key) {
  DCHECK(transition_number < number_of_transitions());
  WeakFixedArray::Set(ToKeyIndex(transition_number),
                      HeapObjectReference::Strong(key));
}

HeapObjectSlot TransitionArray::GetTargetSlot(int transition_number) {
  DCHECK(transition_number < number_of_transitions());
  return HeapObjectSlot(RawFieldOfElementAt(ToTargetIndex(transition_number)));
}

// static
PropertyDetails TransitionsAccessor::GetTargetDetails(Name name, Map target) {
  DCHECK(!IsSpecialTransition(name.GetReadOnlyRoots(), name));
  InternalIndex descriptor = target.LastAdded();
  DescriptorArray descriptors = target.instance_descriptors(kRelaxedLoad);
  // Transitions are allowed only for the last added property.
  DCHECK(descriptors.GetKey(descriptor).Equals(name));
  return descriptors.GetDetails(descriptor);
}

PropertyDetails TransitionsAccessor::GetSimpleTargetDetails(Map transition) {
  return transition.GetLastDescriptorDetails(isolate_);
}

// static
Name TransitionsAccessor::GetSimpleTransitionKey(Map transition) {
  InternalIndex descriptor = transition.LastAdded();
  return transition.instance_descriptors(kRelaxedLoad).GetKey(descriptor);
}

// static
Map TransitionsAccessor::GetTargetFromRaw(MaybeObject raw) {
  return Map::cast(raw->GetHeapObjectAssumeWeak());
}

MaybeObject TransitionArray::GetRawTarget(int transition_number) {
  DCHECK(transition_number < number_of_transitions());
  return Get(ToTargetIndex(transition_number));
}

Map TransitionArray::GetTarget(int transition_number) {
  MaybeObject raw = GetRawTarget(transition_number);
  return TransitionsAccessor::GetTargetFromRaw(raw);
}

Map TransitionsAccessor::GetTarget(int transition_number) {
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
    case kMigrationTarget:
      UNREACHABLE();
      return Map();
    case kWeakRef:
      return Map::cast(raw_transitions_->GetHeapObjectAssumeWeak());
    case kFullTransitionArray:
      return transitions().GetTarget(transition_number);
  }
  UNREACHABLE();
}

void TransitionArray::SetRawTarget(int transition_number, MaybeObject value) {
  DCHECK(transition_number < number_of_transitions());
  DCHECK(value->IsWeak());
  DCHECK(value->GetHeapObjectAssumeWeak().IsMap());
  WeakFixedArray::Set(ToTargetIndex(transition_number), value);
}

bool TransitionArray::GetTargetIfExists(int transition_number, Isolate* isolate,
                                        Map* target) {
  MaybeObject raw = GetRawTarget(transition_number);
  HeapObject heap_object;
  // If the raw target is a Smi, then this TransitionArray is in the process of
  // being deserialized, and doesn't yet have an initialized entry for this
  // transition.
  if (raw.IsSmi()) {
    DCHECK(isolate->has_active_deserializer());
    DCHECK_EQ(raw.ToSmi(), Deserializer::uninitialized_field_value());
    return false;
  }
  if (raw->GetHeapObjectIfStrong(&heap_object) &&
      heap_object.IsUndefined(isolate)) {
    return false;
  }
  *target = TransitionsAccessor::GetTargetFromRaw(raw);
  return true;
}

int TransitionArray::SearchNameForTesting(Name name, int* out_insertion_index) {
  return SearchName(name, out_insertion_index);
}

Map TransitionArray::SearchAndGetTargetForTesting(
    PropertyKind kind, Name name, PropertyAttributes attributes) {
  return SearchAndGetTarget(kind, name, attributes);
}

int TransitionArray::SearchSpecial(Symbol symbol, int* out_insertion_index) {
  return SearchName(symbol, out_insertion_index);
}

int TransitionArray::SearchName(Name name, int* out_insertion_index) {
  DCHECK(name.IsUniqueName());
  return internal::Search<ALL_ENTRIES>(this, name, number_of_entries(),
                                       out_insertion_index);
}

TransitionsAccessor::TransitionsAccessor(Isolate* isolate, Map map,
                                         DisallowGarbageCollection* no_gc)
    : isolate_(isolate), map_(map), concurrent_access_(false) {
  Initialize();
  USE(no_gc);
}

TransitionsAccessor::TransitionsAccessor(Isolate* isolate, Handle<Map> map,
                                         bool concurrent_access)
    : isolate_(isolate),
      map_handle_(map),
      map_(*map),
      concurrent_access_(concurrent_access) {
  Initialize();
}

void TransitionsAccessor::Reload() {
  DCHECK(!map_handle_.is_null());
  map_ = *map_handle_;
  Initialize();
}

int TransitionsAccessor::Capacity() { return transitions().Capacity(); }

void TransitionsAccessor::Initialize() {
  raw_transitions_ = map_.raw_transitions(isolate_);
  HeapObject heap_object;
  if (raw_transitions_->IsSmi() || raw_transitions_->IsCleared()) {
    encoding_ = kUninitialized;
  } else if (raw_transitions_->IsWeak()) {
    encoding_ = kWeakRef;
  } else if (raw_transitions_->GetHeapObjectIfStrong(isolate_, &heap_object)) {
    if (heap_object.IsTransitionArray()) {
      encoding_ = kFullTransitionArray;
    } else if (heap_object.IsPrototypeInfo()) {
      encoding_ = kPrototypeInfo;
    } else {
      DCHECK(map_.is_deprecated());
      DCHECK(heap_object.IsMap());
      encoding_ = kMigrationTarget;
    }
  } else {
    UNREACHABLE();
  }
#if DEBUG
  needs_reload_ = false;
#endif
}

int TransitionArray::number_of_transitions() const {
  if (length() < kFirstIndex) return 0;
  return Get(kTransitionLengthIndex).ToSmi().value();
}

int TransitionArray::CompareKeys(Name key1, uint32_t hash1, PropertyKind kind1,
                                 PropertyAttributes attributes1, Name key2,
                                 uint32_t hash2, PropertyKind kind2,
                                 PropertyAttributes attributes2) {
  int cmp = CompareNames(key1, hash1, key2, hash2);
  if (cmp != 0) return cmp;

  return CompareDetails(kind1, attributes1, kind2, attributes2);
}

int TransitionArray::CompareNames(Name key1, uint32_t hash1, Name key2,
                                  uint32_t hash2) {
  if (key1 != key2) {
    // In case of hash collisions key1 is always "less" than key2.
    return hash1 <= hash2 ? -1 : 1;
  }

  return 0;
}

int TransitionArray::CompareDetails(PropertyKind kind1,
                                    PropertyAttributes attributes1,
                                    PropertyKind kind2,
                                    PropertyAttributes attributes2) {
  if (kind1 != kind2) {
    return static_cast<int>(kind1) < static_cast<int>(kind2) ? -1 : 1;
  }

  if (attributes1 != attributes2) {
    return static_cast<int>(attributes1) < static_cast<int>(attributes2) ? -1
                                                                         : 1;
  }

  return 0;
}

void TransitionArray::Set(int transition_number, Name key, MaybeObject target) {
  WeakFixedArray::Set(ToKeyIndex(transition_number),
                      MaybeObject::FromObject(key));
  WeakFixedArray::Set(ToTargetIndex(transition_number), target);
}

Name TransitionArray::GetSortedKey(int transition_number) {
  return GetKey(transition_number);
}

int TransitionArray::number_of_entries() const {
  return number_of_transitions();
}

int TransitionArray::Capacity() {
  if (length() <= kFirstIndex) return 0;
  return (length() - kFirstIndex) / kEntrySize;
}

void TransitionArray::SetNumberOfTransitions(int number_of_transitions) {
  DCHECK(number_of_transitions <= Capacity());
  WeakFixedArray::Set(
      kTransitionLengthIndex,
      MaybeObject::FromSmi(Smi::FromInt(number_of_transitions)));
}

Handle<String> TransitionsAccessor::ExpectedTransitionKey() {
  DisallowGarbageCollection no_gc;
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
    case kMigrationTarget:
    case kFullTransitionArray:
      return Handle<String>::null();
    case kWeakRef: {
      Map target = Map::cast(raw_transitions_->GetHeapObjectAssumeWeak());
      PropertyDetails details = GetSimpleTargetDetails(target);
      if (details.location() != kField) return Handle<String>::null();
      DCHECK_EQ(kData, details.kind());
      if (details.attributes() != NONE) return Handle<String>::null();
      Name name = GetSimpleTransitionKey(target);
      if (!name.IsString()) return Handle<String>::null();
      return handle(String::cast(name), isolate_);
    }
  }
  UNREACHABLE();
}

Handle<Map> TransitionsAccessor::ExpectedTransitionTarget() {
  DCHECK(!ExpectedTransitionKey().is_null());
  return handle(GetTarget(0), isolate_);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TRANSITIONS_INL_H_
