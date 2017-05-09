/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "src/inspector/java-script-call-frame.h"

#include "src/debug/debug-interface.h"
#include "src/inspector/string-util.h"

namespace v8_inspector {

JavaScriptCallFrame::JavaScriptCallFrame(v8::Local<v8::Context> debuggerContext,
                                         v8::Local<v8::Object> callFrame)
    : m_isolate(debuggerContext->GetIsolate()),
      m_debuggerContext(m_isolate, debuggerContext),
      m_callFrame(m_isolate, callFrame) {}

JavaScriptCallFrame::~JavaScriptCallFrame() {}

int JavaScriptCallFrame::callV8FunctionReturnInt(const char* name) const {
  v8::HandleScope handleScope(m_isolate);
  v8::MicrotasksScope microtasks(m_isolate,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(m_isolate, m_debuggerContext);
  v8::Local<v8::Object> callFrame =
      v8::Local<v8::Object>::New(m_isolate, m_callFrame);
  v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(
      callFrame->Get(context, toV8StringInternalized(m_isolate, name))
          .ToLocalChecked());
  v8::Local<v8::Value> result;
  if (!func->Call(context, callFrame, 0, nullptr).ToLocal(&result) ||
      !result->IsInt32())
    return 0;
  return result.As<v8::Int32>()->Value();
}

int JavaScriptCallFrame::contextId() const {
  return callV8FunctionReturnInt("contextId");
}

bool JavaScriptCallFrame::isAtReturn() const {
  v8::HandleScope handleScope(m_isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(m_isolate, m_debuggerContext);
  v8::Local<v8::Object> callFrame =
      v8::Local<v8::Object>::New(m_isolate, m_callFrame);
  v8::Local<v8::Value> result;
  if (!callFrame->Get(context, toV8StringInternalized(m_isolate, "isAtReturn"))
           .ToLocal(&result) ||
      !result->IsBoolean())
    return false;
  return result.As<v8::Boolean>()->BooleanValue(context).FromMaybe(false);
}

v8::MaybeLocal<v8::Object> JavaScriptCallFrame::details() const {
  v8::MicrotasksScope microtasks(m_isolate,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(m_isolate, m_debuggerContext);
  v8::Local<v8::Object> callFrame =
      v8::Local<v8::Object>::New(m_isolate, m_callFrame);
  v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(
      callFrame->Get(context, toV8StringInternalized(m_isolate, "details"))
          .ToLocalChecked());
  v8::TryCatch try_catch(m_isolate);
  v8::Local<v8::Value> details;
  if (func->Call(context, callFrame, 0, nullptr).ToLocal(&details)) {
    return v8::Local<v8::Object>::Cast(details);
  }
  return v8::MaybeLocal<v8::Object>();
}

v8::MaybeLocal<v8::Value> JavaScriptCallFrame::evaluate(
    v8::Local<v8::Value> expression, bool throwOnSideEffect) {
  v8::MicrotasksScope microtasks(m_isolate,
                                 v8::MicrotasksScope::kRunMicrotasks);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(m_isolate, m_debuggerContext);
  v8::Local<v8::Object> callFrame =
      v8::Local<v8::Object>::New(m_isolate, m_callFrame);
  v8::Local<v8::Function> evalFunction = v8::Local<v8::Function>::Cast(
      callFrame->Get(context, toV8StringInternalized(m_isolate, "evaluate"))
          .ToLocalChecked());
  v8::Local<v8::Value> argv[] = {
      expression, v8::Boolean::New(m_isolate, throwOnSideEffect)};
  return evalFunction->Call(context, callFrame, arraysize(argv), argv);
}

v8::MaybeLocal<v8::Value> JavaScriptCallFrame::restart() {
  v8::MicrotasksScope microtasks(m_isolate,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(m_isolate, m_debuggerContext);
  v8::Local<v8::Object> callFrame =
      v8::Local<v8::Object>::New(m_isolate, m_callFrame);
  v8::Local<v8::Function> restartFunction = v8::Local<v8::Function>::Cast(
      callFrame->Get(context, toV8StringInternalized(m_isolate, "restart"))
          .ToLocalChecked());
  v8::TryCatch try_catch(m_isolate);
  v8::debug::SetLiveEditEnabled(m_isolate, true);
  v8::MaybeLocal<v8::Value> result = restartFunction->Call(
      m_debuggerContext.Get(m_isolate), callFrame, 0, nullptr);
  v8::debug::SetLiveEditEnabled(m_isolate, false);
  return result;
}

v8::MaybeLocal<v8::Value> JavaScriptCallFrame::setVariableValue(
    int scopeNumber, v8::Local<v8::Value> variableName,
    v8::Local<v8::Value> newValue) {
  v8::MicrotasksScope microtasks(m_isolate,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(m_isolate, m_debuggerContext);
  v8::Local<v8::Object> callFrame =
      v8::Local<v8::Object>::New(m_isolate, m_callFrame);
  v8::Local<v8::Function> setVariableValueFunction =
      v8::Local<v8::Function>::Cast(
          callFrame
              ->Get(context,
                    toV8StringInternalized(m_isolate, "setVariableValue"))
              .ToLocalChecked());
  v8::Local<v8::Value> argv[] = {
      v8::Local<v8::Value>(v8::Integer::New(m_isolate, scopeNumber)),
      variableName, newValue};
  v8::TryCatch try_catch(m_isolate);
  return setVariableValueFunction->Call(context, callFrame, arraysize(argv),
                                        argv);
}

}  // namespace v8_inspector
