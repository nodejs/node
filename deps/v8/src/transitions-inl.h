// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRANSITIONS_INL_H_
#define V8_TRANSITIONS_INL_H_

#include "src/transitions.h"

namespace v8 {
namespace internal {


TransitionArray* TransitionArray::cast(Object* object) {
  DCHECK(object->IsTransitionArray());
  return reinterpret_cast<TransitionArray*>(object);
}


Object* TransitionArray::next_link() { return get(kNextLinkIndex); }


void TransitionArray::set_next_link(Object* next, WriteBarrierMode mode) {
  return set(kNextLinkIndex, next, mode);
}


bool TransitionArray::HasPrototypeTransitions() {
  return get(kPrototypeTransitionsIndex) != Smi::FromInt(0);
}


FixedArray* TransitionArray::GetPrototypeTransitions() {
  DCHECK(HasPrototypeTransitions());  // Callers must check first.
  Object* prototype_transitions = get(kPrototypeTransitionsIndex);
  return FixedArray::cast(prototype_transitions);
}


void TransitionArray::SetPrototypeTransitions(FixedArray* transitions) {
  DCHECK(transitions->IsFixedArray());
  set(kPrototypeTransitionsIndex, transitions);
}


Object** TransitionArray::GetPrototypeTransitionsSlot() {
  return RawFieldOfElementAt(kPrototypeTransitionsIndex);
}


Object** TransitionArray::GetKeySlot(int transition_number) {
  DCHECK(transition_number < number_of_transitions());
  return RawFieldOfElementAt(ToKeyIndex(transition_number));
}


Name* TransitionArray::GetKey(int transition_number) {
  DCHECK(transition_number < number_of_transitions());
  return Name::cast(get(ToKeyIndex(transition_number)));
}


Name* TransitionArray::GetKey(Object* raw_transitions, int transition_number) {
  if (IsSimpleTransition(raw_transitions)) {
    DCHECK(transition_number == 0);
    return GetSimpleTransitionKey(GetSimpleTransition(raw_transitions));
  }
  DCHECK(IsFullTransitionArray(raw_transitions));
  return TransitionArray::cast(raw_transitions)->GetKey(transition_number);
}


void TransitionArray::SetKey(int transition_number, Name* key) {
  DCHECK(transition_number < number_of_transitions());
  set(ToKeyIndex(transition_number), key);
}


Map* TransitionArray::GetTarget(int transition_number) {
  DCHECK(transition_number < number_of_transitions());
  return Map::cast(get(ToTargetIndex(transition_number)));
}


Map* TransitionArray::GetTarget(Object* raw_transitions,
                                int transition_number) {
  if (IsSimpleTransition(raw_transitions)) {
    DCHECK(transition_number == 0);
    return GetSimpleTransition(raw_transitions);
  }
  DCHECK(IsFullTransitionArray(raw_transitions));
  return TransitionArray::cast(raw_transitions)->GetTarget(transition_number);
}


void TransitionArray::SetTarget(int transition_number, Map* value) {
  DCHECK(transition_number < number_of_transitions());
  set(ToTargetIndex(transition_number), value);
}


int TransitionArray::SearchName(Name* name, int* out_insertion_index) {
  return internal::Search<ALL_ENTRIES>(this, name, 0, out_insertion_index);
}


#ifdef DEBUG
bool TransitionArray::IsSpecialTransition(Name* name) {
  if (!name->IsSymbol()) return false;
  Heap* heap = name->GetHeap();
  return name == heap->nonextensible_symbol() ||
         name == heap->sealed_symbol() || name == heap->frozen_symbol() ||
         name == heap->elements_transition_symbol() ||
         name == heap->strict_function_transition_symbol() ||
         name == heap->strong_function_transition_symbol() ||
         name == heap->observed_symbol();
}
#endif


int TransitionArray::CompareKeys(Name* key1, uint32_t hash1, PropertyKind kind1,
                                 PropertyAttributes attributes1, Name* key2,
                                 uint32_t hash2, PropertyKind kind2,
                                 PropertyAttributes attributes2) {
  int cmp = CompareNames(key1, hash1, key2, hash2);
  if (cmp != 0) return cmp;

  return CompareDetails(kind1, attributes1, kind2, attributes2);
}


int TransitionArray::CompareNames(Name* key1, uint32_t hash1, Name* key2,
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


PropertyDetails TransitionArray::GetTargetDetails(Name* name, Map* target) {
  DCHECK(!IsSpecialTransition(name));
  int descriptor = target->LastAdded();
  DescriptorArray* descriptors = target->instance_descriptors();
  // Transitions are allowed only for the last added property.
  DCHECK(descriptors->GetKey(descriptor)->Equals(name));
  return descriptors->GetDetails(descriptor);
}


void TransitionArray::Set(int transition_number, Name* key, Map* target) {
  set(ToKeyIndex(transition_number), key);
  set(ToTargetIndex(transition_number), target);
}


void TransitionArray::SetNumberOfTransitions(int number_of_transitions) {
  DCHECK(number_of_transitions <= Capacity(this));
  set(kTransitionLengthIndex, Smi::FromInt(number_of_transitions));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TRANSITIONS_INL_H_
