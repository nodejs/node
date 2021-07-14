// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/accessors.h"
#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/messages.h"
#include "src/logging/counters.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-descriptor.h"

namespace v8 {
namespace internal {

// ES6 section 19.5.1.1 Error ( message )
BUILTIN(ErrorConstructor) {
  HandleScope scope(isolate);
  Handle<Object> options = FLAG_harmony_error_cause
                               ? args.atOrUndefined(isolate, 2)
                               : isolate->factory()->undefined_value();
  RETURN_RESULT_OR_FAILURE(
      isolate, ErrorUtils::Construct(isolate, args.target(), args.new_target(),
                                     args.atOrUndefined(isolate, 1), options));
}

// static
BUILTIN(ErrorCaptureStackTrace) {
  HandleScope scope(isolate);
  Handle<Object> object_obj = args.atOrUndefined(isolate, 1);

  isolate->CountUsage(v8::Isolate::kErrorCaptureStackTrace);

  if (!object_obj->IsJSObject()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kInvalidArgument, object_obj));
  }

  Handle<JSObject> object = Handle<JSObject>::cast(object_obj);
  Handle<Object> caller = args.atOrUndefined(isolate, 2);
  FrameSkipMode mode = caller->IsJSFunction() ? SKIP_UNTIL_SEEN : SKIP_FIRST;

  // Collect the stack trace.

  RETURN_FAILURE_ON_EXCEPTION(isolate,
                              isolate->CaptureAndSetDetailedStackTrace(object));
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, isolate->CaptureAndSetSimpleStackTrace(object, mode, caller));

  // Add the stack accessors.

  Handle<AccessorInfo> error_stack = isolate->factory()->error_stack_accessor();
  Handle<Name> name(Name::cast(error_stack->name()), isolate);

  // Explicitly check for frozen objects. Other access checks are performed by
  // the LookupIterator in SetAccessor below.
  if (!JSObject::IsExtensible(object)) {
    return isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kDefineDisallowed, name));
  }

  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::SetAccessor(object, name, error_stack, DONT_ENUM));
  return ReadOnlyRoots(isolate).undefined_value();
}

// ES6 section 19.5.3.4 Error.prototype.toString ( )
BUILTIN(ErrorPrototypeToString) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(isolate,
                           ErrorUtils::ToString(isolate, args.receiver()));
}

}  // namespace internal
}  // namespace v8
