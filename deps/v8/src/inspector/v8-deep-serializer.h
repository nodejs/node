// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_DEEP_SERIALIZER_H_
#define V8_INSPECTOR_V8_DEEP_SERIALIZER_H_

#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/v8-serialization-duplicate-tracker.h"

namespace v8_inspector {

class V8DeepSerializer {
 public:
  static protocol::Response serializeV8Value(
      v8::Local<v8::Object> value, v8::Local<v8::Context> context, int maxDepth,
      v8::Local<v8::Object> additionalParameters,
      V8SerializationDuplicateTracker& duplicateTracker,
      protocol::DictionaryValue& result);

  V8_EXPORT explicit V8DeepSerializer(v8::Isolate* isolate);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_DEEP_SERIALIZER_H_
