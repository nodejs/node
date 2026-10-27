// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_SERIALIZATION_DUPLICATE_TRACKER_H_
#define V8_INSPECTOR_V8_SERIALIZATION_DUPLICATE_TRACKER_H_

#include <vector>

#include "include/v8-container.h"
#include "include/v8-primitive.h"
#include "src/base/vector.h"
#include "src/debug/debug-interface.h"
#include "src/inspector/protocol/Runtime.h"

namespace v8_inspector {

struct ProtocolValueBudget {
  // The remaining number of array elements and object properties.
  uint32_t values = 10'000'000;
  // The remaining number of characters in strings and property names. The
  // serialized result has to fit into a single string anyway.
  uint32_t characters = static_cast<uint32_t>(v8::String::kMaxLength);

  bool takeValues(uint32_t count) {
    if (count > values) return false;
    values -= count;
    return true;
  }

  bool takeCharacters(v8::Local<v8::String> string) {
    uint32_t length = static_cast<uint32_t>(string->Length());
    if (length > characters) return false;
    characters -= length;
    return true;
  }
};

class V8SerializationDuplicateTracker {
 public:
  // Returns a `protocol::DictionaryValue` value either empty if the V8 value
  // was not serialized yet, or filled in as a reference to previousely
  // serialized protocol value. Returns nullptr if a V8 exception occurs.
  V8_EXPORT std::unique_ptr<protocol::DictionaryValue> LinkExistingOrCreate(
      v8::Local<v8::Value> v8Value, bool* isKnown);

  V8_EXPORT explicit V8SerializationDuplicateTracker(
      v8::Local<v8::Context> context);

  bool takeCharacters(v8::Local<v8::String> string) {
    return m_budget.takeCharacters(string);
  }
  ProtocolValueBudget* budget() { return &m_budget; }

 private:
  ProtocolValueBudget m_budget;
  v8::Local<v8::Context> m_context;
  int m_counter;
  // Maps v8 value to the index of the corresponding serialized value in
  // `m_serializedValues`. The map lives in sandbox-corruptible memory, so it
  // must never hold raw (out-of-sandbox) pointers. Instead it holds indices
  // that are range-checked on lookup, so that a corrupted or replayed map
  // value can at worst alias another serialized value of the same
  // serialization, but never produce a dangling pointer.
  v8::Local<v8::Map> m_v8ObjectToSerializedDictionary;
  // Serialized values registered during this serialization. The values are
  // owned by the resulting protocol tree, which outlives this tracker.
  std::vector<protocol::DictionaryValue*> m_serializedValues;

  // Looks up a previously serialized value; returns false if Map::Get fails.
  V8_EXPORT bool FindKnownSerializedValue(v8::Local<v8::Value> v8Value,
                                          protocol::DictionaryValue** result);

  // Records a newly serialized value; returns false if Map::Set fails.
  V8_EXPORT bool SetKnownSerializedValue(
      v8::Local<v8::Value> v8Value, protocol::DictionaryValue* serializedValue);
};
}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_SERIALIZATION_DUPLICATE_TRACKER_H_
