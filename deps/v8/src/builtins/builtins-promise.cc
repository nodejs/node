// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-promise.h"

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/counters.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

BUILTIN(IsPromise) {
  SealHandleScope scope(isolate);

  Handle<Object> object = args.atOrUndefined(isolate, 1);
  return isolate->heap()->ToBoolean(object->IsJSPromise());
}

}  // namespace internal
}  // namespace v8
