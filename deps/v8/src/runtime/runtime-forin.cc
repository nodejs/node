// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

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
  // TODO(turbofan): Fast case for array indices.
  Handle<Name> name;
  if (!Object::ToName(isolate, key).ToHandle(&name)) {
    return isolate->heap()->exception();
  }
  Maybe<bool> result = JSReceiver::HasProperty(receiver, name);
  if (!result.IsJust()) return isolate->heap()->exception();
  if (result.FromJust()) return *name;
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_ForInNext) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, cache_array, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, cache_type, 2);
  CONVERT_SMI_ARG_CHECKED(index, 3);
  Handle<Object> key = handle(cache_array->get(index), isolate);
  // Don't need filtering if expected map still matches that of the receiver,
  // and neither for proxies.
  if (receiver->map() == *cache_type || *cache_type == Smi::FromInt(0)) {
    return *key;
  }
  // TODO(turbofan): Fast case for array indices.
  Handle<Name> name;
  if (!Object::ToName(isolate, key).ToHandle(&name)) {
    return isolate->heap()->exception();
  }
  Maybe<bool> result = JSReceiver::HasProperty(receiver, name);
  if (!result.IsJust()) return isolate->heap()->exception();
  if (result.FromJust()) return *name;
  return isolate->heap()->undefined_value();
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
