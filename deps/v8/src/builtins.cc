// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins.h"

#include "src/api.h"
#include "src/api-natives.h"
#include "src/arguments.h"
#include "src/base/once.h"
#include "src/bootstrapper.h"
#include "src/dateparser-inl.h"
#include "src/elements.h"
#include "src/frames-inl.h"
#include "src/gdb-jit.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/profiler/cpu-profiler.h"
#include "src/property-descriptor.h"
#include "src/prototype.h"
#include "src/string-builder.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

namespace {

// Arguments object passed to C++ builtins.
template <BuiltinExtraArguments extra_args>
class BuiltinArguments : public Arguments {
 public:
  BuiltinArguments(int length, Object** arguments)
      : Arguments(length, arguments) {
    // Check we have at least the receiver.
    DCHECK_LE(1, this->length());
  }

  Object*& operator[] (int index) {
    DCHECK(index < length());
    return Arguments::operator[](index);
  }

  template <class S> Handle<S> at(int index) {
    DCHECK(index < length());
    return Arguments::at<S>(index);
  }

  Handle<Object> atOrUndefined(Isolate* isolate, int index) {
    if (index >= length()) {
      return isolate->factory()->undefined_value();
    }
    return at<Object>(index);
  }

  Handle<Object> receiver() {
    return Arguments::at<Object>(0);
  }

  template <class S>
  Handle<S> target();
  Handle<HeapObject> new_target();

  // Gets the total number of arguments including the receiver (but
  // excluding extra arguments).
  int length() const;
};


// Specialize BuiltinArguments for the extra arguments.

template <>
int BuiltinArguments<BuiltinExtraArguments::kNone>::length() const {
  return Arguments::length();
}

template <>
int BuiltinArguments<BuiltinExtraArguments::kTarget>::length() const {
  return Arguments::length() - 1;
}

template <>
template <class S>
Handle<S> BuiltinArguments<BuiltinExtraArguments::kTarget>::target() {
  return Arguments::at<S>(Arguments::length() - 1);
}

template <>
int BuiltinArguments<BuiltinExtraArguments::kNewTarget>::length() const {
  return Arguments::length() - 1;
}

template <>
Handle<HeapObject>
BuiltinArguments<BuiltinExtraArguments::kNewTarget>::new_target() {
  return Arguments::at<HeapObject>(Arguments::length() - 1);
}

template <>
int BuiltinArguments<BuiltinExtraArguments::kTargetAndNewTarget>::length()
    const {
  return Arguments::length() - 2;
}

template <>
template <class S>
Handle<S>
BuiltinArguments<BuiltinExtraArguments::kTargetAndNewTarget>::target() {
  return Arguments::at<S>(Arguments::length() - 2);
}

template <>
Handle<HeapObject>
BuiltinArguments<BuiltinExtraArguments::kTargetAndNewTarget>::new_target() {
  return Arguments::at<HeapObject>(Arguments::length() - 1);
}


#define DEF_ARG_TYPE(name, spec) \
  typedef BuiltinArguments<BuiltinExtraArguments::spec> name##ArgumentsType;
BUILTIN_LIST_C(DEF_ARG_TYPE)
#undef DEF_ARG_TYPE


// ----------------------------------------------------------------------------
// Support macro for defining builtins in C++.
// ----------------------------------------------------------------------------
//
// A builtin function is defined by writing:
//
//   BUILTIN(name) {
//     ...
//   }
//
// In the body of the builtin function the arguments can be accessed
// through the BuiltinArguments object args.

#define BUILTIN(name)                                                          \
  MUST_USE_RESULT static Object* Builtin_Impl_##name(name##ArgumentsType args, \
                                                     Isolate* isolate);        \
  MUST_USE_RESULT static Object* Builtin_##name(                               \
      int args_length, Object** args_object, Isolate* isolate) {               \
    isolate->counters()->runtime_calls()->Increment();                         \
    RuntimeCallStats* stats = isolate->counters()->runtime_call_stats();       \
    RuntimeCallTimerScope timer(isolate, &stats->Builtin_##name);              \
    name##ArgumentsType args(args_length, args_object);                        \
    Object* value = Builtin_Impl_##name(args, isolate);                        \
    return value;                                                              \
  }                                                                            \
                                                                               \
  MUST_USE_RESULT static Object* Builtin_Impl_##name(name##ArgumentsType args, \
                                                     Isolate* isolate)

// ----------------------------------------------------------------------------


#define CHECK_RECEIVER(Type, name, method)                                  \
  if (!args.receiver()->Is##Type()) {                                       \
    THROW_NEW_ERROR_RETURN_FAILURE(                                         \
        isolate,                                                            \
        NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,          \
                     isolate->factory()->NewStringFromAsciiChecked(method), \
                     args.receiver()));                                     \
  }                                                                         \
  Handle<Type> name = Handle<Type>::cast(args.receiver())


inline bool ClampedToInteger(Object* object, int* out) {
  // This is an extended version of ECMA-262 7.1.11 handling signed values
  // Try to convert object to a number and clamp values to [kMinInt, kMaxInt]
  if (object->IsSmi()) {
    *out = Smi::cast(object)->value();
    return true;
  } else if (object->IsHeapNumber()) {
    double value = HeapNumber::cast(object)->value();
    if (std::isnan(value)) {
      *out = 0;
    } else if (value > kMaxInt) {
      *out = kMaxInt;
    } else if (value < kMinInt) {
      *out = kMinInt;
    } else {
      *out = static_cast<int>(value);
    }
    return true;
  } else if (object->IsUndefined() || object->IsNull()) {
    *out = 0;
    return true;
  } else if (object->IsBoolean()) {
    *out = object->IsTrue();
    return true;
  }
  return false;
}


inline bool GetSloppyArgumentsLength(Isolate* isolate, Handle<JSObject> object,
                                     int* out) {
  Map* arguments_map = isolate->native_context()->sloppy_arguments_map();
  if (object->map() != arguments_map) return false;
  DCHECK(object->HasFastElements());
  Object* len_obj = object->InObjectPropertyAt(JSArgumentsObject::kLengthIndex);
  if (!len_obj->IsSmi()) return false;
  *out = Max(0, Smi::cast(len_obj)->value());
  return *out <= object->elements()->length();
}


inline bool PrototypeHasNoElements(PrototypeIterator* iter) {
  DisallowHeapAllocation no_gc;
  for (; !iter->IsAtEnd(); iter->Advance()) {
    if (iter->GetCurrent()->IsJSProxy()) return false;
    JSObject* current = iter->GetCurrent<JSObject>();
    if (current->IsAccessCheckNeeded()) return false;
    if (current->HasIndexedInterceptor()) return false;
    if (current->HasStringWrapperElements()) return false;
    if (current->elements()->length() != 0) return false;
  }
  return true;
}

inline bool IsJSArrayFastElementMovingAllowed(Isolate* isolate,
                                              JSArray* receiver) {
  DisallowHeapAllocation no_gc;
  // If the array prototype chain is intact (and free of elements), and if the
  // receiver's prototype is the array prototype, then we are done.
  Object* prototype = receiver->map()->prototype();
  if (prototype->IsJSArray() &&
      isolate->is_initial_array_prototype(JSArray::cast(prototype)) &&
      isolate->IsFastArrayConstructorPrototypeChainIntact()) {
    return true;
  }

  // Slow case.
  PrototypeIterator iter(isolate, receiver);
  return PrototypeHasNoElements(&iter);
}

inline bool HasSimpleElements(JSObject* current) {
  if (current->IsAccessCheckNeeded()) return false;
  if (current->HasIndexedInterceptor()) return false;
  if (current->HasStringWrapperElements()) return false;
  if (current->GetElementsAccessor()->HasAccessors(current)) return false;
  return true;
}

inline bool HasOnlySimpleReceiverElements(Isolate* isolate,
                                          JSReceiver* receiver) {
  // Check that we have no accessors on the receiver's elements.
  JSObject* object = JSObject::cast(receiver);
  if (!HasSimpleElements(object)) return false;
  // Check that ther are not elements on the prototype.
  DisallowHeapAllocation no_gc;
  PrototypeIterator iter(isolate, receiver);
  return PrototypeHasNoElements(&iter);
}

inline bool HasOnlySimpleElements(Isolate* isolate, JSReceiver* receiver) {
  // Check that ther are not elements on the prototype.
  DisallowHeapAllocation no_gc;
  PrototypeIterator iter(isolate, receiver,
                         PrototypeIterator::START_AT_RECEIVER);
  for (; !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrent()->IsJSProxy()) return false;
    JSObject* current = iter.GetCurrent<JSObject>();
    if (!HasSimpleElements(current)) return false;
  }
  return true;
}

// Returns empty handle if not applicable.
MUST_USE_RESULT
inline MaybeHandle<FixedArrayBase> EnsureJSArrayWithWritableFastElements(
    Isolate* isolate, Handle<Object> receiver, Arguments* args,
    int first_added_arg) {
  // We explicitly add a HandleScope to avoid creating several copies of the
  // same handle which would otherwise cause issue when left-trimming later-on.
  HandleScope scope(isolate);
  if (!receiver->IsJSArray()) return MaybeHandle<FixedArrayBase>();
  Handle<JSArray> array = Handle<JSArray>::cast(receiver);
  // If there may be elements accessors in the prototype chain, the fast path
  // cannot be used if there arguments to add to the array.
  Heap* heap = isolate->heap();
  if (args != NULL && !IsJSArrayFastElementMovingAllowed(isolate, *array)) {
    return MaybeHandle<FixedArrayBase>();
  }
  if (array->map()->is_observed()) return MaybeHandle<FixedArrayBase>();
  if (!array->map()->is_extensible()) return MaybeHandle<FixedArrayBase>();
  Handle<FixedArrayBase> elms(array->elements(), isolate);
  Map* map = elms->map();
  if (map == heap->fixed_array_map()) {
    if (args == NULL || array->HasFastObjectElements()) {
      return scope.CloseAndEscape(elms);
    }
  } else if (map == heap->fixed_cow_array_map()) {
    elms = JSObject::EnsureWritableFastElements(array);
    if (args == NULL || array->HasFastObjectElements()) {
      return scope.CloseAndEscape(elms);
    }
  } else if (map == heap->fixed_double_array_map()) {
    if (args == NULL) {
      return scope.CloseAndEscape(elms);
    }
  } else {
    return MaybeHandle<FixedArrayBase>();
  }

  // Adding elements to the array prototype would break code that makes sure
  // it has no elements. Handle that elsewhere.
  if (isolate->IsAnyInitialArrayPrototype(array)) {
    return MaybeHandle<FixedArrayBase>();
  }

  // Need to ensure that the arguments passed in args can be contained in
  // the array.
  int args_length = args->length();
  if (first_added_arg >= args_length) {
    return scope.CloseAndEscape(elms);
  }

  ElementsKind origin_kind = array->map()->elements_kind();
  DCHECK(!IsFastObjectElementsKind(origin_kind));
  ElementsKind target_kind = origin_kind;
  {
    DisallowHeapAllocation no_gc;
    int arg_count = args_length - first_added_arg;
    Object** arguments = args->arguments() - first_added_arg - (arg_count - 1);
    for (int i = 0; i < arg_count; i++) {
      Object* arg = arguments[i];
      if (arg->IsHeapObject()) {
        if (arg->IsHeapNumber()) {
          target_kind = FAST_DOUBLE_ELEMENTS;
        } else {
          target_kind = FAST_ELEMENTS;
          break;
        }
      }
    }
  }
  if (target_kind != origin_kind) {
    JSObject::TransitionElementsKind(array, target_kind);
    elms = handle(array->elements(), isolate);
  }
  return scope.CloseAndEscape(elms);
}


MUST_USE_RESULT static Object* CallJsIntrinsic(
    Isolate* isolate, Handle<JSFunction> function,
    BuiltinArguments<BuiltinExtraArguments::kNone> args) {
  HandleScope handleScope(isolate);
  int argc = args.length() - 1;
  ScopedVector<Handle<Object> > argv(argc);
  for (int i = 0; i < argc; ++i) {
    argv[i] = args.at<Object>(i + 1);
  }
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate,
                      function,
                      args.receiver(),
                      argc,
                      argv.start()));
  return *result;
}


}  // namespace


BUILTIN(Illegal) {
  UNREACHABLE();
  return isolate->heap()->undefined_value();  // Make compiler happy.
}


BUILTIN(EmptyFunction) { return isolate->heap()->undefined_value(); }


BUILTIN(ArrayPush) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  MaybeHandle<FixedArrayBase> maybe_elms_obj =
      EnsureJSArrayWithWritableFastElements(isolate, receiver, &args, 1);
  Handle<FixedArrayBase> elms_obj;
  if (!maybe_elms_obj.ToHandle(&elms_obj)) {
    return CallJsIntrinsic(isolate, isolate->array_push(), args);
  }
  // Fast Elements Path
  int push_size = args.length() - 1;
  Handle<JSArray> array = Handle<JSArray>::cast(receiver);
  int len = Smi::cast(array->length())->value();
  if (push_size == 0) {
    return Smi::FromInt(len);
  }
  if (push_size > 0 &&
      JSArray::WouldChangeReadOnlyLength(array, len + push_size)) {
    return CallJsIntrinsic(isolate, isolate->array_push(), args);
  }
  DCHECK(!array->map()->is_observed());
  ElementsAccessor* accessor = array->GetElementsAccessor();
  int new_length = accessor->Push(array, elms_obj, &args, push_size);
  return Smi::FromInt(new_length);
}


BUILTIN(ArrayPop) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  MaybeHandle<FixedArrayBase> maybe_elms_obj =
      EnsureJSArrayWithWritableFastElements(isolate, receiver, NULL, 0);
  Handle<FixedArrayBase> elms_obj;
  if (!maybe_elms_obj.ToHandle(&elms_obj)) {
    return CallJsIntrinsic(isolate, isolate->array_pop(), args);
  }

  Handle<JSArray> array = Handle<JSArray>::cast(receiver);
  DCHECK(!array->map()->is_observed());

  uint32_t len = static_cast<uint32_t>(Smi::cast(array->length())->value());
  if (len == 0) return isolate->heap()->undefined_value();

  if (JSArray::HasReadOnlyLength(array)) {
    return CallJsIntrinsic(isolate, isolate->array_pop(), args);
  }

  Handle<Object> result;
  if (IsJSArrayFastElementMovingAllowed(isolate, JSArray::cast(*receiver))) {
    // Fast Elements Path
    result = array->GetElementsAccessor()->Pop(array, elms_obj);
  } else {
    // Use Slow Lookup otherwise
    uint32_t new_length = len - 1;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, Object::GetElement(isolate, array, new_length));
    JSArray::SetLength(array, new_length);
  }
  return *result;
}


BUILTIN(ArrayShift) {
  HandleScope scope(isolate);
  Heap* heap = isolate->heap();
  Handle<Object> receiver = args.receiver();
  MaybeHandle<FixedArrayBase> maybe_elms_obj =
      EnsureJSArrayWithWritableFastElements(isolate, receiver, NULL, 0);
  Handle<FixedArrayBase> elms_obj;
  if (!maybe_elms_obj.ToHandle(&elms_obj) ||
      !IsJSArrayFastElementMovingAllowed(isolate, JSArray::cast(*receiver))) {
    return CallJsIntrinsic(isolate, isolate->array_shift(), args);
  }
  Handle<JSArray> array = Handle<JSArray>::cast(receiver);
  DCHECK(!array->map()->is_observed());

  int len = Smi::cast(array->length())->value();
  if (len == 0) return heap->undefined_value();

  if (JSArray::HasReadOnlyLength(array)) {
    return CallJsIntrinsic(isolate, isolate->array_shift(), args);
  }

  Handle<Object> first = array->GetElementsAccessor()->Shift(array, elms_obj);
  return *first;
}


BUILTIN(ArrayUnshift) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  MaybeHandle<FixedArrayBase> maybe_elms_obj =
      EnsureJSArrayWithWritableFastElements(isolate, receiver, &args, 1);
  Handle<FixedArrayBase> elms_obj;
  if (!maybe_elms_obj.ToHandle(&elms_obj)) {
    return CallJsIntrinsic(isolate, isolate->array_unshift(), args);
  }
  Handle<JSArray> array = Handle<JSArray>::cast(receiver);
  DCHECK(!array->map()->is_observed());
  int to_add = args.length() - 1;
  if (to_add == 0) {
    return array->length();
  }
  // Currently fixed arrays cannot grow too big, so
  // we should never hit this case.
  DCHECK(to_add <= (Smi::kMaxValue - Smi::cast(array->length())->value()));

  if (to_add > 0 && JSArray::HasReadOnlyLength(array)) {
    return CallJsIntrinsic(isolate, isolate->array_unshift(), args);
  }

  ElementsAccessor* accessor = array->GetElementsAccessor();
  int new_length = accessor->Unshift(array, elms_obj, &args, to_add);
  return Smi::FromInt(new_length);
}


BUILTIN(ArraySlice) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  Handle<JSObject> object;
  Handle<FixedArrayBase> elms_obj;
  int len = -1;
  int relative_start = 0;
  int relative_end = 0;
  bool is_sloppy_arguments = false;

  if (receiver->IsJSArray()) {
    DisallowHeapAllocation no_gc;
    JSArray* array = JSArray::cast(*receiver);
    if (!array->HasFastElements() ||
        !IsJSArrayFastElementMovingAllowed(isolate, array) ||
        !isolate->IsArraySpeciesLookupChainIntact() ||
        // If this is a subclass of Array, then call out to JS
        !array->map()->new_target_is_base()) {
      AllowHeapAllocation allow_allocation;
      return CallJsIntrinsic(isolate, isolate->array_slice(), args);
    }
    len = Smi::cast(array->length())->value();
    object = Handle<JSObject>::cast(receiver);
    elms_obj = handle(array->elements(), isolate);
  } else if (receiver->IsJSObject() &&
             GetSloppyArgumentsLength(isolate, Handle<JSObject>::cast(receiver),
                                      &len)) {
    // Array.prototype.slice(arguments, ...) is quite a common idiom
    // (notably more than 50% of invocations in Web apps).
    // Treat it in C++ as well.
    is_sloppy_arguments = true;
    object = Handle<JSObject>::cast(receiver);
    elms_obj = handle(object->elements(), isolate);
  } else {
    AllowHeapAllocation allow_allocation;
    return CallJsIntrinsic(isolate, isolate->array_slice(), args);
  }
  DCHECK(len >= 0);
  int argument_count = args.length() - 1;
  // Note carefully chosen defaults---if argument is missing,
  // it's undefined which gets converted to 0 for relative_start
  // and to len for relative_end.
  relative_start = 0;
  relative_end = len;
  if (argument_count > 0) {
    DisallowHeapAllocation no_gc;
    if (!ClampedToInteger(args[1], &relative_start)) {
      AllowHeapAllocation allow_allocation;
      return CallJsIntrinsic(isolate, isolate->array_slice(), args);
    }
    if (argument_count > 1) {
      Object* end_arg = args[2];
      // slice handles the end_arg specially
      if (end_arg->IsUndefined()) {
        relative_end = len;
      } else if (!ClampedToInteger(end_arg, &relative_end)) {
        AllowHeapAllocation allow_allocation;
        return CallJsIntrinsic(isolate, isolate->array_slice(), args);
      }
    }
  }

  // ECMAScript 232, 3rd Edition, Section 15.4.4.10, step 6.
  uint32_t actual_start = (relative_start < 0) ? Max(len + relative_start, 0)
                                               : Min(relative_start, len);

  // ECMAScript 232, 3rd Edition, Section 15.4.4.10, step 8.
  uint32_t actual_end =
      (relative_end < 0) ? Max(len + relative_end, 0) : Min(relative_end, len);

  if (actual_end <= actual_start) {
    Handle<JSArray> result_array = isolate->factory()->NewJSArray(
        GetPackedElementsKind(object->GetElementsKind()), 0, 0);
    return *result_array;
  }

  ElementsAccessor* accessor = object->GetElementsAccessor();
  if (is_sloppy_arguments &&
      !accessor->IsPacked(object, elms_obj, actual_start, actual_end)) {
    // Don't deal with arguments with holes in C++
    AllowHeapAllocation allow_allocation;
    return CallJsIntrinsic(isolate, isolate->array_slice(), args);
  }
  Handle<JSArray> result_array =
      accessor->Slice(object, elms_obj, actual_start, actual_end);
  return *result_array;
}


BUILTIN(ArraySplice) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  MaybeHandle<FixedArrayBase> maybe_elms_obj =
      EnsureJSArrayWithWritableFastElements(isolate, receiver, &args, 3);
  Handle<FixedArrayBase> elms_obj;
  if (!maybe_elms_obj.ToHandle(&elms_obj) ||
      // If this is a subclass of Array, then call out to JS
      !JSArray::cast(*receiver)->map()->new_target_is_base() ||
      // If anything with @@species has been messed with, call out to JS
      !isolate->IsArraySpeciesLookupChainIntact()) {
    return CallJsIntrinsic(isolate, isolate->array_splice(), args);
  }
  Handle<JSArray> array = Handle<JSArray>::cast(receiver);
  DCHECK(!array->map()->is_observed());

  int argument_count = args.length() - 1;
  int relative_start = 0;
  if (argument_count > 0) {
    DisallowHeapAllocation no_gc;
    if (!ClampedToInteger(args[1], &relative_start)) {
      AllowHeapAllocation allow_allocation;
      return CallJsIntrinsic(isolate, isolate->array_splice(), args);
    }
  }
  int len = Smi::cast(array->length())->value();
  // clip relative start to [0, len]
  int actual_start = (relative_start < 0) ? Max(len + relative_start, 0)
                                          : Min(relative_start, len);

  int actual_delete_count;
  if (argument_count == 1) {
    // SpiderMonkey, TraceMonkey and JSC treat the case where no delete count is
    // given as a request to delete all the elements from the start.
    // And it differs from the case of undefined delete count.
    // This does not follow ECMA-262, but we do the same for compatibility.
    DCHECK(len - actual_start >= 0);
    actual_delete_count = len - actual_start;
  } else {
    int delete_count = 0;
    DisallowHeapAllocation no_gc;
    if (argument_count > 1) {
      if (!ClampedToInteger(args[2], &delete_count)) {
        AllowHeapAllocation allow_allocation;
        return CallJsIntrinsic(isolate, isolate->array_splice(), args);
      }
    }
    actual_delete_count = Min(Max(delete_count, 0), len - actual_start);
  }

  int add_count = (argument_count > 1) ? (argument_count - 2) : 0;
  int new_length = len - actual_delete_count + add_count;

  if (new_length != len && JSArray::HasReadOnlyLength(array)) {
    AllowHeapAllocation allow_allocation;
    return CallJsIntrinsic(isolate, isolate->array_splice(), args);
  }
  ElementsAccessor* accessor = array->GetElementsAccessor();
  Handle<JSArray> result_array = accessor->Splice(
      array, elms_obj, actual_start, actual_delete_count, &args, add_count);
  return *result_array;
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
  ArrayConcatVisitor(Isolate* isolate, Handle<Object> storage,
                     bool fast_elements)
      : isolate_(isolate),
        storage_(isolate->global_handles()->Create(*storage)),
        index_offset_(0u),
        bit_field_(FastElementsField::encode(fast_elements) |
                   ExceedsLimitField::encode(false) |
                   IsFixedArrayField::encode(storage->IsFixedArray())) {
    DCHECK(!(this->fast_elements() && !is_fixed_array()));
  }

  ~ArrayConcatVisitor() { clear_storage(); }

  bool visit(uint32_t i, Handle<Object> elm) {
    uint32_t index = index_offset_ + i;

    if (!is_fixed_array()) {
      Handle<Object> element_value;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate_, element_value,
          Object::SetElement(isolate_, storage_, index, elm, STRICT), false);
      return true;
    }

    if (i >= JSObject::kMaxElementCount - index_offset_) {
      set_exceeds_array_limit(true);
      // Exception hasn't been thrown at this point. Return true to
      // break out, and caller will throw. !visit would imply that
      // there is already a pending exception.
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
    Handle<SeededNumberDictionary> dict(
        SeededNumberDictionary::cast(*storage_));
    // The object holding this backing store has just been allocated, so
    // it cannot yet be used as a prototype.
    Handle<SeededNumberDictionary> result =
        SeededNumberDictionary::AtNumberPut(dict, index, elm, false);
    if (!result.is_identical_to(dict)) {
      // Dictionary needed to grow.
      clear_storage();
      set_storage(*result);
    }
    return true;
  }

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
            static_cast<uint32_t>(FixedArrayBase::cast(*storage_)->length())) {
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
        array, fast_elements() ? FAST_HOLEY_ELEMENTS : DICTIONARY_ELEMENTS);
    array->set_map(*map);
    array->set_length(*length);
    array->set_elements(*storage_fixed_array());
    return array;
  }

  // Storage is either a FixedArray (if is_fixed_array()) or a JSReciever
  // (otherwise)
  Handle<FixedArray> storage_fixed_array() {
    DCHECK(is_fixed_array());
    return Handle<FixedArray>::cast(storage_);
  }
  Handle<JSReceiver> storage_jsreceiver() {
    DCHECK(!is_fixed_array());
    return Handle<JSReceiver>::cast(storage_);
  }

 private:
  // Convert storage to dictionary mode.
  void SetDictionaryMode() {
    DCHECK(fast_elements() && is_fixed_array());
    Handle<FixedArray> current_storage = storage_fixed_array();
    Handle<SeededNumberDictionary> slow_storage(
        SeededNumberDictionary::New(isolate_, current_storage->length()));
    uint32_t current_length = static_cast<uint32_t>(current_storage->length());
    for (uint32_t i = 0; i < current_length; i++) {
      HandleScope loop_scope(isolate_);
      Handle<Object> element(current_storage->get(i), isolate_);
      if (!element->IsTheHole()) {
        // The object holding this backing store has just been allocated, so
        // it cannot yet be used as a prototype.
        Handle<SeededNumberDictionary> new_storage =
            SeededNumberDictionary::AtNumberPut(slow_storage, i, element,
                                                false);
        if (!new_storage.is_identical_to(slow_storage)) {
          slow_storage = loop_scope.CloseAndEscape(new_storage);
        }
      }
    }
    clear_storage();
    set_storage(*slow_storage);
    set_fast_elements(false);
  }

  inline void clear_storage() {
    GlobalHandles::Destroy(Handle<Object>::cast(storage_).location());
  }

  inline void set_storage(FixedArray* storage) {
    DCHECK(is_fixed_array());
    storage_ = isolate_->global_handles()->Create(storage);
  }

  class FastElementsField : public BitField<bool, 0, 1> {};
  class ExceedsLimitField : public BitField<bool, 1, 1> {};
  class IsFixedArrayField : public BitField<bool, 2, 1> {};

  bool fast_elements() const { return FastElementsField::decode(bit_field_); }
  void set_fast_elements(bool fast) {
    bit_field_ = FastElementsField::update(bit_field_, fast);
  }
  void set_exceeds_array_limit(bool exceeds) {
    bit_field_ = ExceedsLimitField::update(bit_field_, exceeds);
  }
  bool is_fixed_array() const { return IsFixedArrayField::decode(bit_field_); }

  Isolate* isolate_;
  Handle<Object> storage_;  // Always a global handle.
  // Index after last seen index. Always less than or equal to
  // JSObject::kMaxElementCount.
  uint32_t index_offset_;
  uint32_t bit_field_;
};


uint32_t EstimateElementCount(Handle<JSArray> array) {
  uint32_t length = static_cast<uint32_t>(array->length()->Number());
  int element_count = 0;
  switch (array->GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      // Fast elements can't have lengths that are not representable by
      // a 32-bit signed integer.
      DCHECK(static_cast<int32_t>(FixedArray::kMaxLength) >= 0);
      int fast_length = static_cast<int>(length);
      Handle<FixedArray> elements(FixedArray::cast(array->elements()));
      for (int i = 0; i < fast_length; i++) {
        if (!elements->get(i)->IsTheHole()) element_count++;
      }
      break;
    }
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS: {
      // Fast elements can't have lengths that are not representable by
      // a 32-bit signed integer.
      DCHECK(static_cast<int32_t>(FixedDoubleArray::kMaxLength) >= 0);
      int fast_length = static_cast<int>(length);
      if (array->elements()->IsFixedArray()) {
        DCHECK(FixedArray::cast(array->elements())->length() == 0);
        break;
      }
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(array->elements()));
      for (int i = 0; i < fast_length; i++) {
        if (!elements->is_the_hole(i)) element_count++;
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      Handle<SeededNumberDictionary> dictionary(
          SeededNumberDictionary::cast(array->elements()));
      int capacity = dictionary->Capacity();
      for (int i = 0; i < capacity; i++) {
        Handle<Object> key(dictionary->KeyAt(i), array->GetIsolate());
        if (dictionary->IsKey(*key)) {
          element_count++;
        }
      }
      break;
    }
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) case TYPE##_ELEMENTS:

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
      return 0;
  }
  // As an estimate, we assume that the prototype doesn't contain any
  // inherited elements.
  return element_count;
}


// Used for sorting indices in a List<uint32_t>.
int compareUInt32(const uint32_t* ap, const uint32_t* bp) {
  uint32_t a = *ap;
  uint32_t b = *bp;
  return (a == b) ? 0 : (a < b) ? -1 : 1;
}


void CollectElementIndices(Handle<JSObject> object, uint32_t range,
                           List<uint32_t>* indices) {
  Isolate* isolate = object->GetIsolate();
  ElementsKind kind = object->GetElementsKind();
  switch (kind) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      Handle<FixedArray> elements(FixedArray::cast(object->elements()));
      uint32_t length = static_cast<uint32_t>(elements->length());
      if (range < length) length = range;
      for (uint32_t i = 0; i < length; i++) {
        if (!elements->get(i)->IsTheHole()) {
          indices->Add(i);
        }
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      if (object->elements()->IsFixedArray()) {
        DCHECK(object->elements()->length() == 0);
        break;
      }
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(object->elements()));
      uint32_t length = static_cast<uint32_t>(elements->length());
      if (range < length) length = range;
      for (uint32_t i = 0; i < length; i++) {
        if (!elements->is_the_hole(i)) {
          indices->Add(i);
        }
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      Handle<SeededNumberDictionary> dict(
          SeededNumberDictionary::cast(object->elements()));
      uint32_t capacity = dict->Capacity();
      for (uint32_t j = 0; j < capacity; j++) {
        HandleScope loop_scope(isolate);
        Handle<Object> k(dict->KeyAt(j), isolate);
        if (dict->IsKey(*k)) {
          DCHECK(k->IsNumber());
          uint32_t index = static_cast<uint32_t>(k->Number());
          if (index < range) {
            indices->Add(index);
          }
        }
      }
      break;
    }
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      {
        uint32_t length = static_cast<uint32_t>(
            FixedArrayBase::cast(object->elements())->length());
        if (range <= length) {
          length = range;
          // We will add all indices, so we might as well clear it first
          // and avoid duplicates.
          indices->Clear();
        }
        for (uint32_t i = 0; i < length; i++) {
          indices->Add(i);
        }
        if (length == range) return;  // All indices accounted for already.
        break;
      }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS: {
      ElementsAccessor* accessor = object->GetElementsAccessor();
      for (uint32_t i = 0; i < range; i++) {
        if (accessor->HasElement(object, i)) {
          indices->Add(i);
        }
      }
      break;
    }
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS: {
      DCHECK(object->IsJSValue());
      Handle<JSValue> js_value = Handle<JSValue>::cast(object);
      DCHECK(js_value->value()->IsString());
      Handle<String> string(String::cast(js_value->value()), isolate);
      uint32_t length = static_cast<uint32_t>(string->length());
      uint32_t i = 0;
      uint32_t limit = Min(length, range);
      for (; i < limit; i++) {
        indices->Add(i);
      }
      ElementsAccessor* accessor = object->GetElementsAccessor();
      for (; i < range; i++) {
        if (accessor->HasElement(object, i)) {
          indices->Add(i);
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
    CollectElementIndices(PrototypeIterator::GetCurrent<JSObject>(iter), range,
                          indices);
  }
}


bool IterateElementsSlow(Isolate* isolate, Handle<JSReceiver> receiver,
                         uint32_t length, ArrayConcatVisitor* visitor) {
  for (uint32_t i = 0; i < length; ++i) {
    HandleScope loop_scope(isolate);
    Maybe<bool> maybe = JSReceiver::HasElement(receiver, i);
    if (!maybe.IsJust()) return false;
    if (maybe.FromJust()) {
      Handle<Object> element_value;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, element_value,
                                       Object::GetElement(isolate, receiver, i),
                                       false);
      if (!visitor->visit(i, element_value)) return false;
    }
  }
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
    length = static_cast<uint32_t>(array->length()->Number());
  } else {
    Handle<Object> val;
    Handle<Object> key = isolate->factory()->length_string();
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, val, Runtime::GetObjectProperty(isolate, receiver, key),
        false);
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, val,
                                     Object::ToLength(isolate, val), false);
    // TODO(caitp): Support larger element indexes (up to 2^53-1).
    if (!val->ToUint32(&length)) {
      length = 0;
    }
    // TODO(cbruni): handle other element kind as well
    return IterateElementsSlow(isolate, receiver, length, visitor);
  }

  if (!HasOnlySimpleElements(isolate, *receiver)) {
    return IterateElementsSlow(isolate, receiver, length, visitor);
  }
  Handle<JSObject> array = Handle<JSObject>::cast(receiver);

  switch (array->GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      // Run through the elements FixedArray and use HasElement and GetElement
      // to check the prototype for missing elements.
      Handle<FixedArray> elements(FixedArray::cast(array->elements()));
      int fast_length = static_cast<int>(length);
      DCHECK_LE(fast_length, elements->length());
      for (int j = 0; j < fast_length; j++) {
        HandleScope loop_scope(isolate);
        Handle<Object> element_value(elements->get(j), isolate);
        if (!element_value->IsTheHole()) {
          if (!visitor->visit(j, element_value)) return false;
        } else {
          Maybe<bool> maybe = JSReceiver::HasElement(array, j);
          if (!maybe.IsJust()) return false;
          if (maybe.FromJust()) {
            // Call GetElement on array, not its prototype, or getters won't
            // have the correct receiver.
            ASSIGN_RETURN_ON_EXCEPTION_VALUE(
                isolate, element_value, Object::GetElement(isolate, array, j),
                false);
            if (!visitor->visit(j, element_value)) return false;
          }
        }
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      // Empty array is FixedArray but not FixedDoubleArray.
      if (length == 0) break;
      // Run through the elements FixedArray and use HasElement and GetElement
      // to check the prototype for missing elements.
      if (array->elements()->IsFixedArray()) {
        DCHECK(array->elements()->length() == 0);
        break;
      }
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(array->elements()));
      int fast_length = static_cast<int>(length);
      DCHECK(fast_length <= elements->length());
      for (int j = 0; j < fast_length; j++) {
        HandleScope loop_scope(isolate);
        if (!elements->is_the_hole(j)) {
          double double_value = elements->get_scalar(j);
          Handle<Object> element_value =
              isolate->factory()->NewNumber(double_value);
          if (!visitor->visit(j, element_value)) return false;
        } else {
          Maybe<bool> maybe = JSReceiver::HasElement(array, j);
          if (!maybe.IsJust()) return false;
          if (maybe.FromJust()) {
            // Call GetElement on array, not its prototype, or getters won't
            // have the correct receiver.
            Handle<Object> element_value;
            ASSIGN_RETURN_ON_EXCEPTION_VALUE(
                isolate, element_value, Object::GetElement(isolate, array, j),
                false);
            if (!visitor->visit(j, element_value)) return false;
          }
        }
      }
      break;
    }

    case DICTIONARY_ELEMENTS: {
      Handle<SeededNumberDictionary> dict(array->element_dictionary());
      List<uint32_t> indices(dict->Capacity() / 2);
      // Collect all indices in the object and the prototypes less
      // than length. This might introduce duplicates in the indices list.
      CollectElementIndices(array, length, &indices);
      indices.Sort(&compareUInt32);
      int j = 0;
      int n = indices.length();
      while (j < n) {
        HandleScope loop_scope(isolate);
        uint32_t index = indices[j];
        Handle<Object> element;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, element, Object::GetElement(isolate, array, index), false);
        if (!visitor->visit(index, element)) return false;
        // Skip to next different index (i.e., omit duplicates).
        do {
          j++;
        } while (j < n && indices[j] == index);
      }
      break;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS: {
      for (uint32_t index = 0; index < length; index++) {
        HandleScope loop_scope(isolate);
        Handle<Object> element;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, element, Object::GetElement(isolate, array, index), false);
        if (!visitor->visit(index, element)) return false;
      }
      break;
    }
    case NO_ELEMENTS:
      break;
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) case TYPE##_ELEMENTS:
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      return IterateElementsSlow(isolate, receiver, length, visitor);
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
      // |array| is guaranteed to be an array or typed array.
      UNREACHABLE();
      break;
  }
  visitor->increase_index_offset(length);
  return true;
}


bool HasConcatSpreadableModifier(Isolate* isolate, Handle<JSArray> obj) {
  Handle<Symbol> key(isolate->factory()->is_concat_spreadable_symbol());
  Maybe<bool> maybe = JSReceiver::HasProperty(obj, key);
  return maybe.FromMaybe(false);
}


static Maybe<bool> IsConcatSpreadable(Isolate* isolate, Handle<Object> obj) {
  HandleScope handle_scope(isolate);
  if (!obj->IsJSReceiver()) return Just(false);
  Handle<Symbol> key(isolate->factory()->is_concat_spreadable_symbol());
  Handle<Object> value;
  MaybeHandle<Object> maybeValue =
      i::Runtime::GetObjectProperty(isolate, obj, key);
  if (!maybeValue.ToHandle(&value)) return Nothing<bool>();
  if (!value->IsUndefined()) return Just(value->BooleanValue());
  return Object::IsArray(obj);
}


Object* Slow_ArrayConcat(Arguments* args, Handle<Object> species,
                         Isolate* isolate) {
  int argument_count = args->length();

  bool is_array_species = *species == isolate->context()->array_function();

  // Pass 1: estimate the length and number of elements of the result.
  // The actual length can be larger if any of the arguments have getters
  // that mutate other arguments (but will otherwise be precise).
  // The number of elements is precise if there are no inherited elements.

  ElementsKind kind = FAST_SMI_ELEMENTS;

  uint32_t estimate_result_length = 0;
  uint32_t estimate_nof_elements = 0;
  for (int i = 0; i < argument_count; i++) {
    HandleScope loop_scope(isolate);
    Handle<Object> obj((*args)[i], isolate);
    uint32_t length_estimate;
    uint32_t element_estimate;
    if (obj->IsJSArray()) {
      Handle<JSArray> array(Handle<JSArray>::cast(obj));
      length_estimate = static_cast<uint32_t>(array->length()->Number());
      if (length_estimate != 0) {
        ElementsKind array_kind =
            GetPackedElementsKind(array->GetElementsKind());
        kind = GetMoreGeneralElementsKind(kind, array_kind);
      }
      element_estimate = EstimateElementCount(array);
    } else {
      if (obj->IsHeapObject()) {
        kind = GetMoreGeneralElementsKind(
            kind, obj->IsNumber() ? FAST_DOUBLE_ELEMENTS : FAST_ELEMENTS);
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
    if (JSObject::kMaxElementCount - estimate_nof_elements < element_estimate) {
      estimate_nof_elements = JSObject::kMaxElementCount;
    } else {
      estimate_nof_elements += element_estimate;
    }
  }

  // If estimated number of elements is more than half of length, a
  // fixed array (fast case) is more time and space-efficient than a
  // dictionary.
  bool fast_case =
      is_array_species && (estimate_nof_elements * 2) >= estimate_result_length;

  if (fast_case && kind == FAST_DOUBLE_ELEMENTS) {
    Handle<FixedArrayBase> storage =
        isolate->factory()->NewFixedDoubleArray(estimate_result_length);
    int j = 0;
    bool failure = false;
    if (estimate_result_length > 0) {
      Handle<FixedDoubleArray> double_storage =
          Handle<FixedDoubleArray>::cast(storage);
      for (int i = 0; i < argument_count; i++) {
        Handle<Object> obj((*args)[i], isolate);
        if (obj->IsSmi()) {
          double_storage->set(j, Smi::cast(*obj)->value());
          j++;
        } else if (obj->IsNumber()) {
          double_storage->set(j, obj->Number());
          j++;
        } else {
          JSArray* array = JSArray::cast(*obj);
          uint32_t length = static_cast<uint32_t>(array->length()->Number());
          switch (array->GetElementsKind()) {
            case FAST_HOLEY_DOUBLE_ELEMENTS:
            case FAST_DOUBLE_ELEMENTS: {
              // Empty array is FixedArray but not FixedDoubleArray.
              if (length == 0) break;
              FixedDoubleArray* elements =
                  FixedDoubleArray::cast(array->elements());
              for (uint32_t i = 0; i < length; i++) {
                if (elements->is_the_hole(i)) {
                  // TODO(jkummerow/verwaest): We could be a bit more clever
                  // here: Check if there are no elements/getters on the
                  // prototype chain, and if so, allow creation of a holey
                  // result array.
                  // Same thing below (holey smi case).
                  failure = true;
                  break;
                }
                double double_value = elements->get_scalar(i);
                double_storage->set(j, double_value);
                j++;
              }
              break;
            }
            case FAST_HOLEY_SMI_ELEMENTS:
            case FAST_SMI_ELEMENTS: {
              FixedArray* elements(FixedArray::cast(array->elements()));
              for (uint32_t i = 0; i < length; i++) {
                Object* element = elements->get(i);
                if (element->IsTheHole()) {
                  failure = true;
                  break;
                }
                int32_t int_value = Smi::cast(element)->value();
                double_storage->set(j, int_value);
                j++;
              }
              break;
            }
            case FAST_HOLEY_ELEMENTS:
            case FAST_ELEMENTS:
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

  Handle<Object> storage;
  if (fast_case) {
    // The backing storage array must have non-existing elements to preserve
    // holes across concat operations.
    storage =
        isolate->factory()->NewFixedArrayWithHoles(estimate_result_length);
  } else if (is_array_species) {
    // TODO(126): move 25% pre-allocation logic into Dictionary::Allocate
    uint32_t at_least_space_for =
        estimate_nof_elements + (estimate_nof_elements >> 2);
    storage = SeededNumberDictionary::New(isolate, at_least_space_for);
  } else {
    DCHECK(species->IsConstructor());
    Handle<Object> length(Smi::FromInt(0), isolate);
    Handle<Object> storage_object;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, storage_object,
        Execution::New(isolate, species, species, 1, &length));
    storage = storage_object;
  }

  ArrayConcatVisitor visitor(isolate, storage, fast_case);

  for (int i = 0; i < argument_count; i++) {
    Handle<Object> obj((*args)[i], isolate);
    Maybe<bool> spreadable = IsConcatSpreadable(isolate, obj);
    MAYBE_RETURN(spreadable, isolate->heap()->exception());
    if (spreadable.FromJust()) {
      Handle<JSReceiver> object = Handle<JSReceiver>::cast(obj);
      if (!IterateElements(isolate, object, &visitor)) {
        return isolate->heap()->exception();
      }
    } else {
      visitor.visit(0, obj);
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
    return *visitor.storage_jsreceiver();
  }
}


MaybeHandle<JSArray> Fast_ArrayConcat(Isolate* isolate, Arguments* args) {
  int n_arguments = args->length();
  int result_len = 0;
  {
    DisallowHeapAllocation no_gc;
    // Iterate through all the arguments performing checks
    // and calculating total length.
    for (int i = 0; i < n_arguments; i++) {
      Object* arg = (*args)[i];
      if (!arg->IsJSArray()) return MaybeHandle<JSArray>();
      if (!HasOnlySimpleReceiverElements(isolate, JSObject::cast(arg))) {
        return MaybeHandle<JSArray>();
      }
      // TODO(cbruni): support fast concatenation of DICTIONARY_ELEMENTS.
      if (!JSObject::cast(arg)->HasFastElements()) {
        return MaybeHandle<JSArray>();
      }
      Handle<JSArray> array(JSArray::cast(arg), isolate);
      if (HasConcatSpreadableModifier(isolate, array)) {
        return MaybeHandle<JSArray>();
      }
      int len = Smi::cast(array->length())->value();

      // We shouldn't overflow when adding another len.
      const int kHalfOfMaxInt = 1 << (kBitsPerInt - 2);
      STATIC_ASSERT(FixedArray::kMaxLength < kHalfOfMaxInt);
      USE(kHalfOfMaxInt);
      result_len += len;
      DCHECK(result_len >= 0);
      // Throw an Error if we overflow the FixedArray limits
      if (FixedArray::kMaxLength < result_len) {
        THROW_NEW_ERROR(isolate,
                        NewRangeError(MessageTemplate::kInvalidArrayLength),
                        JSArray);
      }
    }
  }
  return ElementsAccessor::Concat(isolate, args, n_arguments);
}

}  // namespace


// ES6 22.1.3.1 Array.prototype.concat
BUILTIN(ArrayConcat) {
  HandleScope scope(isolate);

  Handle<Object> receiver = args.receiver();
  // TODO(bmeurer): Do we really care about the exact exception message here?
  if (receiver->IsNull() || receiver->IsUndefined()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Array.prototype.concat")));
  }
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, receiver, Object::ToObject(isolate, args.receiver()));
  args[0] = *receiver;

  Handle<JSArray> result_array;

  // Reading @@species happens before anything else with a side effect, so
  // we can do it here to determine whether to take the fast path.
  Handle<Object> species;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, species, Object::ArraySpeciesConstructor(isolate, receiver));
  if (*species == isolate->context()->native_context()->array_function()) {
    if (Fast_ArrayConcat(isolate, &args).ToHandle(&result_array)) {
      return *result_array;
    }
    if (isolate->has_pending_exception()) return isolate->heap()->exception();
  }
  return Slow_ArrayConcat(&args, species, isolate);
}


// ES6 22.1.2.2 Array.isArray
BUILTIN(ArrayIsArray) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<Object> object = args.at<Object>(1);
  Maybe<bool> result = Object::IsArray(object);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

namespace {

MUST_USE_RESULT Maybe<bool> FastAssign(Handle<JSReceiver> to,
                                       Handle<Object> next_source) {
  // Non-empty strings are the only non-JSReceivers that need to be handled
  // explicitly by Object.assign.
  if (!next_source->IsJSReceiver()) {
    return Just(!next_source->IsString() ||
                String::cast(*next_source)->length() == 0);
  }

  Isolate* isolate = to->GetIsolate();
  Handle<Map> map(JSReceiver::cast(*next_source)->map(), isolate);

  if (!map->IsJSObjectMap()) return Just(false);
  if (!map->OnlyHasSimpleProperties()) return Just(false);

  Handle<JSObject> from = Handle<JSObject>::cast(next_source);
  if (from->elements() != isolate->heap()->empty_fixed_array()) {
    return Just(false);
  }

  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);
  int length = map->NumberOfOwnDescriptors();

  bool stable = true;

  for (int i = 0; i < length; i++) {
    Handle<Name> next_key(descriptors->GetKey(i), isolate);
    Handle<Object> prop_value;
    // Directly decode from the descriptor array if |from| did not change shape.
    if (stable) {
      PropertyDetails details = descriptors->GetDetails(i);
      if (!details.IsEnumerable()) continue;
      if (details.kind() == kData) {
        if (details.location() == kDescriptor) {
          prop_value = handle(descriptors->GetValue(i), isolate);
        } else {
          Representation representation = details.representation();
          FieldIndex index = FieldIndex::ForDescriptor(*map, i);
          prop_value = JSObject::FastPropertyAt(from, representation, index);
        }
      } else {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, prop_value,
                                         Object::GetProperty(from, next_key),
                                         Nothing<bool>());
        stable = from->map() == *map;
      }
    } else {
      // If the map did change, do a slower lookup. We are still guaranteed that
      // the object has a simple shape, and that the key is a name.
      LookupIterator it(from, next_key, LookupIterator::OWN_SKIP_INTERCEPTOR);
      if (!it.IsFound()) continue;
      DCHECK(it.state() == LookupIterator::DATA ||
             it.state() == LookupIterator::ACCESSOR);
      if (!it.IsEnumerable()) continue;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
    }
    LookupIterator it(to, next_key);
    bool call_to_js = it.IsFound() && it.state() != LookupIterator::DATA;
    Maybe<bool> result = Object::SetProperty(
        &it, prop_value, STRICT, Object::CERTAINLY_NOT_STORE_FROM_KEYED);
    if (result.IsNothing()) return result;
    if (stable && call_to_js) stable = from->map() == *map;
  }

  return Just(true);
}

}  // namespace

// ES6 19.1.2.1 Object.assign
BUILTIN(ObjectAssign) {
  HandleScope scope(isolate);
  Handle<Object> target = args.atOrUndefined(isolate, 1);

  // 1. Let to be ? ToObject(target).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, target,
                                     Object::ToObject(isolate, target));
  Handle<JSReceiver> to = Handle<JSReceiver>::cast(target);
  // 2. If only one argument was passed, return to.
  if (args.length() == 2) return *to;
  // 3. Let sources be the List of argument values starting with the
  //    second argument.
  // 4. For each element nextSource of sources, in ascending index order,
  for (int i = 2; i < args.length(); ++i) {
    Handle<Object> next_source = args.at<Object>(i);
    Maybe<bool> fast_assign = FastAssign(to, next_source);
    if (fast_assign.IsNothing()) return isolate->heap()->exception();
    if (fast_assign.FromJust()) continue;
    // 4a. If nextSource is undefined or null, let keys be an empty List.
    // 4b. Else,
    // 4b i. Let from be ToObject(nextSource).
    // Only non-empty strings and JSReceivers have enumerable properties.
    Handle<JSReceiver> from =
        Object::ToObject(isolate, next_source).ToHandleChecked();
    // 4b ii. Let keys be ? from.[[OwnPropertyKeys]]().
    Handle<FixedArray> keys;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, keys,
        JSReceiver::GetKeys(from, OWN_ONLY, ALL_PROPERTIES, KEEP_NUMBERS));
    // 4c. Repeat for each element nextKey of keys in List order,
    for (int j = 0; j < keys->length(); ++j) {
      Handle<Object> next_key(keys->get(j), isolate);
      // 4c i. Let desc be ? from.[[GetOwnProperty]](nextKey).
      PropertyDescriptor desc;
      Maybe<bool> found =
          JSReceiver::GetOwnPropertyDescriptor(isolate, from, next_key, &desc);
      if (found.IsNothing()) return isolate->heap()->exception();
      // 4c ii. If desc is not undefined and desc.[[Enumerable]] is true, then
      if (found.FromJust() && desc.enumerable()) {
        // 4c ii 1. Let propValue be ? Get(from, nextKey).
        Handle<Object> prop_value;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, prop_value,
            Runtime::GetObjectProperty(isolate, from, next_key));
        // 4c ii 2. Let status be ? Set(to, nextKey, propValue, true).
        Handle<Object> status;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, status, Runtime::SetObjectProperty(isolate, to, next_key,
                                                        prop_value, STRICT));
      }
    }
  }
  // 5. Return to.
  return *to;
}


// ES6 section 19.1.2.2 Object.create ( O [ , Properties ] )
BUILTIN(ObjectCreate) {
  HandleScope scope(isolate);
  Handle<Object> prototype = args.atOrUndefined(isolate, 1);
  if (!prototype->IsNull() && !prototype->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kProtoObjectOrNull, prototype));
  }

  // Generate the map with the specified {prototype} based on the Object
  // function's initial map from the current native context.
  // TODO(bmeurer): Use a dedicated cache for Object.create; think about
  // slack tracking for Object.create.
  Handle<Map> map(isolate->native_context()->object_function()->initial_map(),
                  isolate);
  if (map->prototype() != *prototype) {
    map = Map::TransitionToPrototype(map, prototype, FAST_PROTOTYPE);
  }

  // Actually allocate the object.
  Handle<JSObject> object = isolate->factory()->NewJSObjectFromMap(map);

  // Define the properties if properties was specified and is not undefined.
  Handle<Object> properties = args.atOrUndefined(isolate, 2);
  if (!properties->IsUndefined()) {
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, JSReceiver::DefineProperties(isolate, object, properties));
  }

  return *object;
}


// ES6 section 19.1.2.5 Object.freeze ( O )
BUILTIN(ObjectFreeze) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  if (object->IsJSReceiver()) {
    MAYBE_RETURN(JSReceiver::SetIntegrityLevel(Handle<JSReceiver>::cast(object),
                                               FROZEN, Object::THROW_ON_ERROR),
                 isolate->heap()->exception());
  }
  return *object;
}


// ES6 section 19.1.2.6 Object.getOwnPropertyDescriptor ( O, P )
BUILTIN(ObjectGetOwnPropertyDescriptor) {
  HandleScope scope(isolate);
  // 1. Let obj be ? ToObject(O).
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));
  // 2. Let key be ? ToPropertyKey(P).
  Handle<Object> property = args.atOrUndefined(isolate, 2);
  Handle<Name> key;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, key,
                                     Object::ToName(isolate, property));
  // 3. Let desc be ? obj.[[GetOwnProperty]](key).
  PropertyDescriptor desc;
  Maybe<bool> found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, receiver, key, &desc);
  MAYBE_RETURN(found, isolate->heap()->exception());
  // 4. Return FromPropertyDescriptor(desc).
  if (!found.FromJust()) return isolate->heap()->undefined_value();
  return *desc.ToObject(isolate);
}


namespace {

Object* GetOwnPropertyKeys(Isolate* isolate,
                           BuiltinArguments<BuiltinExtraArguments::kNone> args,
                           PropertyFilter filter) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));
  Handle<FixedArray> keys;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, keys,
      JSReceiver::GetKeys(receiver, OWN_ONLY, filter, CONVERT_TO_STRING));
  return *isolate->factory()->NewJSArrayWithElements(keys);
}

}  // namespace


// ES6 section 19.1.2.7 Object.getOwnPropertyNames ( O )
BUILTIN(ObjectGetOwnPropertyNames) {
  return GetOwnPropertyKeys(isolate, args, SKIP_SYMBOLS);
}


// ES6 section 19.1.2.8 Object.getOwnPropertySymbols ( O )
BUILTIN(ObjectGetOwnPropertySymbols) {
  return GetOwnPropertyKeys(isolate, args, SKIP_STRINGS);
}


// ES#sec-object.is Object.is ( value1, value2 )
BUILTIN(ObjectIs) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> value1 = args.at<Object>(1);
  Handle<Object> value2 = args.at<Object>(2);
  return isolate->heap()->ToBoolean(value1->SameValue(*value2));
}


// ES6 section 19.1.2.11 Object.isExtensible ( O )
BUILTIN(ObjectIsExtensible) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Maybe<bool> result =
      object->IsJSReceiver()
          ? JSReceiver::IsExtensible(Handle<JSReceiver>::cast(object))
          : Just(false);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}


// ES6 section 19.1.2.12 Object.isFrozen ( O )
BUILTIN(ObjectIsFrozen) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Maybe<bool> result = object->IsJSReceiver()
                           ? JSReceiver::TestIntegrityLevel(
                                 Handle<JSReceiver>::cast(object), FROZEN)
                           : Just(true);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}


// ES6 section 19.1.2.13 Object.isSealed ( O )
BUILTIN(ObjectIsSealed) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Maybe<bool> result = object->IsJSReceiver()
                           ? JSReceiver::TestIntegrityLevel(
                                 Handle<JSReceiver>::cast(object), SEALED)
                           : Just(true);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}


// ES6 section 19.1.2.14 Object.keys ( O )
BUILTIN(ObjectKeys) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));

  Handle<FixedArray> keys;
  int enum_length = receiver->map()->EnumLength();
  if (enum_length != kInvalidEnumCacheSentinel &&
      JSObject::cast(*receiver)->elements() ==
          isolate->heap()->empty_fixed_array()) {
    DCHECK(receiver->IsJSObject());
    DCHECK(!JSObject::cast(*receiver)->HasNamedInterceptor());
    DCHECK(!JSObject::cast(*receiver)->IsAccessCheckNeeded());
    DCHECK(!receiver->map()->has_hidden_prototype());
    DCHECK(JSObject::cast(*receiver)->HasFastProperties());
    if (enum_length == 0) {
      keys = isolate->factory()->empty_fixed_array();
    } else {
      Handle<FixedArray> cache(
          receiver->map()->instance_descriptors()->GetEnumCache());
      keys = isolate->factory()->CopyFixedArrayUpTo(cache, enum_length);
    }
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, keys,
        JSReceiver::GetKeys(receiver, OWN_ONLY, ENUMERABLE_STRINGS,
                            CONVERT_TO_STRING));
  }
  return *isolate->factory()->NewJSArrayWithElements(keys, FAST_ELEMENTS);
}

BUILTIN(ObjectValues) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));
  Handle<FixedArray> values;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, values, JSReceiver::GetOwnValues(receiver, ENUMERABLE_STRINGS));
  return *isolate->factory()->NewJSArrayWithElements(values);
}


BUILTIN(ObjectEntries) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));
  Handle<FixedArray> entries;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, entries,
      JSReceiver::GetOwnEntries(receiver, ENUMERABLE_STRINGS));
  return *isolate->factory()->NewJSArrayWithElements(entries);
}

BUILTIN(ObjectGetOwnPropertyDescriptors) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  Handle<Object> undefined = isolate->factory()->undefined_value();

  Handle<JSReceiver> receiver;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     Object::ToObject(isolate, object));

  Handle<FixedArray> keys;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, keys, JSReceiver::GetKeys(receiver, OWN_ONLY, ALL_PROPERTIES,
                                         CONVERT_TO_STRING));

  Handle<Object> descriptors =
      isolate->factory()->NewJSObject(isolate->object_function());

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Name> key = Handle<Name>::cast(FixedArray::get(*keys, i, isolate));
    PropertyDescriptor descriptor;
    Maybe<bool> did_get_descriptor = JSReceiver::GetOwnPropertyDescriptor(
        isolate, receiver, key, &descriptor);
    MAYBE_RETURN(did_get_descriptor, isolate->heap()->exception());

    Handle<Object> from_descriptor = did_get_descriptor.FromJust()
                                         ? descriptor.ToObject(isolate)
                                         : undefined;

    LookupIterator it = LookupIterator::PropertyOrElement(
        isolate, descriptors, key, LookupIterator::OWN);
    Maybe<bool> success = JSReceiver::CreateDataProperty(&it, from_descriptor,
                                                         Object::DONT_THROW);
    CHECK(success.FromJust());
  }

  return *descriptors;
}

// ES6 section 19.1.2.15 Object.preventExtensions ( O )
BUILTIN(ObjectPreventExtensions) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  if (object->IsJSReceiver()) {
    MAYBE_RETURN(JSReceiver::PreventExtensions(Handle<JSReceiver>::cast(object),
                                               Object::THROW_ON_ERROR),
                 isolate->heap()->exception());
  }
  return *object;
}


// ES6 section 19.1.2.17 Object.seal ( O )
BUILTIN(ObjectSeal) {
  HandleScope scope(isolate);
  Handle<Object> object = args.atOrUndefined(isolate, 1);
  if (object->IsJSReceiver()) {
    MAYBE_RETURN(JSReceiver::SetIntegrityLevel(Handle<JSReceiver>::cast(object),
                                               SEALED, Object::THROW_ON_ERROR),
                 isolate->heap()->exception());
  }
  return *object;
}


namespace {

bool CodeGenerationFromStringsAllowed(Isolate* isolate,
                                      Handle<Context> context) {
  DCHECK(context->allow_code_gen_from_strings()->IsFalse());
  // Check with callback if set.
  AllowCodeGenerationFromStringsCallback callback =
      isolate->allow_code_gen_callback();
  if (callback == NULL) {
    // No callback set and code generation disallowed.
    return false;
  } else {
    // Callback set. Let it decide if code generation is allowed.
    VMState<EXTERNAL> state(isolate);
    return callback(v8::Utils::ToLocal(context));
  }
}


MaybeHandle<JSFunction> CompileString(Handle<Context> context,
                                      Handle<String> source,
                                      ParseRestriction restriction) {
  Isolate* const isolate = context->GetIsolate();
  Handle<Context> native_context(context->native_context(), isolate);

  // Check if native context allows code generation from
  // strings. Throw an exception if it doesn't.
  if (native_context->allow_code_gen_from_strings()->IsFalse() &&
      !CodeGenerationFromStringsAllowed(isolate, native_context)) {
    Handle<Object> error_message =
        native_context->ErrorMessageForCodeGenerationFromStrings();
    THROW_NEW_ERROR(isolate, NewEvalError(MessageTemplate::kCodeGenFromStrings,
                                          error_message),
                    JSFunction);
  }

  // Compile source string in the native context.
  Handle<SharedFunctionInfo> outer_info(native_context->closure()->shared(),
                                        isolate);
  return Compiler::GetFunctionFromEval(source, outer_info, native_context,
                                       SLOPPY, restriction,
                                       RelocInfo::kNoPosition);
}

}  // namespace


// ES6 section 18.2.1 eval (x)
BUILTIN(GlobalEval) {
  HandleScope scope(isolate);
  Handle<Object> x = args.atOrUndefined(isolate, 1);
  Handle<JSFunction> target = args.target<JSFunction>();
  Handle<JSObject> target_global_proxy(target->global_proxy(), isolate);
  if (!x->IsString()) return *x;
  Handle<JSFunction> function;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, function,
      CompileString(handle(target->native_context(), isolate),
                    Handle<String>::cast(x), NO_PARSE_RESTRICTION));
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, function, target_global_proxy, 0, nullptr));
  return *result;
}


// ES6 section 26.1.3 Reflect.defineProperty
BUILTIN(ReflectDefineProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  Handle<Object> target = args.at<Object>(1);
  Handle<Object> key = args.at<Object>(2);
  Handle<Object> attributes = args.at<Object>(3);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.defineProperty")));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  PropertyDescriptor desc;
  if (!PropertyDescriptor::ToPropertyDescriptor(isolate, attributes, &desc)) {
    return isolate->heap()->exception();
  }

  Maybe<bool> result =
      JSReceiver::DefineOwnProperty(isolate, Handle<JSReceiver>::cast(target),
                                    name, &desc, Object::DONT_THROW);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}


// ES6 section 26.1.4 Reflect.deleteProperty
BUILTIN(ReflectDeleteProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> target = args.at<Object>(1);
  Handle<Object> key = args.at<Object>(2);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.deleteProperty")));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  Maybe<bool> result = JSReceiver::DeletePropertyOrElement(
      Handle<JSReceiver>::cast(target), name, SLOPPY);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}


// ES6 section 26.1.6 Reflect.get
BUILTIN(ReflectGet) {
  HandleScope scope(isolate);
  Handle<Object> target = args.atOrUndefined(isolate, 1);
  Handle<Object> key = args.atOrUndefined(isolate, 2);
  Handle<Object> receiver = args.length() > 3 ? args.at<Object>(3) : target;

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.get")));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::GetPropertyOrElement(
                           receiver, name, Handle<JSReceiver>::cast(target)));

  return *result;
}


// ES6 section 26.1.7 Reflect.getOwnPropertyDescriptor
BUILTIN(ReflectGetOwnPropertyDescriptor) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> target = args.at<Object>(1);
  Handle<Object> key = args.at<Object>(2);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.getOwnPropertyDescriptor")));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  PropertyDescriptor desc;
  Maybe<bool> found = JSReceiver::GetOwnPropertyDescriptor(
      isolate, Handle<JSReceiver>::cast(target), name, &desc);
  MAYBE_RETURN(found, isolate->heap()->exception());
  if (!found.FromJust()) return isolate->heap()->undefined_value();
  return *desc.ToObject(isolate);
}


// ES6 section 26.1.8 Reflect.getPrototypeOf
BUILTIN(ReflectGetPrototypeOf) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<Object> target = args.at<Object>(1);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.getPrototypeOf")));
  }
  Handle<Object> prototype;
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(target);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, prototype, JSReceiver::GetPrototype(isolate, receiver));
  return *prototype;
}


// ES6 section 26.1.9 Reflect.has
BUILTIN(ReflectHas) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> target = args.at<Object>(1);
  Handle<Object> key = args.at<Object>(2);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.has")));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  Maybe<bool> result =
      JSReceiver::HasProperty(Handle<JSReceiver>::cast(target), name);
  return result.IsJust() ? *isolate->factory()->ToBoolean(result.FromJust())
                         : isolate->heap()->exception();
}


// ES6 section 26.1.10 Reflect.isExtensible
BUILTIN(ReflectIsExtensible) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<Object> target = args.at<Object>(1);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.isExtensible")));
  }

  Maybe<bool> result =
      JSReceiver::IsExtensible(Handle<JSReceiver>::cast(target));
  MAYBE_RETURN(result, isolate->heap()->exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}


// ES6 section 26.1.11 Reflect.ownKeys
BUILTIN(ReflectOwnKeys) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<Object> target = args.at<Object>(1);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.ownKeys")));
  }

  Handle<FixedArray> keys;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, keys,
      JSReceiver::GetKeys(Handle<JSReceiver>::cast(target), OWN_ONLY,
                          ALL_PROPERTIES, CONVERT_TO_STRING));
  return *isolate->factory()->NewJSArrayWithElements(keys);
}


// ES6 section 26.1.12 Reflect.preventExtensions
BUILTIN(ReflectPreventExtensions) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<Object> target = args.at<Object>(1);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.preventExtensions")));
  }

  Maybe<bool> result = JSReceiver::PreventExtensions(
      Handle<JSReceiver>::cast(target), Object::DONT_THROW);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}


// ES6 section 26.1.13 Reflect.set
BUILTIN(ReflectSet) {
  HandleScope scope(isolate);
  Handle<Object> target = args.atOrUndefined(isolate, 1);
  Handle<Object> key = args.atOrUndefined(isolate, 2);
  Handle<Object> value = args.atOrUndefined(isolate, 3);
  Handle<Object> receiver = args.length() > 4 ? args.at<Object>(4) : target;

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.set")));
  }

  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, key));

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, receiver, name, Handle<JSReceiver>::cast(target));
  Maybe<bool> result = Object::SetSuperProperty(
      &it, value, SLOPPY, Object::MAY_BE_STORE_FROM_KEYED);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}


// ES6 section 26.1.14 Reflect.setPrototypeOf
BUILTIN(ReflectSetPrototypeOf) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> target = args.at<Object>(1);
  Handle<Object> proto = args.at<Object>(2);

  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Reflect.setPrototypeOf")));
  }

  if (!proto->IsJSReceiver() && !proto->IsNull()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kProtoObjectOrNull, proto));
  }

  Maybe<bool> result = JSReceiver::SetPrototype(
      Handle<JSReceiver>::cast(target), proto, true, Object::DONT_THROW);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}


// -----------------------------------------------------------------------------
// ES6 section 19.3 Boolean Objects


// ES6 section 19.3.1.1 Boolean ( value ) for the [[Call]] case.
BUILTIN(BooleanConstructor) {
  HandleScope scope(isolate);
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  return isolate->heap()->ToBoolean(value->BooleanValue());
}


// ES6 section 19.3.1.1 Boolean ( value ) for the [[Construct]] case.
BUILTIN(BooleanConstructor_ConstructStub) {
  HandleScope scope(isolate);
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  Handle<JSFunction> target = args.target<JSFunction>();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  DCHECK(*target == target->native_context()->boolean_function());
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::New(target, new_target));
  Handle<JSValue>::cast(result)->set_value(
      isolate->heap()->ToBoolean(value->BooleanValue()));
  return *result;
}


// ES6 section 19.3.3.2 Boolean.prototype.toString ( )
BUILTIN(BooleanPrototypeToString) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  if (receiver->IsJSValue()) {
    receiver = handle(Handle<JSValue>::cast(receiver)->value(), isolate);
  }
  if (!receiver->IsBoolean()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotGeneric,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Boolean.prototype.toString")));
  }
  return Handle<Oddball>::cast(receiver)->to_string();
}


// ES6 section 19.3.3.3 Boolean.prototype.valueOf ( )
BUILTIN(BooleanPrototypeValueOf) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  if (receiver->IsJSValue()) {
    receiver = handle(Handle<JSValue>::cast(receiver)->value(), isolate);
  }
  if (!receiver->IsBoolean()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotGeneric,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Boolean.prototype.valueOf")));
  }
  return *receiver;
}


// -----------------------------------------------------------------------------
// ES6 section 24.2 DataView Objects


// ES6 section 24.2.2 The DataView Constructor for the [[Call]] case.
BUILTIN(DataViewConstructor) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate,
      NewTypeError(MessageTemplate::kConstructorNotFunction,
                   isolate->factory()->NewStringFromAsciiChecked("DataView")));
}


// ES6 section 24.2.2 The DataView Constructor for the [[Construct]] case.
BUILTIN(DataViewConstructor_ConstructStub) {
  HandleScope scope(isolate);
  Handle<JSFunction> target = args.target<JSFunction>();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  Handle<Object> buffer = args.atOrUndefined(isolate, 1);
  Handle<Object> byte_offset = args.atOrUndefined(isolate, 2);
  Handle<Object> byte_length = args.atOrUndefined(isolate, 3);

  // 2. If Type(buffer) is not Object, throw a TypeError exception.
  // 3. If buffer does not have an [[ArrayBufferData]] internal slot, throw a
  //    TypeError exception.
  if (!buffer->IsJSArrayBuffer()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDataViewNotArrayBuffer));
  }
  Handle<JSArrayBuffer> array_buffer = Handle<JSArrayBuffer>::cast(buffer);

  // 4. Let numberOffset be ? ToNumber(byteOffset).
  Handle<Object> number_offset;
  if (byte_offset->IsUndefined()) {
    // We intentionally violate the specification at this point to allow
    // for new DataView(buffer) invocations to be equivalent to the full
    // new DataView(buffer, 0) invocation.
    number_offset = handle(Smi::FromInt(0), isolate);
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number_offset,
                                       Object::ToNumber(byte_offset));
  }

  // 5. Let offset be ToInteger(numberOffset).
  Handle<Object> offset;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, offset,
                                     Object::ToInteger(isolate, number_offset));

  // 6. If numberOffset ≠ offset or offset < 0, throw a RangeError exception.
  if (number_offset->Number() != offset->Number() || offset->Number() < 0.0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidDataViewOffset));
  }

  // 7. If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
  // We currently violate the specification at this point.

  // 8. Let bufferByteLength be the value of buffer's [[ArrayBufferByteLength]]
  // internal slot.
  double const buffer_byte_length = array_buffer->byte_length()->Number();

  // 9. If offset > bufferByteLength, throw a RangeError exception
  if (offset->Number() > buffer_byte_length) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidDataViewOffset));
  }

  Handle<Object> view_byte_length;
  if (byte_length->IsUndefined()) {
    // 10. If byteLength is undefined, then
    //       a. Let viewByteLength be bufferByteLength - offset.
    view_byte_length =
        isolate->factory()->NewNumber(buffer_byte_length - offset->Number());
  } else {
    // 11. Else,
    //       a. Let viewByteLength be ? ToLength(byteLength).
    //       b. If offset+viewByteLength > bufferByteLength, throw a RangeError
    //          exception
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, view_byte_length, Object::ToLength(isolate, byte_length));
    if (offset->Number() + view_byte_length->Number() > buffer_byte_length) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewRangeError(MessageTemplate::kInvalidDataViewLength));
    }
  }

  // 12. Let O be ? OrdinaryCreateFromConstructor(NewTarget,
  //     "%DataViewPrototype%", «[[DataView]], [[ViewedArrayBuffer]],
  //     [[ByteLength]], [[ByteOffset]]»).
  // 13. Set O's [[DataView]] internal slot to true.
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::New(target, new_target));
  for (int i = 0; i < ArrayBufferView::kInternalFieldCount; ++i) {
    Handle<JSDataView>::cast(result)->SetInternalField(i, Smi::FromInt(0));
  }

  // 14. Set O's [[ViewedArrayBuffer]] internal slot to buffer.
  Handle<JSDataView>::cast(result)->set_buffer(*array_buffer);

  // 15. Set O's [[ByteLength]] internal slot to viewByteLength.
  Handle<JSDataView>::cast(result)->set_byte_length(*view_byte_length);

  // 16. Set O's [[ByteOffset]] internal slot to offset.
  Handle<JSDataView>::cast(result)->set_byte_offset(*offset);

  // 17. Return O.
  return *result;
}


// -----------------------------------------------------------------------------
// ES6 section 20.3 Date Objects


namespace {

// ES6 section 20.3.1.1 Time Values and Time Range
const double kMinYear = -1000000.0;
const double kMaxYear = -kMinYear;
const double kMinMonth = -10000000.0;
const double kMaxMonth = -kMinMonth;


// 20.3.1.2 Day Number and Time within Day
const double kMsPerDay = 86400000.0;


// ES6 section 20.3.1.11 Hours, Minutes, Second, and Milliseconds
const double kMsPerSecond = 1000.0;
const double kMsPerMinute = 60000.0;
const double kMsPerHour = 3600000.0;


// ES6 section 20.3.1.14 MakeDate (day, time)
double MakeDate(double day, double time) {
  if (std::isfinite(day) && std::isfinite(time)) {
    return time + day * kMsPerDay;
  }
  return std::numeric_limits<double>::quiet_NaN();
}


// ES6 section 20.3.1.13 MakeDay (year, month, date)
double MakeDay(double year, double month, double date) {
  if ((kMinYear <= year && year <= kMaxYear) &&
      (kMinMonth <= month && month <= kMaxMonth) && std::isfinite(date)) {
    int y = FastD2I(year);
    int m = FastD2I(month);
    y += m / 12;
    m %= 12;
    if (m < 0) {
      m += 12;
      y -= 1;
    }
    DCHECK_LE(0, m);
    DCHECK_LT(m, 12);

    // kYearDelta is an arbitrary number such that:
    // a) kYearDelta = -1 (mod 400)
    // b) year + kYearDelta > 0 for years in the range defined by
    //    ECMA 262 - 15.9.1.1, i.e. upto 100,000,000 days on either side of
    //    Jan 1 1970. This is required so that we don't run into integer
    //    division of negative numbers.
    // c) there shouldn't be an overflow for 32-bit integers in the following
    //    operations.
    static const int kYearDelta = 399999;
    static const int kBaseDay =
        365 * (1970 + kYearDelta) + (1970 + kYearDelta) / 4 -
        (1970 + kYearDelta) / 100 + (1970 + kYearDelta) / 400;
    int day_from_year = 365 * (y + kYearDelta) + (y + kYearDelta) / 4 -
                        (y + kYearDelta) / 100 + (y + kYearDelta) / 400 -
                        kBaseDay;
    if ((y % 4 != 0) || (y % 100 == 0 && y % 400 != 0)) {
      static const int kDayFromMonth[] = {0,   31,  59,  90,  120, 151,
                                          181, 212, 243, 273, 304, 334};
      day_from_year += kDayFromMonth[m];
    } else {
      static const int kDayFromMonth[] = {0,   31,  60,  91,  121, 152,
                                          182, 213, 244, 274, 305, 335};
      day_from_year += kDayFromMonth[m];
    }
    return static_cast<double>(day_from_year - 1) + date;
  }
  return std::numeric_limits<double>::quiet_NaN();
}


// ES6 section 20.3.1.12 MakeTime (hour, min, sec, ms)
double MakeTime(double hour, double min, double sec, double ms) {
  if (std::isfinite(hour) && std::isfinite(min) && std::isfinite(sec) &&
      std::isfinite(ms)) {
    double const h = DoubleToInteger(hour);
    double const m = DoubleToInteger(min);
    double const s = DoubleToInteger(sec);
    double const milli = DoubleToInteger(ms);
    return h * kMsPerHour + m * kMsPerMinute + s * kMsPerSecond + milli;
  }
  return std::numeric_limits<double>::quiet_NaN();
}


// ES6 section 20.3.1.15 TimeClip (time)
double TimeClip(double time) {
  if (-DateCache::kMaxTimeInMs <= time && time <= DateCache::kMaxTimeInMs) {
    return DoubleToInteger(time) + 0.0;
  }
  return std::numeric_limits<double>::quiet_NaN();
}


const char* kShortWeekDays[] = {"Sun", "Mon", "Tue", "Wed",
                                "Thu", "Fri", "Sat"};
const char* kShortMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


// ES6 section 20.3.1.16 Date Time String Format
double ParseDateTimeString(Handle<String> str) {
  Isolate* const isolate = str->GetIsolate();
  str = String::Flatten(str);
  // TODO(bmeurer): Change DateParser to not use the FixedArray.
  Handle<FixedArray> tmp =
      isolate->factory()->NewFixedArray(DateParser::OUTPUT_SIZE);
  DisallowHeapAllocation no_gc;
  String::FlatContent str_content = str->GetFlatContent();
  bool result;
  if (str_content.IsOneByte()) {
    result = DateParser::Parse(str_content.ToOneByteVector(), *tmp,
                               isolate->unicode_cache());
  } else {
    result = DateParser::Parse(str_content.ToUC16Vector(), *tmp,
                               isolate->unicode_cache());
  }
  if (!result) return std::numeric_limits<double>::quiet_NaN();
  double const day = MakeDay(tmp->get(0)->Number(), tmp->get(1)->Number(),
                             tmp->get(2)->Number());
  double const time = MakeTime(tmp->get(3)->Number(), tmp->get(4)->Number(),
                               tmp->get(5)->Number(), tmp->get(6)->Number());
  double date = MakeDate(day, time);
  if (tmp->get(7)->IsNull()) {
    if (!std::isnan(date)) {
      date = isolate->date_cache()->ToUTC(static_cast<int64_t>(date));
    }
  } else {
    date -= tmp->get(7)->Number() * 1000.0;
  }
  return date;
}


enum ToDateStringMode { kDateOnly, kTimeOnly, kDateAndTime };


// ES6 section 20.3.4.41.1 ToDateString(tv)
void ToDateString(double time_val, Vector<char> str, DateCache* date_cache,
                  ToDateStringMode mode = kDateAndTime) {
  if (std::isnan(time_val)) {
    SNPrintF(str, "Invalid Date");
    return;
  }
  int64_t time_ms = static_cast<int64_t>(time_val);
  int64_t local_time_ms = date_cache->ToLocal(time_ms);
  int year, month, day, weekday, hour, min, sec, ms;
  date_cache->BreakDownTime(local_time_ms, &year, &month, &day, &weekday, &hour,
                            &min, &sec, &ms);
  int timezone_offset = -date_cache->TimezoneOffset(time_ms);
  int timezone_hour = std::abs(timezone_offset) / 60;
  int timezone_min = std::abs(timezone_offset) % 60;
  const char* local_timezone = date_cache->LocalTimezone(time_ms);
  switch (mode) {
    case kDateOnly:
      SNPrintF(str, "%s %s %02d %4d", kShortWeekDays[weekday],
               kShortMonths[month], day, year);
      return;
    case kTimeOnly:
      SNPrintF(str, "%02d:%02d:%02d GMT%c%02d%02d (%s)", hour, min, sec,
               (timezone_offset < 0) ? '-' : '+', timezone_hour, timezone_min,
               local_timezone);
      return;
    case kDateAndTime:
      SNPrintF(str, "%s %s %02d %4d %02d:%02d:%02d GMT%c%02d%02d (%s)",
               kShortWeekDays[weekday], kShortMonths[month], day, year, hour,
               min, sec, (timezone_offset < 0) ? '-' : '+', timezone_hour,
               timezone_min, local_timezone);
      return;
  }
  UNREACHABLE();
}


Object* SetLocalDateValue(Handle<JSDate> date, double time_val) {
  if (time_val >= -DateCache::kMaxTimeBeforeUTCInMs &&
      time_val <= DateCache::kMaxTimeBeforeUTCInMs) {
    Isolate* const isolate = date->GetIsolate();
    time_val = isolate->date_cache()->ToUTC(static_cast<int64_t>(time_val));
  } else {
    time_val = std::numeric_limits<double>::quiet_NaN();
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}

}  // namespace


// ES6 section 20.3.2 The Date Constructor for the [[Call]] case.
BUILTIN(DateConstructor) {
  HandleScope scope(isolate);
  double const time_val = JSDate::CurrentTimeValue(isolate);
  char buffer[128];
  Vector<char> str(buffer, arraysize(buffer));
  ToDateString(time_val, str, isolate->date_cache());
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      isolate->factory()->NewStringFromUtf8(CStrVector(buffer)));
  return *result;
}


// ES6 section 20.3.2 The Date Constructor for the [[Construct]] case.
BUILTIN(DateConstructor_ConstructStub) {
  HandleScope scope(isolate);
  int const argc = args.length() - 1;
  Handle<JSFunction> target = args.target<JSFunction>();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  double time_val;
  if (argc == 0) {
    time_val = JSDate::CurrentTimeValue(isolate);
  } else if (argc == 1) {
    Handle<Object> value = args.at<Object>(1);
    if (value->IsJSDate()) {
      time_val = Handle<JSDate>::cast(value)->value()->Number();
    } else {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value,
                                         Object::ToPrimitive(value));
      if (value->IsString()) {
        time_val = ParseDateTimeString(Handle<String>::cast(value));
      } else {
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value,
                                           Object::ToNumber(value));
        time_val = value->Number();
      }
    }
  } else {
    Handle<Object> year_object;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, year_object,
                                       Object::ToNumber(args.at<Object>(1)));
    Handle<Object> month_object;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month_object,
                                       Object::ToNumber(args.at<Object>(2)));
    double year = year_object->Number();
    double month = month_object->Number();
    double date = 1.0, hours = 0.0, minutes = 0.0, seconds = 0.0, ms = 0.0;
    if (argc >= 3) {
      Handle<Object> date_object;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, date_object,
                                         Object::ToNumber(args.at<Object>(3)));
      date = date_object->Number();
      if (argc >= 4) {
        Handle<Object> hours_object;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, hours_object, Object::ToNumber(args.at<Object>(4)));
        hours = hours_object->Number();
        if (argc >= 5) {
          Handle<Object> minutes_object;
          ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
              isolate, minutes_object, Object::ToNumber(args.at<Object>(5)));
          minutes = minutes_object->Number();
          if (argc >= 6) {
            Handle<Object> seconds_object;
            ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
                isolate, seconds_object, Object::ToNumber(args.at<Object>(6)));
            seconds = seconds_object->Number();
            if (argc >= 7) {
              Handle<Object> ms_object;
              ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
                  isolate, ms_object, Object::ToNumber(args.at<Object>(7)));
              ms = ms_object->Number();
            }
          }
        }
      }
    }
    if (!std::isnan(year)) {
      double const y = DoubleToInteger(year);
      if (0.0 <= y && y <= 99) year = 1900 + y;
    }
    double const day = MakeDay(year, month, date);
    double const time = MakeTime(hours, minutes, seconds, ms);
    time_val = MakeDate(day, time);
    if (time_val >= -DateCache::kMaxTimeBeforeUTCInMs &&
        time_val <= DateCache::kMaxTimeBeforeUTCInMs) {
      time_val = isolate->date_cache()->ToUTC(static_cast<int64_t>(time_val));
    } else {
      time_val = std::numeric_limits<double>::quiet_NaN();
    }
  }
  Handle<JSDate> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSDate::New(target, new_target, time_val));
  return *result;
}


// ES6 section 20.3.3.1 Date.now ( )
BUILTIN(DateNow) {
  HandleScope scope(isolate);
  return *isolate->factory()->NewNumber(JSDate::CurrentTimeValue(isolate));
}


// ES6 section 20.3.3.2 Date.parse ( string )
BUILTIN(DateParse) {
  HandleScope scope(isolate);
  Handle<String> string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, string,
      Object::ToString(isolate, args.atOrUndefined(isolate, 1)));
  return *isolate->factory()->NewNumber(ParseDateTimeString(string));
}


// ES6 section 20.3.3.4 Date.UTC (year,month,date,hours,minutes,seconds,ms)
BUILTIN(DateUTC) {
  HandleScope scope(isolate);
  int const argc = args.length() - 1;
  double year = std::numeric_limits<double>::quiet_NaN();
  double month = std::numeric_limits<double>::quiet_NaN();
  double date = 1.0, hours = 0.0, minutes = 0.0, seconds = 0.0, ms = 0.0;
  if (argc >= 1) {
    Handle<Object> year_object;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, year_object,
                                       Object::ToNumber(args.at<Object>(1)));
    year = year_object->Number();
    if (argc >= 2) {
      Handle<Object> month_object;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month_object,
                                         Object::ToNumber(args.at<Object>(2)));
      month = month_object->Number();
      if (argc >= 3) {
        Handle<Object> date_object;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, date_object, Object::ToNumber(args.at<Object>(3)));
        date = date_object->Number();
        if (argc >= 4) {
          Handle<Object> hours_object;
          ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
              isolate, hours_object, Object::ToNumber(args.at<Object>(4)));
          hours = hours_object->Number();
          if (argc >= 5) {
            Handle<Object> minutes_object;
            ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
                isolate, minutes_object, Object::ToNumber(args.at<Object>(5)));
            minutes = minutes_object->Number();
            if (argc >= 6) {
              Handle<Object> seconds_object;
              ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
                  isolate, seconds_object,
                  Object::ToNumber(args.at<Object>(6)));
              seconds = seconds_object->Number();
              if (argc >= 7) {
                Handle<Object> ms_object;
                ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
                    isolate, ms_object, Object::ToNumber(args.at<Object>(7)));
                ms = ms_object->Number();
              }
            }
          }
        }
      }
    }
  }
  if (!std::isnan(year)) {
    double const y = DoubleToInteger(year);
    if (0.0 <= y && y <= 99) year = 1900 + y;
  }
  double const day = MakeDay(year, month, date);
  double const time = MakeTime(hours, minutes, seconds, ms);
  return *isolate->factory()->NewNumber(TimeClip(MakeDate(day, time)));
}


// ES6 section 20.3.4.20 Date.prototype.setDate ( date )
BUILTIN(DatePrototypeSetDate) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setDate");
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value, Object::ToNumber(value));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int const days = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, days);
    int year, month, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
    time_val = MakeDate(MakeDay(year, month, value->Number()), time_within_day);
  }
  return SetLocalDateValue(date, time_val);
}


// ES6 section 20.3.4.21 Date.prototype.setFullYear (year, month, date)
BUILTIN(DatePrototypeSetFullYear) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setFullYear");
  int const argc = args.length() - 1;
  Handle<Object> year = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, year, Object::ToNumber(year));
  double y = year->Number(), m = 0.0, dt = 1.0;
  int time_within_day = 0;
  if (!std::isnan(date->value()->Number())) {
    int64_t const time_ms = static_cast<int64_t>(date->value()->Number());
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int const days = isolate->date_cache()->DaysFromTime(local_time_ms);
    time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, days);
    int year, month, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
    m = month;
    dt = day;
  }
  if (argc >= 2) {
    Handle<Object> month = args.at<Object>(2);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month, Object::ToNumber(month));
    m = month->Number();
    if (argc >= 3) {
      Handle<Object> date = args.at<Object>(3);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, date, Object::ToNumber(date));
      dt = date->Number();
    }
  }
  double time_val = MakeDate(MakeDay(y, m, dt), time_within_day);
  return SetLocalDateValue(date, time_val);
}


// ES6 section 20.3.4.22 Date.prototype.setHours(hour, min, sec, ms)
BUILTIN(DatePrototypeSetHours) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setHours");
  int const argc = args.length() - 1;
  Handle<Object> hour = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, hour, Object::ToNumber(hour));
  double h = hour->Number();
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int day = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, day);
    double m = (time_within_day / (60 * 1000)) % 60;
    double s = (time_within_day / 1000) % 60;
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> min = args.at<Object>(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, min, Object::ToNumber(min));
      m = min->Number();
      if (argc >= 3) {
        Handle<Object> sec = args.at<Object>(3);
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
        s = sec->Number();
        if (argc >= 4) {
          Handle<Object> ms = args.at<Object>(4);
          ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
          milli = ms->Number();
        }
      }
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return SetLocalDateValue(date, time_val);
}


// ES6 section 20.3.4.23 Date.prototype.setMilliseconds(ms)
BUILTIN(DatePrototypeSetMilliseconds) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setMilliseconds");
  Handle<Object> ms = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int day = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    int m = (time_within_day / (60 * 1000)) % 60;
    int s = (time_within_day / 1000) % 60;
    time_val = MakeDate(day, MakeTime(h, m, s, ms->Number()));
  }
  return SetLocalDateValue(date, time_val);
}


// ES6 section 20.3.4.24 Date.prototype.setMinutes ( min, sec, ms )
BUILTIN(DatePrototypeSetMinutes) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setMinutes");
  int const argc = args.length() - 1;
  Handle<Object> min = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, min, Object::ToNumber(min));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int day = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    double m = min->Number();
    double s = (time_within_day / 1000) % 60;
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> sec = args.at<Object>(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
      s = sec->Number();
      if (argc >= 3) {
        Handle<Object> ms = args.at<Object>(3);
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
        milli = ms->Number();
      }
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return SetLocalDateValue(date, time_val);
}


// ES6 section 20.3.4.25 Date.prototype.setMonth ( month, date )
BUILTIN(DatePrototypeSetMonth) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setMonth");
  int const argc = args.length() - 1;
  Handle<Object> month = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month, Object::ToNumber(month));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int days = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, days);
    int year, unused, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &unused, &day);
    double m = month->Number();
    double dt = day;
    if (argc >= 2) {
      Handle<Object> date = args.at<Object>(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, date, Object::ToNumber(date));
      dt = date->Number();
    }
    time_val = MakeDate(MakeDay(year, m, dt), time_within_day);
  }
  return SetLocalDateValue(date, time_val);
}


// ES6 section 20.3.4.26 Date.prototype.setSeconds ( sec, ms )
BUILTIN(DatePrototypeSetSeconds) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setSeconds");
  int const argc = args.length() - 1;
  Handle<Object> sec = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int day = isolate->date_cache()->DaysFromTime(local_time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    double m = (time_within_day / (60 * 1000)) % 60;
    double s = sec->Number();
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> ms = args.at<Object>(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
      milli = ms->Number();
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return SetLocalDateValue(date, time_val);
}


// ES6 section 20.3.4.27 Date.prototype.setTime ( time )
BUILTIN(DatePrototypeSetTime) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setTime");
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value, Object::ToNumber(value));
  return *JSDate::SetValue(date, TimeClip(value->Number()));
}


// ES6 section 20.3.4.28 Date.prototype.setUTCDate ( date )
BUILTIN(DatePrototypeSetUTCDate) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCDate");
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value, Object::ToNumber(value));
  if (std::isnan(date->value()->Number())) return date->value();
  int64_t const time_ms = static_cast<int64_t>(date->value()->Number());
  int const days = isolate->date_cache()->DaysFromTime(time_ms);
  int const time_within_day = isolate->date_cache()->TimeInDay(time_ms, days);
  int year, month, day;
  isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
  double const time_val =
      MakeDate(MakeDay(year, month, value->Number()), time_within_day);
  return *JSDate::SetValue(date, TimeClip(time_val));
}


// ES6 section 20.3.4.29 Date.prototype.setUTCFullYear (year, month, date)
BUILTIN(DatePrototypeSetUTCFullYear) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCFullYear");
  int const argc = args.length() - 1;
  Handle<Object> year = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, year, Object::ToNumber(year));
  double y = year->Number(), m = 0.0, dt = 1.0;
  int time_within_day = 0;
  if (!std::isnan(date->value()->Number())) {
    int64_t const time_ms = static_cast<int64_t>(date->value()->Number());
    int const days = isolate->date_cache()->DaysFromTime(time_ms);
    time_within_day = isolate->date_cache()->TimeInDay(time_ms, days);
    int year, month, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
    m = month;
    dt = day;
  }
  if (argc >= 2) {
    Handle<Object> month = args.at<Object>(2);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month, Object::ToNumber(month));
    m = month->Number();
    if (argc >= 3) {
      Handle<Object> date = args.at<Object>(3);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, date, Object::ToNumber(date));
      dt = date->Number();
    }
  }
  double const time_val = MakeDate(MakeDay(y, m, dt), time_within_day);
  return *JSDate::SetValue(date, TimeClip(time_val));
}


// ES6 section 20.3.4.30 Date.prototype.setUTCHours(hour, min, sec, ms)
BUILTIN(DatePrototypeSetUTCHours) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCHours");
  int const argc = args.length() - 1;
  Handle<Object> hour = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, hour, Object::ToNumber(hour));
  double h = hour->Number();
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int day = isolate->date_cache()->DaysFromTime(time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(time_ms, day);
    double m = (time_within_day / (60 * 1000)) % 60;
    double s = (time_within_day / 1000) % 60;
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> min = args.at<Object>(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, min, Object::ToNumber(min));
      m = min->Number();
      if (argc >= 3) {
        Handle<Object> sec = args.at<Object>(3);
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
        s = sec->Number();
        if (argc >= 4) {
          Handle<Object> ms = args.at<Object>(4);
          ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
          milli = ms->Number();
        }
      }
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}


// ES6 section 20.3.4.31 Date.prototype.setUTCMilliseconds(ms)
BUILTIN(DatePrototypeSetUTCMilliseconds) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCMilliseconds");
  Handle<Object> ms = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int day = isolate->date_cache()->DaysFromTime(time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    int m = (time_within_day / (60 * 1000)) % 60;
    int s = (time_within_day / 1000) % 60;
    time_val = MakeDate(day, MakeTime(h, m, s, ms->Number()));
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}


// ES6 section 20.3.4.32 Date.prototype.setUTCMinutes ( min, sec, ms )
BUILTIN(DatePrototypeSetUTCMinutes) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCMinutes");
  int const argc = args.length() - 1;
  Handle<Object> min = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, min, Object::ToNumber(min));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int day = isolate->date_cache()->DaysFromTime(time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    double m = min->Number();
    double s = (time_within_day / 1000) % 60;
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> sec = args.at<Object>(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
      s = sec->Number();
      if (argc >= 3) {
        Handle<Object> ms = args.at<Object>(3);
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
        milli = ms->Number();
      }
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}


// ES6 section 20.3.4.31 Date.prototype.setUTCMonth ( month, date )
BUILTIN(DatePrototypeSetUTCMonth) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCMonth");
  int const argc = args.length() - 1;
  Handle<Object> month = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, month, Object::ToNumber(month));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int days = isolate->date_cache()->DaysFromTime(time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(time_ms, days);
    int year, unused, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &unused, &day);
    double m = month->Number();
    double dt = day;
    if (argc >= 2) {
      Handle<Object> date = args.at<Object>(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, date, Object::ToNumber(date));
      dt = date->Number();
    }
    time_val = MakeDate(MakeDay(year, m, dt), time_within_day);
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}


// ES6 section 20.3.4.34 Date.prototype.setUTCSeconds ( sec, ms )
BUILTIN(DatePrototypeSetUTCSeconds) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setUTCSeconds");
  int const argc = args.length() - 1;
  Handle<Object> sec = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, sec, Object::ToNumber(sec));
  double time_val = date->value()->Number();
  if (!std::isnan(time_val)) {
    int64_t const time_ms = static_cast<int64_t>(time_val);
    int day = isolate->date_cache()->DaysFromTime(time_ms);
    int time_within_day = isolate->date_cache()->TimeInDay(time_ms, day);
    int h = time_within_day / (60 * 60 * 1000);
    double m = (time_within_day / (60 * 1000)) % 60;
    double s = sec->Number();
    double milli = time_within_day % 1000;
    if (argc >= 2) {
      Handle<Object> ms = args.at<Object>(2);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, ms, Object::ToNumber(ms));
      milli = ms->Number();
    }
    time_val = MakeDate(day, MakeTime(h, m, s, milli));
  }
  return *JSDate::SetValue(date, TimeClip(time_val));
}


// ES6 section 20.3.4.35 Date.prototype.toDateString ( )
BUILTIN(DatePrototypeToDateString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.toDateString");
  char buffer[128];
  Vector<char> str(buffer, arraysize(buffer));
  ToDateString(date->value()->Number(), str, isolate->date_cache(), kDateOnly);
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      isolate->factory()->NewStringFromUtf8(CStrVector(buffer)));
  return *result;
}


// ES6 section 20.3.4.36 Date.prototype.toISOString ( )
BUILTIN(DatePrototypeToISOString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.toISOString");
  double const time_val = date->value()->Number();
  if (std::isnan(time_val)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidTimeValue));
  }
  int64_t const time_ms = static_cast<int64_t>(time_val);
  int year, month, day, weekday, hour, min, sec, ms;
  isolate->date_cache()->BreakDownTime(time_ms, &year, &month, &day, &weekday,
                                       &hour, &min, &sec, &ms);
  char buffer[128];
  Vector<char> str(buffer, arraysize(buffer));
  if (year >= 0 && year <= 9999) {
    SNPrintF(str, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", year, month + 1, day,
             hour, min, sec, ms);
  } else if (year < 0) {
    SNPrintF(str, "-%06d-%02d-%02dT%02d:%02d:%02d.%03dZ", -year, month + 1, day,
             hour, min, sec, ms);
  } else {
    SNPrintF(str, "+%06d-%02d-%02dT%02d:%02d:%02d.%03dZ", year, month + 1, day,
             hour, min, sec, ms);
  }
  return *isolate->factory()->NewStringFromAsciiChecked(str.start());
}


// ES6 section 20.3.4.41 Date.prototype.toString ( )
BUILTIN(DatePrototypeToString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.toString");
  char buffer[128];
  Vector<char> str(buffer, arraysize(buffer));
  ToDateString(date->value()->Number(), str, isolate->date_cache());
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      isolate->factory()->NewStringFromUtf8(CStrVector(buffer)));
  return *result;
}


// ES6 section 20.3.4.42 Date.prototype.toTimeString ( )
BUILTIN(DatePrototypeToTimeString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.toTimeString");
  char buffer[128];
  Vector<char> str(buffer, arraysize(buffer));
  ToDateString(date->value()->Number(), str, isolate->date_cache(), kTimeOnly);
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      isolate->factory()->NewStringFromUtf8(CStrVector(buffer)));
  return *result;
}


// ES6 section 20.3.4.43 Date.prototype.toUTCString ( )
BUILTIN(DatePrototypeToUTCString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.toUTCString");
  double const time_val = date->value()->Number();
  if (std::isnan(time_val)) {
    return *isolate->factory()->NewStringFromAsciiChecked("Invalid Date");
  }
  char buffer[128];
  Vector<char> str(buffer, arraysize(buffer));
  int64_t time_ms = static_cast<int64_t>(time_val);
  int year, month, day, weekday, hour, min, sec, ms;
  isolate->date_cache()->BreakDownTime(time_ms, &year, &month, &day, &weekday,
                                       &hour, &min, &sec, &ms);
  SNPrintF(str, "%s, %02d %s %4d %02d:%02d:%02d GMT", kShortWeekDays[weekday],
           day, kShortMonths[month], year, hour, min, sec);
  return *isolate->factory()->NewStringFromAsciiChecked(str.start());
}


// ES6 section 20.3.4.44 Date.prototype.valueOf ( )
BUILTIN(DatePrototypeValueOf) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.valueOf");
  return date->value();
}


// ES6 section 20.3.4.45 Date.prototype [ @@toPrimitive ] ( hint )
BUILTIN(DatePrototypeToPrimitive) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CHECK_RECEIVER(JSReceiver, receiver, "Date.prototype [ @@toPrimitive ]");
  Handle<Object> hint = args.at<Object>(1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSDate::ToPrimitive(receiver, hint));
  return *result;
}


// ES6 section B.2.4.1 Date.prototype.getYear ( )
BUILTIN(DatePrototypeGetYear) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.getYear");
  double time_val = date->value()->Number();
  if (std::isnan(time_val)) return date->value();
  int64_t time_ms = static_cast<int64_t>(time_val);
  int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
  int days = isolate->date_cache()->DaysFromTime(local_time_ms);
  int year, month, day;
  isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
  return Smi::FromInt(year - 1900);
}


// ES6 section B.2.4.2 Date.prototype.setYear ( year )
BUILTIN(DatePrototypeSetYear) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDate, date, "Date.prototype.setYear");
  Handle<Object> year = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, year, Object::ToNumber(year));
  double m = 0.0, dt = 1.0, y = year->Number();
  if (0.0 <= y && y <= 99.0) {
    y = 1900.0 + DoubleToInteger(y);
  }
  int time_within_day = 0;
  if (!std::isnan(date->value()->Number())) {
    int64_t const time_ms = static_cast<int64_t>(date->value()->Number());
    int64_t local_time_ms = isolate->date_cache()->ToLocal(time_ms);
    int const days = isolate->date_cache()->DaysFromTime(local_time_ms);
    time_within_day = isolate->date_cache()->TimeInDay(local_time_ms, days);
    int year, month, day;
    isolate->date_cache()->YearMonthDayFromDays(days, &year, &month, &day);
    m = month;
    dt = day;
  }
  double time_val = MakeDate(MakeDay(y, m, dt), time_within_day);
  return SetLocalDateValue(date, time_val);
}


// static
void Builtins::Generate_DatePrototypeGetDate(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kDay);
}


// static
void Builtins::Generate_DatePrototypeGetDay(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kWeekday);
}


// static
void Builtins::Generate_DatePrototypeGetFullYear(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kYear);
}


// static
void Builtins::Generate_DatePrototypeGetHours(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kHour);
}


// static
void Builtins::Generate_DatePrototypeGetMilliseconds(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kMillisecond);
}


// static
void Builtins::Generate_DatePrototypeGetMinutes(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kMinute);
}


// static
void Builtins::Generate_DatePrototypeGetMonth(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kMonth);
}


// static
void Builtins::Generate_DatePrototypeGetSeconds(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kSecond);
}


// static
void Builtins::Generate_DatePrototypeGetTime(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kDateValue);
}


// static
void Builtins::Generate_DatePrototypeGetTimezoneOffset(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kTimezoneOffset);
}


// static
void Builtins::Generate_DatePrototypeGetUTCDate(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kDayUTC);
}


// static
void Builtins::Generate_DatePrototypeGetUTCDay(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kWeekdayUTC);
}


// static
void Builtins::Generate_DatePrototypeGetUTCFullYear(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kYearUTC);
}


// static
void Builtins::Generate_DatePrototypeGetUTCHours(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kHourUTC);
}


// static
void Builtins::Generate_DatePrototypeGetUTCMilliseconds(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kMillisecondUTC);
}


// static
void Builtins::Generate_DatePrototypeGetUTCMinutes(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kMinuteUTC);
}


// static
void Builtins::Generate_DatePrototypeGetUTCMonth(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kMonthUTC);
}


// static
void Builtins::Generate_DatePrototypeGetUTCSeconds(MacroAssembler* masm) {
  Generate_DatePrototype_GetField(masm, JSDate::kSecondUTC);
}


namespace {

// ES6 section 19.2.1.1.1 CreateDynamicFunction
MaybeHandle<JSFunction> CreateDynamicFunction(
    Isolate* isolate,
    BuiltinArguments<BuiltinExtraArguments::kTargetAndNewTarget> args,
    const char* token) {
  // Compute number of arguments, ignoring the receiver.
  DCHECK_LE(1, args.length());
  int const argc = args.length() - 1;

  // Build the source string.
  Handle<String> source;
  {
    IncrementalStringBuilder builder(isolate);
    builder.AppendCharacter('(');
    builder.AppendCString(token);
    builder.AppendCharacter('(');
    bool parenthesis_in_arg_string = false;
    if (argc > 1) {
      for (int i = 1; i < argc; ++i) {
        if (i > 1) builder.AppendCharacter(',');
        Handle<String> param;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, param, Object::ToString(isolate, args.at<Object>(i)),
            JSFunction);
        param = String::Flatten(param);
        builder.AppendString(param);
        // If the formal parameters string include ) - an illegal
        // character - it may make the combined function expression
        // compile. We avoid this problem by checking for this early on.
        DisallowHeapAllocation no_gc;  // Ensure vectors stay valid.
        String::FlatContent param_content = param->GetFlatContent();
        for (int i = 0, length = param->length(); i < length; ++i) {
          if (param_content.Get(i) == ')') {
            parenthesis_in_arg_string = true;
            break;
          }
        }
      }
      // If the formal parameters include an unbalanced block comment, the
      // function must be rejected. Since JavaScript does not allow nested
      // comments we can include a trailing block comment to catch this.
      builder.AppendCString("\n/**/");
    }
    builder.AppendCString(") {\n");
    if (argc > 0) {
      Handle<String> body;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, body, Object::ToString(isolate, args.at<Object>(argc)),
          JSFunction);
      builder.AppendString(body);
    }
    builder.AppendCString("\n})");
    ASSIGN_RETURN_ON_EXCEPTION(isolate, source, builder.Finish(), JSFunction);

    // The SyntaxError must be thrown after all the (observable) ToString
    // conversions are done.
    if (parenthesis_in_arg_string) {
      THROW_NEW_ERROR(isolate,
                      NewSyntaxError(MessageTemplate::kParenthesisInArgString),
                      JSFunction);
    }
  }

  // Compile the string in the constructor and not a helper so that errors to
  // come from here.
  Handle<JSFunction> target = args.target<JSFunction>();
  Handle<JSObject> target_global_proxy(target->global_proxy(), isolate);
  Handle<JSFunction> function;
  {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, function,
        CompileString(handle(target->native_context(), isolate), source,
                      ONLY_SINGLE_FUNCTION_LITERAL),
        JSFunction);
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, function, target_global_proxy, 0, nullptr),
        JSFunction);
    function = Handle<JSFunction>::cast(result);
    function->shared()->set_name_should_print_as_anonymous(true);
  }

  // If new.target is equal to target then the function created
  // is already correctly setup and nothing else should be done
  // here. But if new.target is not equal to target then we are
  // have a Function builtin subclassing case and therefore the
  // function has wrong initial map. To fix that we create a new
  // function object with correct initial map.
  Handle<Object> unchecked_new_target = args.new_target();
  if (!unchecked_new_target->IsUndefined() &&
      !unchecked_new_target.is_identical_to(target)) {
    Handle<JSReceiver> new_target =
        Handle<JSReceiver>::cast(unchecked_new_target);
    Handle<Map> initial_map;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, initial_map,
        JSFunction::GetDerivedMap(isolate, target, new_target), JSFunction);

    Handle<SharedFunctionInfo> shared_info(function->shared(), isolate);
    Handle<Map> map = Map::AsLanguageMode(
        initial_map, shared_info->language_mode(), shared_info->kind());

    Handle<Context> context(function->context(), isolate);
    function = isolate->factory()->NewFunctionFromSharedFunctionInfo(
        map, shared_info, context, NOT_TENURED);
  }
  return function;
}

}  // namespace


// ES6 section 19.2.1.1 Function ( p1, p2, ... , pn, body )
BUILTIN(FunctionConstructor) {
  HandleScope scope(isolate);
  Handle<JSFunction> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, CreateDynamicFunction(isolate, args, "function"));
  return *result;
}


// ES6 section 19.2.3.2 Function.prototype.bind ( thisArg, ...args )
BUILTIN(FunctionPrototypeBind) {
  HandleScope scope(isolate);
  DCHECK_LE(1, args.length());
  if (!args.receiver()->IsCallable()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kFunctionBind));
  }

  // Allocate the bound function with the given {this_arg} and {args}.
  Handle<JSReceiver> target = args.at<JSReceiver>(0);
  Handle<Object> this_arg = isolate->factory()->undefined_value();
  ScopedVector<Handle<Object>> argv(std::max(0, args.length() - 2));
  if (args.length() > 1) {
    this_arg = args.at<Object>(1);
    for (int i = 2; i < args.length(); ++i) {
      argv[i - 2] = args.at<Object>(i);
    }
  }
  Handle<JSBoundFunction> function;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, function,
      isolate->factory()->NewJSBoundFunction(target, this_arg, argv));

  // TODO(bmeurer): Optimize the rest for the common cases where {target} is
  // a function with some initial map or even a bound function.
  // Setup the "length" property based on the "length" of the {target}.
  Handle<Object> length(Smi::FromInt(0), isolate);
  Maybe<bool> target_has_length =
      JSReceiver::HasOwnProperty(target, isolate->factory()->length_string());
  if (!target_has_length.IsJust()) {
    return isolate->heap()->exception();
  } else if (target_has_length.FromJust()) {
    Handle<Object> target_length;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, target_length,
        JSReceiver::GetProperty(target, isolate->factory()->length_string()));
    if (target_length->IsNumber()) {
      length = isolate->factory()->NewNumber(std::max(
          0.0, DoubleToInteger(target_length->Number()) - argv.length()));
    }
  }
  function->set_length(*length);

  // Setup the "name" property based on the "name" of the {target}.
  Handle<Object> target_name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, target_name,
      JSReceiver::GetProperty(target, isolate->factory()->name_string()));
  Handle<String> name;
  if (!target_name->IsString()) {
    name = isolate->factory()->bound__string();
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, name, Name::ToFunctionName(Handle<String>::cast(target_name)));
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, name, isolate->factory()->NewConsString(
                           isolate->factory()->bound__string(), name));
  }
  function->set_name(*name);
  return *function;
}


// ES6 section 19.2.3.5 Function.prototype.toString ( )
BUILTIN(FunctionPrototypeToString) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  if (receiver->IsJSBoundFunction()) {
    return *JSBoundFunction::ToString(Handle<JSBoundFunction>::cast(receiver));
  } else if (receiver->IsJSFunction()) {
    return *JSFunction::ToString(Handle<JSFunction>::cast(receiver));
  }
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kNotGeneric,
                            isolate->factory()->NewStringFromAsciiChecked(
                                "Function.prototype.toString")));
}


// ES6 section 25.2.1.1 GeneratorFunction (p1, p2, ... , pn, body)
BUILTIN(GeneratorFunctionConstructor) {
  HandleScope scope(isolate);
  Handle<JSFunction> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, CreateDynamicFunction(isolate, args, "function*"));
  return *result;
}

// ES6 section 19.2.3.6 Function.prototype[@@hasInstance](V)
BUILTIN(FunctionHasInstance) {
  HandleScope scope(isolate);
  Handle<Object> callable = args.receiver();
  Handle<Object> object = args.atOrUndefined(isolate, 1);

  // {callable} must have a [[Call]] internal method.
  if (!callable->IsCallable()) {
    return isolate->heap()->false_value();
  }
  // If {object} is not a receiver, return false.
  if (!object->IsJSReceiver()) {
    return isolate->heap()->false_value();
  }
  // Check if {callable} is bound, if so, get [[BoundTargetFunction]] from it
  // and use that instead of {callable}.
  while (callable->IsJSBoundFunction()) {
    callable =
        handle(Handle<JSBoundFunction>::cast(callable)->bound_target_function(),
               isolate);
  }
  DCHECK(callable->IsCallable());
  // Get the "prototype" of {callable}; raise an error if it's not a receiver.
  Handle<Object> prototype;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, prototype,
      Object::GetProperty(callable, isolate->factory()->prototype_string()));
  if (!prototype->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kInstanceofNonobjectProto, prototype));
  }
  // Return whether or not {prototype} is in the prototype chain of {object}.
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(object);
  Maybe<bool> result =
      JSReceiver::HasInPrototypeChain(isolate, receiver, prototype);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}

// ES6 section 19.4.1.1 Symbol ( [ description ] ) for the [[Call]] case.
BUILTIN(SymbolConstructor) {
  HandleScope scope(isolate);
  Handle<Symbol> result = isolate->factory()->NewSymbol();
  Handle<Object> description = args.atOrUndefined(isolate, 1);
  if (!description->IsUndefined()) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, description,
                                       Object::ToString(isolate, description));
    result->set_name(*description);
  }
  return *result;
}


// ES6 section 19.4.1.1 Symbol ( [ description ] ) for the [[Construct]] case.
BUILTIN(SymbolConstructor_ConstructStub) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kNotConstructor,
                            isolate->factory()->Symbol_string()));
}


// ES6 19.1.3.6 Object.prototype.toString
BUILTIN(ObjectProtoToString) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSObject::ObjectProtoToString(isolate, object));
  return *result;
}

// -----------------------------------------------------------------------------
// ES6 section 21.1 String Objects

namespace {

bool ToUint16(Handle<Object> value, uint16_t* result) {
  if (value->IsNumber() || Object::ToNumber(value).ToHandle(&value)) {
    *result = DoubleToUint32(value->Number());
    return true;
  }
  return false;
}

}  // namespace

// ES6 21.1.2.1 String.fromCharCode ( ...codeUnits )
BUILTIN(StringFromCharCode) {
  HandleScope scope(isolate);
  // Check resulting string length.
  int index = 0;
  Handle<String> result;
  int const length = args.length() - 1;
  if (length == 0) return isolate->heap()->empty_string();
  DCHECK_LT(0, length);
  // Load the first character code.
  uint16_t code;
  if (!ToUint16(args.at<Object>(1), &code)) return isolate->heap()->exception();
  // Assume that the resulting String contains only one byte characters.
  if (code <= String::kMaxOneByteCharCodeU) {
    // Check for single one-byte character fast case.
    if (length == 1) {
      return *isolate->factory()->LookupSingleCharacterStringFromCode(code);
    }
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawOneByteString(length));
    do {
      Handle<SeqOneByteString>::cast(result)->Set(index, code);
      if (++index == length) break;
      if (!ToUint16(args.at<Object>(1 + index), &code)) {
        return isolate->heap()->exception();
      }
    } while (code <= String::kMaxOneByteCharCodeU);
  }
  // Check if all characters fit into the one byte range.
  if (index < length) {
    // Fallback to two byte string.
    Handle<String> new_result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, new_result, isolate->factory()->NewRawTwoByteString(length));
    for (int new_index = 0; new_index < index; ++new_index) {
      uint16_t new_code =
          Handle<SeqOneByteString>::cast(result)->Get(new_index);
      Handle<SeqTwoByteString>::cast(new_result)->Set(new_index, new_code);
    }
    while (true) {
      Handle<SeqTwoByteString>::cast(new_result)->Set(index, code);
      if (++index == length) break;
      if (!ToUint16(args.at<Object>(1 + index), &code)) {
        return isolate->heap()->exception();
      }
    }
    result = new_result;
  }
  return *result;
}

// -----------------------------------------------------------------------------
// ES6 section 21.1 ArrayBuffer Objects

// ES6 section 24.1.2.1 ArrayBuffer ( length ) for the [[Call]] case.
BUILTIN(ArrayBufferConstructor) {
  HandleScope scope(isolate);
  Handle<JSFunction> target = args.target<JSFunction>();
  DCHECK(*target == target->native_context()->array_buffer_fun() ||
         *target == target->native_context()->shared_array_buffer_fun());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                            handle(target->shared()->name(), isolate)));
}


// ES6 section 24.1.2.1 ArrayBuffer ( length ) for the [[Construct]] case.
BUILTIN(ArrayBufferConstructor_ConstructStub) {
  HandleScope scope(isolate);
  Handle<JSFunction> target = args.target<JSFunction>();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  Handle<Object> length = args.atOrUndefined(isolate, 1);
  DCHECK(*target == target->native_context()->array_buffer_fun() ||
         *target == target->native_context()->shared_array_buffer_fun());
  Handle<Object> number_length;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number_length,
                                     Object::ToInteger(isolate, length));
  if (number_length->Number() < 0.0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferLength));
  }
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::New(target, new_target));
  size_t byte_length;
  if (!TryNumberToSize(isolate, *number_length, &byte_length)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferLength));
  }
  SharedFlag shared_flag =
      (*target == target->native_context()->array_buffer_fun())
          ? SharedFlag::kNotShared
          : SharedFlag::kShared;
  if (!JSArrayBuffer::SetupAllocatingData(Handle<JSArrayBuffer>::cast(result),
                                          isolate, byte_length, true,
                                          shared_flag)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kArrayBufferAllocationFailed));
  }
  return *result;
}


// ES6 section 24.1.3.1 ArrayBuffer.isView ( arg )
BUILTIN(ArrayBufferIsView) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  Object* arg = args[1];
  return isolate->heap()->ToBoolean(arg->IsJSArrayBufferView());
}


// ES6 section 26.2.1.1 Proxy ( target, handler ) for the [[Call]] case.
BUILTIN(ProxyConstructor) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate,
      NewTypeError(MessageTemplate::kConstructorNotFunction,
                   isolate->factory()->NewStringFromAsciiChecked("Proxy")));
}


// ES6 section 26.2.1.1 Proxy ( target, handler ) for the [[Construct]] case.
BUILTIN(ProxyConstructor_ConstructStub) {
  HandleScope scope(isolate);
  DCHECK(isolate->proxy_function()->IsConstructor());
  Handle<Object> target = args.atOrUndefined(isolate, 1);
  Handle<Object> handler = args.atOrUndefined(isolate, 2);
  Handle<JSProxy> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSProxy::New(isolate, target, handler));
  return *result;
}


// -----------------------------------------------------------------------------
// Throwers for restricted function properties and strict arguments object
// properties


BUILTIN(RestrictedFunctionPropertiesThrower) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kRestrictedFunctionProperties));
}


BUILTIN(RestrictedStrictArgumentsPropertiesThrower) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kStrictPoisonPill));
}


// -----------------------------------------------------------------------------
//


namespace {

template <bool is_construct>
MUST_USE_RESULT MaybeHandle<Object> HandleApiCallHelper(
    Isolate* isolate, BuiltinArguments<BuiltinExtraArguments::kTarget> args) {
  HandleScope scope(isolate);
  Handle<HeapObject> function = args.target<HeapObject>();
  Handle<JSReceiver> receiver;
  // TODO(ishell): turn this back to a DCHECK.
  CHECK(function->IsFunctionTemplateInfo() ||
        Handle<JSFunction>::cast(function)->shared()->IsApiFunction());

  Handle<FunctionTemplateInfo> fun_data =
      function->IsFunctionTemplateInfo()
          ? Handle<FunctionTemplateInfo>::cast(function)
          : handle(JSFunction::cast(*function)->shared()->get_api_func_data());
  if (is_construct) {
    DCHECK(args.receiver()->IsTheHole());
    if (fun_data->instance_template()->IsUndefined()) {
      v8::Local<ObjectTemplate> templ =
          ObjectTemplate::New(reinterpret_cast<v8::Isolate*>(isolate),
                              ToApiHandle<v8::FunctionTemplate>(fun_data));
      fun_data->set_instance_template(*Utils::OpenHandle(*templ));
    }
    Handle<ObjectTemplateInfo> instance_template(
        ObjectTemplateInfo::cast(fun_data->instance_template()), isolate);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, receiver,
                               ApiNatives::InstantiateObject(instance_template),
                               Object);
    args[0] = *receiver;
    DCHECK_EQ(*receiver, *args.receiver());
  } else {
    DCHECK(args.receiver()->IsJSReceiver());
    receiver = args.at<JSReceiver>(0);
  }

  if (!is_construct && !fun_data->accept_any_receiver()) {
    if (receiver->IsJSObject() && receiver->IsAccessCheckNeeded()) {
      Handle<JSObject> js_receiver = Handle<JSObject>::cast(receiver);
      if (!isolate->MayAccess(handle(isolate->context()), js_receiver)) {
        isolate->ReportFailedAccessCheck(js_receiver);
        RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      }
    }
  }

  Object* raw_holder = fun_data->GetCompatibleReceiver(isolate, *receiver);

  if (raw_holder->IsNull()) {
    // This function cannot be called with the given receiver.  Abort!
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kIllegalInvocation),
                    Object);
  }

  Object* raw_call_data = fun_data->call_code();
  if (!raw_call_data->IsUndefined()) {
    // TODO(ishell): remove this debugging code.
    CHECK(raw_call_data->IsCallHandlerInfo());
    CallHandlerInfo* call_data = CallHandlerInfo::cast(raw_call_data);
    Object* callback_obj = call_data->callback();
    v8::FunctionCallback callback =
        v8::ToCData<v8::FunctionCallback>(callback_obj);
    Object* data_obj = call_data->data();

    LOG(isolate, ApiObjectAccess("call", JSObject::cast(*args.receiver())));
    DCHECK(raw_holder->IsJSObject());

    FunctionCallbackArguments custom(isolate,
                                     data_obj,
                                     *function,
                                     raw_holder,
                                     &args[0] - 1,
                                     args.length() - 1,
                                     is_construct);

    v8::Local<v8::Value> value = custom.Call(callback);
    Handle<Object> result;
    if (value.IsEmpty()) {
      result = isolate->factory()->undefined_value();
    } else {
      result = v8::Utils::OpenHandle(*value);
      result->VerifyApiCallResultType();
    }

    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (!is_construct || result->IsJSObject()) {
      return scope.CloseAndEscape(result);
    }
  }

  return scope.CloseAndEscape(receiver);
}

}  // namespace


BUILTIN(HandleApiCall) {
  HandleScope scope(isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     HandleApiCallHelper<false>(isolate, args));
  return *result;
}


BUILTIN(HandleApiCallConstruct) {
  HandleScope scope(isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     HandleApiCallHelper<true>(isolate, args));
  return *result;
}

Handle<Code> Builtins::CallFunction(ConvertReceiverMode mode,
                                    TailCallMode tail_call_mode) {
  switch (tail_call_mode) {
    case TailCallMode::kDisallow:
      switch (mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return CallFunction_ReceiverIsNullOrUndefined();
        case ConvertReceiverMode::kNotNullOrUndefined:
          return CallFunction_ReceiverIsNotNullOrUndefined();
        case ConvertReceiverMode::kAny:
          return CallFunction_ReceiverIsAny();
      }
      break;
    case TailCallMode::kAllow:
      switch (mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return TailCallFunction_ReceiverIsNullOrUndefined();
        case ConvertReceiverMode::kNotNullOrUndefined:
          return TailCallFunction_ReceiverIsNotNullOrUndefined();
        case ConvertReceiverMode::kAny:
          return TailCallFunction_ReceiverIsAny();
      }
      break;
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

Handle<Code> Builtins::Call(ConvertReceiverMode mode,
                            TailCallMode tail_call_mode) {
  switch (tail_call_mode) {
    case TailCallMode::kDisallow:
      switch (mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return Call_ReceiverIsNullOrUndefined();
        case ConvertReceiverMode::kNotNullOrUndefined:
          return Call_ReceiverIsNotNullOrUndefined();
        case ConvertReceiverMode::kAny:
          return Call_ReceiverIsAny();
      }
      break;
    case TailCallMode::kAllow:
      switch (mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return TailCall_ReceiverIsNullOrUndefined();
        case ConvertReceiverMode::kNotNullOrUndefined:
          return TailCall_ReceiverIsNotNullOrUndefined();
        case ConvertReceiverMode::kAny:
          return TailCall_ReceiverIsAny();
      }
      break;
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

Handle<Code> Builtins::CallBoundFunction(TailCallMode tail_call_mode) {
  switch (tail_call_mode) {
    case TailCallMode::kDisallow:
      return CallBoundFunction();
    case TailCallMode::kAllow:
      return TailCallBoundFunction();
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

Handle<Code> Builtins::InterpreterPushArgsAndCall(TailCallMode tail_call_mode) {
  switch (tail_call_mode) {
    case TailCallMode::kDisallow:
      return InterpreterPushArgsAndCall();
    case TailCallMode::kAllow:
      return InterpreterPushArgsAndTailCall();
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

namespace {

class RelocatableArguments
    : public BuiltinArguments<BuiltinExtraArguments::kTarget>,
      public Relocatable {
 public:
  RelocatableArguments(Isolate* isolate, int length, Object** arguments)
      : BuiltinArguments<BuiltinExtraArguments::kTarget>(length, arguments),
        Relocatable(isolate) {}

  virtual inline void IterateInstance(ObjectVisitor* v) {
    if (length() == 0) return;
    v->VisitPointers(lowest_address(), highest_address() + 1);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RelocatableArguments);
};

}  // namespace

MaybeHandle<Object> Builtins::InvokeApiFunction(Handle<HeapObject> function,
                                                Handle<Object> receiver,
                                                int argc,
                                                Handle<Object> args[]) {
  // Construct BuiltinArguments object: function, arguments reversed, receiver.
  const int kBufferSize = 32;
  Object* small_argv[kBufferSize];
  Object** argv;
  if (argc + 2 <= kBufferSize) {
    argv = small_argv;
  } else {
    argv = new Object* [argc + 2];
  }
  argv[argc + 1] = *receiver;
  for (int i = 0; i < argc; ++i) {
    argv[argc - i] = *args[i];
  }
  argv[0] = *function;
  MaybeHandle<Object> result;
  {
    auto isolate = function->GetIsolate();
    RelocatableArguments arguments(isolate, argc + 2, &argv[argc + 1]);
    result = HandleApiCallHelper<false>(isolate, arguments);
  }
  if (argv != small_argv) {
    delete[] argv;
  }
  return result;
}


// Helper function to handle calls to non-function objects created through the
// API. The object can be called as either a constructor (using new) or just as
// a function (without new).
MUST_USE_RESULT static Object* HandleApiCallAsFunctionOrConstructor(
    Isolate* isolate, bool is_construct_call,
    BuiltinArguments<BuiltinExtraArguments::kNone> args) {
  Heap* heap = isolate->heap();

  Handle<Object> receiver = args.receiver();

  // Get the object called.
  JSObject* obj = JSObject::cast(*receiver);

  // Get the invocation callback from the function descriptor that was
  // used to create the called object.
  DCHECK(obj->map()->is_callable());
  JSFunction* constructor = JSFunction::cast(obj->map()->GetConstructor());
  // TODO(ishell): turn this back to a DCHECK.
  CHECK(constructor->shared()->IsApiFunction());
  Object* handler =
      constructor->shared()->get_api_func_data()->instance_call_handler();
  DCHECK(!handler->IsUndefined());
  // TODO(ishell): remove this debugging code.
  CHECK(handler->IsCallHandlerInfo());
  CallHandlerInfo* call_data = CallHandlerInfo::cast(handler);
  Object* callback_obj = call_data->callback();
  v8::FunctionCallback callback =
      v8::ToCData<v8::FunctionCallback>(callback_obj);

  // Get the data for the call and perform the callback.
  Object* result;
  {
    HandleScope scope(isolate);
    LOG(isolate, ApiObjectAccess("call non-function", obj));

    FunctionCallbackArguments custom(isolate,
                                     call_data->data(),
                                     constructor,
                                     obj,
                                     &args[0] - 1,
                                     args.length() - 1,
                                     is_construct_call);
    v8::Local<v8::Value> value = custom.Call(callback);
    if (value.IsEmpty()) {
      result = heap->undefined_value();
    } else {
      result = *reinterpret_cast<Object**>(*value);
      result->VerifyApiCallResultType();
    }
  }
  // Check for exceptions and return result.
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return result;
}


// Handle calls to non-function objects created through the API. This delegate
// function is used when the call is a normal function call.
BUILTIN(HandleApiCallAsFunction) {
  return HandleApiCallAsFunctionOrConstructor(isolate, false, args);
}


// Handle calls to non-function objects created through the API. This delegate
// function is used when the call is a construct call.
BUILTIN(HandleApiCallAsConstructor) {
  return HandleApiCallAsFunctionOrConstructor(isolate, true, args);
}


static void Generate_LoadIC_Miss(MacroAssembler* masm) {
  LoadIC::GenerateMiss(masm);
}


static void Generate_LoadIC_Normal(MacroAssembler* masm) {
  LoadIC::GenerateNormal(masm);
}


static void Generate_LoadIC_Getter_ForDeopt(MacroAssembler* masm) {
  NamedLoadHandlerCompiler::GenerateLoadViaGetterForDeopt(masm);
}


static void Generate_LoadIC_Slow(MacroAssembler* masm) {
  LoadIC::GenerateRuntimeGetProperty(masm);
}


static void Generate_KeyedLoadIC_Slow(MacroAssembler* masm) {
  KeyedLoadIC::GenerateRuntimeGetProperty(masm);
}


static void Generate_KeyedLoadIC_Miss(MacroAssembler* masm) {
  KeyedLoadIC::GenerateMiss(masm);
}


static void Generate_KeyedLoadIC_Megamorphic(MacroAssembler* masm) {
  KeyedLoadIC::GenerateMegamorphic(masm);
}


static void Generate_StoreIC_Miss(MacroAssembler* masm) {
  StoreIC::GenerateMiss(masm);
}


static void Generate_StoreIC_Normal(MacroAssembler* masm) {
  StoreIC::GenerateNormal(masm);
}


static void Generate_StoreIC_Slow(MacroAssembler* masm) {
  NamedStoreHandlerCompiler::GenerateSlow(masm);
}


static void Generate_KeyedStoreIC_Slow(MacroAssembler* masm) {
  ElementHandlerCompiler::GenerateStoreSlow(masm);
}


static void Generate_StoreIC_Setter_ForDeopt(MacroAssembler* masm) {
  NamedStoreHandlerCompiler::GenerateStoreViaSetterForDeopt(masm);
}


static void Generate_KeyedStoreIC_Megamorphic(MacroAssembler* masm) {
  KeyedStoreIC::GenerateMegamorphic(masm, SLOPPY);
}


static void Generate_KeyedStoreIC_Megamorphic_Strict(MacroAssembler* masm) {
  KeyedStoreIC::GenerateMegamorphic(masm, STRICT);
}


static void Generate_KeyedStoreIC_Miss(MacroAssembler* masm) {
  KeyedStoreIC::GenerateMiss(masm);
}


static void Generate_KeyedStoreIC_Initialize(MacroAssembler* masm) {
  KeyedStoreIC::GenerateInitialize(masm);
}


static void Generate_KeyedStoreIC_Initialize_Strict(MacroAssembler* masm) {
  KeyedStoreIC::GenerateInitialize(masm);
}


static void Generate_KeyedStoreIC_PreMonomorphic(MacroAssembler* masm) {
  KeyedStoreIC::GeneratePreMonomorphic(masm);
}


static void Generate_KeyedStoreIC_PreMonomorphic_Strict(MacroAssembler* masm) {
  KeyedStoreIC::GeneratePreMonomorphic(masm);
}


static void Generate_Return_DebugBreak(MacroAssembler* masm) {
  DebugCodegen::GenerateDebugBreakStub(masm,
                                       DebugCodegen::SAVE_RESULT_REGISTER);
}


static void Generate_Slot_DebugBreak(MacroAssembler* masm) {
  DebugCodegen::GenerateDebugBreakStub(masm,
                                       DebugCodegen::IGNORE_RESULT_REGISTER);
}


static void Generate_FrameDropper_LiveEdit(MacroAssembler* masm) {
  DebugCodegen::GenerateFrameDropperLiveEdit(masm);
}


Builtins::Builtins() : initialized_(false) {
  memset(builtins_, 0, sizeof(builtins_[0]) * builtin_count);
  memset(names_, 0, sizeof(names_[0]) * builtin_count);
}


Builtins::~Builtins() {
}


#define DEF_ENUM_C(name, ignore) FUNCTION_ADDR(Builtin_##name),
Address const Builtins::c_functions_[cfunction_count] = {
  BUILTIN_LIST_C(DEF_ENUM_C)
};
#undef DEF_ENUM_C


struct BuiltinDesc {
  byte* generator;
  byte* c_code;
  const char* s_name;  // name is only used for generating log information.
  int name;
  Code::Flags flags;
  BuiltinExtraArguments extra_args;
};

#define BUILTIN_FUNCTION_TABLE_INIT { V8_ONCE_INIT, {} }

class BuiltinFunctionTable {
 public:
  BuiltinDesc* functions() {
    base::CallOnce(&once_, &Builtins::InitBuiltinFunctionTable);
    return functions_;
  }

  base::OnceType once_;
  BuiltinDesc functions_[Builtins::builtin_count + 1];

  friend class Builtins;
};

static BuiltinFunctionTable builtin_function_table =
    BUILTIN_FUNCTION_TABLE_INIT;

// Define array of pointers to generators and C builtin functions.
// We do this in a sort of roundabout way so that we can do the initialization
// within the lexical scope of Builtins:: and within a context where
// Code::Flags names a non-abstract type.
void Builtins::InitBuiltinFunctionTable() {
  BuiltinDesc* functions = builtin_function_table.functions_;
  functions[builtin_count].generator = NULL;
  functions[builtin_count].c_code = NULL;
  functions[builtin_count].s_name = NULL;
  functions[builtin_count].name = builtin_count;
  functions[builtin_count].flags = static_cast<Code::Flags>(0);
  functions[builtin_count].extra_args = BuiltinExtraArguments::kNone;

#define DEF_FUNCTION_PTR_C(aname, aextra_args)                \
  functions->generator = FUNCTION_ADDR(Generate_Adaptor);     \
  functions->c_code = FUNCTION_ADDR(Builtin_##aname);         \
  functions->s_name = #aname;                                 \
  functions->name = c_##aname;                                \
  functions->flags = Code::ComputeFlags(Code::BUILTIN);       \
  functions->extra_args = BuiltinExtraArguments::aextra_args; \
  ++functions;

#define DEF_FUNCTION_PTR_A(aname, kind, state, extra)              \
  functions->generator = FUNCTION_ADDR(Generate_##aname);          \
  functions->c_code = NULL;                                        \
  functions->s_name = #aname;                                      \
  functions->name = k##aname;                                      \
  functions->flags = Code::ComputeFlags(Code::kind, state, extra); \
  functions->extra_args = BuiltinExtraArguments::kNone;            \
  ++functions;

#define DEF_FUNCTION_PTR_H(aname, kind)                     \
  functions->generator = FUNCTION_ADDR(Generate_##aname);   \
  functions->c_code = NULL;                                 \
  functions->s_name = #aname;                               \
  functions->name = k##aname;                               \
  functions->flags = Code::ComputeHandlerFlags(Code::kind); \
  functions->extra_args = BuiltinExtraArguments::kNone;     \
  ++functions;

  BUILTIN_LIST_C(DEF_FUNCTION_PTR_C)
  BUILTIN_LIST_A(DEF_FUNCTION_PTR_A)
  BUILTIN_LIST_H(DEF_FUNCTION_PTR_H)
  BUILTIN_LIST_DEBUG_A(DEF_FUNCTION_PTR_A)

#undef DEF_FUNCTION_PTR_C
#undef DEF_FUNCTION_PTR_A
}


void Builtins::SetUp(Isolate* isolate, bool create_heap_objects) {
  DCHECK(!initialized_);

  // Create a scope for the handles in the builtins.
  HandleScope scope(isolate);

  const BuiltinDesc* functions = builtin_function_table.functions();

  // For now we generate builtin adaptor code into a stack-allocated
  // buffer, before copying it into individual code objects. Be careful
  // with alignment, some platforms don't like unaligned code.
#ifdef DEBUG
  // We can generate a lot of debug code on Arm64.
  const size_t buffer_size = 32*KB;
#elif V8_TARGET_ARCH_PPC64
  // 8 KB is insufficient on PPC64 when FLAG_debug_code is on.
  const size_t buffer_size = 10 * KB;
#else
  const size_t buffer_size = 8*KB;
#endif
  union { int force_alignment; byte buffer[buffer_size]; } u;

  // Traverse the list of builtins and generate an adaptor in a
  // separate code object for each one.
  for (int i = 0; i < builtin_count; i++) {
    if (create_heap_objects) {
      MacroAssembler masm(isolate, u.buffer, sizeof u.buffer,
                          CodeObjectRequired::kYes);
      // Generate the code/adaptor.
      typedef void (*Generator)(MacroAssembler*, int, BuiltinExtraArguments);
      Generator g = FUNCTION_CAST<Generator>(functions[i].generator);
      // We pass all arguments to the generator, but it may not use all of
      // them.  This works because the first arguments are on top of the
      // stack.
      DCHECK(!masm.has_frame());
      g(&masm, functions[i].name, functions[i].extra_args);
      // Move the code into the object heap.
      CodeDesc desc;
      masm.GetCode(&desc);
      Code::Flags flags = functions[i].flags;
      Handle<Code> code =
          isolate->factory()->NewCode(desc, flags, masm.CodeObject());
      // Log the event and add the code to the builtins array.
      PROFILE(isolate,
              CodeCreateEvent(Logger::BUILTIN_TAG, *code, functions[i].s_name));
      builtins_[i] = *code;
      code->set_builtin_index(i);
#ifdef ENABLE_DISASSEMBLER
      if (FLAG_print_builtin_code) {
        CodeTracer::Scope trace_scope(isolate->GetCodeTracer());
        OFStream os(trace_scope.file());
        os << "Builtin: " << functions[i].s_name << "\n";
        code->Disassemble(functions[i].s_name, os);
        os << "\n";
      }
#endif
    } else {
      // Deserializing. The values will be filled in during IterateBuiltins.
      builtins_[i] = NULL;
    }
    names_[i] = functions[i].s_name;
  }

  // Mark as initialized.
  initialized_ = true;
}


void Builtins::TearDown() {
  initialized_ = false;
}


void Builtins::IterateBuiltins(ObjectVisitor* v) {
  v->VisitPointers(&builtins_[0], &builtins_[0] + builtin_count);
}


const char* Builtins::Lookup(byte* pc) {
  // may be called during initialization (disassembler!)
  if (initialized_) {
    for (int i = 0; i < builtin_count; i++) {
      Code* entry = Code::cast(builtins_[i]);
      if (entry->contains(pc)) {
        return names_[i];
      }
    }
  }
  return NULL;
}


void Builtins::Generate_InterruptCheck(MacroAssembler* masm) {
  masm->TailCallRuntime(Runtime::kInterrupt);
}


void Builtins::Generate_StackCheck(MacroAssembler* masm) {
  masm->TailCallRuntime(Runtime::kStackGuard);
}


#define DEFINE_BUILTIN_ACCESSOR_C(name, ignore)               \
Handle<Code> Builtins::name() {                               \
  Code** code_address =                                       \
      reinterpret_cast<Code**>(builtin_address(k##name));     \
  return Handle<Code>(code_address);                          \
}
#define DEFINE_BUILTIN_ACCESSOR_A(name, kind, state, extra) \
Handle<Code> Builtins::name() {                             \
  Code** code_address =                                     \
      reinterpret_cast<Code**>(builtin_address(k##name));   \
  return Handle<Code>(code_address);                        \
}
#define DEFINE_BUILTIN_ACCESSOR_H(name, kind)               \
Handle<Code> Builtins::name() {                             \
  Code** code_address =                                     \
      reinterpret_cast<Code**>(builtin_address(k##name));   \
  return Handle<Code>(code_address);                        \
}
BUILTIN_LIST_C(DEFINE_BUILTIN_ACCESSOR_C)
BUILTIN_LIST_A(DEFINE_BUILTIN_ACCESSOR_A)
BUILTIN_LIST_H(DEFINE_BUILTIN_ACCESSOR_H)
BUILTIN_LIST_DEBUG_A(DEFINE_BUILTIN_ACCESSOR_A)
#undef DEFINE_BUILTIN_ACCESSOR_C
#undef DEFINE_BUILTIN_ACCESSOR_A


}  // namespace internal
}  // namespace v8
