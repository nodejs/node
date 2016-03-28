// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/conversions-inl.h"
#include "src/elements.h"
#include "src/factory.h"
#include "src/isolate-inl.h"
#include "src/key-accumulator.h"
#include "src/messages.h"
#include "src/prototype.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_FinishArrayPrototypeSetup) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, prototype, 0);
  Object* length = prototype->length();
  RUNTIME_ASSERT(length->IsSmi() && Smi::cast(length)->value() == 0);
  RUNTIME_ASSERT(prototype->HasFastSmiOrObjectElements());
  // This is necessary to enable fast checks for absence of elements
  // on Array.prototype and below.
  prototype->set_elements(isolate->heap()->empty_fixed_array());
  return Smi::FromInt(0);
}


static void InstallBuiltin(Isolate* isolate, Handle<JSObject> holder,
                           const char* name, Builtins::Name builtin_name) {
  Handle<String> key = isolate->factory()->InternalizeUtf8String(name);
  Handle<Code> code(isolate->builtins()->builtin(builtin_name));
  Handle<JSFunction> optimized =
      isolate->factory()->NewFunctionWithoutPrototype(key, code);
  optimized->shared()->DontAdaptArguments();
  JSObject::AddProperty(holder, key, optimized, NONE);
}


RUNTIME_FUNCTION(Runtime_SpecialArrayFunctions) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  Handle<JSObject> holder =
      isolate->factory()->NewJSObject(isolate->object_function());

  InstallBuiltin(isolate, holder, "pop", Builtins::kArrayPop);
  InstallBuiltin(isolate, holder, "push", Builtins::kArrayPush);
  InstallBuiltin(isolate, holder, "shift", Builtins::kArrayShift);
  InstallBuiltin(isolate, holder, "unshift", Builtins::kArrayUnshift);
  InstallBuiltin(isolate, holder, "slice", Builtins::kArraySlice);
  InstallBuiltin(isolate, holder, "splice", Builtins::kArraySplice);

  return *holder;
}


RUNTIME_FUNCTION(Runtime_FixedArrayGet) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(FixedArray, object, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  return object->get(index);
}


RUNTIME_FUNCTION(Runtime_FixedArraySet) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_CHECKED(FixedArray, object, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  CONVERT_ARG_CHECKED(Object, value, 2);
  object->set(index, value);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_TransitionElementsKind) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  CONVERT_ARG_HANDLE_CHECKED(Map, map, 1);
  JSObject::TransitionElementsKind(array, map->elements_kind());
  return *array;
}


// Push an object unto an array of objects if it is not already in the
// array.  Returns true if the element was pushed on the stack and
// false otherwise.
RUNTIME_FUNCTION(Runtime_PushIfAbsent) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, element, 1);
  RUNTIME_ASSERT(array->HasFastSmiOrObjectElements());
  int length = Smi::cast(array->length())->value();
  FixedArray* elements = FixedArray::cast(array->elements());
  for (int i = 0; i < length; i++) {
    if (elements->get(i) == *element) return isolate->heap()->false_value();
  }

  // Strict not needed. Used for cycle detection in Array join implementation.
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::AddDataElement(array, length, element, NONE));
  JSObject::ValidateElements(array);
  return isolate->heap()->true_value();
}


// Moves all own elements of an object, that are below a limit, to positions
// starting at zero. All undefined values are placed after non-undefined values,
// and are followed by non-existing element. Does not change the length
// property.
// Returns the number of non-undefined elements collected.
// Returns -1 if hole removal is not supported by this method.
RUNTIME_FUNCTION(Runtime_RemoveArrayHoles) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[1]);
  if (object->IsJSProxy()) return Smi::FromInt(-1);
  return *JSObject::PrepareElementsForSort(Handle<JSObject>::cast(object),
                                           limit);
}


// Move contents of argument 0 (an array) to argument 1 (an array)
RUNTIME_FUNCTION(Runtime_MoveArrayContents) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, from, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, to, 1);
  JSObject::ValidateElements(from);
  JSObject::ValidateElements(to);

  Handle<FixedArrayBase> new_elements(from->elements());
  ElementsKind from_kind = from->GetElementsKind();
  Handle<Map> new_map = JSObject::GetElementsTransitionMap(to, from_kind);
  JSObject::SetMapAndElements(to, new_map, new_elements);
  to->set_length(from->length());

  JSObject::ResetElements(from);
  from->set_length(Smi::FromInt(0));

  JSObject::ValidateElements(to);
  return *to;
}


// How many elements does this object/array have?
RUNTIME_FUNCTION(Runtime_EstimateNumberOfElements) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  Handle<FixedArrayBase> elements(array->elements(), isolate);
  SealHandleScope shs(isolate);
  if (elements->IsDictionary()) {
    int result =
        Handle<SeededNumberDictionary>::cast(elements)->NumberOfElements();
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
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, length, Uint32, args[1]);

  if (array->HasFastStringWrapperElements()) {
    int string_length =
        String::cast(Handle<JSValue>::cast(array)->value())->length();
    int backing_store_length = array->elements()->length();
    return *isolate->factory()->NewNumberFromUint(
        Min(length,
            static_cast<uint32_t>(Max(string_length, backing_store_length))));
  }

  if (!array->elements()->IsDictionary()) {
    RUNTIME_ASSERT(array->HasFastSmiOrObjectElements() ||
                   array->HasFastDoubleElements());
    uint32_t actual_length = static_cast<uint32_t>(array->elements()->length());
    return *isolate->factory()->NewNumberFromUint(Min(actual_length, length));
  }

  KeyAccumulator accumulator(isolate, OWN_ONLY, ALL_PROPERTIES);
  // No need to separate prototype levels since we only get element keys.
  for (PrototypeIterator iter(isolate, array,
                              PrototypeIterator::START_AT_RECEIVER);
       !iter.IsAtEnd(); iter.Advance()) {
    if (PrototypeIterator::GetCurrent(iter)->IsJSProxy() ||
        PrototypeIterator::GetCurrent<JSObject>(iter)
            ->HasIndexedInterceptor()) {
      // Bail out if we find a proxy or interceptor, likely not worth
      // collecting keys in that case.
      return *isolate->factory()->NewNumberFromUint(length);
    }
    accumulator.NextPrototype();
    Handle<JSObject> current = PrototypeIterator::GetCurrent<JSObject>(iter);
    JSObject::CollectOwnElementKeys(current, &accumulator, ALL_PROPERTIES);
  }
  // Erase any keys >= length.
  // TODO(adamk): Remove this step when the contract of %GetArrayKeys
  // is changed to let this happen on the JS side.
  Handle<FixedArray> keys = accumulator.GetKeys(KEEP_NUMBERS);
  for (int i = 0; i < keys->length(); i++) {
    if (NumberToUint32(keys->get(i)) >= length) keys->set_undefined(i);
  }
  return *isolate->factory()->NewJSArrayWithElements(keys);
}


namespace {

Object* ArrayConstructorCommon(Isolate* isolate, Handle<JSFunction> constructor,
                               Handle<JSReceiver> new_target,
                               Handle<AllocationSite> site,
                               Arguments* caller_args) {
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
  if (caller_args->length() == 1) {
    Handle<Object> argument_one = caller_args->at<Object>(0);
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
  if (holey && !IsFastHoleyElementsKind(to_kind)) {
    to_kind = GetHoleyElementsKind(to_kind);
    // Update the allocation site info to reflect the advice alteration.
    if (!site.is_null()) site->SetElementsKind(to_kind);
  }

  // We should allocate with an initial map that reflects the allocation site
  // advice. Therefore we use AllocateJSObjectFromMap instead of passing
  // the constructor.
  if (to_kind != initial_map->elements_kind()) {
    initial_map = Map::AsElementsKind(initial_map, to_kind);
  }

  // If we don't care to track arrays of to_kind ElementsKind, then
  // don't emit a memento for them.
  Handle<AllocationSite> allocation_site;
  if (AllocationSite::GetMode(to_kind) == TRACK_ALLOCATION_SITE) {
    allocation_site = site;
  }

  Handle<JSArray> array = Handle<JSArray>::cast(
      factory->NewJSObjectFromMap(initial_map, NOT_TENURED, allocation_site));

  factory->NewJSArrayStorage(array, 0, 0, DONT_INITIALIZE_ARRAY_ELEMENTS);

  ElementsKind old_kind = array->GetElementsKind();
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, ArrayConstructInitializeElements(array, caller_args));
  if (!site.is_null() &&
      (old_kind != array->GetElementsKind() || !can_use_type_feedback ||
       !can_inline_array_constructor)) {
    // The arguments passed in caused a transition. This kind of complexity
    // can't be dealt with in the inlined hydrogen array constructor case.
    // We must mark the allocationsite as un-inlinable.
    site->SetDoNotInlineCall();
  }

  return *array;
}

}  // namespace


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
  return ArrayConstructorCommon(isolate, constructor, new_target, site, &argv);
}


RUNTIME_FUNCTION(Runtime_ArrayConstructor) {
  HandleScope scope(isolate);
  // If we get 2 arguments then they are the stub parameters (constructor, type
  // info).  If we get 4, then the first one is a pointer to the arguments
  // passed by the caller, and the last one is the length of the arguments
  // passed to the caller (redundant, but useful to check on the deoptimizer
  // with an assert).
  Arguments empty_args(0, NULL);
  bool no_caller_args = args.length() == 2;
  DCHECK(no_caller_args || args.length() == 4);
  int parameters_start = no_caller_args ? 0 : 1;
  Arguments* caller_args =
      no_caller_args ? &empty_args : reinterpret_cast<Arguments*>(args[0]);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, parameters_start);
  CONVERT_ARG_HANDLE_CHECKED(Object, type_info, parameters_start + 1);
#ifdef DEBUG
  if (!no_caller_args) {
    CONVERT_SMI_ARG_CHECKED(arg_count, parameters_start + 2);
    DCHECK(arg_count == caller_args->length());
  }
#endif

  Handle<AllocationSite> site;
  if (!type_info.is_null() &&
      *type_info != isolate->heap()->undefined_value()) {
    site = Handle<AllocationSite>::cast(type_info);
    DCHECK(!site->SitePointsToLiteral());
  }

  return ArrayConstructorCommon(isolate, constructor, constructor, site,
                                caller_args);
}


RUNTIME_FUNCTION(Runtime_InternalArrayConstructor) {
  HandleScope scope(isolate);
  Arguments empty_args(0, NULL);
  bool no_caller_args = args.length() == 1;
  DCHECK(no_caller_args || args.length() == 3);
  int parameters_start = no_caller_args ? 0 : 1;
  Arguments* caller_args =
      no_caller_args ? &empty_args : reinterpret_cast<Arguments*>(args[0]);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, parameters_start);
#ifdef DEBUG
  if (!no_caller_args) {
    CONVERT_SMI_ARG_CHECKED(arg_count, parameters_start + 1);
    DCHECK(arg_count == caller_args->length());
  }
#endif
  return ArrayConstructorCommon(isolate, constructor, constructor,
                                Handle<AllocationSite>::null(), caller_args);
}


RUNTIME_FUNCTION(Runtime_NormalizeElements) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  RUNTIME_ASSERT(!array->HasFixedTypedArrayElements() &&
                 !array->IsJSGlobalProxy());
  JSObject::NormalizeElements(array);
  return *array;
}


// GrowArrayElements returns a sentinel Smi if the object was normalized.
RUNTIME_FUNCTION(Runtime_GrowArrayElements) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_NUMBER_CHECKED(int, key, Int32, args[1]);

  if (key < 0) {
    return object->elements();
  }

  uint32_t capacity = static_cast<uint32_t>(object->elements()->length());
  uint32_t index = static_cast<uint32_t>(key);

  if (index >= capacity) {
    if (object->WouldConvertToSlowElements(index)) {
      // We don't want to allow operations that cause lazy deopt. Return a Smi
      // as a signal that optimized code should eagerly deoptimize.
      return Smi::FromInt(0);
    }

    uint32_t new_capacity = JSObject::NewElementsCapacity(index + 1);
    object->GetElementsAccessor()->GrowCapacityAndConvert(object, new_capacity);
  }

  // On success, return the fixed array elements.
  return object->elements();
}


RUNTIME_FUNCTION(Runtime_HasComplexElements) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  for (PrototypeIterator iter(isolate, array,
                              PrototypeIterator::START_AT_RECEIVER);
       !iter.IsAtEnd(); iter.Advance()) {
    if (PrototypeIterator::GetCurrent(iter)->IsJSProxy()) {
      return isolate->heap()->true_value();
    }
    Handle<JSObject> current = PrototypeIterator::GetCurrent<JSObject>(iter);
    if (current->HasIndexedInterceptor()) {
      return isolate->heap()->true_value();
    }
    if (!current->HasDictionaryElements()) continue;
    if (current->element_dictionary()->HasComplexElements()) {
      return isolate->heap()->true_value();
    }
  }
  return isolate->heap()->false_value();
}


RUNTIME_FUNCTION(Runtime_IsArray) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSArray());
}


RUNTIME_FUNCTION(Runtime_HasCachedArrayIndex) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  return isolate->heap()->false_value();
}


RUNTIME_FUNCTION(Runtime_GetCachedArrayIndex) {
  // This can never be reached, because Runtime_HasCachedArrayIndex always
  // returns false.
  UNIMPLEMENTED();
  return nullptr;
}


RUNTIME_FUNCTION(Runtime_ArraySpeciesConstructor) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, original_array, 0);
  Handle<Object> constructor;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, constructor,
      Object::ArraySpeciesConstructor(isolate, original_array));
  return *constructor;
}

}  // namespace internal
}  // namespace v8
