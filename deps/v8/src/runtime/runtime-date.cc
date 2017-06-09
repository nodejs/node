// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/conversions-inl.h"
#include "src/date.h"
#include "src/factory.h"
#include "src/isolate-inl.h"
#include "src/messages.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_IsDate) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSDate());
}


RUNTIME_FUNCTION(Runtime_ThrowNotDateError) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                 NewTypeError(MessageTemplate::kNotDateObject));
}


RUNTIME_FUNCTION(Runtime_DateCurrentTime) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  return *isolate->factory()->NewNumber(JSDate::CurrentTimeValue(isolate));
}

}  // namespace internal
}  // namespace v8
