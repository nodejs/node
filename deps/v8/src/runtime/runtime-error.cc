// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/base/platform/time.h"
#include "src/conversions-inl.h"
#include "src/futex-emulation.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_ErrorToString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, recv, 0);
  RETURN_RESULT_OR_FAILURE(isolate, ErrorUtils::ToString(isolate, recv));
}

RUNTIME_FUNCTION(Runtime_IsJSError) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  return isolate->heap()->ToBoolean(args[0]->IsJSError());
}

}  // namespace internal
}  // namespace v8
