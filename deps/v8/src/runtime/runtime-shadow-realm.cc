// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/arguments-inl.h"
#include "src/objects/js-function.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_ShadowRealmWrappedFunctionCreate) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  Handle<NativeContext> native_context = args.at<NativeContext>(0);
  Handle<JSReceiver> value = args.at<JSReceiver>(1);

  RETURN_RESULT_OR_FAILURE(
      isolate, JSWrappedFunction::Create(isolate, native_context, value));
}

}  // namespace internal
}  // namespace v8
