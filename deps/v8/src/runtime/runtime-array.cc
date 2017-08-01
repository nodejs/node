// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/code-stubs.h"
#include "src/conversions-inl.h"
#include "src/elements.h"
#include "src/factory.h"
#include "src/isolate-inl.h"
#include "src/keys.h"
#include "src/messages.h"
#include "src/prototype.h"

namespace v8 {
namespace internal {

static void InstallCode(
    Isolate* isolate, Handle<JSObject> holder, const char* name,
    Handle<Code> code, int argc = -1,
    BuiltinFunctionId id = static_cast<BuiltinFunctionId>(-1)) {
  Handle<String> key = isolate->factory()->InternalizeUtf8String(name);
  Handle<JSFunction> optimized =
      isolate->factory()->NewFunctionWithoutPrototype(key, code, true);
  if (argc < 0) {
    optimized->shared()->DontAdaptArguments();
  } else {
    optimized->shared()->set_internal_formal_parameter_count(argc);
  }
  if (id >= 0) {
    optimized->shared()->set_builtin_function_id(id);
  }
  optimized->shared()->set_language_mode(STRICT);
  optimized->shared()->set_native(true);
  JSObject::AddProperty(holder, key, optimized, NONE);
}

static void InstallBuiltin(
    Isolate* isolate, Handle<JSObject> holder, const char* name,
    Builtins::Name builtin_name, int argc = -1,
    BuiltinFunctionId id = static_cast<BuiltinFunctionId>(-1)) {
  InstallCode(isolate, holder, name,
              handle(isolate->builtins()->builtin(builtin_name), isolate), argc,
              id);
}

RUNTIME_FUNCTION(Runtime_SpecialArrayFunctions) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  Handle<JSObject> holder =
      isolate->factory()->NewJSObject(isolate->object_function());

  InstallBuiltin(isolate, holder, "pop", Builtins::kFastArrayPop);
  InstallBuiltin(isolate, holder, "push", Builtins::kFastArrayPush);
  InstallBuiltin(isolate, holder, "shift", Builtins::kFastArrayShift);
  InstallBuiltin(isolate, holder, "unshift", Builtins::kArrayUnshift);
  InstallBuiltin(isolate, holder, "slice", Builtins::kArraySlice);
  InstallBuiltin(isolate, holder, "splice", Builtins::kArraySplice);
  InstallBuiltin(isolate, holder, "includes", Builtins::kArrayIncludes);
  InstallBuiltin(isolate, holder, "indexOf", Builtins::kArrayIndexOf);
  InstallBuiltin(isolate, holder, "keys", Builtins::kArrayPrototypeKeys, 0,
                 kArrayKeys);
  InstallBuiltin(isolate, holder, "values", Builtins::kArrayPrototypeValues, 0,
                 kArrayValues);
  InstallBuiltin(isolate, holder, "entries", Builtins::kArrayPrototypeEntries,
                 0, kArrayEntries);
  return *holder;
}

RUNTIME_FUNCTION(Runtime_FixedArrayGet) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(FixedArray, object, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  return object->get(index);
}


RUNTIME_FUNCTION(Runtime_FixedArraySet) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_CHECKED(FixedArray, object, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  CONVERT_ARG_CHECKED(Object, value, 2);
  object->set(index, value);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_TransitionElementsKind) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Map, to_map, 1);
  ElementsKind to_kind = to_map->elements_kind();
  ElementsAccessor::ForKind(to_kind)->TransitionElementsKind(object, to_map);
  return *object;
}


// Moves all own elements of an object, that are below a limit, to positions
// starting at zero. All undefined values are placed after non-undefined values,
// and are followed by non-existing element. Does not change the length
// property.
// Returns the number of non-undefined elements collected.
// Returns -1 if hole removal is not supported by this method.
RUNTIME_FUNCTION(Runtime_RemoveArrayHoles) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[1]);
  if (object->IsJSProxy()) return Smi::FromInt(-1);
  return *JSObject::PrepareElementsForSort(Handle<JSObject>::cast(object),
                                           limit);
}


// Move contents of argument 0 (an array) to argument 1 (an array)
RUNTIME_FUNCTION(Runtime_MoveArrayContents) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
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
  from->set_length(Smi::kZero);

  JSObject::ValidateElements(to);
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
  if (elements->IsDictionary()) {
    int result = SeededNumberDictionary::cast(elements)->NumberOfElements();
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
    if (PrototypeIterator::GetCurrent(iter)->IsJSProxy() ||
        PrototypeIterator::GetCurrent<JSObject>(iter)
            ->HasIndexedInterceptor()) {
      // Bail out if we find a proxy or interceptor, likely not worth
      // collecting keys in that case.
      return *isolate->factory()->NewNumberFromUint(length);
    }
    Handle<JSObject> current = PrototypeIterator::GetCurrent<JSObject>(iter);
    accumulator.CollectOwnElementIndices(array, current);
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

  if (j != keys->length()) {
    isolate->heap()->RightTrimFixedArray(*keys, keys->length() - j);
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

RUNTIME_FUNCTION(Runtime_NormalizeElements) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  CHECK(!array->HasFixedTypedArrayElements());
  CHECK(!array->IsJSGlobalProxy());
  JSObject::NormalizeElements(array);
  return *array;
}


// GrowArrayElements returns a sentinel Smi if the object was normalized.
RUNTIME_FUNCTION(Runtime_GrowArrayElements) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_NUMBER_CHECKED(int, key, Int32, args[1]);

  if (key < 0) {
    return object->elements();
  }

  uint32_t capacity = static_cast<uint32_t>(object->elements()->length());
  uint32_t index = static_cast<uint32_t>(key);

  if (index >= capacity) {
    if (!object->GetElementsAccessor()->GrowCapacity(object, index)) {
      return Smi::kZero;
    }
  }

  // On success, return the fixed array elements.
  return object->elements();
}


RUNTIME_FUNCTION(Runtime_HasComplexElements) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  for (PrototypeIterator iter(isolate, array, kStartAtReceiver);
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

// ES6 22.1.2.2 Array.isArray
RUNTIME_FUNCTION(Runtime_ArrayIsArray) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  Maybe<bool> result = Object::IsArray(object);
  MAYBE_RETURN(result, isolate->heap()->exception());
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
          Object::GetProperty(object, isolate->factory()->length_string()));

      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, len_,
                                         Object::ToLength(isolate, len_));
      len = static_cast<int64_t>(len_->Number());
      DCHECK_EQ(len, len_->Number());
    }
  }

  if (len == 0) return isolate->heap()->false_value();

  // Let n be ? ToInteger(fromIndex). (If fromIndex is undefined, this step
  // produces the value 0.)
  int64_t index = 0;
  if (!from_index->IsUndefined(isolate)) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, from_index,
                                       Object::ToInteger(isolate, from_index));

    if (V8_LIKELY(from_index->IsSmi())) {
      int start_from = Smi::cast(*from_index)->value();
      if (start_from < 0) {
        index = std::max<int64_t>(len + start_from, 0);
      } else {
        index = start_from;
      }
    } else {
      DCHECK(from_index->IsHeapNumber());
      double start_from = from_index->Number();
      if (start_from >= len) return isolate->heap()->false_value();
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
    MAYBE_RETURN(result, isolate->heap()->exception());
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
      return isolate->heap()->true_value();
    }
  }
  return isolate->heap()->false_value();
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
          Object::GetProperty(object, isolate->factory()->length_string()));

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
    MAYBE_RETURN(result, isolate->heap()->exception());
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
      if (!JSReceiver::HasProperty(&it).FromJust()) {
        continue;
      }
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, element_k,
                                         Object::GetProperty(&it));
      if (search_element->StrictEquals(*element_k)) {
        return *index_obj;
      }
    }
  }
  return Smi::FromInt(-1);
}


RUNTIME_FUNCTION(Runtime_SpreadIterablePrepare) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, spread, 0);

  // Iterate over the spread if we need to.
  if (spread->IterationHasObservableEffects()) {
    Handle<JSFunction> spread_iterable_function = isolate->spread_iterable();
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, spread,
        Execution::Call(isolate, spread_iterable_function,
                        isolate->factory()->undefined_value(), 1, &spread));
  }

  return *spread;
}

RUNTIME_FUNCTION(Runtime_SpreadIterableFixed) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, spread, 0);

  // The caller should check if proper iteration is necessary.
  Handle<JSFunction> spread_iterable_function = isolate->spread_iterable();
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, spread,
      Execution::Call(isolate, spread_iterable_function,
                      isolate->factory()->undefined_value(), 1, &spread));

  // Create a new FixedArray and put the result of the spread into it.
  Handle<JSArray> spread_array = Handle<JSArray>::cast(spread);
  uint32_t spread_length;
  CHECK(spread_array->length()->ToArrayIndex(&spread_length));

  Handle<FixedArray> result = isolate->factory()->NewFixedArray(spread_length);
  ElementsAccessor* accessor = spread_array->GetElementsAccessor();
  for (uint32_t i = 0; i < spread_length; i++) {
    DCHECK(accessor->HasElement(*spread_array, i));
    Handle<Object> element = accessor->Get(spread_array, i);
    result->set(i, *element);
  }

  return *result;
}

}  // namespace internal
}  // namespace v8
