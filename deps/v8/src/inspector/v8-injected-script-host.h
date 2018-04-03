// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8INJECTEDSCRIPTHOST_H_
#define V8_INSPECTOR_V8INJECTEDSCRIPTHOST_H_

#include "include/v8.h"

namespace v8_inspector {

class V8InspectorImpl;

// SECURITY NOTE: Although the InjectedScriptHost is intended for use solely by
// the inspector,
// a reference to the InjectedScriptHost may be leaked to the page being
// inspected. Thus, the
// InjectedScriptHost must never implemment methods that have more power over
// the page than the
// page already has itself (e.g. origin restriction bypasses).

class V8InjectedScriptHost {
 public:
  // We expect that debugger outlives any JS context and thus
  // V8InjectedScriptHost (owned by JS)
  // is destroyed before inspector.
  static v8::Local<v8::Object> create(v8::Local<v8::Context>, V8InspectorImpl*);

 private:
  static void nullifyPrototypeCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void getPropertyCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void internalConstructorNameCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void formatAccessorsAsProperties(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void subtypeCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void getInternalPropertiesCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void objectHasOwnPropertyCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void bindCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void proxyTargetValueCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void nativeAccessorDescriptorCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void typedArrayPropertiesCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8INJECTEDSCRIPTHOST_H_
