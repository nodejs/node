// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRANSITIONS_INL_H_
#define V8_TRANSITIONS_INL_H_

#include "src/transitions.h"

#include "src/ic/handler-configuration-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <TransitionsAccessor::Encoding enc>
WeakCell* TransitionsAccessor::GetTargetCell() {
  DCHECK(!needs_reload_);
  if (target_cell_ != nullptr) return target_cell_;
  if (enc == kWeakCell) {
    target_cell_ = WeakCell::cast(raw_transitions_);
  } else if (enc == kHandler) {
    target_cell_ = StoreHandler::GetTransitionCell(raw_transitions_);
  } else {
    UNREACHABLE();
  }
  return target_cell_;
}

TransitionArray* TransitionsAccessor::transitions() {
  DCHECK_EQ(kFullTransitionArray, encoding());
  return TransitionArray::cast(raw_transitions_);
}

CAST_ACCESSOR(TransitionArray)

bool TransitionArray::HasPrototypeTransitions() {
  return get(kPrototypeTransitionsIndex) != Smi::kZero;
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

Name* TransitionsAccessor::GetKey(int transition_number) {
  WeakCell* cell = nullptr;
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
      UNREACHABLE();
      return nullptr;
    case kWeakCell:
      cell = GetTargetCell<kWeakCell>();
      break;
    case kHandler:
      cell = GetTargetCell<kHandler>();
      break;
    case kFullTransitionArray:
      return transitions()->GetKey(transition_number);
  }
  DCHECK(!cell->cleared());
  return GetSimpleTransitionKey(Map::cast(cell->value()));
}

void TransitionArray::SetKey(int transition_number, Name* key) {
  DCHECK(transition_number < number_of_transitions());
  set(ToKeyIndex(transition_number), key);
}

Object** TransitionArray::GetTargetSlot(int transition_number) {
  DCHECK(transition_number < number_of_transitions());
  return RawFieldOfElementAt(ToTargetIndex(transition_number));
}

// static
PropertyDetails TransitionsAccessor::GetTargetDetails(Name* name, Map* target) {
  DCHECK(!IsSpecialTransition(name));
  int descriptor = target->LastAdded();
  DescriptorArray* descriptors = target->instance_descriptors();
  // Transitions are allowed only for the last added property.
  DCHECK(descriptors->GetKey(descriptor)->Equals(name));
  return descriptors->GetDetails(descriptor);
}

// static
Map* TransitionsAccessor::GetTargetFromRaw(Object* raw) {
  if (raw->IsWeakCell()) return Map::cast(WeakCell::cast(raw)->value());
  return Map::cast(StoreHandler::GetTransitionCell(raw)->value());
}

Object* TransitionArray::GetRawTarget(int transition_number) {
  DCHECK(transition_number < number_of_transitions());
  return get(ToTargetIndex(transition_number));
}

Map* TransitionArray::GetTarget(int transition_number) {
  Object* raw = GetRawTarget(transition_number);
  return TransitionsAccessor::GetTargetFromRaw(raw);
}

Map* TransitionsAccessor::GetTarget(int transition_number) {
  WeakCell* cell = nullptr;
  switch (encoding()) {
    case kPrototypeInfo:
    case kUninitialized:
      UNREACHABLE();
      return nullptr;
    case kWeakCell:
      cell = GetTargetCell<kWeakCell>();
      break;
    case kHandler:
      cell = GetTargetCell<kHandler>();
      break;
    case kFullTransitionArray:
      return transitions()->GetTarget(transition_number);
  }
  DCHECK(!cell->cleared());
  return Map::cast(cell->value());
}

void TransitionArray::SetTarget(int transition_number, Object* value) {
  DCHECK(!value->IsMap());
  DCHECK(transition_number < number_of_transitions());
  set(ToTargetIndex(transition_number), value);
}


int TransitionArray::SearchName(Name* name, int* out_insertion_index) {
  DCHECK(name->IsUniqueName());
  return internal::Search<ALL_ENTRIES>(this, name, number_of_entries(),
                                       out_insertion_index);
}

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

void TransitionArray::Set(int transition_number, Name* key, Object* target) {
  set(ToKeyIndex(transition_number), key);
  set(ToTargetIndex(transition_number), target);
}

int TransitionArray::Capacity() {
  if (length() <= kFirstIndex) return 0;
  return (length() - kFirstIndex) / kTransitionSize;
}

void TransitionArray::SetNumberOfTransitions(int number_of_transitions) {
  DCHECK(number_of_transitions <= Capacity());
  set(kTransitionLengthIndex, Smi::FromInt(number_of_transitions));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_TRANSITIONS_INL_H_
