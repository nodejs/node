// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8.h"

#include "objects.h"
#include "transitions-inl.h"
#include "utils.h"

namespace v8 {
namespace internal {


Handle<TransitionArray> TransitionArray::Allocate(Isolate* isolate,
                                                  int number_of_transitions) {
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(ToKeyIndex(number_of_transitions));
  array->set(kPrototypeTransitionsIndex, Smi::FromInt(0));
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


static bool InsertionPointFound(Name* key1, Name* key2) {
  return key1->Hash() > key2->Hash();
}


Handle<TransitionArray> TransitionArray::NewWith(Handle<Map> map,
                                                 Handle<Name> name,
                                                 Handle<Map> target,
                                                 SimpleTransitionFlag flag) {
  Handle<TransitionArray> result;
  Isolate* isolate = name->GetIsolate();

  if (flag == SIMPLE_TRANSITION) {
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
  ASSERT(!containing_map->transitions()->IsFullTransitionArray());
  int nof = containing_map->transitions()->number_of_transitions();

  // A transition array may shrink during GC.
  Handle<TransitionArray> result = Allocate(containing_map->GetIsolate(), nof);
  DisallowHeapAllocation no_gc;
  int new_nof = containing_map->transitions()->number_of_transitions();
  if (new_nof != nof) {
    ASSERT(new_nof == 0);
    result->Shrink(ToKeyIndex(0));
  } else if (nof == 1) {
    result->NoIncrementalWriteBarrierCopyFrom(
        containing_map->transitions(), kSimpleTransitionIndex, 0);
  }

  result->set_back_pointer_storage(
      containing_map->transitions()->back_pointer_storage());
  return result;
}


Handle<TransitionArray> TransitionArray::CopyInsert(Handle<Map> map,
                                                    Handle<Name> name,
                                                    Handle<Map> target,
                                                    SimpleTransitionFlag flag) {
  if (!map->HasTransitionArray()) {
    return TransitionArray::NewWith(map, name, target, flag);
  }

  int number_of_transitions = map->transitions()->number_of_transitions();
  int new_size = number_of_transitions;

  int insertion_index = map->transitions()->Search(*name);
  if (insertion_index == kNotFound) ++new_size;

  Handle<TransitionArray> result = Allocate(map->GetIsolate(), new_size);

  // The map's transition array may grown smaller during the allocation above as
  // it was weakly traversed, though it is guaranteed not to disappear. Trim the
  // result copy if needed, and recompute variables.
  ASSERT(map->HasTransitionArray());
  DisallowHeapAllocation no_gc;
  TransitionArray* array = map->transitions();
  if (array->number_of_transitions() != number_of_transitions) {
    ASSERT(array->number_of_transitions() < number_of_transitions);

    number_of_transitions = array->number_of_transitions();
    new_size = number_of_transitions;

    insertion_index = array->Search(*name);
    if (insertion_index == kNotFound) ++new_size;

    result->Shrink(ToKeyIndex(new_size));
  }

  if (array->HasPrototypeTransitions()) {
    result->SetPrototypeTransitions(array->GetPrototypeTransitions());
  }

  if (insertion_index != kNotFound) {
    for (int i = 0; i < number_of_transitions; ++i) {
      if (i != insertion_index) {
        result->NoIncrementalWriteBarrierCopyFrom(array, i, i);
      }
    }
    result->NoIncrementalWriteBarrierSet(insertion_index, *name, *target);
    result->set_back_pointer_storage(array->back_pointer_storage());
    return result;
  }

  insertion_index = 0;
  for (; insertion_index < number_of_transitions; ++insertion_index) {
    if (InsertionPointFound(array->GetKey(insertion_index), *name)) break;
    result->NoIncrementalWriteBarrierCopyFrom(
        array, insertion_index, insertion_index);
  }

  result->NoIncrementalWriteBarrierSet(insertion_index, *name, *target);

  for (; insertion_index < number_of_transitions; ++insertion_index) {
    result->NoIncrementalWriteBarrierCopyFrom(
        array, insertion_index, insertion_index + 1);
  }

  result->set_back_pointer_storage(array->back_pointer_storage());
  return result;
}


} }  // namespace v8::internal
