// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8INSPECTORSESSIONIMPL_H_
#define V8_INSPECTOR_V8INSPECTORSESSIONIMPL_H_

#include <vector>

#include "src/base/macros.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/protocol/Schema.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

class InjectedScript;
class RemoteObjectIdBase;
class V8ConsoleAgentImpl;
class V8DebuggerAgentImpl;
class V8InspectorImpl;
class V8HeapProfilerAgentImpl;
class V8ProfilerAgentImpl;
class V8RuntimeAgentImpl;
class V8SchemaAgentImpl;

using protocol::Response;

class V8InspectorSessionImpl : public V8InspectorSession,
                               public protocol::FrontendChannel {
 public:
  static std::unique_ptr<V8InspectorSessionImpl> create(
      V8InspectorImpl*, int contextGroupId, int sessionId,
      V8Inspector::Channel*, const StringView& state);
  ~V8InspectorSessionImpl();

  V8InspectorImpl* inspector() const { return m_inspector; }
  V8ConsoleAgentImpl* consoleAgent() { return m_consoleAgent.get(); }
  V8DebuggerAgentImpl* debuggerAgent() { return m_debuggerAgent.get(); }
  V8SchemaAgentImpl* schemaAgent() { return m_schemaAgent.get(); }
  V8ProfilerAgentImpl* profilerAgent() { return m_profilerAgent.get(); }
  V8RuntimeAgentImpl* runtimeAgent() { return m_runtimeAgent.get(); }
  int contextGroupId() const { return m_contextGroupId; }
  int sessionId() const { return m_sessionId; }

  Response findInjectedScript(int contextId, InjectedScript*&);
  Response findInjectedScript(RemoteObjectIdBase*, InjectedScript*&);
  void reset();
  void discardInjectedScripts();
  void reportAllContexts(V8RuntimeAgentImpl*);
  void setCustomObjectFormatterEnabled(bool);
  std::unique_ptr<protocol::Runtime::RemoteObject> wrapObject(
      v8::Local<v8::Context>, v8::Local<v8::Value>, const String16& groupName,
      bool generatePreview);
  std::unique_ptr<protocol::Runtime::RemoteObject> wrapTable(
      v8::Local<v8::Context>, v8::Local<v8::Value> table,
      v8::Local<v8::Value> columns);
  std::vector<std::unique_ptr<protocol::Schema::Domain>> supportedDomainsImpl();
  Response unwrapObject(const String16& objectId, v8::Local<v8::Value>*,
                        v8::Local<v8::Context>*, String16* objectGroup);
  void releaseObjectGroup(const String16& objectGroup);

  // V8InspectorSession implementation.
  void dispatchProtocolMessage(const StringView& message) override;
  std::unique_ptr<StringBuffer> stateJSON() override;
  std::vector<std::unique_ptr<protocol::Schema::API::Domain>> supportedDomains()
      override;
  void addInspectedObject(
      std::unique_ptr<V8InspectorSession::Inspectable>) override;
  void schedulePauseOnNextStatement(const StringView& breakReason,
                                    const StringView& breakDetails) override;
  void cancelPauseOnNextStatement() override;
  void breakProgram(const StringView& breakReason,
                    const StringView& breakDetails) override;
  void setSkipAllPauses(bool) override;
  void resume() override;
  void stepOver() override;
  std::vector<std::unique_ptr<protocol::Debugger::API::SearchMatch>>
  searchInTextByLines(const StringView& text, const StringView& query,
                      bool caseSensitive, bool isRegex) override;
  void releaseObjectGroup(const StringView& objectGroup) override;
  bool unwrapObject(std::unique_ptr<StringBuffer>*, const StringView& objectId,
                    v8::Local<v8::Value>*, v8::Local<v8::Context>*,
                    std::unique_ptr<StringBuffer>* objectGroup) override;
  std::unique_ptr<protocol::Runtime::API::RemoteObject> wrapObject(
      v8::Local<v8::Context>, v8::Local<v8::Value>,
      const StringView& groupName) override;

  V8InspectorSession::Inspectable* inspectedObject(unsigned num);
  static const unsigned kInspectedObjectBufferSize = 5;

 private:
  V8InspectorSessionImpl(V8InspectorImpl*, int contextGroupId, int sessionId,
                         V8Inspector::Channel*, const StringView& state);
  protocol::DictionaryValue* agentState(const String16& name);

  // protocol::FrontendChannel implementation.
  void sendProtocolResponse(
      int callId, std::unique_ptr<protocol::Serializable> message) override;
  void sendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void flushProtocolNotifications() override;

  int m_contextGroupId;
  int m_sessionId;
  V8InspectorImpl* m_inspector;
  V8Inspector::Channel* m_channel;
  bool m_customObjectFormatterEnabled;

  protocol::UberDispatcher m_dispatcher;
  std::unique_ptr<protocol::DictionaryValue> m_state;

  std::unique_ptr<V8RuntimeAgentImpl> m_runtimeAgent;
  std::unique_ptr<V8DebuggerAgentImpl> m_debuggerAgent;
  std::unique_ptr<V8HeapProfilerAgentImpl> m_heapProfilerAgent;
  std::unique_ptr<V8ProfilerAgentImpl> m_profilerAgent;
  std::unique_ptr<V8ConsoleAgentImpl> m_consoleAgent;
  std::unique_ptr<V8SchemaAgentImpl> m_schemaAgent;
  std::vector<std::unique_ptr<V8InspectorSession::Inspectable>>
      m_inspectedObjects;

  DISALLOW_COPY_AND_ASSIGN(V8InspectorSessionImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8INSPECTORSESSIONIMPL_H_
