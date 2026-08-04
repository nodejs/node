// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_SERIALIZATION_DUPLICATE_TRACKER_H_
#define V8_INSPECTOR_V8_SERIALIZATION_DUPLICATE_TRACKER_H_

#include "include/v8-container.h"
#include "src/base/vector.h"
#include "src/debug/debug-interface.h"
#include "src/inspector/protocol/Runtime.h"

namespace v8_inspector {

class V8SerializationDuplicateTracker {
 public:
  // Returns a `protocol::DictionaryValue` value either empty if the V8 value
  // was not serialized yet, or filled in as a reference to previousely
  // serialized protocol value.
  V8_EXPORT std::unique_ptr<protocol::DictionaryValue> LinkExistingOrCreate(
      v8::Local<v8::Value> v8Value, bool* isKnown);

  V8_EXPORT explicit V8SerializationDuplicateTracker(
      v8::Local<v8::Context> context);

 private:
  v8::Local<v8::Context> m_context;
  int m_counter;
  // Maps v8 value to corresponding serialized value.
  v8::Local<v8::Map> m_v8ObjectToSerializedDictionary;

  V8_EXPORT protocol::DictionaryValue* FindKnownSerializedValue(
      v8::Local<v8::Value> v8Value);

  V8_EXPORT void SetKnownSerializedValue(
      v8::Local<v8::Value> v8Value, protocol::DictionaryValue* serializedValue);
};
}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_SERIALIZATION_DUPLICATE_TRACKER_H_
