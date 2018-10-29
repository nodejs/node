// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"
#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/conversions-inl.h"
#include "src/counters.h"
#include "src/futex-emulation.h"
#include "src/globals.h"
#include "src/heap/factory.h"
#include "src/objects-inl.h"
#include "src/objects/js-array-buffer-inl.h"

namespace v8 {
namespace internal {

// See builtins-arraybuffer.cc for implementations of
// SharedArrayBuffer.prototye.byteLength and SharedArrayBuffer.prototype.slice

inline bool AtomicIsLockFree(uint32_t size) {
  return size == 1 || size == 2 || size == 4;
}

// ES #sec-atomics.islockfree
BUILTIN(AtomicsIsLockFree) {
  HandleScope scope(isolate);
  Handle<Object> size = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, size,
                                     Object::ToNumber(isolate, size));
  return *isolate->factory()->ToBoolean(AtomicIsLockFree(size->Number()));
}

// ES #sec-validatesharedintegertypedarray
V8_WARN_UNUSED_RESULT MaybeHandle<JSTypedArray> ValidateSharedIntegerTypedArray(
    Isolate* isolate, Handle<Object> object, bool only_int32 = false) {
  if (object->IsJSTypedArray()) {
    Handle<JSTypedArray> typed_array = Handle<JSTypedArray>::cast(object);
    if (typed_array->GetBuffer()->is_shared()) {
      if (only_int32) {
        if (typed_array->type() == kExternalInt32Array) return typed_array;
      } else {
        if (typed_array->type() != kExternalFloat32Array &&
            typed_array->type() != kExternalFloat64Array &&
            typed_array->type() != kExternalUint8ClampedArray)
          return typed_array;
      }
    }
  }

  THROW_NEW_ERROR(
      isolate,
      NewTypeError(only_int32 ? MessageTemplate::kNotInt32SharedTypedArray
                              : MessageTemplate::kNotIntegerSharedTypedArray,
                   object),
      JSTypedArray);
}

// ES #sec-validateatomicaccess
// ValidateAtomicAccess( typedArray, requestIndex )
V8_WARN_UNUSED_RESULT Maybe<size_t> ValidateAtomicAccess(
    Isolate* isolate, Handle<JSTypedArray> typed_array,
    Handle<Object> request_index) {
  Handle<Object> access_index_obj;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, access_index_obj,
      Object::ToIndex(isolate, request_index,
                      MessageTemplate::kInvalidAtomicAccessIndex),
      Nothing<size_t>());

  size_t access_index;
  if (!TryNumberToSize(*access_index_obj, &access_index) ||
      typed_array->WasNeutered() ||
      access_index >= typed_array->length_value()) {
    isolate->Throw(*isolate->factory()->NewRangeError(
        MessageTemplate::kInvalidAtomicAccessIndex));
    return Nothing<size_t>();
  }
  return Just<size_t>(access_index);
}

namespace {
MaybeHandle<Object> AtomicsWake(Isolate* isolate, Handle<Object> array,
                                Handle<Object> index, Handle<Object> count) {
  Handle<JSTypedArray> sta;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, sta, ValidateSharedIntegerTypedArray(isolate, array, true),
      Object);

  Maybe<size_t> maybe_index = ValidateAtomicAccess(isolate, sta, index);
  MAYBE_RETURN_NULL(maybe_index);
  size_t i = maybe_index.FromJust();

  uint32_t c;
  if (count->IsUndefined(isolate)) {
    c = kMaxUInt32;
  } else {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, count,
                               Object::ToInteger(isolate, count), Object);
    double count_double = count->Number();
    if (count_double < 0)
      count_double = 0;
    else if (count_double > kMaxUInt32)
      count_double = kMaxUInt32;
    c = static_cast<uint32_t>(count_double);
  }

  Handle<JSArrayBuffer> array_buffer = sta->GetBuffer();
  size_t addr = (i << 2) + sta->byte_offset();

  return Handle<Object>(FutexEmulation::Wake(array_buffer, addr, c), isolate);
}

}  // namespace

// ES #sec-atomics.wake
// Atomics.wake( typedArray, index, count )
BUILTIN(AtomicsWake) {
  HandleScope scope(isolate);
  Handle<Object> array = args.atOrUndefined(isolate, 1);
  Handle<Object> index = args.atOrUndefined(isolate, 2);
  Handle<Object> count = args.atOrUndefined(isolate, 3);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kAtomicsWake);
  RETURN_RESULT_OR_FAILURE(isolate, AtomicsWake(isolate, array, index, count));
}

// ES #sec-atomics.notify
// Atomics.notify( typedArray, index, count )
BUILTIN(AtomicsNotify) {
  HandleScope scope(isolate);
  Handle<Object> array = args.atOrUndefined(isolate, 1);
  Handle<Object> index = args.atOrUndefined(isolate, 2);
  Handle<Object> count = args.atOrUndefined(isolate, 3);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kAtomicsNotify);
  RETURN_RESULT_OR_FAILURE(isolate, AtomicsWake(isolate, array, index, count));
}

// ES #sec-atomics.wait
// Atomics.wait( typedArray, index, value, timeout )
BUILTIN(AtomicsWait) {
  HandleScope scope(isolate);
  Handle<Object> array = args.atOrUndefined(isolate, 1);
  Handle<Object> index = args.atOrUndefined(isolate, 2);
  Handle<Object> value = args.atOrUndefined(isolate, 3);
  Handle<Object> timeout = args.atOrUndefined(isolate, 4);

  Handle<JSTypedArray> sta;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, sta, ValidateSharedIntegerTypedArray(isolate, array, true));

  Maybe<size_t> maybe_index = ValidateAtomicAccess(isolate, sta, index);
  if (maybe_index.IsNothing()) return ReadOnlyRoots(isolate).exception();
  size_t i = maybe_index.FromJust();

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value,
                                     Object::ToInt32(isolate, value));
  int32_t value_int32 = NumberToInt32(*value);

  double timeout_number;
  if (timeout->IsUndefined(isolate)) {
    timeout_number = ReadOnlyRoots(isolate).infinity_value()->Number();
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, timeout,
                                       Object::ToNumber(isolate, timeout));
    timeout_number = timeout->Number();
    if (std::isnan(timeout_number))
      timeout_number = ReadOnlyRoots(isolate).infinity_value()->Number();
    else if (timeout_number < 0)
      timeout_number = 0;
  }

  if (!isolate->allow_atomics_wait()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kAtomicsWaitNotAllowed));
  }

  Handle<JSArrayBuffer> array_buffer = sta->GetBuffer();
  size_t addr = (i << 2) + sta->byte_offset();

  return FutexEmulation::Wait(isolate, array_buffer, addr, value_int32,
                              timeout_number);
}

}  // namespace internal
}  // namespace v8
