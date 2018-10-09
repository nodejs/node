// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arguments-inl.h"
#include "src/code-stubs.h"
#include "src/conversions-inl.h"
#include "src/debug/debug.h"
#include "src/elements.h"
#include "src/heap/factory.h"
#include "src/isolate-inl.h"
#include "src/keys.h"
#include "src/messages.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/prototype.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_TransitionElementsKind) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Map, to_map, 1);
  ElementsKind to_kind = to_map->elements_kind();
  ElementsAccessor::ForKind(to_kind)->TransitionElementsKind(object, to_map);
  return *object;
}

namespace {
// Find the next free position. undefined and holes are both considered
// free spots. Returns "Nothing" if an exception occurred.
V8_WARN_UNUSED_RESULT
Maybe<uint32_t> FindNextFreePosition(Isolate* isolate,
                                     Handle<JSReceiver> receiver,
                                     uint32_t current_pos) {
  for (uint32_t position = current_pos;; ++position) {
    Maybe<bool> has_element = JSReceiver::HasElement(receiver, position);
    MAYBE_RETURN(has_element, Nothing<uint32_t>());
    if (!has_element.FromJust()) return Just(position);

    Handle<Object> element;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, element, JSReceiver::GetElement(isolate, receiver, position),
        Nothing<uint32_t>());
    if (element->IsUndefined(isolate)) return Just(position);
  }
}

// As RemoveArrayHoles, but also handles Dictionary elements that stay
// Dictionary (requires_slow_elements() is true), proxies and objects that
// might have accessors.
V8_WARN_UNUSED_RESULT
Object* RemoveArrayHolesGeneric(Isolate* isolate, Handle<JSReceiver> receiver,
                                uint32_t limit) {
  HandleScope scope(isolate);

  // For proxies, we do not collect the keys, instead we use all indices in
  // the full range of [0, limit).
  Handle<FixedArray> keys;
  if (!receiver->IsJSProxy()) {
    keys = JSReceiver::GetOwnElementIndices(isolate, receiver,
                                            Handle<JSObject>::cast(receiver));
  }

  uint32_t num_undefined = 0;
  uint32_t current_pos = 0;
  int num_indices = keys.is_null() ? limit : keys->length();

  // Compact keys with undefined values and moves non-undefined
  // values to the front.
  // The loop does two things simultaneously:
  //   (1) Count the number of 'undefined', i.e.
  //       i.e.: HasProperty(receiver, key) && Get(receiver, key) == undefined
  //   (2) Move all non-undefined values to the front. The variable current_pos
  //       is used to track free spots in the array starting at the beginning.
  //       Holes and 'undefined' are considered free spots.
  //       A hole is when HasElement(receiver, key) is false.
  for (int i = 0; i < num_indices; ++i) {
    uint32_t key = keys.is_null() ? i : NumberToUint32(keys->get(i));

    // We only care about array indices that are smaller than the limit.
    // The keys are sorted, so we can break as soon as we encounter the first.
    if (key >= limit) break;

    Maybe<bool> has_element = JSReceiver::HasElement(receiver, key);
    MAYBE_RETURN(has_element, ReadOnlyRoots(isolate).exception());
    if (!has_element.FromJust()) {
      continue;
    }

    Handle<Object> element;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, element, JSReceiver::GetElement(isolate, receiver, key));

    if (element->IsUndefined(isolate)) {
      ++num_undefined;
    } else {
      // Find next free position to move elements to.
      Maybe<uint32_t> free_position =
          FindNextFreePosition(isolate, receiver, current_pos);
      MAYBE_RETURN(free_position, ReadOnlyRoots(isolate).exception());
      current_pos = free_position.FromJust();

      // Do not move elements that are already in the "packed" area.
      if (key <= current_pos) continue;

      // array[current_pos] = array[key].
      // Deleting array[key] is done later. This is to preserve the same
      // semantics as the old JS implementation when working with non-extensible
      // objects:
      // If the array contains undefineds, the position at 'key' might later
      // bet set to 'undefined'. If we delete the element now and later set it
      // to undefined, the set operation would throw an exception.
      RETURN_FAILURE_ON_EXCEPTION(
          isolate, JSReceiver::SetElement(isolate, receiver, current_pos,
                                          element, LanguageMode::kStrict));
      ++current_pos;
    }
  }

  // Set [current_pos, current_pos + num_undefined) to undefined.
  uint32_t result = current_pos;
  for (uint32_t i = 0; i < num_undefined; ++i) {
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, JSReceiver::SetElement(isolate, receiver, current_pos++,
                                        isolate->factory()->undefined_value(),
                                        LanguageMode::kStrict));
  }
  // TODO(szuend): Re-enable when we also copy from the prototype chain for
  //               JSArrays. Then we can use HasOwnProperty instead of
  //               HasElement and this condition will hold.
  // DCHECK_LE(current_pos, num_indices);

  // Deleting everything after the undefineds up unto the limit.
  for (int i = num_indices - 1; i >= 0; --i) {
    uint32_t key = keys.is_null() ? i : NumberToUint32(keys->get(i));
    if (key < current_pos) break;
    if (key >= limit) continue;

    Maybe<bool> delete_result = JSReceiver::DeleteElement(receiver, key);
    MAYBE_RETURN(delete_result, ReadOnlyRoots(isolate).exception());
  }

  return *isolate->factory()->NewNumberFromUint(result);
}

// Collects all defined (non-hole) and non-undefined (array) elements at the
// start of the elements array.  If the object is in dictionary mode, it is
// converted to fast elements mode.  Undefined values are placed after
// non-undefined values.  Returns the number of non-undefined values.
V8_WARN_UNUSED_RESULT
Object* RemoveArrayHoles(Isolate* isolate, Handle<JSReceiver> receiver,
                         uint32_t limit) {
  if (receiver->IsJSProxy()) {
    return RemoveArrayHolesGeneric(isolate, receiver, limit);
  }

  Handle<JSObject> object = Handle<JSObject>::cast(receiver);
  if (object->HasStringWrapperElements()) {
    int len = String::cast(Handle<JSValue>::cast(object)->value())->length();
    return Smi::FromInt(len);
  }

  if (object->HasSloppyArgumentsElements() || !object->map()->is_extensible()) {
    return RemoveArrayHolesGeneric(isolate, receiver, limit);
  }

  JSObject::ValidateElements(*object);
  if (object->HasDictionaryElements()) {
    // Convert to fast elements containing only the existing properties.
    // Ordering is irrelevant, since we are going to sort anyway.
    Handle<NumberDictionary> dict(object->element_dictionary(), isolate);
    if (object->IsJSArray() || dict->requires_slow_elements() ||
        dict->max_number_key() >= limit) {
      return RemoveArrayHolesGeneric(isolate, receiver, limit);
    }
    // Convert to fast elements.
    Handle<Map> new_map =
        JSObject::GetElementsTransitionMap(object, HOLEY_ELEMENTS);

    PretenureFlag tenure = Heap::InNewSpace(*object) ? NOT_TENURED : TENURED;
    Handle<FixedArray> fast_elements =
        isolate->factory()->NewFixedArray(dict->NumberOfElements(), tenure);
    dict->CopyValuesTo(*fast_elements);

    JSObject::SetMapAndElements(object, new_map, fast_elements);
    JSObject::ValidateElements(*object);
  } else if (object->HasFixedTypedArrayElements()) {
    // Typed arrays cannot have holes or undefined elements.
    int array_length = FixedArrayBase::cast(object->elements())->length();
    return Smi::FromInt(Min(limit, static_cast<uint32_t>(array_length)));
  } else if (!object->HasDoubleElements()) {
    JSObject::EnsureWritableFastElements(object);
  }
  DCHECK(object->HasSmiOrObjectElements() || object->HasDoubleElements());

  // Collect holes at the end, undefined before that and the rest at the
  // start, and return the number of non-hole, non-undefined values.

  Handle<FixedArrayBase> elements_base(object->elements(), isolate);
  uint32_t elements_length = static_cast<uint32_t>(elements_base->length());
  if (limit > elements_length) {
    limit = elements_length;
  }
  if (limit == 0) {
    return Smi::kZero;
  }

  uint32_t result = 0;
  if (elements_base->map() == ReadOnlyRoots(isolate).fixed_double_array_map()) {
    FixedDoubleArray* elements = FixedDoubleArray::cast(*elements_base);
    // Split elements into defined and the_hole, in that order.
    unsigned int holes = limit;
    // Assume most arrays contain no holes and undefined values, so minimize the
    // number of stores of non-undefined, non-the-hole values.
    for (unsigned int i = 0; i < holes; i++) {
      if (elements->is_the_hole(i)) {
        holes--;
      } else {
        continue;
      }
      // Position i needs to be filled.
      while (holes > i) {
        if (elements->is_the_hole(holes)) {
          holes--;
        } else {
          elements->set(i, elements->get_scalar(holes));
          break;
        }
      }
    }
    result = holes;
    while (holes < limit) {
      elements->set_the_hole(holes);
      holes++;
    }
  } else {
    FixedArray* elements = FixedArray::cast(*elements_base);
    DisallowHeapAllocation no_gc;

    // Split elements into defined, undefined and the_hole, in that order.  Only
    // count locations for undefined and the hole, and fill them afterwards.
    WriteBarrierMode write_barrier = elements->GetWriteBarrierMode(no_gc);
    unsigned int undefs = limit;
    unsigned int holes = limit;
    // Assume most arrays contain no holes and undefined values, so minimize the
    // number of stores of non-undefined, non-the-hole values.
    for (unsigned int i = 0; i < undefs; i++) {
      Object* current = elements->get(i);
      if (current->IsTheHole(isolate)) {
        holes--;
        undefs--;
      } else if (current->IsUndefined(isolate)) {
        undefs--;
      } else {
        continue;
      }
      // Position i needs to be filled.
      while (undefs > i) {
        current = elements->get(undefs);
        if (current->IsTheHole(isolate)) {
          holes--;
          undefs--;
        } else if (current->IsUndefined(isolate)) {
          undefs--;
        } else {
          elements->set(i, current, write_barrier);
          break;
        }
      }
    }
    result = undefs;
    while (undefs < holes) {
      elements->set_undefined(isolate, undefs);
      undefs++;
    }
    while (holes < limit) {
      elements->set_the_hole(isolate, holes);
      holes++;
    }
  }

  return *isolate->factory()->NewNumberFromUint(result);
}

// Copy element at index from source to target only if target does not have the
// element on its own. Returns true if a copy occurred, false if not
// and Nothing if an exception occurred.
V8_WARN_UNUSED_RESULT
Maybe<bool> ConditionalCopy(Isolate* isolate, Handle<JSReceiver> source,
                            Handle<JSReceiver> target, uint32_t index) {
  Maybe<bool> source_has_prop = JSReceiver::HasOwnProperty(source, index);
  MAYBE_RETURN(source_has_prop, Nothing<bool>());
  if (!source_has_prop.FromJust()) return Just(false);

  Maybe<bool> target_has_prop = JSReceiver::HasOwnProperty(target, index);
  MAYBE_RETURN(target_has_prop, Nothing<bool>());
  if (target_has_prop.FromJust()) return Just(false);

  Handle<Object> source_element;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, source_element, JSReceiver::GetElement(isolate, source, index),
      Nothing<bool>());

  Handle<Object> set_result;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, set_result,
      JSReceiver::SetElement(isolate, target, index, source_element,
                             LanguageMode::kStrict),
      Nothing<bool>());

  return Just(true);
}

// Copy elements in the range 0..length from objects prototype chain
// to object itself, if object has holes. Returns null on error and undefined on
// success.
V8_WARN_UNUSED_RESULT
MaybeHandle<Object> CopyFromPrototype(Isolate* isolate,
                                      Handle<JSReceiver> object,
                                      uint32_t length) {
  for (PrototypeIterator iter(isolate, object, kStartAtPrototype);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<JSReceiver> current(PrototypeIterator::GetCurrent<JSReceiver>(iter));

    if (current->IsJSProxy()) {
      for (uint32_t i = 0; i < length; ++i) {
        MAYBE_RETURN_NULL(ConditionalCopy(isolate, current, object, i));
      }
    } else {
      Handle<FixedArray> keys = JSReceiver::GetOwnElementIndices(
          isolate, object, Handle<JSObject>::cast(current));

      uint32_t num_indices = keys->length();
      for (uint32_t i = 0; i < num_indices; ++i) {
        uint32_t idx = NumberToUint32(keys->get(i));

        // Prototype might have indices that go past length, but we are only
        // interested in the range [0, length).
        if (idx >= length) break;

        MAYBE_RETURN_NULL(ConditionalCopy(isolate, current, object, idx));
      }
    }
  }
  return isolate->factory()->undefined_value();
}

}  // namespace

RUNTIME_FUNCTION(Runtime_PrepareElementsForSort) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, length, Uint32, args[1]);

  if (isolate->debug_execution_mode() == DebugInfo::kSideEffects) {
    if (!isolate->debug()->PerformSideEffectCheckForObject(object)) {
      return ReadOnlyRoots(isolate).exception();
    }
  }

  // Counter for sorting arrays that have non-packed elements and where either
  // the ElementsProtector is invalid or the prototype does not match
  // Array.prototype.
  if (object->IsJSArray() &&
      !Handle<JSArray>::cast(object)->HasFastPackedElements()) {
    JSObject* initial_array_proto = JSObject::cast(
        isolate->native_context()->get(Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
    if (!isolate->IsNoElementsProtectorIntact() ||
        object->map()->prototype() != initial_array_proto) {
      isolate->CountUsage(
          v8::Isolate::kArrayPrototypeSortJSArrayModifiedPrototype);
    }
  }

  if (!object->IsJSArray()) {
    RETURN_FAILURE_ON_EXCEPTION(isolate,
                                CopyFromPrototype(isolate, object, length));
  }
  return RemoveArrayHoles(isolate, object, length);
}

// Move contents of argument 0 (an array) to argument 1 (an array)
RUNTIME_FUNCTION(Runtime_MoveArrayContents) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSArray, from, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, to, 1);
  JSObject::ValidateElements(*from);
  JSObject::ValidateElements(*to);

  Handle<FixedArrayBase> new_elements(from->elements(), isolate);
  ElementsKind from_kind = from->GetElementsKind();
  Handle<Map> new_map = JSObject::GetElementsTransitionMap(to, from_kind);
  JSObject::SetMapAndElements(to, new_map, new_elements);
  to->set_length(from->length());

  from->initialize_elements();
  from->set_length(Smi::kZero);

  JSObject::ValidateElements(*to);
  return *to;
}


// How many elements does this object/array have?
RUNTIME_FUNCTION(Runtime_EstimateNumberOfElements) {
  DisallowHeapAllocation no_gc;
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSArray, array, 0);
  FixedArrayBase* elements = array->elements();
  SealHandleScope shs(isolate);
  if (elements->IsNumberDictionary()) {
    int result = NumberDictionary::cast(elements)->NumberOfElements();
    return Smi::FromInt(result);
  } else {
    DCHECK(array->length()->IsSmi());
    // For packed elements, we know the exact number of elements
    int length = elements->length();
    ElementsKind kind = array->GetElementsKind();
    if (IsFastPackedElementsKind(kind)) {
      return Smi::FromInt(length);
    }
    // For holey elements, take samples from the buffer checking for holes
    // to generate the estimate.
    const int kNumberOfHoleCheckSamples = 97;
    int increment = (length < kNumberOfHoleCheckSamples)
                        ? 1
                        : static_cast<int>(length / kNumberOfHoleCheckSamples);
    ElementsAccessor* accessor = array->GetElementsAccessor();
    int holes = 0;
    for (int i = 0; i < length; i += increment) {
      if (!accessor->HasElement(array, i, elements)) {
        ++holes;
      }
    }
    int estimate = static_cast<int>((kNumberOfHoleCheckSamples - holes) /
                                    kNumberOfHoleCheckSamples * length);
    return Smi::FromInt(estimate);
  }
}


// Returns an array that tells you where in the [0, length) interval an array
// might have elements.  Can either return an array of keys (positive integers
// or undefined) or a number representing the positive length of an interval
// starting at index 0.
// Intervals can span over some keys that are not in the object.
RUNTIME_FUNCTION(Runtime_GetArrayKeys) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, length, Uint32, args[1]);
  ElementsKind kind = array->GetElementsKind();

  if (IsFastElementsKind(kind) || IsFixedTypedArrayElementsKind(kind)) {
    uint32_t actual_length = static_cast<uint32_t>(array->elements()->length());
    return *isolate->factory()->NewNumberFromUint(Min(actual_length, length));
  }

  if (kind == FAST_STRING_WRAPPER_ELEMENTS) {
    int string_length =
        String::cast(Handle<JSValue>::cast(array)->value())->length();
    int backing_store_length = array->elements()->length();
    return *isolate->factory()->NewNumberFromUint(
        Min(length,
            static_cast<uint32_t>(Max(string_length, backing_store_length))));
  }

  KeyAccumulator accumulator(isolate, KeyCollectionMode::kOwnOnly,
                             ALL_PROPERTIES);
  for (PrototypeIterator iter(isolate, array, kStartAtReceiver);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<JSReceiver> current(PrototypeIterator::GetCurrent<JSReceiver>(iter));
    if (current->HasComplexElements()) {
      return *isolate->factory()->NewNumberFromUint(length);
    }
    accumulator.CollectOwnElementIndices(array,
                                         Handle<JSObject>::cast(current));
  }
  // Erase any keys >= length.
  Handle<FixedArray> keys =
      accumulator.GetKeys(GetKeysConversion::kKeepNumbers);
  int j = 0;
  for (int i = 0; i < keys->length(); i++) {
    if (NumberToUint32(keys->get(i)) >= length) continue;
    if (i != j) keys->set(j, keys->get(i));
    j++;
  }

  keys = FixedArray::ShrinkOrEmpty(isolate, keys, j);
  return *isolate->factory()->NewJSArrayWithElements(keys);
}

RUNTIME_FUNCTION(Runtime_TrySliceSimpleNonFastElements) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_SMI_ARG_CHECKED(first, 1);
  CONVERT_SMI_ARG_CHECKED(count, 2);
  uint32_t length = first + count;

  // Only handle elements kinds that have a ElementsAccessor Slice
  // implementation.
  if (receiver->IsJSArray()) {
    // This "fastish" path must make sure the destination array is a JSArray.
    if (!isolate->IsArraySpeciesLookupChainIntact() ||
        !JSArray::cast(*receiver)->HasArrayPrototype(isolate)) {
      return Smi::FromInt(0);
    }
  } else {
    int len;
    if (!receiver->IsJSObject() ||
        !JSSloppyArgumentsObject::GetSloppyArgumentsLength(
            isolate, Handle<JSObject>::cast(receiver), &len) ||
        (length > static_cast<uint32_t>(len))) {
      return Smi::FromInt(0);
    }
  }

  // This "fastish" path must also ensure that elements are simple (no
  // geters/setters), no elements on prototype chain.
  Handle<JSObject> object(Handle<JSObject>::cast(receiver));
  if (!JSObject::PrototypeHasNoElements(isolate, *object) ||
      object->HasComplexElements()) {
    return Smi::FromInt(0);
  }

  ElementsAccessor* accessor = object->GetElementsAccessor();
  return *accessor->Slice(object, first, length);
}

RUNTIME_FUNCTION(Runtime_NewArray) {
  HandleScope scope(isolate);
  DCHECK_LE(3, args.length());
  int const argc = args.length() - 3;
  // TODO(bmeurer): Remove this Arguments nonsense.
  Arguments argv(argc, args.arguments() - 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, new_target, argc + 1);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, type_info, argc + 2);
  // TODO(bmeurer): Use MaybeHandle to pass around the AllocationSite.
  Handle<AllocationSite> site = type_info->IsAllocationSite()
                                    ? Handle<AllocationSite>::cast(type_info)
                                    : Handle<AllocationSite>::null();

  Factory* factory = isolate->factory();

  // If called through new, new.target can be:
  // - a subclass of constructor,
  // - a proxy wrapper around constructor, or
  // - the constructor itself.
  // If called through Reflect.construct, it's guaranteed to be a constructor by
  // REFLECT_CONSTRUCT_PREPARE.
  DCHECK(new_target->IsConstructor());

  bool holey = false;
  bool can_use_type_feedback = !site.is_null();
  bool can_inline_array_constructor = true;
  if (argv.length() == 1) {
    Handle<Object> argument_one = argv.at<Object>(0);
    if (argument_one->IsSmi()) {
      int value = Handle<Smi>::cast(argument_one)->value();
      if (value < 0 ||
          JSArray::SetLengthWouldNormalize(isolate->heap(), value)) {
        // the array is a dictionary in this case.
        can_use_type_feedback = false;
      } else if (value != 0) {
        holey = true;
        if (value >= JSArray::kInitialMaxFastElementArray) {
          can_inline_array_constructor = false;
        }
      }
    } else {
      // Non-smi length argument produces a dictionary
      can_use_type_feedback = false;
    }
  }

  Handle<Map> initial_map;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, initial_map,
      JSFunction::GetDerivedMap(isolate, constructor, new_target));

  ElementsKind to_kind = can_use_type_feedback ? site->GetElementsKind()
                                               : initial_map->elements_kind();
  if (holey && !IsHoleyElementsKind(to_kind)) {
    to_kind = GetHoleyElementsKind(to_kind);
    // Update the allocation site info to reflect the advice alteration.
    if (!site.is_null()) site->SetElementsKind(to_kind);
  }

  // We should allocate with an initial map that reflects the allocation site
  // advice. Therefore we use AllocateJSObjectFromMap instead of passing
  // the constructor.
  initial_map = Map::AsElementsKind(isolate, initial_map, to_kind);

  // If we don't care to track arrays of to_kind ElementsKind, then
  // don't emit a memento for them.
  Handle<AllocationSite> allocation_site;
  if (AllocationSite::ShouldTrack(to_kind)) {
    allocation_site = site;
  }

  Handle<JSArray> array = Handle<JSArray>::cast(
      factory->NewJSObjectFromMap(initial_map, NOT_TENURED, allocation_site));

  factory->NewJSArrayStorage(array, 0, 0, DONT_INITIALIZE_ARRAY_ELEMENTS);

  ElementsKind old_kind = array->GetElementsKind();
  RETURN_FAILURE_ON_EXCEPTION(isolate,
                              ArrayConstructInitializeElements(array, &argv));
  if (!site.is_null()) {
    if ((old_kind != array->GetElementsKind() || !can_use_type_feedback ||
         !can_inline_array_constructor)) {
      // The arguments passed in caused a transition. This kind of complexity
      // can't be dealt with in the inlined optimized array constructor case.
      // We must mark the allocationsite as un-inlinable.
      site->SetDoNotInlineCall();
    }
  } else {
    if (old_kind != array->GetElementsKind() || !can_inline_array_constructor) {
      // We don't have an AllocationSite for this Array constructor invocation,
      // i.e. it might a call from Array#map or from an Array subclass, so we
      // just flip the bit on the global protector cell instead.
      // TODO(bmeurer): Find a better way to mark this. Global protectors
      // tend to back-fire over time...
      if (isolate->IsArrayConstructorIntact()) {
        isolate->InvalidateArrayConstructorProtector();
      }
    }
  }

  return *array;
}

RUNTIME_FUNCTION(Runtime_NormalizeElements) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  CHECK(!array->HasFixedTypedArrayElements());
  CHECK(!array->IsJSGlobalProxy());
  JSObject::NormalizeElements(array);
  return *array;
}

// GrowArrayElements returns a sentinel Smi if the object was normalized or if
// the key is negative.
RUNTIME_FUNCTION(Runtime_GrowArrayElements) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_NUMBER_CHECKED(int, key, Int32, args[1]);

  if (key < 0) return Smi::kZero;

  uint32_t capacity = static_cast<uint32_t>(object->elements()->length());
  uint32_t index = static_cast<uint32_t>(key);

  if (index >= capacity) {
    if (!object->GetElementsAccessor()->GrowCapacity(object, index)) {
      return Smi::kZero;
    }
  }

  return object->elements();
}


RUNTIME_FUNCTION(Runtime_HasComplexElements) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  for (PrototypeIterator iter(isolate, array, kStartAtReceiver);
       !iter.IsAtEnd(); iter.Advance()) {
    if (PrototypeIterator::GetCurrent<JSReceiver>(iter)->HasComplexElements()) {
      return ReadOnlyRoots(isolate).true_value();
    }
  }
  return ReadOnlyRoots(isolate).false_value();
}

// ES6 22.1.2.2 Array.isArray
RUNTIME_FUNCTION(Runtime_ArrayIsArray) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  Maybe<bool> result = Object::IsArray(object);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_IsArray) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSArray());
}

RUNTIME_FUNCTION(Runtime_ArraySpeciesConstructor) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, original_array, 0);
  RETURN_RESULT_OR_FAILURE(
      isolate, Object::ArraySpeciesConstructor(isolate, original_array));
}

// ES7 22.1.3.11 Array.prototype.includes
RUNTIME_FUNCTION(Runtime_ArrayIncludes_Slow) {
  HandleScope shs(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, search_element, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, from_index, 2);

  // Let O be ? ToObject(this value).
  Handle<JSReceiver> object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, object, Object::ToObject(isolate, handle(args[0], isolate)));

  // Let len be ? ToLength(? Get(O, "length")).
  int64_t len;
  {
    if (object->map()->instance_type() == JS_ARRAY_TYPE) {
      uint32_t len32 = 0;
      bool success = JSArray::cast(*object)->length()->ToArrayLength(&len32);
      DCHECK(success);
      USE(success);
      len = len32;
    } else {
      Handle<Object> len_;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, len_,
          Object::GetProperty(isolate, object,
                              isolate->factory()->length_string()));

      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, len_,
                                         Object::ToLength(isolate, len_));
      len = static_cast<int64_t>(len_->Number());
      DCHECK_EQ(len, len_->Number());
    }
  }

  if (len == 0) return ReadOnlyRoots(isolate).false_value();

  // Let n be ? ToInteger(fromIndex). (If fromIndex is undefined, this step
  // produces the value 0.)
  int64_t index = 0;
  if (!from_index->IsUndefined(isolate)) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, from_index,
                                       Object::ToInteger(isolate, from_index));

    if (V8_LIKELY(from_index->IsSmi())) {
      int start_from = Smi::ToInt(*from_index);
      if (start_from < 0) {
        index = std::max<int64_t>(len + start_from, 0);
      } else {
        index = start_from;
      }
    } else {
      DCHECK(from_index->IsHeapNumber());
      double start_from = from_index->Number();
      if (start_from >= len) return ReadOnlyRoots(isolate).false_value();
      if (V8_LIKELY(std::isfinite(start_from))) {
        if (start_from < 0) {
          index = static_cast<int64_t>(std::max<double>(start_from + len, 0));
        } else {
          index = start_from;
        }
      }
    }

    DCHECK_GE(index, 0);
  }

  // If the receiver is not a special receiver type, and the length is a valid
  // element index, perform fast operation tailored to specific ElementsKinds.
  if (!object->map()->IsSpecialReceiverMap() && len < kMaxUInt32 &&
      JSObject::PrototypeHasNoElements(isolate, JSObject::cast(*object))) {
    Handle<JSObject> obj = Handle<JSObject>::cast(object);
    ElementsAccessor* elements = obj->GetElementsAccessor();
    Maybe<bool> result = elements->IncludesValue(isolate, obj, search_element,
                                                 static_cast<uint32_t>(index),
                                                 static_cast<uint32_t>(len));
    MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
    return *isolate->factory()->ToBoolean(result.FromJust());
  }

  // Otherwise, perform slow lookups for special receiver types
  for (; index < len; ++index) {
    // Let elementK be the result of ? Get(O, ! ToString(k)).
    Handle<Object> element_k;
    {
      Handle<Object> index_obj = isolate->factory()->NewNumberFromInt64(index);
      bool success;
      LookupIterator it = LookupIterator::PropertyOrElement(
          isolate, object, index_obj, &success);
      DCHECK(success);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, element_k,
                                         Object::GetProperty(&it));
    }

    // If SameValueZero(searchElement, elementK) is true, return true.
    if (search_element->SameValueZero(*element_k)) {
      return ReadOnlyRoots(isolate).true_value();
    }
  }
  return ReadOnlyRoots(isolate).false_value();
}

RUNTIME_FUNCTION(Runtime_ArrayIndexOf) {
  HandleScope shs(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, search_element, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, from_index, 2);

  // Let O be ? ToObject(this value).
  Handle<JSReceiver> object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, object,
      Object::ToObject(isolate, args.at(0), "Array.prototype.indexOf"));

  // Let len be ? ToLength(? Get(O, "length")).
  int64_t len;
  {
    if (object->IsJSArray()) {
      uint32_t len32 = 0;
      bool success = JSArray::cast(*object)->length()->ToArrayLength(&len32);
      DCHECK(success);
      USE(success);
      len = len32;
    } else {
      Handle<Object> len_;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, len_,
          Object::GetProperty(isolate, object,
                              isolate->factory()->length_string()));

      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, len_,
                                         Object::ToLength(isolate, len_));
      len = static_cast<int64_t>(len_->Number());
      DCHECK_EQ(len, len_->Number());
    }
  }

  if (len == 0) return Smi::FromInt(-1);

  // Let n be ? ToInteger(fromIndex). (If fromIndex is undefined, this step
  // produces the value 0.)
  int64_t start_from;
  {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, from_index,
                                       Object::ToInteger(isolate, from_index));
    double fp = from_index->Number();
    if (fp > len) return Smi::FromInt(-1);
    if (V8_LIKELY(fp >=
                  static_cast<double>(std::numeric_limits<int64_t>::min()))) {
      DCHECK(fp < std::numeric_limits<int64_t>::max());
      start_from = static_cast<int64_t>(fp);
    } else {
      start_from = std::numeric_limits<int64_t>::min();
    }
  }

  int64_t index;
  if (start_from >= 0) {
    index = start_from;
  } else {
    index = len + start_from;
    if (index < 0) {
      index = 0;
    }
  }

  // If the receiver is not a special receiver type, and the length is a valid
  // element index, perform fast operation tailored to specific ElementsKinds.
  if (!object->map()->IsSpecialReceiverMap() && len < kMaxUInt32 &&
      JSObject::PrototypeHasNoElements(isolate, JSObject::cast(*object))) {
    Handle<JSObject> obj = Handle<JSObject>::cast(object);
    ElementsAccessor* elements = obj->GetElementsAccessor();
    Maybe<int64_t> result = elements->IndexOfValue(isolate, obj, search_element,
                                                   static_cast<uint32_t>(index),
                                                   static_cast<uint32_t>(len));
    MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
    return *isolate->factory()->NewNumberFromInt64(result.FromJust());
  }

  // Otherwise, perform slow lookups for special receiver types
  for (; index < len; ++index) {
    // Let elementK be the result of ? Get(O, ! ToString(k)).
    Handle<Object> element_k;
    {
      Handle<Object> index_obj = isolate->factory()->NewNumberFromInt64(index);
      bool success;
      LookupIterator it = LookupIterator::PropertyOrElement(
          isolate, object, index_obj, &success);
      DCHECK(success);
      Maybe<bool> present = JSReceiver::HasProperty(&it);
      MAYBE_RETURN(present, ReadOnlyRoots(isolate).exception());
      if (!present.FromJust()) continue;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, element_k,
                                         Object::GetProperty(&it));
      if (search_element->StrictEquals(*element_k)) {
        return *index_obj;
      }
    }
  }
  return Smi::FromInt(-1);
}

}  // namespace internal
}  // namespace v8
