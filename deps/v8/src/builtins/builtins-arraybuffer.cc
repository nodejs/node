// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 21.1 ArrayBuffer Objects

// ES6 section 24.1.2.1 ArrayBuffer ( length ) for the [[Call]] case.
BUILTIN(ArrayBufferConstructor) {
  HandleScope scope(isolate);
  Handle<JSFunction> target = args.target<JSFunction>();
  DCHECK(*target == target->native_context()->array_buffer_fun() ||
         *target == target->native_context()->shared_array_buffer_fun());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                            handle(target->shared()->name(), isolate)));
}

// ES6 section 24.1.2.1 ArrayBuffer ( length ) for the [[Construct]] case.
BUILTIN(ArrayBufferConstructor_ConstructStub) {
  HandleScope scope(isolate);
  Handle<JSFunction> target = args.target<JSFunction>();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  Handle<Object> length = args.atOrUndefined(isolate, 1);
  DCHECK(*target == target->native_context()->array_buffer_fun() ||
         *target == target->native_context()->shared_array_buffer_fun());
  Handle<Object> number_length;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number_length,
                                     Object::ToInteger(isolate, length));
  if (number_length->Number() < 0.0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferLength));
  }
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::New(target, new_target));
  size_t byte_length;
  if (!TryNumberToSize(*number_length, &byte_length)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferLength));
  }
  SharedFlag shared_flag =
      (*target == target->native_context()->array_buffer_fun())
          ? SharedFlag::kNotShared
          : SharedFlag::kShared;
  if (!JSArrayBuffer::SetupAllocatingData(Handle<JSArrayBuffer>::cast(result),
                                          isolate, byte_length, true,
                                          shared_flag)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kArrayBufferAllocationFailed));
  }
  return *result;
}

// ES6 section 24.1.4.1 get ArrayBuffer.prototype.byteLength
BUILTIN(ArrayBufferPrototypeGetByteLength) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSArrayBuffer, array_buffer,
                 "get ArrayBuffer.prototype.byteLength");

  if (array_buffer->is_shared()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "get ArrayBuffer.prototype.byteLength"),
                              args.receiver()));
  }
  // TODO(franzih): According to the ES6 spec, we should throw a TypeError
  // here if the JSArrayBuffer is detached.
  return array_buffer->byte_length();
}

// ES6 section 24.1.3.1 ArrayBuffer.isView ( arg )
BUILTIN(ArrayBufferIsView) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  Object* arg = args[1];
  return isolate->heap()->ToBoolean(arg->IsJSArrayBufferView());
}

}  // namespace internal
}  // namespace v8
