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

RUNTIME_FUNCTION(Runtime_SetAllowAtomicsWait) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_BOOLEAN_ARG_CHECKED(set, 0);

  isolate->set_allow_atomics_wait(set);
  return isolate->heap()->undefined_value();
}
}  // namespace internal
}  // namespace v8
