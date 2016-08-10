// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorSessionImpl_h
#define V8InspectorSessionImpl_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/inspector_protocol/DispatcherBase.h"
#include "platform/inspector_protocol/Platform.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/v8_inspector/protocol/Runtime.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"

#include <v8.h>

#include <vector>

namespace blink {

class InjectedScript;
class RemoteObjectIdBase;
class V8ConsoleAgentImpl;
class V8DebuggerAgentImpl;
class V8InspectorImpl;
class V8HeapProfilerAgentImpl;
class V8ProfilerAgentImpl;
class V8RuntimeAgentImpl;

class V8InspectorSessionImpl : public V8InspectorSession {
    PROTOCOL_DISALLOW_COPY(V8InspectorSessionImpl);
public:
    static std::unique_ptr<V8InspectorSessionImpl> create(V8InspectorImpl*, int contextGroupId, protocol::FrontendChannel*, const String16* state);
    ~V8InspectorSessionImpl();

    V8InspectorImpl* inspector() const { return m_inspector; }
    V8ConsoleAgentImpl* consoleAgent() { return m_consoleAgent.get(); }
    V8DebuggerAgentImpl* debuggerAgent() { return m_debuggerAgent.get(); }
    V8ProfilerAgentImpl* profilerAgent() { return m_profilerAgent.get(); }
    V8RuntimeAgentImpl* runtimeAgent() { return m_runtimeAgent.get(); }
    int contextGroupId() const { return m_contextGroupId; }

    InjectedScript* findInjectedScript(ErrorString*, int contextId);
    InjectedScript* findInjectedScript(ErrorString*, RemoteObjectIdBase*);
    void reset();
    void discardInjectedScripts();
    void reportAllContexts(V8RuntimeAgentImpl*);
    void setCustomObjectFormatterEnabled(bool);
    std::unique_ptr<protocol::Runtime::RemoteObject> wrapObject(v8::Local<v8::Context>, v8::Local<v8::Value>, const String16& groupName, bool generatePreview);
    std::unique_ptr<protocol::Runtime::RemoteObject> wrapTable(v8::Local<v8::Context>, v8::Local<v8::Value> table, v8::Local<v8::Value> columns);

    // V8InspectorSession implementation.
    void dispatchProtocolMessage(const String16& message) override;
    String16 stateJSON() override;
    void addInspectedObject(std::unique_ptr<V8InspectorSession::Inspectable>) override;
    void schedulePauseOnNextStatement(const String16& breakReason, const String16& breakDetails) override;
    void cancelPauseOnNextStatement() override;
    void breakProgram(const String16& breakReason, const String16& breakDetails) override;
    void setSkipAllPauses(bool) override;
    void resume() override;
    void stepOver() override;
    std::unique_ptr<protocol::Array<protocol::Debugger::API::SearchMatch>> searchInTextByLines(const String16& text, const String16& query, bool caseSensitive, bool isRegex) override;
    void releaseObjectGroup(const String16& objectGroup) override;
    bool unwrapObject(ErrorString*, const String16& objectId, v8::Local<v8::Value>*, v8::Local<v8::Context>*, String16* objectGroup) override;
    std::unique_ptr<protocol::Runtime::API::RemoteObject> wrapObject(v8::Local<v8::Context>, v8::Local<v8::Value>, const String16& groupName) override;

    V8InspectorSession::Inspectable* inspectedObject(unsigned num);
    static const unsigned kInspectedObjectBufferSize = 5;

private:
    V8InspectorSessionImpl(V8InspectorImpl*, int contextGroupId, protocol::FrontendChannel*, const String16* state);
    protocol::DictionaryValue* agentState(const String16& name);

    int m_contextGroupId;
    V8InspectorImpl* m_inspector;
    bool m_customObjectFormatterEnabled;

    protocol::UberDispatcher m_dispatcher;
    std::unique_ptr<protocol::DictionaryValue> m_state;

    std::unique_ptr<V8RuntimeAgentImpl> m_runtimeAgent;
    std::unique_ptr<V8DebuggerAgentImpl> m_debuggerAgent;
    std::unique_ptr<V8HeapProfilerAgentImpl> m_heapProfilerAgent;
    std::unique_ptr<V8ProfilerAgentImpl> m_profilerAgent;
    std::unique_ptr<V8ConsoleAgentImpl> m_consoleAgent;
    std::vector<std::unique_ptr<V8InspectorSession::Inspectable>> m_inspectedObjects;
};

} // namespace blink

#endif
