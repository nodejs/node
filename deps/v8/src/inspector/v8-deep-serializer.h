// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_DEEP_SERIALIZER_H_
#define V8_INSPECTOR_V8_DEEP_SERIALIZER_H_

#include "src/inspector/protocol/Runtime.h"

namespace v8_inspector {

class V8DeepSerializer {
 public:
  // Serializes `value` as the root of a deep serialization.
  static protocol::Response serializeV8Value(
      v8::Local<v8::Value> value, v8::Local<v8::Context> context, int maxDepth,
      v8::Local<v8::Object> additionalParameters,
      std::unique_ptr<protocol::Runtime::DeepSerializedValue>* result);

  V8_EXPORT explicit V8DeepSerializer(v8::Isolate* isolate);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_DEEP_SERIALIZER_H_
