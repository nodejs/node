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
// https://github.com/tc39/ecmascript_sharedmem

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_AtomicsWait) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_INT32_ARG_CHECKED(value, 2);
  CONVERT_DOUBLE_ARG_CHECKED(timeout, 3);
  CHECK(sta->GetBuffer()->is_shared());
  CHECK_LT(index, NumberToSize(sta->length()));
  CHECK_EQ(sta->type(), kExternalInt32Array);
  CHECK(timeout == V8_INFINITY || !std::isnan(timeout));

  Handle<JSArrayBuffer> array_buffer = sta->GetBuffer();
  size_t addr = (index << 2) + NumberToSize(sta->byte_offset());

  return FutexEmulation::Wait(isolate, array_buffer, addr, value, timeout);
}

RUNTIME_FUNCTION(Runtime_AtomicsWake) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_INT32_ARG_CHECKED(count, 2);
  CHECK(sta->GetBuffer()->is_shared());
  CHECK_LT(index, NumberToSize(sta->length()));
  CHECK_EQ(sta->type(), kExternalInt32Array);

  Handle<JSArrayBuffer> array_buffer = sta->GetBuffer();
  size_t addr = (index << 2) + NumberToSize(sta->byte_offset());

  return FutexEmulation::Wake(isolate, array_buffer, addr, count);
}

RUNTIME_FUNCTION(Runtime_AtomicsNumWaitersForTesting) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CHECK(sta->GetBuffer()->is_shared());
  CHECK_LT(index, NumberToSize(sta->length()));
  CHECK_EQ(sta->type(), kExternalInt32Array);

  Handle<JSArrayBuffer> array_buffer = sta->GetBuffer();
  size_t addr = (index << 2) + NumberToSize(sta->byte_offset());

  return FutexEmulation::NumWaitersForTesting(isolate, array_buffer, addr);
}
}  // namespace internal
}  // namespace v8
