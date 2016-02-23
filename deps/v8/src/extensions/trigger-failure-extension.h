// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTENSIONS_TRIGGER_FAILURE_EXTENSION_H_
#define V8_EXTENSIONS_TRIGGER_FAILURE_EXTENSION_H_

#include "include/v8.h"

namespace v8 {
namespace internal {

class TriggerFailureExtension : public v8::Extension {
 public:
  TriggerFailureExtension() : v8::Extension("v8/trigger-failure", kSource) {}
  virtual v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name);
  static void TriggerCheckFalse(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void TriggerAssertFalse(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void TriggerSlowAssertFalse(
      const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  static const char* const kSource;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXTENSIONS_TRIGGER_FAILURE_EXTENSION_H_
