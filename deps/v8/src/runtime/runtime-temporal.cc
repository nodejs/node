// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/objects/js-temporal-objects.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_IsInvalidTemporalCalendarField) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<String> s = args.at<String>(0);
  Handle<FixedArray> f = args.at<FixedArray>(1);
  RETURN_RESULT_OR_FAILURE(
      isolate, temporal::IsInvalidTemporalCalendarField(isolate, s, f));
}

}  // namespace internal
}  // namespace v8
