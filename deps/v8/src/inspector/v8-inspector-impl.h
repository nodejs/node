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

#ifndef V8_INSPECTOR_V8_INSPECTOR_IMPL_H_
#define V8_INSPECTOR_V8_INSPECTOR_IMPL_H_

#include <functional>
#include <map>
#include <unordered_map>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/inspector/injected-script.h"
#include "src/inspector/protocol/Protocol.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

class InspectedContext;
class V8Console;
class V8ConsoleMessageStorage;
class V8Debugger;
class V8DebuggerAgentImpl;
class V8InspectorSessionImpl;
class V8ProfilerAgentImpl;
class V8RuntimeAgentImpl;
class V8StackTraceImpl;

class V8InspectorImpl : public V8Inspector {
 public:
  V8InspectorImpl(v8::Isolate*, V8InspectorClient*);
  ~V8InspectorImpl() override;

  v8::Isolate* isolate() const { return m_isolate; }
  V8InspectorClient* client() { return m_client; }
  V8Debugger* debugger() { return m_debugger.get(); }
  int contextGroupId(v8::Local<v8::Context>) const;
  int contextGroupId(int contextId) const;
  uint64_t isolateId() const { return m_isolateId; }

  v8::MaybeLocal<v8::Value> compileAndRunInternalScript(v8::Local<v8::Context>,
                                                        v8::Local<v8::String>);
  v8::MaybeLocal<v8::Script> compileScript(v8::Local<v8::Context>,
                                           const String16& code,
                                           const String16& fileName);
  v8::Local<v8::Context> regexContext();

  // V8Inspector implementation.
  std::unique_ptr<V8InspectorSession> connect(int contextGroupId,
                                              V8Inspector::Channel*,
                                              const StringView& state) override;
  void contextCreated(const V8ContextInfo&) override;
  void contextDestroyed(v8::Local<v8::Context>) override;
  v8::MaybeLocal<v8::Context> contextById(int contextId) override;
  void contextCollected(int contextGroupId, int contextId);
  void resetContextGroup(int contextGroupId) override;
  void idleStarted() override;
  void idleFinished() override;
  unsigned exceptionThrown(v8::Local<v8::Context>, const StringView& message,
                           v8::Local<v8::Value> exception,
                           const StringView& detailedMessage,
                           const StringView& url, unsigned lineNumber,
                           unsigned columnNumber, std::unique_ptr<V8StackTrace>,
                           int scriptId) override;
  void exceptionRevoked(v8::Local<v8::Context>, unsigned exceptionId,
                        const StringView& message) override;
  std::unique_ptr<V8StackTrace> createStackTrace(
      v8::Local<v8::StackTrace>) override;
  std::unique_ptr<V8StackTrace> captureStackTrace(bool fullStack) override;
  void asyncTaskScheduled(const StringView& taskName, void* task,
                          bool recurring) override;
  void asyncTaskCanceled(void* task) override;
  void asyncTaskStarted(void* task) override;
  void asyncTaskFinished(void* task) override;
  void allAsyncTasksCanceled() override;

  V8StackTraceId storeCurrentStackTrace(const StringView& description) override;
  void externalAsyncTaskStarted(const V8StackTraceId& parent) override;
  void externalAsyncTaskFinished(const V8StackTraceId& parent) override;

  unsigned nextExceptionId() { return ++m_lastExceptionId; }
  void enableStackCapturingIfNeeded();
  void disableStackCapturingIfNeeded();
  void muteExceptions(int contextGroupId);
  void unmuteExceptions(int contextGroupId);
  V8ConsoleMessageStorage* ensureConsoleMessageStorage(int contextGroupId);
  bool hasConsoleMessageStorage(int contextGroupId);
  void discardInspectedContext(int contextGroupId, int contextId);
  void disconnect(V8InspectorSessionImpl*);
  V8InspectorSessionImpl* sessionById(int contextGroupId, int sessionId);
  InspectedContext* getContext(int groupId, int contextId) const;
  InspectedContext* getContext(int contextId) const;
  V8Console* console();
  void forEachContext(int contextGroupId,
                      const std::function<void(InspectedContext*)>& callback);
  void forEachSession(
      int contextGroupId,
      const std::function<void(V8InspectorSessionImpl*)>& callback);

  class EvaluateScope {
   public:
    explicit EvaluateScope(const InjectedScript::Scope& scope);
    ~EvaluateScope();

    protocol::Response setTimeout(double timeout);

   private:
    class TerminateTask;
    struct CancelToken;

    const InjectedScript::Scope& m_scope;
    v8::Isolate* m_isolate;
    std::shared_ptr<CancelToken> m_cancelToken;
    v8::Isolate::SafeForTerminationScope m_safeForTerminationScope;
  };

 private:
  v8::Isolate* m_isolate;
  V8InspectorClient* m_client;
  std::unique_ptr<V8Debugger> m_debugger;
  v8::Global<v8::Context> m_regexContext;
  int m_capturingStackTracesCount;
  unsigned m_lastExceptionId;
  int m_lastContextId;
  int m_lastSessionId = 0;
  uint64_t m_isolateId;

  using MuteExceptionsMap = std::unordered_map<int, int>;
  MuteExceptionsMap m_muteExceptionsMap;

  using ContextByIdMap =
      std::unordered_map<int, std::unique_ptr<InspectedContext>>;
  using ContextsByGroupMap =
      std::unordered_map<int, std::unique_ptr<ContextByIdMap>>;
  ContextsByGroupMap m_contexts;

  // contextGroupId -> sessionId -> session
  std::unordered_map<int, std::map<int, V8InspectorSessionImpl*>> m_sessions;

  using ConsoleStorageMap =
      std::unordered_map<int, std::unique_ptr<V8ConsoleMessageStorage>>;
  ConsoleStorageMap m_consoleStorageMap;

  std::unordered_map<int, int> m_contextIdToGroupIdMap;

  std::unique_ptr<V8Console> m_console;

  DISALLOW_COPY_AND_ASSIGN(V8InspectorImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_INSPECTOR_IMPL_H_
