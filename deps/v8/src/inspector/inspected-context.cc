// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/inspected-context.h"

#include "src/inspector/injected-script.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-console.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-value-copier.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

void InspectedContext::weakCallback(
    const v8::WeakCallbackInfo<InspectedContext>& data) {
  InspectedContext* context = data.GetParameter();
  if (!context->m_context.IsEmpty()) {
    context->m_context.Reset();
    data.SetSecondPassCallback(&InspectedContext::weakCallback);
  } else {
    context->m_inspector->discardInspectedContext(context->m_contextGroupId,
                                                  context->m_contextId);
  }
}

void InspectedContext::consoleWeakCallback(
    const v8::WeakCallbackInfo<InspectedContext>& data) {
  data.GetParameter()->m_console.Reset();
}

InspectedContext::InspectedContext(V8InspectorImpl* inspector,
                                   const V8ContextInfo& info, int contextId)
    : m_inspector(inspector),
      m_context(info.context->GetIsolate(), info.context),
      m_contextId(contextId),
      m_contextGroupId(info.contextGroupId),
      m_origin(toString16(info.origin)),
      m_humanReadableName(toString16(info.humanReadableName)),
      m_auxData(toString16(info.auxData)),
      m_reported(false) {
  m_context.SetWeak(this, &InspectedContext::weakCallback,
                    v8::WeakCallbackType::kParameter);

  v8::Isolate* isolate = m_inspector->isolate();
  v8::Local<v8::Object> global = info.context->Global();
  v8::Local<v8::Object> console =
      V8Console::createConsole(this, info.hasMemoryOnConsole);
  if (!global
           ->Set(info.context, toV8StringInternalized(isolate, "console"),
                 console)
           .FromMaybe(false))
    return;
  m_console.Reset(isolate, console);
  m_console.SetWeak(this, &InspectedContext::consoleWeakCallback,
                    v8::WeakCallbackType::kParameter);
}

InspectedContext::~InspectedContext() {
  if (!m_context.IsEmpty() && !m_console.IsEmpty()) {
    v8::HandleScope scope(isolate());
    V8Console::clearInspectedContextIfNeeded(context(),
                                             m_console.Get(isolate()));
  }
}

v8::Local<v8::Context> InspectedContext::context() const {
  return m_context.Get(isolate());
}

v8::Isolate* InspectedContext::isolate() const {
  return m_inspector->isolate();
}

bool InspectedContext::createInjectedScript() {
  DCHECK(!m_injectedScript);
  std::unique_ptr<InjectedScript> injectedScript = InjectedScript::create(this);
  // InjectedScript::create can destroy |this|.
  if (!injectedScript) return false;
  m_injectedScript = std::move(injectedScript);
  return true;
}

void InspectedContext::discardInjectedScript() { m_injectedScript.reset(); }

}  // namespace v8_inspector
