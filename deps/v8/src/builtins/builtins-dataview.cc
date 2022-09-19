// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/logging/counters.h"
#include "src/numbers/conversions.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES #sec-dataview-objects

// ES #sec-dataview-constructor
BUILTIN(DataViewConstructor) {
  const char* const kMethodName = "DataView constructor";
  HandleScope scope(isolate);
  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "DataView")));
  }
  // [[Construct]]
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  Handle<Object> buffer = args.atOrUndefined(isolate, 1);
  Handle<Object> byte_offset = args.atOrUndefined(isolate, 2);
  Handle<Object> byte_length = args.atOrUndefined(isolate, 3);

  // 2. Perform ? RequireInternalSlot(buffer, [[ArrayBufferData]]).
  if (!buffer->IsJSArrayBuffer()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDataViewNotArrayBuffer));
  }
  Handle<JSArrayBuffer> array_buffer = Handle<JSArrayBuffer>::cast(buffer);

  // 3. Let offset be ? ToIndex(byteOffset).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, byte_offset,
      Object::ToIndex(isolate, byte_offset, MessageTemplate::kInvalidOffset));
  size_t view_byte_offset = byte_offset->Number();

  // 4. If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
  if (array_buffer->was_detached()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  kMethodName)));
  }

  // 5. Let bufferByteLength be ArrayBufferByteLength(buffer, SeqCst).
  size_t buffer_byte_length = array_buffer->GetByteLength();

  // 6. If offset > bufferByteLength, throw a RangeError exception.
  if (view_byte_offset > buffer_byte_length) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidOffset, byte_offset));
  }

  // 7. Let bufferIsResizable be IsResizableArrayBuffer(buffer).
  // 8. Let byteLengthChecked be empty.
  // 9. If bufferIsResizable is true and byteLength is undefined, then
  //       a. Let viewByteLength be auto.
  // 10. Else if byteLength is undefined, then
  //       a. Let viewByteLength be bufferByteLength - offset.
  size_t view_byte_length;
  bool length_tracking = false;
  if (byte_length->IsUndefined(isolate)) {
    view_byte_length = buffer_byte_length - view_byte_offset;
    length_tracking = array_buffer->is_resizable();
  } else {
    // 11. Else,
    //       a. Set byteLengthChecked be ? ToIndex(byteLength).
    //       b. Let viewByteLength be byteLengthChecked.
    //       c. If offset + viewByteLength > bufferByteLength, throw a
    //          RangeError exception.
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, byte_length,
        Object::ToIndex(isolate, byte_length,
                        MessageTemplate::kInvalidDataViewLength));
    if (view_byte_offset + byte_length->Number() > buffer_byte_length) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate,
          NewRangeError(MessageTemplate::kInvalidDataViewLength, byte_length));
    }
    view_byte_length = byte_length->Number();
  }

  // 12. Let O be ? OrdinaryCreateFromConstructor(NewTarget,
  //     "%DataViewPrototype%", «[[DataView]], [[ViewedArrayBuffer]],
  //     [[ByteLength]], [[ByteOffset]]»).
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  Handle<JSDataView> data_view = Handle<JSDataView>::cast(result);
  for (int i = 0; i < ArrayBufferView::kEmbedderFieldCount; ++i) {
    // TODO(v8:10391, saelo): Handle external pointers in EmbedderDataSlot
    data_view->SetEmbedderField(i, Smi::zero());
  }
  data_view->set_bit_field(0);
  data_view->set_is_backed_by_rab(array_buffer->is_resizable() &&
                                  !array_buffer->is_shared());
  data_view->set_is_length_tracking(length_tracking);

  // We have to set the internal slots before the checks on steps 13 - 17 or
  // the TorqueGeneratedClassVerifier ended up complaining that the slot is
  // empty or invalid on heap teardown.
  // The result object is not observable from JavaScript when steps 13 - 17
  // early abort so it is fine to set internal slots here.

  // 18. Set O.[[ViewedArrayBuffer]] to buffer.
  data_view->set_buffer(*array_buffer);

  // 19. Set O.[[ByteLength]] to viewByteLength.
  data_view->set_byte_length(length_tracking ? 0 : view_byte_length);

  // 20. Set O.[[ByteOffset]] to offset.
  data_view->set_byte_offset(view_byte_offset);
  data_view->set_data_pointer(
      isolate,
      static_cast<uint8_t*>(array_buffer->backing_store()) + view_byte_offset);

  // 13. If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
  if (array_buffer->was_detached()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  kMethodName)));
  }

  // 14. Let getBufferByteLength be
  //     MakeIdempotentArrayBufferByteLengthGetter(SeqCst).
  // 15. Set bufferByteLength be getBufferByteLength(buffer).
  buffer_byte_length = array_buffer->GetByteLength();

  // 16. If offset > bufferByteLength, throw a RangeError exception.
  if (view_byte_offset > buffer_byte_length) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidOffset, byte_offset));
  }

  // 17. If byteLengthChecked is not empty, then
  //       a. If offset + viewByteLength > bufferByteLength, throw a RangeError
  //       exception.
  if (!length_tracking &&
      view_byte_offset + view_byte_length > buffer_byte_length) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidDataViewLength));
  }

  // 21. Return O.
  return *result;
}

}  // namespace internal
}  // namespace v8
