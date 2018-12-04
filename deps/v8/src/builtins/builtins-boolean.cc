// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/counters.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES #sec-boolean-objects

// ES #sec-boolean-constructor
BUILTIN(BooleanConstructor) {
  HandleScope scope(isolate);
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    Handle<Object> value = args.atOrUndefined(isolate, 1);
    return isolate->heap()->ToBoolean(value->BooleanValue(isolate));
  }
  // [[Construct]]
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  DCHECK(*target == target->native_context()->boolean_function());
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  Handle<JSValue>::cast(result)->set_value(
      isolate->heap()->ToBoolean(value->BooleanValue(isolate)));
  return *result;
}

}  // namespace internal
}  // namespace v8
