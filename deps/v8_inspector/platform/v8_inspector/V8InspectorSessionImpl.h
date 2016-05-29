// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorSessionImpl_h
#define V8InspectorSessionImpl_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/TypeBuilder.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"
#include "platform/v8_inspector/public/V8InspectorSessionClient.h"
#include "platform/v8_inspector/public/V8RuntimeAgent.h"
#include "wtf/PtrUtil.h"

#include <v8.h>

namespace blink {

class InjectedScript;
class RemoteObjectIdBase;
class V8DebuggerAgentImpl;
class V8DebuggerImpl;
class V8HeapProfilerAgentImpl;
class V8ProfilerAgentImpl;
class V8RuntimeAgentImpl;

class V8InspectorSessionImpl : public V8InspectorSession {
    PROTOCOL_DISALLOW_COPY(V8InspectorSessionImpl);
public:
    static std::unique_ptr<V8InspectorSessionImpl> create(V8DebuggerImpl*, int contextGroupId);
    ~V8InspectorSessionImpl();

    V8DebuggerImpl* debugger() const { return m_debugger; }
    V8InspectorSessionClient* client() const { return m_client; }
    V8DebuggerAgentImpl* debuggerAgentImpl() { return m_debuggerAgent.get(); }
    V8ProfilerAgentImpl* profilerAgentImpl() { return m_profilerAgent.get(); }
    V8RuntimeAgentImpl* runtimeAgentImpl() { return m_runtimeAgent.get(); }
    int contextGroupId() const { return m_contextGroupId; }

    InjectedScript* findInjectedScript(ErrorString*, int contextId);
    InjectedScript* findInjectedScript(ErrorString*, RemoteObjectIdBase*);
    void reset();
    void discardInjectedScripts();
    void reportAllContexts(V8RuntimeAgentImpl*);
    void setCustomObjectFormatterEnabled(bool);
    void changeInstrumentationCounter(int delta);

    // V8InspectorSession implementation.
    void setClient(V8InspectorSessionClient*) override;
    void addInspectedObject(std::unique_ptr<V8InspectorSession::Inspectable>) override;
    V8DebuggerAgent* debuggerAgent() override;
    V8HeapProfilerAgent* heapProfilerAgent() override;
    V8ProfilerAgent* profilerAgent() override;
    V8RuntimeAgent* runtimeAgent() override;
    void schedulePauseOnNextStatement(const String16& breakReason, std::unique_ptr<protocol::DictionaryValue> data) override;
    void cancelPauseOnNextStatement() override;
    void breakProgram(const String16& breakReason, std::unique_ptr<protocol::DictionaryValue> data) override;
    void breakProgramOnException(const String16& breakReason, std::unique_ptr<protocol::DictionaryValue> data) override;
    void setSkipAllPauses(bool) override;
    void asyncTaskScheduled(const String16& taskName, void* task, bool recurring) override;
    void asyncTaskCanceled(void* task) override;
    void asyncTaskStarted(void* task) override;
    void asyncTaskFinished(void* task) override;
    void allAsyncTasksCanceled() override;
    void releaseObjectGroup(const String16& objectGroup) override;
    v8::Local<v8::Value> findObject(ErrorString*, const String16& objectId, v8::Local<v8::Context>* = nullptr, String16* groupName = nullptr) override;
    std::unique_ptr<protocol::Runtime::RemoteObject> wrapObject(v8::Local<v8::Context>, v8::Local<v8::Value>, const String16& groupName, bool generatePreview = false) override;
    std::unique_ptr<protocol::Runtime::RemoteObject> wrapTable(v8::Local<v8::Context>, v8::Local<v8::Value> table, v8::Local<v8::Value> columns) override;

    V8InspectorSession::Inspectable* inspectedObject(unsigned num);
    static const unsigned kInspectedObjectBufferSize = 5;

private:
    V8InspectorSessionImpl(V8DebuggerImpl*, int contextGroupId);

    int m_contextGroupId;
    V8DebuggerImpl* m_debugger;
    V8InspectorSessionClient* m_client;
    bool m_customObjectFormatterEnabled;
    int m_instrumentationCounter;

    std::unique_ptr<V8RuntimeAgentImpl> m_runtimeAgent;
    std::unique_ptr<V8DebuggerAgentImpl> m_debuggerAgent;
    std::unique_ptr<V8HeapProfilerAgentImpl> m_heapProfilerAgent;
    std::unique_ptr<V8ProfilerAgentImpl> m_profilerAgent;
    protocol::Vector<std::unique_ptr<V8InspectorSession::Inspectable>> m_inspectedObjects;
};

} // namespace blink

#endif
