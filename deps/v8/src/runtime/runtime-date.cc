// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/conversions-inl.h"
#include "src/date.h"
#include "src/heap/factory.h"
#include "src/isolate-inl.h"
#include "src/messages.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_DateCurrentTime) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  return *isolate->factory()->NewNumber(JSDate::CurrentTimeValue(isolate));
}

}  // namespace internal
}  // namespace v8
