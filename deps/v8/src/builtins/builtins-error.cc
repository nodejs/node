// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/accessors.h"
#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/counters.h"
#include "src/messages.h"
#include "src/objects-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/property-descriptor.h"

namespace v8 {
namespace internal {

// ES6 section 19.5.1.1 Error ( message )
BUILTIN(ErrorConstructor) {
  HandleScope scope(isolate);

  FrameSkipMode mode = SKIP_FIRST;
  Handle<Object> caller;

  // When we're passed a JSFunction as new target, we can skip frames until that
  // specific function is seen instead of unconditionally skipping the first
  // frame.
  if (args.new_target()->IsJSFunction()) {
    mode = SKIP_UNTIL_SEEN;
    caller = args.new_target();
  }

  RETURN_RESULT_OR_FAILURE(
      isolate, ErrorUtils::Construct(isolate, args.target(),
                                     Handle<Object>::cast(args.new_target()),
                                     args.atOrUndefined(isolate, 1), mode,
                                     caller, false));
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

namespace {

Object* MakeGenericError(Isolate* isolate, BuiltinArguments args,
                         Handle<JSFunction> constructor) {
  Handle<Object> template_index = args.atOrUndefined(isolate, 1);
  Handle<Object> arg0 = args.atOrUndefined(isolate, 2);
  Handle<Object> arg1 = args.atOrUndefined(isolate, 3);
  Handle<Object> arg2 = args.atOrUndefined(isolate, 4);

  DCHECK(template_index->IsSmi());

  RETURN_RESULT_OR_FAILURE(
      isolate, ErrorUtils::MakeGenericError(isolate, constructor,
                                            Smi::ToInt(*template_index), arg0,
                                            arg1, arg2, SKIP_NONE));
}

}  // namespace

BUILTIN(MakeError) {
  HandleScope scope(isolate);
  return MakeGenericError(isolate, args, isolate->error_function());
}

BUILTIN(MakeRangeError) {
  HandleScope scope(isolate);
  return MakeGenericError(isolate, args, isolate->range_error_function());
}

BUILTIN(MakeSyntaxError) {
  HandleScope scope(isolate);
  return MakeGenericError(isolate, args, isolate->syntax_error_function());
}

BUILTIN(MakeTypeError) {
  HandleScope scope(isolate);
  return MakeGenericError(isolate, args, isolate->type_error_function());
}

BUILTIN(MakeURIError) {
  HandleScope scope(isolate);
  Handle<JSFunction> constructor = isolate->uri_error_function();
  Handle<Object> undefined = isolate->factory()->undefined_value();
  const int template_index = MessageTemplate::kURIMalformed;
  RETURN_RESULT_OR_FAILURE(
      isolate,
      ErrorUtils::MakeGenericError(isolate, constructor, template_index,
                                   undefined, undefined, undefined, SKIP_NONE));
}

}  // namespace internal
}  // namespace v8
