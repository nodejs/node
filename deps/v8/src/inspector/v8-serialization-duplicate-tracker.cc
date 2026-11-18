// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-serialization-duplicate-tracker.h"

#include "include/v8-context.h"
#include "include/v8-isolate.h"
#include "include/v8-primitive.h"
#include "src/base/logging.h"

namespace v8_inspector {

std::unique_ptr<protocol::DictionaryValue>
V8SerializationDuplicateTracker::LinkExistingOrCreate(
    v8::Local<v8::Value> v8Value, bool* isKnown) {
  // Look up whether this V8 value was already serialized.
  protocol::DictionaryValue* maybeKnownSerializedValue = nullptr;
  // Bail out if the V8 Map lookup failed (e.g. on stack overflow).
  if (!FindKnownSerializedValue(v8Value, &maybeKnownSerializedValue)) {
    return nullptr;
  }

  std::unique_ptr<protocol::DictionaryValue> result =
      protocol::DictionaryValue::create();

  if (maybeKnownSerializedValue == nullptr) {
    *isKnown = false;
    // Keep reference to the serialized value, so that
    // `weakLocalObjectReference` can be set later.
    if (!SetKnownSerializedValue(v8Value, result.get())) {
      // Bail out if storing into the V8 Map failed (e.g. on stack overflow).
      return nullptr;
    }
  } else {
    *isKnown = true;

    String16 type;
    maybeKnownSerializedValue->getString("type", &type);
    result->setString("type", type);

    int weakLocalObjectReference;
    // If `maybeKnownSerializedValue` has no `weakLocalObjectReference` yet,
    // it's need to be set.
    if (!maybeKnownSerializedValue->getInteger("weakLocalObjectReference",
                                               &weakLocalObjectReference)) {
      weakLocalObjectReference = m_counter++;
      maybeKnownSerializedValue->setInteger("weakLocalObjectReference",
                                            weakLocalObjectReference);
    }
    result->setInteger("weakLocalObjectReference", weakLocalObjectReference);
  }

  return result;
}

bool V8SerializationDuplicateTracker::SetKnownSerializedValue(
    v8::Local<v8::Value> v8Value, protocol::DictionaryValue* serializedValue) {
  // Don't store the raw `serializedValue` pointer in the (in-sandbox) map,
  // but only its index into `m_serializedValues`.
  int32_t index = static_cast<int32_t>(m_serializedValues.size());
  if (m_v8ObjectToSerializedDictionary
          ->Set(m_context, v8Value,
                v8::Integer::New(v8::Isolate::GetCurrent(), index))
          .IsEmpty()) {
    // Return false when Map::Set fails instead of aborting via ToLocalChecked.
    return false;
  }
  // Record the serialized dictionary on success.
  m_serializedValues.push_back(serializedValue);
  return true;
}

bool V8SerializationDuplicateTracker::FindKnownSerializedValue(
    v8::Local<v8::Value> v8Value, protocol::DictionaryValue** result) {
  v8::Local<v8::Value> knownValue;
  // Abort lookup if Map::Get fails due to an exception or stack overflow.
  if (!m_v8ObjectToSerializedDictionary->Get(m_context, v8Value)
           .ToLocal(&knownValue)) {
    return false;
  }
  // Treat a non-Int32 entry (e.g. undefined when absent) as not found.
  if (!knownValue->IsInt32()) {
    *result = nullptr;
    return true;
  }

  // The map lives in the sandbox and might have been corrupted, so the index
  // must be treated as untrusted and bounds-checked.
  int32_t index = knownValue.As<v8::Int32>()->Value();
  if (index < 0 || static_cast<size_t>(index) >= m_serializedValues.size()) {
    *result = nullptr;
    return true;
  }
  *result = m_serializedValues[index];
  return true;
}

V8SerializationDuplicateTracker::V8SerializationDuplicateTracker(
    v8::Local<v8::Context> context)
    : m_context(context),
      m_counter(1),
      m_v8ObjectToSerializedDictionary(
          v8::Map::New(v8::Isolate::GetCurrent())) {}
}  // namespace v8_inspector
