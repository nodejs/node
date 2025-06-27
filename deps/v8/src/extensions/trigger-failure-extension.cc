// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/extensions/trigger-failure-extension.h"

#include "include/v8-template.h"
#include "src/base/logging.h"
#include "src/common/checks.h"

namespace v8 {
namespace internal {


const char* const TriggerFailureExtension::kSource =
    "native function triggerCheckFalse();"
    "native function triggerAssertFalse();"
    "native function triggerSlowAssertFalse();";


v8::Local<v8::FunctionTemplate>
TriggerFailureExtension::GetNativeFunctionTemplate(v8::Isolate* isolate,
                                                   v8::Local<v8::String> str) {
  if (strcmp(*v8::String::Utf8Value(isolate, str), "triggerCheckFalse") == 0) {
    return v8::FunctionTemplate::New(
        isolate,
        TriggerFailureExtension::TriggerCheckFalse);
  } else if (strcmp(*v8::String::Utf8Value(isolate, str),
                    "triggerAssertFalse") == 0) {
    return v8::FunctionTemplate::New(
        isolate,
        TriggerFailureExtension::TriggerAssertFalse);
  } else {
    CHECK_EQ(0, strcmp(*v8::String::Utf8Value(isolate, str),
                       "triggerSlowAssertFalse"));
    return v8::FunctionTemplate::New(
        isolate,
        TriggerFailureExtension::TriggerSlowAssertFalse);
  }
}

void TriggerFailureExtension::TriggerCheckFalse(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(false);
}

void TriggerFailureExtension::TriggerAssertFalse(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(false);
}

void TriggerFailureExtension::TriggerSlowAssertFalse(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  SLOW_DCHECK(false);
}

}  // namespace internal
}  // namespace v8
