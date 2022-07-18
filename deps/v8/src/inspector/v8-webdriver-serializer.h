// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_WEBDRIVER_SERIALIZER_H_
#define V8_INSPECTOR_V8_WEBDRIVER_SERIALIZER_H_

#include "include/v8-container.h"
#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-regexp.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/v8-value-utils.h"

namespace v8_inspector {
class V8WebDriverSerializer {
 public:
  static std::unique_ptr<protocol::Runtime::WebDriverValue> serializeV8Value(
      v8::Local<v8::Object> value, v8::Local<v8::Context> context,
      int max_depth);
};
}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_WEBDRIVER_SERIALIZER_H_
