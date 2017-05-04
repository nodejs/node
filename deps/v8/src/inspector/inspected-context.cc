// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/inspected-context.h"

#include "src/debug/debug-interface.h"
#include "src/inspector/injected-script.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-console.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-value-copier.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

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
  v8::debug::SetContextId(info.context, contextId);
  if (!info.hasMemoryOnConsole) return;
  v8::Context::Scope contextScope(info.context);
  v8::Local<v8::Object> global = info.context->Global();
  v8::Local<v8::Value> console;
  if (global->Get(info.context, toV8String(m_inspector->isolate(), "console"))
          .ToLocal(&console) &&
      console->IsObject()) {
    m_inspector->console()->installMemoryGetter(
        info.context, v8::Local<v8::Object>::Cast(console));
  }
}

InspectedContext::~InspectedContext() {
}

// static
int InspectedContext::contextId(v8::Local<v8::Context> context) {
  return v8::debug::GetContextId(context);
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
