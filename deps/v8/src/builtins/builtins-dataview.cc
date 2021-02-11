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
  HandleScope scope(isolate);
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

  // 2. If Type(buffer) is not Object, throw a TypeError exception.
  // 3. If buffer does not have an [[ArrayBufferData]] internal slot, throw a
  //    TypeError exception.
  if (!buffer->IsJSArrayBuffer()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDataViewNotArrayBuffer));
  }
  Handle<JSArrayBuffer> array_buffer = Handle<JSArrayBuffer>::cast(buffer);

  // 4. Let offset be ? ToIndex(byteOffset).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, byte_offset,
      Object::ToIndex(isolate, byte_offset, MessageTemplate::kInvalidOffset));
  size_t view_byte_offset = byte_offset->Number();

  // 5. If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
  //    We currently violate the specification at this point. TODO: Fix that.

  // 6. Let bufferByteLength be the value of buffer's
  // [[ArrayBufferByteLength]] internal slot.
  size_t const buffer_byte_length = array_buffer->byte_length();

  // 7. If offset > bufferByteLength, throw a RangeError exception.
  if (view_byte_offset > buffer_byte_length) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidOffset, byte_offset));
  }

  size_t view_byte_length;
  if (byte_length->IsUndefined(isolate)) {
    // 8. If byteLength is either not present or undefined, then
    //       a. Let viewByteLength be bufferByteLength - offset.
    view_byte_length = buffer_byte_length - view_byte_offset;
  } else {
    // 9. Else,
    //       a. Let viewByteLength be ? ToIndex(byteLength).
    //       b. If offset+viewByteLength > bufferByteLength, throw a
    //          RangeError exception.
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, byte_length,
        Object::ToIndex(isolate, byte_length,
                        MessageTemplate::kInvalidDataViewLength));
    if (view_byte_offset + byte_length->Number() > buffer_byte_length) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewRangeError(MessageTemplate::kInvalidDataViewLength));
    }
    view_byte_length = byte_length->Number();
  }

  // 10. Let O be ? OrdinaryCreateFromConstructor(NewTarget,
  //     "%DataViewPrototype%", «[[DataView]], [[ViewedArrayBuffer]],
  //     [[ByteLength]], [[ByteOffset]]»).
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  for (int i = 0; i < ArrayBufferView::kEmbedderFieldCount; ++i) {
    // TODO(v8:10391, saelo): Handle external pointers in EmbedderDataSlot
    Handle<JSDataView>::cast(result)->SetEmbedderField(i, Smi::zero());
  }

  // 11. Set O's [[ViewedArrayBuffer]] internal slot to buffer.
  Handle<JSDataView>::cast(result)->set_buffer(*array_buffer);

  // 12. Set O's [[ByteLength]] internal slot to viewByteLength.
  Handle<JSDataView>::cast(result)->set_byte_length(view_byte_length);

  // 13. Set O's [[ByteOffset]] internal slot to offset.
  Handle<JSDataView>::cast(result)->set_byte_offset(view_byte_offset);
  Handle<JSDataView>::cast(result)->AllocateExternalPointerEntries(isolate);
  Handle<JSDataView>::cast(result)->set_data_pointer(
      isolate,
      static_cast<uint8_t*>(array_buffer->backing_store()) + view_byte_offset);

  // 14. Return O.
  return *result;
}

}  // namespace internal
}  // namespace v8
