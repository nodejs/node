// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

#include "src/string-builder.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {

#define CHECK_CALLSITE(recv, method)                                          \
  CHECK_RECEIVER(JSObject, recv, method);                                     \
  if (!JSReceiver::HasOwnProperty(                                            \
           recv, isolate->factory()->call_site_position_symbol())             \
           .FromMaybe(false)) {                                               \
    THROW_NEW_ERROR_RETURN_FAILURE(                                           \
        isolate,                                                              \
        NewTypeError(MessageTemplate::kCallSiteMethod,                        \
                     isolate->factory()->NewStringFromAsciiChecked(method))); \
  }

namespace {

Object* PositiveNumberOrNull(int value, Isolate* isolate) {
  if (value >= 0) return *isolate->factory()->NewNumberFromInt(value);
  return isolate->heap()->null_value();
}

}  // namespace

BUILTIN(CallSitePrototypeGetColumnNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getColumnNumber");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());
  return PositiveNumberOrNull(call_site.GetColumnNumber(), isolate);
}

BUILTIN(CallSitePrototypeGetEvalOrigin) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getEvalOrigin");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());
  return *call_site.GetEvalOrigin();
}

BUILTIN(CallSitePrototypeGetFileName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getFileName");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());
  return *call_site.GetFileName();
}

namespace {

bool CallSiteIsStrict(Isolate* isolate, Handle<JSObject> receiver) {
  Handle<Object> strict;
  Handle<Symbol> symbol = isolate->factory()->call_site_strict_symbol();
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, strict,
                                     JSObject::GetProperty(receiver, symbol));
  return strict->BooleanValue();
}

}  // namespace

BUILTIN(CallSitePrototypeGetFunction) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getFunction");

  if (CallSiteIsStrict(isolate, recv))
    return *isolate->factory()->undefined_value();

  Handle<Symbol> symbol = isolate->factory()->call_site_function_symbol();
  RETURN_RESULT_OR_FAILURE(isolate, JSObject::GetProperty(recv, symbol));
}

BUILTIN(CallSitePrototypeGetFunctionName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getFunctionName");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());
  return *call_site.GetFunctionName();
}

BUILTIN(CallSitePrototypeGetLineNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getLineNumber");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());

  int line_number = call_site.IsWasm() ? call_site.wasm_func_index()
                                       : call_site.GetLineNumber();
  return PositiveNumberOrNull(line_number, isolate);
}

BUILTIN(CallSitePrototypeGetMethodName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getMethodName");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());
  return *call_site.GetMethodName();
}

BUILTIN(CallSitePrototypeGetPosition) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getPosition");

  Handle<Symbol> symbol = isolate->factory()->call_site_position_symbol();
  RETURN_RESULT_OR_FAILURE(isolate, JSObject::GetProperty(recv, symbol));
}

BUILTIN(CallSitePrototypeGetScriptNameOrSourceURL) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getScriptNameOrSourceUrl");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());
  return *call_site.GetScriptNameOrSourceUrl();
}

BUILTIN(CallSitePrototypeGetThis) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getThis");

  if (CallSiteIsStrict(isolate, recv))
    return *isolate->factory()->undefined_value();

  Handle<Object> receiver;
  Handle<Symbol> symbol = isolate->factory()->call_site_receiver_symbol();
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver,
                                     JSObject::GetProperty(recv, symbol));

  if (*receiver == isolate->heap()->call_site_constructor_symbol())
    return *isolate->factory()->undefined_value();

  return *receiver;
}

BUILTIN(CallSitePrototypeGetTypeName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getTypeName");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());
  return *call_site.GetTypeName();
}

BUILTIN(CallSitePrototypeIsConstructor) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "isConstructor");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());
  return isolate->heap()->ToBoolean(call_site.IsConstructor());
}

BUILTIN(CallSitePrototypeIsEval) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "isEval");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());
  return isolate->heap()->ToBoolean(call_site.IsEval());
}

BUILTIN(CallSitePrototypeIsNative) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "isNative");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());
  return isolate->heap()->ToBoolean(call_site.IsNative());
}

BUILTIN(CallSitePrototypeIsToplevel) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "isToplevel");

  CallSite call_site(isolate, recv);
  CHECK(call_site.IsJavaScript() || call_site.IsWasm());
  return isolate->heap()->ToBoolean(call_site.IsToplevel());
}

BUILTIN(CallSitePrototypeToString) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "toString");
  RETURN_RESULT_OR_FAILURE(isolate, CallSiteUtils::ToString(isolate, recv));
}

#undef CHECK_CALLSITE

}  // namespace internal
}  // namespace v8
