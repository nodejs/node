// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_CUSTOM_PREVIEW_H_
#define V8_INSPECTOR_CUSTOM_PREVIEW_H_

#include <cstdint>
#include <memory>

#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/value-mirror.h"

namespace v8_inspector {

const int kMaxCustomPreviewDepth = 20;

// Limits the total size of the JSONML that custom formatters can produce for a
// single preview (including nested ["object", {...}] tags).
struct CustomPreviewBudget {
  // The remaining number of array elements and object properties.
  uint32_t values = 100'000;
  // The remaining number of characters in strings and property names.
  uint32_t characters = kMaxArrayLength;
  bool exhausted = false;
};

void generateCustomPreview(
    v8::Isolate* isolate, int sessionId, const String16& groupName,
    v8::Local<v8::Object> object, v8::MaybeLocal<v8::Value> config,
    int maxDepth, CustomPreviewBudget* budget,
    std::unique_ptr<protocol::Runtime::CustomPreview>* preview);

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_CUSTOM_PREVIEW_H_
