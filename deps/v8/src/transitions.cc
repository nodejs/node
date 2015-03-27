// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/objects.h"
#include "src/transitions-inl.h"
#include "src/utils.h"

namespace v8 {
namespace internal {


Handle<TransitionArray> TransitionArray::Allocate(Isolate* isolate,
                                                  int number_of_transitions,
                                                  int slack) {
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(
      LengthFor(number_of_transitions + slack));
  array->set(kPrototypeTransitionsIndex, Smi::FromInt(0));
  array->set(kTransitionLengthIndex, Smi::FromInt(number_of_transitions));
  return Handle<TransitionArray>::cast(array);
}


Handle<TransitionArray> TransitionArray::AllocateSimple(Isolate* isolate,
                                                        Handle<Map> target) {
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(kSimpleTransitionSize);
  array->set(kSimpleTransitionTarget, *target);
  return Handle<TransitionArray>::cast(array);
}


void TransitionArray::NoIncrementalWriteBarrierCopyFrom(TransitionArray* origin,
                                                        int origin_transition,
                                                        int target_transition) {
  NoIncrementalWriteBarrierSet(target_transition,
                               origin->GetKey(origin_transition),
                               origin->GetTarget(origin_transition));
}


Handle<TransitionArray> TransitionArray::NewWith(Handle<Map> map,
                                                 Handle<Name> name,
                                                 Handle<Map> target,
                                                 SimpleTransitionFlag flag) {
  Handle<TransitionArray> result;
  Isolate* isolate = name->GetIsolate();

  if (flag == SIMPLE_PROPERTY_TRANSITION) {
    result = AllocateSimple(isolate, target);
  } else {
    result = Allocate(isolate, 1);
    result->NoIncrementalWriteBarrierSet(0, *name, *target);
  }
  result->set_back_pointer_storage(map->GetBackPointer());
  return result;
}


Handle<TransitionArray> TransitionArray::ExtendToFullTransitionArray(
    Handle<Map> containing_map) {
  DCHECK(!containing_map->transitions()->IsFullTransitionArray());
  int nof = containing_map->transitions()->number_of_transitions();

  // A transition array may shrink during GC.
  Handle<TransitionArray> result = Allocate(containing_map->GetIsolate(), nof);
  DisallowHeapAllocation no_gc;
  int new_nof = containing_map->transitions()->number_of_transitions();
  if (new_nof != nof) {
    DCHECK(new_nof == 0);
    result->Shrink(ToKeyIndex(0));
    result->SetNumberOfTransitions(0);
  } else if (nof == 1) {
    result->NoIncrementalWriteBarrierCopyFrom(
        containing_map->transitions(), kSimpleTransitionIndex, 0);
  }

  result->set_back_pointer_storage(
      containing_map->transitions()->back_pointer_storage());
  return result;
}


Handle<TransitionArray> TransitionArray::Insert(Handle<Map> map,
                                                Handle<Name> name,
                                                Handle<Map> target,
                                                SimpleTransitionFlag flag) {
  if (!map->HasTransitionArray()) {
    return TransitionArray::NewWith(map, name, target, flag);
  }

  int number_of_transitions = map->transitions()->number_of_transitions();
  int new_nof = number_of_transitions;

  bool is_special_transition = flag == SPECIAL_TRANSITION;
  DCHECK_EQ(is_special_transition, IsSpecialTransition(*name));
  PropertyDetails details = is_special_transition
                                ? PropertyDetails(NONE, DATA, 0)
                                : GetTargetDetails(*name, *target);

  int insertion_index = kNotFound;
  int index =
      is_special_transition
          ? map->transitions()->SearchSpecial(Symbol::cast(*name),
                                              &insertion_index)
          : map->transitions()->Search(details.kind(), *name,
                                       details.attributes(), &insertion_index);
  if (index == kNotFound) {
    ++new_nof;
  } else {
    insertion_index = index;
  }
  DCHECK(insertion_index >= 0 && insertion_index <= number_of_transitions);

  CHECK(new_nof <= kMaxNumberOfTransitions);

  if (new_nof <= map->transitions()->number_of_transitions_storage()) {
    DisallowHeapAllocation no_gc;
    TransitionArray* array = map->transitions();

    if (index != kNotFound) {
      array->SetTarget(index, *target);
      return handle(array);
    }

    array->SetNumberOfTransitions(new_nof);
    for (index = number_of_transitions; index > insertion_index; --index) {
      Name* key = array->GetKey(index - 1);
      array->SetKey(index, key);
      array->SetTarget(index, array->GetTarget(index - 1));
    }
    array->SetKey(index, *name);
    array->SetTarget(index, *target);
    SLOW_DCHECK(array->IsSortedNoDuplicates());
    return handle(array);
  }

  Handle<TransitionArray> result = Allocate(
      map->GetIsolate(), new_nof,
      Map::SlackForArraySize(number_of_transitions, kMaxNumberOfTransitions));

  // The map's transition array may grown smaller during the allocation above as
  // it was weakly traversed, though it is guaranteed not to disappear. Trim the
  // result copy if needed, and recompute variables.
  DCHECK(map->HasTransitionArray());
  DisallowHeapAllocation no_gc;
  TransitionArray* array = map->transitions();
  if (array->number_of_transitions() != number_of_transitions) {
    DCHECK(array->number_of_transitions() < number_of_transitions);

    number_of_transitions = array->number_of_transitions();
    new_nof = number_of_transitions;

    insertion_index = kNotFound;
    index = is_special_transition ? map->transitions()->SearchSpecial(
                                        Symbol::cast(*name), &insertion_index)
                                  : map->transitions()->Search(
                                        details.kind(), *name,
                                        details.attributes(), &insertion_index);
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
    result->NoIncrementalWriteBarrierCopyFrom(array, i, i);
  }
  result->NoIncrementalWriteBarrierSet(insertion_index, *name, *target);
  for (int i = insertion_index; i < number_of_transitions; ++i) {
    result->NoIncrementalWriteBarrierCopyFrom(array, i, i + 1);
  }

  result->set_back_pointer_storage(array->back_pointer_storage());
  SLOW_DCHECK(result->IsSortedNoDuplicates());
  return result;
}


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
  if (transition == kNotFound) {
    return kNotFound;
  }
  return SearchDetails(transition, kind, attributes, out_insertion_index);
}
} }  // namespace v8::internal
