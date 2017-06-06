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
  v8::Isolate* isolate = m_inspector->isolate();
  info.context->SetEmbedderData(static_cast<int>(v8::Context::kDebugIdIndex),
                                v8::Int32::New(isolate, contextId));
  v8::Local<v8::Object> global = info.context->Global();
  v8::Local<v8::Object> console =
      m_inspector->console()->createConsole(info.context);
  if (info.hasMemoryOnConsole) {
    m_inspector->console()->installMemoryGetter(info.context, console);
  }
  if (!global
           ->Set(info.context, toV8StringInternalized(isolate, "console"),
                 console)
           .FromMaybe(false)) {
    return;
  }
}

InspectedContext::~InspectedContext() {
}

// static
int InspectedContext::contextId(v8::Local<v8::Context> context) {
  v8::Local<v8::Value> data =
      context->GetEmbedderData(static_cast<int>(v8::Context::kDebugIdIndex));
  if (data.IsEmpty() || !data->IsInt32()) return 0;
  return static_cast<int>(data.As<v8::Int32>()->Value());
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
