// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/base/platform/time.h"
#include "src/common/globals.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/futex-emulation.h"
#include "src/logging/counters.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/js-array-buffer-inl.h"

// Implement Futex API for SharedArrayBuffers as defined in the
// SharedArrayBuffer draft spec, found here:
// https://github.com/tc39/ecmascript_sharedmem

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_AtomicsNumWaitersForTesting) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CHECK(!sta->WasDetached());
  CHECK(sta->GetBuffer()->is_shared());
  CHECK_LT(index, sta->length());
  CHECK_EQ(sta->type(), kExternalInt32Array);

  Handle<JSArrayBuffer> array_buffer = sta->GetBuffer();
  size_t addr = (index << 2) + sta->byte_offset();

  return FutexEmulation::NumWaitersForTesting(array_buffer, addr);
}

RUNTIME_FUNCTION(Runtime_AtomicsNumAsyncWaitersForTesting) {
  DCHECK_EQ(0, args.length());
  return FutexEmulation::NumAsyncWaitersForTesting(isolate);
}

RUNTIME_FUNCTION(Runtime_AtomicsNumUnresolvedAsyncPromisesForTesting) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CHECK(!sta->WasDetached());
  CHECK(sta->GetBuffer()->is_shared());
  CHECK_LT(index, sta->length());
  CHECK_EQ(sta->type(), kExternalInt32Array);

  Handle<JSArrayBuffer> array_buffer = sta->GetBuffer();
  size_t addr = (index << 2) + sta->byte_offset();

  return FutexEmulation::NumUnresolvedAsyncPromisesForTesting(array_buffer,
                                                              addr);
}

RUNTIME_FUNCTION(Runtime_SetAllowAtomicsWait) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_BOOLEAN_ARG_CHECKED(set, 0);

  isolate->set_allow_atomics_wait(set);
  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace internal
}  // namespace v8
