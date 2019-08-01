// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/heap/heap-inl.h"  // For ToBoolean.
#include "src/logging/counters.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

#define CHECK_CALLSITE(recv, method)                                          \
  CHECK_RECEIVER(JSObject, recv, method);                                     \
  if (!JSReceiver::HasOwnProperty(                                            \
           recv, isolate->factory()->call_site_frame_array_symbol())          \
           .FromMaybe(false)) {                                               \
    THROW_NEW_ERROR_RETURN_FAILURE(                                           \
        isolate,                                                              \
        NewTypeError(MessageTemplate::kCallSiteMethod,                        \
                     isolate->factory()->NewStringFromAsciiChecked(method))); \
  }

namespace {

Object PositiveNumberOrNull(int value, Isolate* isolate) {
  if (value >= 0) return *isolate->factory()->NewNumberFromInt(value);
  return ReadOnlyRoots(isolate).null_value();
}

Handle<FrameArray> GetFrameArray(Isolate* isolate, Handle<JSObject> object) {
  Handle<Object> frame_array_obj = JSObject::GetDataProperty(
      object, isolate->factory()->call_site_frame_array_symbol());
  return Handle<FrameArray>::cast(frame_array_obj);
}

int GetFrameIndex(Isolate* isolate, Handle<JSObject> object) {
  Handle<Object> frame_index_obj = JSObject::GetDataProperty(
      object, isolate->factory()->call_site_frame_index_symbol());
  return Smi::ToInt(*frame_index_obj);
}

}  // namespace

BUILTIN(CallSitePrototypeGetColumnNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getColumnNumber");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return PositiveNumberOrNull(it.Frame()->GetColumnNumber(), isolate);
}

BUILTIN(CallSitePrototypeGetEvalOrigin) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getEvalOrigin");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return *it.Frame()->GetEvalOrigin();
}

BUILTIN(CallSitePrototypeGetFileName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getFileName");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return *it.Frame()->GetFileName();
}

BUILTIN(CallSitePrototypeGetFunction) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getFunction");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));

  StackFrameBase* frame = it.Frame();
  if (frame->IsStrict()) return ReadOnlyRoots(isolate).undefined_value();
  return *frame->GetFunction();
}

BUILTIN(CallSitePrototypeGetFunctionName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getFunctionName");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return *it.Frame()->GetFunctionName();
}

BUILTIN(CallSitePrototypeGetLineNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getLineNumber");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return PositiveNumberOrNull(it.Frame()->GetLineNumber(), isolate);
}

BUILTIN(CallSitePrototypeGetMethodName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getMethodName");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return *it.Frame()->GetMethodName();
}

BUILTIN(CallSitePrototypeGetPosition) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getPosition");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return Smi::FromInt(it.Frame()->GetPosition());
}

BUILTIN(CallSitePrototypeGetPromiseIndex) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getPromiseIndex");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return PositiveNumberOrNull(it.Frame()->GetPromiseIndex(), isolate);
}

BUILTIN(CallSitePrototypeGetScriptNameOrSourceURL) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getScriptNameOrSourceUrl");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return *it.Frame()->GetScriptNameOrSourceUrl();
}

BUILTIN(CallSitePrototypeGetThis) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getThis");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));

  StackFrameBase* frame = it.Frame();
  if (frame->IsStrict()) return ReadOnlyRoots(isolate).undefined_value();
  return *frame->GetReceiver();
}

BUILTIN(CallSitePrototypeGetTypeName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "getTypeName");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return *it.Frame()->GetTypeName();
}

BUILTIN(CallSitePrototypeIsAsync) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "isAsync");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return isolate->heap()->ToBoolean(it.Frame()->IsAsync());
}

BUILTIN(CallSitePrototypeIsConstructor) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "isConstructor");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return isolate->heap()->ToBoolean(it.Frame()->IsConstructor());
}

BUILTIN(CallSitePrototypeIsEval) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "isEval");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return isolate->heap()->ToBoolean(it.Frame()->IsEval());
}

BUILTIN(CallSitePrototypeIsNative) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "isNative");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return isolate->heap()->ToBoolean(it.Frame()->IsNative());
}

BUILTIN(CallSitePrototypeIsPromiseAll) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "isPromiseAll");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return isolate->heap()->ToBoolean(it.Frame()->IsPromiseAll());
}

BUILTIN(CallSitePrototypeIsToplevel) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "isToplevel");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  return isolate->heap()->ToBoolean(it.Frame()->IsToplevel());
}

BUILTIN(CallSitePrototypeToString) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(recv, "toString");
  FrameArrayIterator it(isolate, GetFrameArray(isolate, recv),
                        GetFrameIndex(isolate, recv));
  RETURN_RESULT_OR_FAILURE(isolate, it.Frame()->ToString());
}

#undef CHECK_CALLSITE

}  // namespace internal
}  // namespace v8
