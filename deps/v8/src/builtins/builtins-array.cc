// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/debug/debug.h"
#include "src/execution/isolate.h"
#include "src/handles/global-handles.h"
#include "src/logging/counters.h"
#include "src/objects/contexts.h"
#include "src/objects/elements-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/lookup.h"
#include "src/objects/objects-inl.h"
#include "src/objects/prototype.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

namespace {

inline bool IsJSArrayFastElementMovingAllowed(Isolate* isolate,
                                              JSArray receiver) {
  return JSObject::PrototypeHasNoElements(isolate, receiver);
}

inline bool HasSimpleElements(JSObject current) {
  return !current.map().IsCustomElementsReceiverMap() &&
         !current.GetElementsAccessor()->HasAccessors(current);
}

inline bool HasOnlySimpleReceiverElements(Isolate* isolate, JSObject receiver) {
  // Check that we have no accessors on the receiver's elements.
  if (!HasSimpleElements(receiver)) return false;
  return JSObject::PrototypeHasNoElements(isolate, receiver);
}

inline bool HasOnlySimpleElements(Isolate* isolate, JSReceiver receiver) {
  DisallowHeapAllocation no_gc;
  PrototypeIterator iter(isolate, receiver, kStartAtReceiver);
  for (; !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrent().IsJSProxy()) return false;
    JSObject current = iter.GetCurrent<JSObject>();
    if (!HasSimpleElements(current)) return false;
  }
  return true;
}

// This method may transition the elements kind of the JSArray once, to make
// sure that all elements provided as arguments in the specified range can be
// added without further elements kinds transitions.
void MatchArrayElementsKindToArguments(Isolate* isolate, Handle<JSArray> array,
                                       BuiltinArguments* args,
                                       int first_arg_index, int num_arguments) {
  int args_length = args->length();
  if (first_arg_index >= args_length) return;

  ElementsKind origin_kind = array->GetElementsKind();

  // We do not need to transition for PACKED/HOLEY_ELEMENTS.
  if (IsObjectElementsKind(origin_kind)) return;

  ElementsKind target_kind = origin_kind;
  {
    DisallowHeapAllocation no_gc;
    int last_arg_index = std::min(first_arg_index + num_arguments, args_length);
    for (int i = first_arg_index; i < last_arg_index; i++) {
      Object arg = (*args)[i];
      if (arg.IsHeapObject()) {
        if (arg.IsHeapNumber()) {
          target_kind = PACKED_DOUBLE_ELEMENTS;
        } else {
          target_kind = PACKED_ELEMENTS;
          break;
        }
      }
    }
  }
  if (target_kind != origin_kind) {
    // Use a short-lived HandleScope to avoid creating several copies of the
    // elements handle which would cause issues when left-trimming later-on.
    HandleScope scope(isolate);
    JSObject::TransitionElementsKind(array, target_kind);
  }
}

// Returns |false| if not applicable.
// TODO(szuend): Refactor this function because it is getting hard to
//               understand what each call-site actually checks.
V8_WARN_UNUSED_RESULT
inline bool EnsureJSArrayWithWritableFastElements(Isolate* isolate,
                                                  Handle<Object> receiver,
                                                  BuiltinArguments* args,
                                                  int first_arg_index,
                                                  int num_arguments) {
  if (!receiver->IsJSArray()) return false;
  Handle<JSArray> array = Handle<JSArray>::cast(receiver);
  ElementsKind origin_kind = array->GetElementsKind();
  if (IsDictionaryElementsKind(origin_kind)) return false;
  if (!array->map().is_extensible()) return false;
  if (args == nullptr) return true;

  // If there may be elements accessors in the prototype chain, the fast path
  // cannot be used if there arguments to add to the array.
  if (!IsJSArrayFastElementMovingAllowed(isolate, *array)) return false;

  // Adding elements to the array prototype would break code that makes sure
  // it has no elements. Handle that elsewhere.
  if (isolate->IsAnyInitialArrayPrototype(array)) return false;

  // Need to ensure that the arguments passed in args can be contained in
  // the array.
  MatchArrayElementsKindToArguments(isolate, array, args, first_arg_index,
                                    num_arguments);
  return true;
}

// If |index| is Undefined, returns init_if_undefined.
// If |index| is negative, returns length + index.
// If |index| is positive, returns index.
// Returned value is guaranteed to be in the interval of [0, length].
V8_WARN_UNUSED_RESULT Maybe<double> GetRelativeIndex(Isolate* isolate,
                                                     double length,
                                                     Handle<Object> index,
                                                     double init_if_undefined) {
  double relative_index = init_if_undefined;
  if (!index->IsUndefined()) {
    Handle<Object> relative_index_obj;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, relative_index_obj,
                                     Object::ToInteger(isolate, index),
                                     Nothing<double>());
    relative_index = relative_index_obj->Number();
  }

  if (relative_index < 0) {
    return Just(std::max(length + relative_index, 0.0));
  }

  return Just(std::min(relative_index, length));
}

// Returns "length", has "fast-path" for JSArrays.
V8_WARN_UNUSED_RESULT Maybe<double> GetLengthProperty(
    Isolate* isolate, Handle<JSReceiver> receiver) {
  if (receiver->IsJSArray()) {
    Handle<JSArray> array = Handle<JSArray>::cast(receiver);
    double length = array->length().Number();
    DCHECK(0 <= length && length <= kMaxSafeInteger);

    return Just(length);
  }

  Handle<Object> raw_length_number;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, raw_length_number,
      Object::GetLengthFromArrayLike(isolate, receiver), Nothing<double>());
  return Just(raw_length_number->Number());
}

// Set "length" property, has "fast-path" for JSArrays.
// Returns Nothing if something went wrong.
V8_WARN_UNUSED_RESULT MaybeHandle<Object> SetLengthProperty(
    Isolate* isolate, Handle<JSReceiver> receiver, double length) {
  if (receiver->IsJSArray()) {
    Handle<JSArray> array = Handle<JSArray>::cast(receiver);
    if (!JSArray::HasReadOnlyLength(array)) {
      DCHECK_LE(length, kMaxUInt32);
      JSArray::SetLength(array, static_cast<uint32_t>(length));
      return receiver;
    }
  }

  return Object::SetProperty(
      isolate, receiver, isolate->factory()->length_string(),
      isolate->factory()->NewNumber(length), StoreOrigin::kMaybeKeyed,
      Just(ShouldThrow::kThrowOnError));
}

V8_WARN_UNUSED_RESULT Object GenericArrayFill(Isolate* isolate,
                                              Handle<JSReceiver> receiver,
                                              Handle<Object> value,
                                              double start, double end) {
  // 7. Repeat, while k < final.
  while (start < end) {
    // a. Let Pk be ! ToString(k).
    Handle<String> index = isolate->factory()->NumberToString(
        isolate->factory()->NewNumber(start));

    // b. Perform ? Set(O, Pk, value, true).
    RETURN_FAILURE_ON_EXCEPTION(isolate, Object::SetPropertyOrElement(
                                             isolate, receiver, index, value,
                                             Just(ShouldThrow::kThrowOnError)));

    // c. Increase k by 1.
    ++start;
  }

  // 8. Return O.
  return *receiver;
}

V8_WARN_UNUSED_RESULT bool TryFastArrayFill(
    Isolate* isolate, BuiltinArguments* args, Handle<JSReceiver> receiver,
    Handle<Object> value, double start_index, double end_index) {
  // If indices are too large, use generic path since they are stored as
  // properties, not in the element backing store.
  if (end_index > kMaxUInt32) return false;
  if (!receiver->IsJSObject()) return false;

  if (!EnsureJSArrayWithWritableFastElements(isolate, receiver, args, 1, 1)) {
    return false;
  }

  Handle<JSArray> array = Handle<JSArray>::cast(receiver);

  // If no argument was provided, we fill the array with 'undefined'.
  // EnsureJSArrayWith... does not handle that case so we do it here.
  // TODO(szuend): Pass target elements kind to EnsureJSArrayWith... when
  //               it gets refactored.
  if (args->length() == 1 && array->GetElementsKind() != PACKED_ELEMENTS) {
    // Use a short-lived HandleScope to avoid creating several copies of the
    // elements handle which would cause issues when left-trimming later-on.
    HandleScope scope(isolate);
    JSObject::TransitionElementsKind(array, PACKED_ELEMENTS);
  }

  DCHECK_LE(start_index, kMaxUInt32);
  DCHECK_LE(end_index, kMaxUInt32);

  uint32_t start, end;
  CHECK(DoubleToUint32IfEqualToSelf(start_index, &start));
  CHECK(DoubleToUint32IfEqualToSelf(end_index, &end));

  ElementsAccessor* accessor = array->GetElementsAccessor();
  accessor->Fill(array, value, start, end);
  return true;
}
}  // namespace

BUILTIN(ArrayPrototypeFill) {
  HandleScope scope(isolate);

  if (isolate->debug_execution_mode() == DebugInfo::kSideEffects) {
    if (!isolate->debug()->PerformSideEffectCheckForObject(args.receiver())) {
      return ReadOnlyRoots(isolate).exception();
    }
  }

  // 1. Let O be ? ToObject(this value).
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, receiver, Object::ToObject(isolate, args.receiver()));

  // 2. Let len be ? ToLength(? Get(O, "length")).
  double length;
  MAYBE_ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, length, GetLengthProperty(isolate, receiver));

  // 3. Let relativeStart be ? ToInteger(start).
  // 4. If relativeStart < 0, let k be max((len + relativeStart), 0);
  //    else let k be min(relativeStart, len).
  Handle<Object> start = args.atOrUndefined(isolate, 2);

  double start_index;
  MAYBE_ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, start_index, GetRelativeIndex(isolate, length, start, 0));

  // 5. If end is undefined, let relativeEnd be len;
  //    else let relativeEnd be ? ToInteger(end).
  // 6. If relativeEnd < 0, let final be max((len + relativeEnd), 0);
  //    else let final be min(relativeEnd, len).
  Handle<Object> end = args.atOrUndefined(isolate, 3);

  double end_index;
  MAYBE_ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, end_index, GetRelativeIndex(isolate, length, end, length));

  if (start_index >= end_index) return *receiver;

  // Ensure indexes are within array bounds
  DCHECK_LE(0, start_index);
  DCHECK_LE(start_index, end_index);
  DCHECK_LE(end_index, length);

  Handle<Object> value = args.atOrUndefined(isolate, 1);

  if (TryFastArrayFill(isolate, &args, receiver, value, start_index,
                       end_index)) {
    return *receiver;
  }
  return GenericArrayFill(isolate, receiver, value, start_index, end_index);
}

namespace {
V8_WARN_UNUSED_RESULT Object GenericArrayPush(Isolate* isolate,
                                              BuiltinArguments* args) {
  // 1. Let O be ? ToObject(this value).
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, receiver, Object::ToObject(isolate, args->receiver()));

  // 2. Let len be ? ToLength(? Get(O, "length")).
  Handle<Object> raw_length_number;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, raw_length_number,
      Object::GetLengthFromArrayLike(isolate, receiver));

  // 3. Let args be a List whose elements are, in left to right order,
  //    the arguments that were passed to this function invocation.
  // 4. Let arg_count be the number of elements in args.
  int arg_count = args->length() - 1;

  // 5. If len + arg_count > 2^53-1, throw a TypeError exception.
  double length = raw_length_number->Number();
  if (arg_count > kMaxSafeInteger - length) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kPushPastSafeLength,
                              isolate->factory()->NewNumberFromInt(arg_count),
                              raw_length_number));
  }

  // 6. Repeat, while args is not empty.
  for (int i = 0; i < arg_count; ++i) {
    // a. Remove the first element from args and let E be the value of the
    //    element.
    Handle<Object> element = args->at(i + 1);

    // b. Perform ? Set(O, ! ToString(len), E, true).
    if (length <= static_cast<double>(JSArray::kMaxArrayIndex)) {
      RETURN_FAILURE_ON_EXCEPTION(
          isolate, Object::SetElement(isolate, receiver, length, element,
                                      ShouldThrow::kThrowOnError));
    } else {
      bool success;
      LookupIterator it = LookupIterator::PropertyOrElement(
          isolate, receiver, isolate->factory()->NewNumber(length), &success);
      // Must succeed since we always pass a valid key.
      DCHECK(success);
      MAYBE_RETURN(Object::SetProperty(&it, element, StoreOrigin::kMaybeKeyed,
                                       Just(ShouldThrow::kThrowOnError)),
                   ReadOnlyRoots(isolate).exception());
    }

    // c. Let len be len+1.
    ++length;
  }

  // 7. Perform ? Set(O, "length", len, true).
  Handle<Object> final_length = isolate->factory()->NewNumber(length);
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, Object::SetProperty(isolate, receiver,
                                   isolate->factory()->length_string(),
                                   final_length, StoreOrigin::kMaybeKeyed,
                                   Just(ShouldThrow::kThrowOnError)));

  // 8. Return len.
  return *final_length;
}
}  // namespace

BUILTIN(ArrayPush) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  if (!EnsureJSArrayWithWritableFastElements(isolate, receiver, &args, 1,
                                             args.length() - 1)) {
    return GenericArrayPush(isolate, &args);
  }

  // Fast Elements Path
  int to_add = args.length() - 1;
  Handle<JSArray> array = Handle<JSArray>::cast(receiver);
  uint32_t len = static_cast<uint32_t>(array->length().Number());
  if (to_add == 0) return *isolate->factory()->NewNumberFromUint(len);

  // Currently fixed arrays cannot grow too big, so we should never hit this.
  DCHECK_LE(to_add, Smi::kMaxValue - Smi::ToInt(array->length()));

  if (JSArray::HasReadOnlyLength(array)) {
    return GenericArrayPush(isolate, &args);
  }

  ElementsAccessor* accessor = array->GetElementsAccessor();
  uint32_t new_length = accessor->Push(array, &args, to_add);
  return *isolate->factory()->NewNumberFromUint((new_length));
}

namespace {

V8_WARN_UNUSED_RESULT Object GenericArrayPop(Isolate* isolate,
                                             BuiltinArguments* args) {
  // 1. Let O be ? ToObject(this value).
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, receiver, Object::ToObject(isolate, args->receiver()));

  // 2. Let len be ? ToLength(? Get(O, "length")).
  Handle<Object> raw_length_number;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, raw_length_number,
      Object::GetLengthFromArrayLike(isolate, receiver));
  double length = raw_length_number->Number();

  // 3. If len is zero, then.
  if (length == 0) {
    // a. Perform ? Set(O, "length", 0, true).
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, Object::SetProperty(isolate, receiver,
                                     isolate->factory()->length_string(),
                                     Handle<Smi>(Smi::zero(), isolate),
                                     StoreOrigin::kMaybeKeyed,
                                     Just(ShouldThrow::kThrowOnError)));

    // b. Return undefined.
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // 4. Else len > 0.
  // a. Let new_len be len-1.
  Handle<Object> new_length = isolate->factory()->NewNumber(length - 1);

  // b. Let index be ! ToString(newLen).
  Handle<String> index = isolate->factory()->NumberToString(new_length);

  // c. Let element be ? Get(O, index).
  Handle<Object> element;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, element, Object::GetPropertyOrElement(isolate, receiver, index));

  // d. Perform ? DeletePropertyOrThrow(O, index).
  MAYBE_RETURN(JSReceiver::DeletePropertyOrElement(receiver, index,
                                                   LanguageMode::kStrict),
               ReadOnlyRoots(isolate).exception());

  // e. Perform ? Set(O, "length", newLen, true).
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, Object::SetProperty(isolate, receiver,
                                   isolate->factory()->length_string(),
                                   new_length, StoreOrigin::kMaybeKeyed,
                                   Just(ShouldThrow::kThrowOnError)));

  // f. Return element.
  return *element;
}

}  // namespace

BUILTIN(ArrayPop) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  if (!EnsureJSArrayWithWritableFastElements(isolate, receiver, nullptr, 0,
                                             0)) {
    return GenericArrayPop(isolate, &args);
  }
  Handle<JSArray> array = Handle<JSArray>::cast(receiver);

  uint32_t len = static_cast<uint32_t>(array->length().Number());
  if (len == 0) return ReadOnlyRoots(isolate).undefined_value();

  if (JSArray::HasReadOnlyLength(array)) {
    return GenericArrayPop(isolate, &args);
  }

  Handle<Object> result;
  if (IsJSArrayFastElementMovingAllowed(isolate, JSArray::cast(*receiver))) {
    // Fast Elements Path
    result = array->GetElementsAccessor()->Pop(array);
  } else {
    // Use Slow Lookup otherwise
    uint32_t new_length = len - 1;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, JSReceiver::GetElement(isolate, array, new_length));
    JSArray::SetLength(array, new_length);
  }

  return *result;
}

namespace {

// Returns true, iff we can use ElementsAccessor for shifting.
V8_WARN_UNUSED_RESULT bool CanUseFastArrayShift(Isolate* isolate,
                                                Handle<JSReceiver> receiver) {
  if (!EnsureJSArrayWithWritableFastElements(isolate, receiver, nullptr, 0,
                                             0) ||
      !IsJSArrayFastElementMovingAllowed(isolate, JSArray::cast(*receiver))) {
    return false;
  }

  Handle<JSArray> array = Handle<JSArray>::cast(receiver);
  return !JSArray::HasReadOnlyLength(array);
}

V8_WARN_UNUSED_RESULT Object GenericArrayShift(Isolate* isolate,
                                               Handle<JSReceiver> receiver,
                                               double length) {
  // 4. Let first be ? Get(O, "0").
  Handle<Object> first;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, first,
                                     Object::GetElement(isolate, receiver, 0));

  // 5. Let k be 1.
  double k = 1;

  // 6. Repeat, while k < len.
  while (k < length) {
    // a. Let from be ! ToString(k).
    Handle<String> from =
        isolate->factory()->NumberToString(isolate->factory()->NewNumber(k));

    // b. Let to be ! ToString(k-1).
    Handle<String> to = isolate->factory()->NumberToString(
        isolate->factory()->NewNumber(k - 1));

    // c. Let fromPresent be ? HasProperty(O, from).
    bool from_present;
    MAYBE_ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, from_present, JSReceiver::HasProperty(receiver, from));

    // d. If fromPresent is true, then.
    if (from_present) {
      // i. Let fromVal be ? Get(O, from).
      Handle<Object> from_val;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, from_val,
          Object::GetPropertyOrElement(isolate, receiver, from));

      // ii. Perform ? Set(O, to, fromVal, true).
      RETURN_FAILURE_ON_EXCEPTION(
          isolate,
          Object::SetPropertyOrElement(isolate, receiver, to, from_val,
                                       Just(ShouldThrow::kThrowOnError)));
    } else {  // e. Else fromPresent is false,
      // i. Perform ? DeletePropertyOrThrow(O, to).
      MAYBE_RETURN(JSReceiver::DeletePropertyOrElement(receiver, to,
                                                       LanguageMode::kStrict),
                   ReadOnlyRoots(isolate).exception());
    }

    // f. Increase k by 1.
    ++k;
  }

  // 7. Perform ? DeletePropertyOrThrow(O, ! ToString(len-1)).
  Handle<String> new_length = isolate->factory()->NumberToString(
      isolate->factory()->NewNumber(length - 1));
  MAYBE_RETURN(JSReceiver::DeletePropertyOrElement(receiver, new_length,
                                                   LanguageMode::kStrict),
               ReadOnlyRoots(isolate).exception());

  // 8. Perform ? Set(O, "length", len-1, true).
  RETURN_FAILURE_ON_EXCEPTION(isolate,
                              SetLengthProperty(isolate, receiver, length - 1));

  // 9. Return first.
  return *first;
}
}  // namespace

BUILTIN(ArrayShift) {
  HandleScope scope(isolate);

  // 1. Let O be ? ToObject(this value).
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, receiver, Object::ToObject(isolate, args.receiver()));

  // 2. Let len be ? ToLength(? Get(O, "length")).
  double length;
  MAYBE_ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, length, GetLengthProperty(isolate, receiver));

  // 3. If len is zero, then.
  if (length == 0) {
    // a. Perform ? Set(O, "length", 0, true).
    RETURN_FAILURE_ON_EXCEPTION(isolate,
                                SetLengthProperty(isolate, receiver, length));

    // b. Return undefined.
    return ReadOnlyRoots(isolate).undefined_value();
  }

  if (CanUseFastArrayShift(isolate, receiver)) {
    Handle<JSArray> array = Handle<JSArray>::cast(receiver);
    return *array->GetElementsAccessor()->Shift(array);
  }

  return GenericArrayShift(isolate, receiver, length);
}

BUILTIN(ArrayUnshift) {
  HandleScope scope(isolate);
  DCHECK(args.receiver()->IsJSArray());
  Handle<JSArray> array = Handle<JSArray>::cast(args.receiver());

  // These are checked in the Torque builtin.
  DCHECK(array->map().is_extensible());
  DCHECK(!IsDictionaryElementsKind(array->GetElementsKind()));
  DCHECK(IsJSArrayFastElementMovingAllowed(isolate, *array));
  DCHECK(!isolate->IsAnyInitialArrayPrototype(array));

  MatchArrayElementsKindToArguments(isolate, array, &args, 1,
                                    args.length() - 1);

  int to_add = args.length() - 1;
  if (to_add == 0) return array->length();

  // Currently fixed arrays cannot grow too big, so we should never hit this.
  DCHECK_LE(to_add, Smi::kMaxValue - Smi::ToInt(array->length()));
  DCHECK(!JSArray::HasReadOnlyLength(array));

  ElementsAccessor* accessor = array->GetElementsAccessor();
  int new_length = accessor->Unshift(array, &args, to_add);
  return Smi::FromInt(new_length);
}

// Array Concat -------------------------------------------------------------

namespace {

/**
 * A simple visitor visits every element of Array's.
 * The backend storage can be a fixed array for fast elements case,
 * or a dictionary for sparse array. Since Dictionary is a subtype
 * of FixedArray, the class can be used by both fast and slow cases.
 * The second parameter of the constructor, fast_elements, specifies
 * whether the storage is a FixedArray or Dictionary.
 *
 * An index limit is used to deal with the situation that a result array
 * length overflows 32-bit non-negative integer.
 */
class ArrayConcatVisitor {
 public:
  ArrayConcatVisitor(Isolate* isolate, Handle<HeapObject> storage,
                     bool fast_elements)
      : isolate_(isolate),
        storage_(isolate->global_handles()->Create(*storage)),
        index_offset_(0u),
        bit_field_(FastElementsField::encode(fast_elements) |
                   ExceedsLimitField::encode(false) |
                   IsFixedArrayField::encode(storage->IsFixedArray()) |
                   HasSimpleElementsField::encode(
                       storage->IsFixedArray() ||
                       !storage->map().IsCustomElementsReceiverMap())) {
    DCHECK(!(this->fast_elements() && !is_fixed_array()));
  }

  ~ArrayConcatVisitor() { clear_storage(); }

  V8_WARN_UNUSED_RESULT bool visit(uint32_t i, Handle<Object> elm) {
    uint32_t index = index_offset_ + i;

    if (i >= JSObject::kMaxElementCount - index_offset_) {
      set_exceeds_array_limit(true);
      // Exception hasn't been thrown at this point. Return true to
      // break out, and caller will throw. !visit would imply that
      // there is already a pending exception.
      return true;
    }

    if (!is_fixed_array()) {
      LookupIterator it(isolate_, storage_, index, LookupIterator::OWN);
      MAYBE_RETURN(
          JSReceiver::CreateDataProperty(&it, elm, Just(kThrowOnError)), false);
      return true;
    }

    if (fast_elements()) {
      if (index < static_cast<uint32_t>(storage_fixed_array()->length())) {
        storage_fixed_array()->set(index, *elm);
        return true;
      }
      // Our initial estimate of length was foiled, possibly by
      // getters on the arrays increasing the length of later arrays
      // during iteration.
      // This shouldn't happen in anything but pathological cases.
      SetDictionaryMode();
      // Fall-through to dictionary mode.
    }
    DCHECK(!fast_elements());
    Handle<NumberDictionary> dict(NumberDictionary::cast(*storage_), isolate_);
    // The object holding this backing store has just been allocated, so
    // it cannot yet be used as a prototype.
    Handle<JSObject> not_a_prototype_holder;
    Handle<NumberDictionary> result = NumberDictionary::Set(
        isolate_, dict, index, elm, not_a_prototype_holder);
    if (!result.is_identical_to(dict)) {
      // Dictionary needed to grow.
      clear_storage();
      set_storage(*result);
    }
    return true;
  }

  uint32_t index_offset() const { return index_offset_; }

  void increase_index_offset(uint32_t delta) {
    if (JSObject::kMaxElementCount - index_offset_ < delta) {
      index_offset_ = JSObject::kMaxElementCount;
    } else {
      index_offset_ += delta;
    }
    // If the initial length estimate was off (see special case in visit()),
    // but the array blowing the limit didn't contain elements beyond the
    // provided-for index range, go to dictionary mode now.
    if (fast_elements() &&
        index_offset_ >
            static_cast<uint32_t>(FixedArrayBase::cast(*storage_).length())) {
      SetDictionaryMode();
    }
  }

  bool exceeds_array_limit() const {
    return ExceedsLimitField::decode(bit_field_);
  }

  Handle<JSArray> ToArray() {
    DCHECK(is_fixed_array());
    Handle<JSArray> array = isolate_->factory()->NewJSArray(0);
    Handle<Object> length =
        isolate_->factory()->NewNumber(static_cast<double>(index_offset_));
    Handle<Map> map = JSObject::GetElementsTransitionMap(
        array, fast_elements() ? HOLEY_ELEMENTS : DICTIONARY_ELEMENTS);
    array->set_length(*length);
    array->set_elements(*storage_fixed_array());
    array->synchronized_set_map(*map);
    return array;
  }

  V8_WARN_UNUSED_RESULT MaybeHandle<JSReceiver> ToJSReceiver() {
    DCHECK(!is_fixed_array());
    Handle<JSReceiver> result = Handle<JSReceiver>::cast(storage_);
    Handle<Object> length =
        isolate_->factory()->NewNumber(static_cast<double>(index_offset_));
    RETURN_ON_EXCEPTION(
        isolate_,
        Object::SetProperty(
            isolate_, result, isolate_->factory()->length_string(), length,
            StoreOrigin::kMaybeKeyed, Just(ShouldThrow::kThrowOnError)),
        JSReceiver);
    return result;
  }
  bool has_simple_elements() const {
    return HasSimpleElementsField::decode(bit_field_);
  }

 private:
  // Convert storage to dictionary mode.
  void SetDictionaryMode() {
    DCHECK(fast_elements() && is_fixed_array());
    Handle<FixedArray> current_storage = storage_fixed_array();
    Handle<NumberDictionary> slow_storage(
        NumberDictionary::New(isolate_, current_storage->length()));
    uint32_t current_length = static_cast<uint32_t>(current_storage->length());
    FOR_WITH_HANDLE_SCOPE(
        isolate_, uint32_t, i = 0, i, i < current_length, i++, {
          Handle<Object> element(current_storage->get(i), isolate_);
          if (!element->IsTheHole(isolate_)) {
            // The object holding this backing store has just been allocated, so
            // it cannot yet be used as a prototype.
            Handle<JSObject> not_a_prototype_holder;
            Handle<NumberDictionary> new_storage = NumberDictionary::Set(
                isolate_, slow_storage, i, element, not_a_prototype_holder);
            if (!new_storage.is_identical_to(slow_storage)) {
              slow_storage = loop_scope.CloseAndEscape(new_storage);
            }
          }
        });
    clear_storage();
    set_storage(*slow_storage);
    set_fast_elements(false);
  }

  inline void clear_storage() { GlobalHandles::Destroy(storage_.location()); }

  inline void set_storage(FixedArray storage) {
    DCHECK(is_fixed_array());
    DCHECK(has_simple_elements());
    storage_ = isolate_->global_handles()->Create(storage);
  }

  class FastElementsField : public BitField<bool, 0, 1> {};
  class ExceedsLimitField : public BitField<bool, 1, 1> {};
  class IsFixedArrayField : public BitField<bool, 2, 1> {};
  class HasSimpleElementsField : public BitField<bool, 3, 1> {};

  bool fast_elements() const { return FastElementsField::decode(bit_field_); }
  void set_fast_elements(bool fast) {
    bit_field_ = FastElementsField::update(bit_field_, fast);
  }
  void set_exceeds_array_limit(bool exceeds) {
    bit_field_ = ExceedsLimitField::update(bit_field_, exceeds);
  }
  bool is_fixed_array() const { return IsFixedArrayField::decode(bit_field_); }
  Handle<FixedArray> storage_fixed_array() {
    DCHECK(is_fixed_array());
    DCHECK(has_simple_elements());
    return Handle<FixedArray>::cast(storage_);
  }

  Isolate* isolate_;
  Handle<Object> storage_;  // Always a global handle.
  // Index after last seen index. Always less than or equal to
  // JSObject::kMaxElementCount.
  uint32_t index_offset_;
  uint32_t bit_field_;
};

uint32_t EstimateElementCount(Isolate* isolate, Handle<JSArray> array) {
  DisallowHeapAllocation no_gc;
  uint32_t length = static_cast<uint32_t>(array->length().Number());
  int element_count = 0;
  switch (array->GetElementsKind()) {
    case PACKED_SMI_ELEMENTS:
    case HOLEY_SMI_ELEMENTS:
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case HOLEY_ELEMENTS: {
      // Fast elements can't have lengths that are not representable by
      // a 32-bit signed integer.
      DCHECK_GE(static_cast<int32_t>(FixedArray::kMaxLength), 0);
      int fast_length = static_cast<int>(length);
      FixedArray elements = FixedArray::cast(array->elements());
      for (int i = 0; i < fast_length; i++) {
        if (!elements.get(i).IsTheHole(isolate)) element_count++;
      }
      break;
    }
    case PACKED_DOUBLE_ELEMENTS:
    case HOLEY_DOUBLE_ELEMENTS: {
      // Fast elements can't have lengths that are not representable by
      // a 32-bit signed integer.
      DCHECK_GE(static_cast<int32_t>(FixedDoubleArray::kMaxLength), 0);
      int fast_length = static_cast<int>(length);
      if (array->elements().IsFixedArray()) {
        DCHECK_EQ(FixedArray::cast(array->elements()).length(), 0);
        break;
      }
      FixedDoubleArray elements = FixedDoubleArray::cast(array->elements());
      for (int i = 0; i < fast_length; i++) {
        if (!elements.is_the_hole(i)) element_count++;
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      NumberDictionary dictionary = NumberDictionary::cast(array->elements());
      int capacity = dictionary.Capacity();
      ReadOnlyRoots roots(isolate);
      for (int i = 0; i < capacity; i++) {
        Object key = dictionary.KeyAt(i);
        if (dictionary.IsKey(roots, key)) {
          element_count++;
        }
      }
      break;
    }
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      // External arrays are always dense.
      return length;
    case NO_ELEMENTS:
      return 0;
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
      UNREACHABLE();
  }
  // As an estimate, we assume that the prototype doesn't contain any
  // inherited elements.
  return element_count;
}

void CollectElementIndices(Isolate* isolate, Handle<JSObject> object,
                           uint32_t range, std::vector<uint32_t>* indices) {
  ElementsKind kind = object->GetElementsKind();
  switch (kind) {
    case PACKED_SMI_ELEMENTS:
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case HOLEY_SMI_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case HOLEY_ELEMENTS: {
      DisallowHeapAllocation no_gc;
      FixedArray elements = FixedArray::cast(object->elements());
      uint32_t length = static_cast<uint32_t>(elements.length());
      if (range < length) length = range;
      for (uint32_t i = 0; i < length; i++) {
        if (!elements.get(i).IsTheHole(isolate)) {
          indices->push_back(i);
        }
      }
      break;
    }
    case HOLEY_DOUBLE_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS: {
      if (object->elements().IsFixedArray()) {
        DCHECK_EQ(object->elements().length(), 0);
        break;
      }
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(object->elements()), isolate);
      uint32_t length = static_cast<uint32_t>(elements->length());
      if (range < length) length = range;
      for (uint32_t i = 0; i < length; i++) {
        if (!elements->is_the_hole(i)) {
          indices->push_back(i);
        }
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      DisallowHeapAllocation no_gc;
      NumberDictionary dict = NumberDictionary::cast(object->elements());
      uint32_t capacity = dict.Capacity();
      ReadOnlyRoots roots(isolate);
      FOR_WITH_HANDLE_SCOPE(isolate, uint32_t, j = 0, j, j < capacity, j++, {
        Object k = dict.KeyAt(j);
        if (!dict.IsKey(roots, k)) continue;
        DCHECK(k.IsNumber());
        uint32_t index = static_cast<uint32_t>(k.Number());
        if (index < range) {
          indices->push_back(index);
        }
      });
      break;
    }
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      {
        // TODO(bmeurer, v8:4153): Change this to size_t later.
        uint32_t length =
            static_cast<uint32_t>(Handle<JSTypedArray>::cast(object)->length());
        if (range <= length) {
          length = range;
          // We will add all indices, so we might as well clear it first
          // and avoid duplicates.
          indices->clear();
        }
        for (uint32_t i = 0; i < length; i++) {
          indices->push_back(i);
        }
        if (length == range) return;  // All indices accounted for already.
        break;
      }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS: {
      DisallowHeapAllocation no_gc;
      FixedArrayBase elements = object->elements();
      JSObject raw_object = *object;
      ElementsAccessor* accessor = object->GetElementsAccessor();
      for (uint32_t i = 0; i < range; i++) {
        if (accessor->HasElement(raw_object, i, elements)) {
          indices->push_back(i);
        }
      }
      break;
    }
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS: {
      DCHECK(object->IsJSPrimitiveWrapper());
      Handle<JSPrimitiveWrapper> js_value =
          Handle<JSPrimitiveWrapper>::cast(object);
      DCHECK(js_value->value().IsString());
      Handle<String> string(String::cast(js_value->value()), isolate);
      uint32_t length = static_cast<uint32_t>(string->length());
      uint32_t i = 0;
      uint32_t limit = Min(length, range);
      for (; i < limit; i++) {
        indices->push_back(i);
      }
      ElementsAccessor* accessor = object->GetElementsAccessor();
      for (; i < range; i++) {
        if (accessor->HasElement(*object, i)) {
          indices->push_back(i);
        }
      }
      break;
    }
    case NO_ELEMENTS:
      break;
  }

  PrototypeIterator iter(isolate, object);
  if (!iter.IsAtEnd()) {
    // The prototype will usually have no inherited element indices,
    // but we have to check.
    CollectElementIndices(
        isolate, PrototypeIterator::GetCurrent<JSObject>(iter), range, indices);
  }
}

bool IterateElementsSlow(Isolate* isolate, Handle<JSReceiver> receiver,
                         uint32_t length, ArrayConcatVisitor* visitor) {
  FOR_WITH_HANDLE_SCOPE(isolate, uint32_t, i = 0, i, i < length, ++i, {
    Maybe<bool> maybe = JSReceiver::HasElement(receiver, i);
    if (maybe.IsNothing()) return false;
    if (maybe.FromJust()) {
      Handle<Object> element_value;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, element_value, JSReceiver::GetElement(isolate, receiver, i),
          false);
      if (!visitor->visit(i, element_value)) return false;
    }
  });
  visitor->increase_index_offset(length);
  return true;
}
/**
 * A helper function that visits "array" elements of a JSReceiver in numerical
 * order.
 *
 * The visitor argument called for each existing element in the array
 * with the element index and the element's value.
 * Afterwards it increments the base-index of the visitor by the array
 * length.
 * Returns false if any access threw an exception, otherwise true.
 */
bool IterateElements(Isolate* isolate, Handle<JSReceiver> receiver,
                     ArrayConcatVisitor* visitor) {
  uint32_t length = 0;

  if (receiver->IsJSArray()) {
    Handle<JSArray> array = Handle<JSArray>::cast(receiver);
    length = static_cast<uint32_t>(array->length().Number());
  } else {
    Handle<Object> val;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, val, Object::GetLengthFromArrayLike(isolate, receiver), false);
    if (visitor->index_offset() + val->Number() > kMaxSafeInteger) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kInvalidArrayLength));
      return false;
    }
    // TODO(caitp): Support larger element indexes (up to 2^53-1).
    if (!val->ToUint32(&length)) {
      length = 0;
    }
    // TODO(cbruni): handle other element kind as well
    return IterateElementsSlow(isolate, receiver, length, visitor);
  }

  if (!HasOnlySimpleElements(isolate, *receiver) ||
      !visitor->has_simple_elements()) {
    return IterateElementsSlow(isolate, receiver, length, visitor);
  }
  Handle<JSObject> array = Handle<JSObject>::cast(receiver);

  switch (array->GetElementsKind()) {
    case PACKED_SMI_ELEMENTS:
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case HOLEY_SMI_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case HOLEY_ELEMENTS: {
      // Run through the elements FixedArray and use HasElement and GetElement
      // to check the prototype for missing elements.
      Handle<FixedArray> elements(FixedArray::cast(array->elements()), isolate);
      int fast_length = static_cast<int>(length);
      DCHECK(fast_length <= elements->length());
      FOR_WITH_HANDLE_SCOPE(isolate, int, j = 0, j, j < fast_length, j++, {
        Handle<Object> element_value(elements->get(j), isolate);
        if (!element_value->IsTheHole(isolate)) {
          if (!visitor->visit(j, element_value)) return false;
        } else {
          Maybe<bool> maybe = JSReceiver::HasElement(array, j);
          if (maybe.IsNothing()) return false;
          if (maybe.FromJust()) {
            // Call GetElement on array, not its prototype, or getters won't
            // have the correct receiver.
            ASSIGN_RETURN_ON_EXCEPTION_VALUE(
                isolate, element_value,
                JSReceiver::GetElement(isolate, array, j), false);
            if (!visitor->visit(j, element_value)) return false;
          }
        }
      });
      break;
    }
    case HOLEY_DOUBLE_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS: {
      // Empty array is FixedArray but not FixedDoubleArray.
      if (length == 0) break;
      // Run through the elements FixedArray and use HasElement and GetElement
      // to check the prototype for missing elements.
      if (array->elements().IsFixedArray()) {
        DCHECK_EQ(array->elements().length(), 0);
        break;
      }
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(array->elements()), isolate);
      int fast_length = static_cast<int>(length);
      DCHECK(fast_length <= elements->length());
      FOR_WITH_HANDLE_SCOPE(isolate, int, j = 0, j, j < fast_length, j++, {
        if (!elements->is_the_hole(j)) {
          double double_value = elements->get_scalar(j);
          Handle<Object> element_value =
              isolate->factory()->NewNumber(double_value);
          if (!visitor->visit(j, element_value)) return false;
        } else {
          Maybe<bool> maybe = JSReceiver::HasElement(array, j);
          if (maybe.IsNothing()) return false;
          if (maybe.FromJust()) {
            // Call GetElement on array, not its prototype, or getters won't
            // have the correct receiver.
            Handle<Object> element_value;
            ASSIGN_RETURN_ON_EXCEPTION_VALUE(
                isolate, element_value,
                JSReceiver::GetElement(isolate, array, j), false);
            if (!visitor->visit(j, element_value)) return false;
          }
        }
      });
      break;
    }

    case DICTIONARY_ELEMENTS: {
      Handle<NumberDictionary> dict(array->element_dictionary(), isolate);
      std::vector<uint32_t> indices;
      indices.reserve(dict->Capacity() / 2);

      // Collect all indices in the object and the prototypes less
      // than length. This might introduce duplicates in the indices list.
      CollectElementIndices(isolate, array, length, &indices);
      std::sort(indices.begin(), indices.end());
      size_t n = indices.size();
      FOR_WITH_HANDLE_SCOPE(isolate, size_t, j = 0, j, j < n, (void)0, {
        uint32_t index = indices[j];
        Handle<Object> element;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, element, JSReceiver::GetElement(isolate, array, index),
            false);
        if (!visitor->visit(index, element)) return false;
        // Skip to next different index (i.e., omit duplicates).
        do {
          j++;
        } while (j < n && indices[j] == index);
      });
      break;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS: {
      FOR_WITH_HANDLE_SCOPE(
          isolate, uint32_t, index = 0, index, index < length, index++, {
            Handle<Object> element;
            ASSIGN_RETURN_ON_EXCEPTION_VALUE(
                isolate, element, JSReceiver::GetElement(isolate, array, index),
                false);
            if (!visitor->visit(index, element)) return false;
          });
      break;
    }
    case NO_ELEMENTS:
      break;
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      return IterateElementsSlow(isolate, receiver, length, visitor);
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
      // |array| is guaranteed to be an array or typed array.
      UNREACHABLE();
  }
  visitor->increase_index_offset(length);
  return true;
}

static Maybe<bool> IsConcatSpreadable(Isolate* isolate, Handle<Object> obj) {
  HandleScope handle_scope(isolate);
  if (!obj->IsJSReceiver()) return Just(false);
  if (!isolate->IsIsConcatSpreadableLookupChainIntact(JSReceiver::cast(*obj))) {
    // Slow path if @@isConcatSpreadable has been used.
    Handle<Symbol> key(isolate->factory()->is_concat_spreadable_symbol());
    Handle<Object> value;
    MaybeHandle<Object> maybeValue =
        i::Runtime::GetObjectProperty(isolate, obj, key);
    if (!maybeValue.ToHandle(&value)) return Nothing<bool>();
    if (!value->IsUndefined(isolate)) return Just(value->BooleanValue(isolate));
  }
  return Object::IsArray(obj);
}

Object Slow_ArrayConcat(BuiltinArguments* args, Handle<Object> species,
                        Isolate* isolate) {
  int argument_count = args->length();

  bool is_array_species = *species == isolate->context().array_function();

  // Pass 1: estimate the length and number of elements of the result.
  // The actual length can be larger if any of the arguments have getters
  // that mutate other arguments (but will otherwise be precise).
  // The number of elements is precise if there are no inherited elements.

  ElementsKind kind = PACKED_SMI_ELEMENTS;

  uint32_t estimate_result_length = 0;
  uint32_t estimate_nof = 0;
  FOR_WITH_HANDLE_SCOPE(isolate, int, i = 0, i, i < argument_count, i++, {
    Handle<Object> obj = args->at(i);
    uint32_t length_estimate;
    uint32_t element_estimate;
    if (obj->IsJSArray()) {
      Handle<JSArray> array(Handle<JSArray>::cast(obj));
      length_estimate = static_cast<uint32_t>(array->length().Number());
      if (length_estimate != 0) {
        ElementsKind array_kind =
            GetPackedElementsKind(array->GetElementsKind());
        if (IsFrozenOrSealedElementsKind(array_kind)) {
          array_kind = PACKED_ELEMENTS;
        }
        kind = GetMoreGeneralElementsKind(kind, array_kind);
      }
      element_estimate = EstimateElementCount(isolate, array);
    } else {
      if (obj->IsHeapObject()) {
        kind = GetMoreGeneralElementsKind(
            kind, obj->IsNumber() ? PACKED_DOUBLE_ELEMENTS : PACKED_ELEMENTS);
      }
      length_estimate = 1;
      element_estimate = 1;
    }
    // Avoid overflows by capping at kMaxElementCount.
    if (JSObject::kMaxElementCount - estimate_result_length < length_estimate) {
      estimate_result_length = JSObject::kMaxElementCount;
    } else {
      estimate_result_length += length_estimate;
    }
    if (JSObject::kMaxElementCount - estimate_nof < element_estimate) {
      estimate_nof = JSObject::kMaxElementCount;
    } else {
      estimate_nof += element_estimate;
    }
  });

  // If estimated number of elements is more than half of length, a
  // fixed array (fast case) is more time and space-efficient than a
  // dictionary.
  bool fast_case = is_array_species &&
                   (estimate_nof * 2) >= estimate_result_length &&
                   isolate->IsIsConcatSpreadableLookupChainIntact();

  if (fast_case && kind == PACKED_DOUBLE_ELEMENTS) {
    Handle<FixedArrayBase> storage =
        isolate->factory()->NewFixedDoubleArray(estimate_result_length);
    int j = 0;
    bool failure = false;
    if (estimate_result_length > 0) {
      Handle<FixedDoubleArray> double_storage =
          Handle<FixedDoubleArray>::cast(storage);
      for (int i = 0; i < argument_count; i++) {
        Handle<Object> obj = args->at(i);
        if (obj->IsSmi()) {
          double_storage->set(j, Smi::ToInt(*obj));
          j++;
        } else if (obj->IsNumber()) {
          double_storage->set(j, obj->Number());
          j++;
        } else {
          DisallowHeapAllocation no_gc;
          JSArray array = JSArray::cast(*obj);
          uint32_t length = static_cast<uint32_t>(array.length().Number());
          switch (array.GetElementsKind()) {
            case HOLEY_DOUBLE_ELEMENTS:
            case PACKED_DOUBLE_ELEMENTS: {
              // Empty array is FixedArray but not FixedDoubleArray.
              if (length == 0) break;
              FixedDoubleArray elements =
                  FixedDoubleArray::cast(array.elements());
              for (uint32_t i = 0; i < length; i++) {
                if (elements.is_the_hole(i)) {
                  // TODO(jkummerow/verwaest): We could be a bit more clever
                  // here: Check if there are no elements/getters on the
                  // prototype chain, and if so, allow creation of a holey
                  // result array.
                  // Same thing below (holey smi case).
                  failure = true;
                  break;
                }
                double double_value = elements.get_scalar(i);
                double_storage->set(j, double_value);
                j++;
              }
              break;
            }
            case HOLEY_SMI_ELEMENTS:
            case PACKED_SMI_ELEMENTS: {
              Object the_hole = ReadOnlyRoots(isolate).the_hole_value();
              FixedArray elements(FixedArray::cast(array.elements()));
              for (uint32_t i = 0; i < length; i++) {
                Object element = elements.get(i);
                if (element == the_hole) {
                  failure = true;
                  break;
                }
                int32_t int_value = Smi::ToInt(element);
                double_storage->set(j, int_value);
                j++;
              }
              break;
            }
            case HOLEY_ELEMENTS:
            case HOLEY_FROZEN_ELEMENTS:
            case HOLEY_SEALED_ELEMENTS:
            case PACKED_ELEMENTS:
            case PACKED_FROZEN_ELEMENTS:
            case PACKED_SEALED_ELEMENTS:
            case DICTIONARY_ELEMENTS:
            case NO_ELEMENTS:
              DCHECK_EQ(0u, length);
              break;
            default:
              UNREACHABLE();
          }
        }
        if (failure) break;
      }
    }
    if (!failure) {
      return *isolate->factory()->NewJSArrayWithElements(storage, kind, j);
    }
    // In case of failure, fall through.
  }

  Handle<HeapObject> storage;
  if (fast_case) {
    // The backing storage array must have non-existing elements to preserve
    // holes across concat operations.
    storage =
        isolate->factory()->NewFixedArrayWithHoles(estimate_result_length);
  } else if (is_array_species) {
    storage = NumberDictionary::New(isolate, estimate_nof);
  } else {
    DCHECK(species->IsConstructor());
    Handle<Object> length(Smi::kZero, isolate);
    Handle<Object> storage_object;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, storage_object,
        Execution::New(isolate, species, species, 1, &length));
    storage = Handle<HeapObject>::cast(storage_object);
  }

  ArrayConcatVisitor visitor(isolate, storage, fast_case);

  for (int i = 0; i < argument_count; i++) {
    Handle<Object> obj = args->at(i);
    Maybe<bool> spreadable = IsConcatSpreadable(isolate, obj);
    MAYBE_RETURN(spreadable, ReadOnlyRoots(isolate).exception());
    if (spreadable.FromJust()) {
      Handle<JSReceiver> object = Handle<JSReceiver>::cast(obj);
      if (!IterateElements(isolate, object, &visitor)) {
        return ReadOnlyRoots(isolate).exception();
      }
    } else {
      if (!visitor.visit(0, obj)) return ReadOnlyRoots(isolate).exception();
      visitor.increase_index_offset(1);
    }
  }

  if (visitor.exceeds_array_limit()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayLength));
  }

  if (is_array_species) {
    return *visitor.ToArray();
  } else {
    RETURN_RESULT_OR_FAILURE(isolate, visitor.ToJSReceiver());
  }
}

bool IsSimpleArray(Isolate* isolate, Handle<JSArray> obj) {
  DisallowHeapAllocation no_gc;
  Map map = obj->map();
  // If there is only the 'length' property we are fine.
  if (map.prototype() == isolate->native_context()->initial_array_prototype() &&
      map.NumberOfOwnDescriptors() == 1) {
    return true;
  }
  // TODO(cbruni): slower lookup for array subclasses and support slow
  // @@IsConcatSpreadable lookup.
  return false;
}

MaybeHandle<JSArray> Fast_ArrayConcat(Isolate* isolate,
                                      BuiltinArguments* args) {
  if (!isolate->IsIsConcatSpreadableLookupChainIntact()) {
    return MaybeHandle<JSArray>();
  }
  // We shouldn't overflow when adding another len.
  const int kHalfOfMaxInt = 1 << (kBitsPerInt - 2);
  STATIC_ASSERT(FixedArray::kMaxLength < kHalfOfMaxInt);
  STATIC_ASSERT(FixedDoubleArray::kMaxLength < kHalfOfMaxInt);
  USE(kHalfOfMaxInt);

  int n_arguments = args->length();
  int result_len = 0;
  {
    DisallowHeapAllocation no_gc;
    // Iterate through all the arguments performing checks
    // and calculating total length.
    for (int i = 0; i < n_arguments; i++) {
      Object arg = (*args)[i];
      if (!arg.IsJSArray()) return MaybeHandle<JSArray>();
      if (!HasOnlySimpleReceiverElements(isolate, JSObject::cast(arg))) {
        return MaybeHandle<JSArray>();
      }
      // TODO(cbruni): support fast concatenation of DICTIONARY_ELEMENTS.
      if (!JSObject::cast(arg).HasFastElements()) {
        return MaybeHandle<JSArray>();
      }
      Handle<JSArray> array(JSArray::cast(arg), isolate);
      if (!IsSimpleArray(isolate, array)) {
        return MaybeHandle<JSArray>();
      }
      // The Array length is guaranted to be <= kHalfOfMaxInt thus we won't
      // overflow.
      result_len += Smi::ToInt(array->length());
      DCHECK_GE(result_len, 0);
      // Throw an Error if we overflow the FixedArray limits
      if (FixedDoubleArray::kMaxLength < result_len ||
          FixedArray::kMaxLength < result_len) {
        AllowHeapAllocation gc;
        THROW_NEW_ERROR(isolate,
                        NewRangeError(MessageTemplate::kInvalidArrayLength),
                        JSArray);
      }
    }
  }
  return ElementsAccessor::Concat(isolate, args, n_arguments, result_len);
}

}  // namespace

// ES6 22.1.3.1 Array.prototype.concat
BUILTIN(ArrayConcat) {
  HandleScope scope(isolate);

  Handle<Object> receiver = args.receiver();
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, receiver,
      Object::ToObject(isolate, args.receiver(), "Array.prototype.concat"));
  args.set_at(0, *receiver);

  Handle<JSArray> result_array;

  // Avoid a real species read to avoid extra lookups to the array constructor
  if (V8_LIKELY(receiver->IsJSArray() &&
                Handle<JSArray>::cast(receiver)->HasArrayPrototype(isolate) &&
                isolate->IsArraySpeciesLookupChainIntact())) {
    if (Fast_ArrayConcat(isolate, &args).ToHandle(&result_array)) {
      return *result_array;
    }
    if (isolate->has_pending_exception())
      return ReadOnlyRoots(isolate).exception();
  }
  // Reading @@species happens before anything else with a side effect, so
  // we can do it here to determine whether to take the fast path.
  Handle<Object> species;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, species, Object::ArraySpeciesConstructor(isolate, receiver));
  if (*species == *isolate->array_function()) {
    if (Fast_ArrayConcat(isolate, &args).ToHandle(&result_array)) {
      return *result_array;
    }
    if (isolate->has_pending_exception())
      return ReadOnlyRoots(isolate).exception();
  }
  return Slow_ArrayConcat(&args, species, isolate);
}

}  // namespace internal
}  // namespace v8
