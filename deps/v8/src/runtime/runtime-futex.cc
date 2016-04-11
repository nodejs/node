// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/base/platform/time.h"
#include "src/conversions-inl.h"
#include "src/futex-emulation.h"
#include "src/globals.h"

// Implement Futex API for SharedArrayBuffers as defined in the
// SharedArrayBuffer draft spec, found here:
// https://github.com/lars-t-hansen/ecmascript_sharedmem

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_AtomicsFutexWait) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_INT32_ARG_CHECKED(value, 2);
  CONVERT_DOUBLE_ARG_CHECKED(timeout, 3);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));
  RUNTIME_ASSERT(sta->type() == kExternalInt32Array);
  RUNTIME_ASSERT(timeout == V8_INFINITY || !std::isnan(timeout));

  Handle<JSArrayBuffer> array_buffer = sta->GetBuffer();
  size_t addr = (index << 2) + NumberToSize(isolate, sta->byte_offset());

  return FutexEmulation::Wait(isolate, array_buffer, addr, value, timeout);
}


RUNTIME_FUNCTION(Runtime_AtomicsFutexWake) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_INT32_ARG_CHECKED(count, 2);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));
  RUNTIME_ASSERT(sta->type() == kExternalInt32Array);

  Handle<JSArrayBuffer> array_buffer = sta->GetBuffer();
  size_t addr = (index << 2) + NumberToSize(isolate, sta->byte_offset());

  return FutexEmulation::Wake(isolate, array_buffer, addr, count);
}


RUNTIME_FUNCTION(Runtime_AtomicsFutexWakeOrRequeue) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index1, 1);
  CONVERT_INT32_ARG_CHECKED(count, 2);
  CONVERT_INT32_ARG_CHECKED(value, 3);
  CONVERT_SIZE_ARG_CHECKED(index2, 4);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index1 < NumberToSize(isolate, sta->length()));
  RUNTIME_ASSERT(index2 < NumberToSize(isolate, sta->length()));
  RUNTIME_ASSERT(sta->type() == kExternalInt32Array);

  Handle<JSArrayBuffer> array_buffer = sta->GetBuffer();
  size_t addr1 = (index1 << 2) + NumberToSize(isolate, sta->byte_offset());
  size_t addr2 = (index2 << 2) + NumberToSize(isolate, sta->byte_offset());

  return FutexEmulation::WakeOrRequeue(isolate, array_buffer, addr1, count,
                                       value, addr2);
}


RUNTIME_FUNCTION(Runtime_AtomicsFutexNumWaitersForTesting) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));
  RUNTIME_ASSERT(sta->type() == kExternalInt32Array);

  Handle<JSArrayBuffer> array_buffer = sta->GetBuffer();
  size_t addr = (index << 2) + NumberToSize(isolate, sta->byte_offset());

  return FutexEmulation::NumWaitersForTesting(isolate, array_buffer, addr);
}
}  // namespace internal
}  // namespace v8
