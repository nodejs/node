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

namespace {

void clearContext(const v8::WeakCallbackInfo<v8::Global<v8::Context>>& data) {
  // Inspected context is created in V8InspectorImpl::contextCreated method
  // and destroyed in V8InspectorImpl::contextDestroyed.
  // Both methods takes valid v8::Local<v8::Context> handle to the same context,
  // it means that context is created before InspectedContext constructor and is
  // always destroyed after InspectedContext destructor therefore this callback
  // should be never called.
  // It's possible only if inspector client doesn't call contextDestroyed which
  // is considered an error.
  CHECK(false);
  data.GetParameter()->Reset();
}

}  // namespace

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
  m_context.SetWeak(&m_context, &clearContext,
                    v8::WeakCallbackType::kParameter);

  v8::Local<v8::Object> global = info.context->Global();
  v8::Local<v8::Object> console =
      V8Console::createConsole(this, info.hasMemoryOnConsole);
  if (!global
           ->Set(info.context, toV8StringInternalized(isolate, "console"),
                 console)
           .FromMaybe(false))
    return;
  m_console.Reset(isolate, console);
  m_console.SetWeak();
}

InspectedContext::~InspectedContext() {
  if (!m_console.IsEmpty()) {
    v8::HandleScope scope(isolate());
    V8Console::clearInspectedContextIfNeeded(context(),
                                             m_console.Get(isolate()));
  }
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
