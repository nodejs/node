// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/factory.h"
#include "src/isolate-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

namespace {

// Returns either a FixedArray or, if the given {receiver} has an enum cache
// that contains all enumerable properties of the {receiver} and its prototypes
// have none, the map of the {receiver}. This is used to speed up the check for
// deletions during a for-in.
MaybeHandle<HeapObject> Enumerate(Handle<JSReceiver> receiver) {
  Isolate* const isolate = receiver->GetIsolate();
  // Test if we have an enum cache for {receiver}.
  if (!receiver->IsSimpleEnum()) {
    Handle<FixedArray> keys;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, keys,
        JSReceiver::GetKeys(receiver, INCLUDE_PROTOS, ENUMERABLE_STRINGS),
        HeapObject);
    // Test again, since cache may have been built by GetKeys() calls above.
    if (!receiver->IsSimpleEnum()) return keys;
  }
  return handle(receiver->map(), isolate);
}


MaybeHandle<Object> Filter(Handle<JSReceiver> receiver, Handle<Object> key) {
  Isolate* const isolate = receiver->GetIsolate();
  // TODO(turbofan): Fast case for array indices.
  Handle<Name> name;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, name, Object::ToName(isolate, key),
                             Object);
  Maybe<bool> result = JSReceiver::HasProperty(receiver, name);
  MAYBE_RETURN_NULL(result);
  if (result.FromJust()) return name;
  return isolate->factory()->undefined_value();
}

}  // namespace


RUNTIME_FUNCTION(Runtime_ForInEnumerate) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  Handle<HeapObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, Enumerate(receiver));
  return *result;
}


RUNTIME_FUNCTION_RETURN_TRIPLE(Runtime_ForInPrepare) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSReceiver> receiver = args.at<JSReceiver>(0);
  Handle<Object> cache_type;
  if (!Enumerate(receiver).ToHandle(&cache_type)) {
    return MakeTriple(isolate->heap()->exception(), nullptr, nullptr);
  }
  Handle<FixedArray> cache_array;
  int cache_length;
  if (cache_type->IsMap()) {
    Handle<Map> cache_map = Handle<Map>::cast(cache_type);
    Handle<DescriptorArray> descriptors(cache_map->instance_descriptors(),
                                        isolate);
    cache_length = cache_map->EnumLength();
    if (cache_length && descriptors->HasEnumCache()) {
      cache_array = handle(descriptors->GetEnumCache(), isolate);
    } else {
      cache_array = isolate->factory()->empty_fixed_array();
      cache_length = 0;
    }
  } else {
    cache_array = Handle<FixedArray>::cast(cache_type);
    cache_length = cache_array->length();
    cache_type = handle(Smi::FromInt(1), isolate);
  }
  return MakeTriple(*cache_type, *cache_array, Smi::FromInt(cache_length));
}


RUNTIME_FUNCTION(Runtime_ForInDone) {
  SealHandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(index, 0);
  CONVERT_SMI_ARG_CHECKED(length, 1);
  DCHECK_LE(0, index);
  DCHECK_LE(index, length);
  return isolate->heap()->ToBoolean(index == length);
}


RUNTIME_FUNCTION(Runtime_ForInFilter) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, Filter(receiver, key));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ForInNext) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, cache_array, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, cache_type, 2);
  CONVERT_SMI_ARG_CHECKED(index, 3);
  Handle<Object> key = handle(cache_array->get(index), isolate);
  // Don't need filtering if expected map still matches that of the receiver.
  if (receiver->map() == *cache_type) {
    return *key;
  }
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, Filter(receiver, key));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ForInStep) {
  SealHandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(index, 0);
  DCHECK_LE(0, index);
  DCHECK_LT(index, Smi::kMaxValue);
  return Smi::FromInt(index + 1);
}

}  // namespace internal
}  // namespace v8
