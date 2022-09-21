// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/logging/counters.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/objects-inl.h"
#include "src/web-snapshot/web-snapshot.h"

namespace v8 {
namespace internal {

BUILTIN(WebSnapshotSerialize) {
  HandleScope scope(isolate);
  if (args.length() < 2 || args.length() > 3) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kInvalidArgument));
  }
  Handle<Object> object = args.at(1);
  Handle<FixedArray> block_list = isolate->factory()->empty_fixed_array();
  Handle<JSArray> block_list_js_array;
  if (args.length() == 3) {
    if (!args[2].IsJSArray()) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError(MessageTemplate::kInvalidArgument));
    }
    block_list_js_array = args.at<JSArray>(2);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, block_list,
        JSReceiver::GetOwnValues(isolate, block_list_js_array,
                                 PropertyFilter::ENUMERABLE_STRINGS));
  }

  auto snapshot_data = std::make_shared<WebSnapshotData>();
  WebSnapshotSerializer serializer(isolate);
  if (!serializer.TakeSnapshot(object, block_list, *snapshot_data)) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  if (!block_list_js_array.is_null() &&
      static_cast<uint32_t>(block_list->length()) <
          serializer.external_object_count()) {
    Handle<FixedArray> externals = serializer.GetExternals();
    Handle<Map> map = JSObject::GetElementsTransitionMap(block_list_js_array,
                                                         PACKED_ELEMENTS);
    block_list_js_array->set_elements(*externals);
    block_list_js_array->set_length(Smi::FromInt(externals->length()));
    block_list_js_array->set_map(*map);
  }

  MaybeHandle<JSArrayBuffer> maybe_result =
      isolate->factory()->NewJSArrayBufferAndBackingStore(
          snapshot_data->buffer_size, InitializedFlag::kUninitialized);
  Handle<JSArrayBuffer> result;
  if (!maybe_result.ToHandle(&result)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kOutOfMemory,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "WebSnapshotSerialize")));
  }
  uint8_t* data =
      reinterpret_cast<uint8_t*>(result->GetBackingStore()->buffer_start());
  memcpy(data, snapshot_data->buffer, snapshot_data->buffer_size);

  return *result;
}

BUILTIN(WebSnapshotDeserialize) {
  HandleScope scope(isolate);
  if (args.length() < 2 || args.length() > 3) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kInvalidArgument));
  }

  if (!args[1].IsJSArrayBuffer()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kInvalidArgument));
  }
  auto buffer = args.at<JSArrayBuffer>(1);
  std::shared_ptr<BackingStore> backing_store = buffer->GetBackingStore();
  if (backing_store.get() == nullptr) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kInvalidArgument));
  }
  const uint8_t* data =
      reinterpret_cast<uint8_t*>(backing_store->buffer_start());

  Handle<FixedArray> injected_references =
      isolate->factory()->empty_fixed_array();
  if (args.length() == 3) {
    if (!args[2].IsJSArray()) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError(MessageTemplate::kInvalidArgument));
    }
    auto js_array = args.at<JSArray>(2);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, injected_references,
        JSReceiver::GetOwnValues(isolate, js_array,
                                 PropertyFilter::ENUMERABLE_STRINGS));
  }

  WebSnapshotDeserializer deserializer(reinterpret_cast<v8::Isolate*>(isolate),
                                       data, backing_store->byte_length());
  if (!deserializer.Deserialize(injected_references)) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  Handle<Object> object;
  if (!deserializer.value().ToHandle(&object)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  return *object;
}

}  // namespace internal
}  // namespace v8
