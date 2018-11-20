/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "src/inspector/v8-function-call.h"

#include "src/inspector/inspected-context.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-debugger.h"
#include "src/inspector/v8-inspector-impl.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

V8FunctionCall::V8FunctionCall(V8InspectorImpl* inspector,
                               v8::Local<v8::Context> context,
                               v8::Local<v8::Value> value, const String16& name)
    : m_inspector(inspector),
      m_context(context),
      m_name(toV8String(context->GetIsolate(), name)),
      m_value(value) {}

void V8FunctionCall::appendArgument(v8::Local<v8::Value> value) {
  m_arguments.push_back(value);
}

void V8FunctionCall::appendArgument(const String16& argument) {
  m_arguments.push_back(toV8String(m_context->GetIsolate(), argument));
}

void V8FunctionCall::appendArgument(int argument) {
  m_arguments.push_back(v8::Number::New(m_context->GetIsolate(), argument));
}

void V8FunctionCall::appendArgument(bool argument) {
  m_arguments.push_back(argument ? v8::True(m_context->GetIsolate())
                                 : v8::False(m_context->GetIsolate()));
}

v8::Local<v8::Value> V8FunctionCall::call(bool& hadException,
                                          bool reportExceptions) {
  v8::TryCatch tryCatch(m_context->GetIsolate());
  tryCatch.SetVerbose(reportExceptions);

  v8::Local<v8::Value> result = callWithoutExceptionHandling();
  hadException = tryCatch.HasCaught();
  return result;
}

v8::Local<v8::Value> V8FunctionCall::callWithoutExceptionHandling() {
  v8::Context::Scope contextScope(m_context);

  v8::Local<v8::Object> thisObject = v8::Local<v8::Object>::Cast(m_value);
  v8::Local<v8::Value> value;
  if (!thisObject->Get(m_context, m_name).ToLocal(&value))
    return v8::Local<v8::Value>();

  DCHECK(value->IsFunction());

  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(value);
  std::unique_ptr<v8::Local<v8::Value>[]> info(
      new v8::Local<v8::Value>[m_arguments.size()]);
  for (size_t i = 0; i < m_arguments.size(); ++i) {
    info[i] = m_arguments[i];
    DCHECK(!info[i].IsEmpty());
  }

  int contextGroupId = m_inspector->contextGroupId(m_context);
  if (contextGroupId) {
    m_inspector->client()->muteMetrics(contextGroupId);
    m_inspector->muteExceptions(contextGroupId);
  }
  v8::MicrotasksScope microtasksScope(m_context->GetIsolate(),
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Isolate::AllowJavascriptExecutionScope(m_context->GetIsolate());
  v8::MaybeLocal<v8::Value> maybeResult = function->Call(
      m_context, thisObject, static_cast<int>(m_arguments.size()), info.get());
  if (contextGroupId) {
    m_inspector->client()->unmuteMetrics(contextGroupId);
    m_inspector->unmuteExceptions(contextGroupId);
  }

  v8::Local<v8::Value> result;
  if (!maybeResult.ToLocal(&result)) return v8::Local<v8::Value>();
  return result;
}

}  // namespace v8_inspector
