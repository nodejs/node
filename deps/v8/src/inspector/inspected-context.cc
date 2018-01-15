// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/inspected-context.h"

#include "src/debug/debug-interface.h"
#include "src/inspector/injected-script.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-console.h"
#include "src/inspector/v8-inspector-impl.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

class InspectedContext::WeakCallbackData {
 public:
  WeakCallbackData(InspectedContext* context, V8InspectorImpl* inspector,
                   int groupId, int contextId)
      : m_context(context),
        m_inspector(inspector),
        m_groupId(groupId),
        m_contextId(contextId) {}

  static void resetContext(const v8::WeakCallbackInfo<WeakCallbackData>& data) {
    // InspectedContext is alive here because weak handler is still alive.
    data.GetParameter()->m_context->m_weakCallbackData = nullptr;
    data.GetParameter()->m_context->m_context.Reset();
    data.SetSecondPassCallback(&callContextCollected);
  }

  static void callContextCollected(
      const v8::WeakCallbackInfo<WeakCallbackData>& data) {
    // InspectedContext can be dead here since anything can happen between first
    // and second pass callback.
    WeakCallbackData* callbackData = data.GetParameter();
    callbackData->m_inspector->contextCollected(callbackData->m_groupId,
                                                callbackData->m_contextId);
    delete callbackData;
  }

 private:
  InspectedContext* m_context;
  V8InspectorImpl* m_inspector;
  int m_groupId;
  int m_contextId;
};

InspectedContext::InspectedContext(V8InspectorImpl* inspector,
                                   const V8ContextInfo& info, int contextId)
    : m_inspector(inspector),
      m_context(info.context->GetIsolate(), info.context),
      m_contextId(contextId),
      m_contextGroupId(info.contextGroupId),
      m_origin(toString16(info.origin)),
      m_humanReadableName(toString16(info.humanReadableName)),
      m_auxData(toString16(info.auxData)) {
  v8::debug::SetContextId(info.context, contextId);
  m_weakCallbackData =
      new WeakCallbackData(this, m_inspector, m_contextGroupId, m_contextId);
  m_context.SetWeak(m_weakCallbackData,
                    &InspectedContext::WeakCallbackData::resetContext,
                    v8::WeakCallbackType::kParameter);
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
  // If we destory InspectedContext before weak callback is invoked then we need
  // to delete data here.
  if (!m_context.IsEmpty()) delete m_weakCallbackData;
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

bool InspectedContext::isReported(int sessionId) const {
  return m_reportedSessionIds.find(sessionId) != m_reportedSessionIds.cend();
}

void InspectedContext::setReported(int sessionId, bool reported) {
  if (reported)
    m_reportedSessionIds.insert(sessionId);
  else
    m_reportedSessionIds.erase(sessionId);
}

InjectedScript* InspectedContext::getInjectedScript(int sessionId) {
  auto it = m_injectedScripts.find(sessionId);
  return it == m_injectedScripts.end() ? nullptr : it->second.get();
}

bool InspectedContext::createInjectedScript(int sessionId) {
  DCHECK(m_injectedScripts.find(sessionId) == m_injectedScripts.end());
  std::unique_ptr<InjectedScript> injectedScript =
      InjectedScript::create(this, sessionId);
  // InjectedScript::create can destroy |this|.
  if (!injectedScript) return false;
  m_injectedScripts[sessionId] = std::move(injectedScript);
  return true;
}

void InspectedContext::discardInjectedScript(int sessionId) {
  m_injectedScripts.erase(sessionId);
}

}  // namespace v8_inspector
