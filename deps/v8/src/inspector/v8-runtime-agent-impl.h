/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef V8_INSPECTOR_V8_RUNTIME_AGENT_IMPL_H_
#define V8_INSPECTOR_V8_RUNTIME_AGENT_IMPL_H_

#include <memory>
#include <unordered_map>

#include "include/v8-persistent-handle.h"
#include "src/base/macros.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/protocol/Runtime.h"

namespace v8 {
class Script;
}  // namespace v8

namespace v8_inspector {

class InjectedScript;
class InspectedContext;
class RemoteObjectIdBase;
class V8ConsoleMessage;
class V8DebuggerBarrier;
class V8InspectorImpl;
class V8InspectorSessionImpl;

using protocol::Response;

class V8RuntimeAgentImpl : public protocol::Runtime::Backend {
 public:
  V8RuntimeAgentImpl(V8InspectorSessionImpl*, protocol::FrontendChannel*,
                     protocol::DictionaryValue* state,
                     std::shared_ptr<V8DebuggerBarrier>);
  ~V8RuntimeAgentImpl() override;
  V8RuntimeAgentImpl(const V8RuntimeAgentImpl&) = delete;
  V8RuntimeAgentImpl& operator=(const V8RuntimeAgentImpl&) = delete;
  void restore();

  // Part of the protocol.
  Response enable() override;
  Response disable() override;
  void evaluate(
      const String16& expression, std::optional<String16> objectGroup,
      std::optional<bool> includeCommandLineAPI, std::optional<bool> silent,
      std::optional<int> executionContextId, std::optional<bool> returnByValue,
      std::optional<bool> generatePreview, std::optional<bool> userGesture,
      std::optional<bool> awaitPromise, std::optional<bool> throwOnSideEffect,
      std::optional<double> timeout, std::optional<bool> disableBreaks,
      std::optional<bool> replMode,
      std::optional<bool> allowUnsafeEvalBlockedByCSP,
      std::optional<String16> uniqueContextId,
      std::unique_ptr<protocol::Runtime::SerializationOptions>
          serializationOptions,
      std::unique_ptr<EvaluateCallback>) override;
  void awaitPromise(const String16& promiseObjectId,
                    std::optional<bool> returnByValue,
                    std::optional<bool> generatePreview,
                    std::unique_ptr<AwaitPromiseCallback>) override;
  void callFunctionOn(
      const String16& expression, std::optional<String16> objectId,
      std::unique_ptr<protocol::Array<protocol::Runtime::CallArgument>>
          optionalArguments,
      std::optional<bool> silent, std::optional<bool> returnByValue,
      std::optional<bool> generatePreview, std::optional<bool> userGesture,
      std::optional<bool> awaitPromise, std::optional<int> executionContextId,
      std::optional<String16> objectGroup,
      std::optional<bool> throwOnSideEffect,
      std::optional<String16> uniqueContextId,
      std::unique_ptr<protocol::Runtime::SerializationOptions>
          serializationOptions,
      std::unique_ptr<CallFunctionOnCallback>) override;
  Response releaseObject(const String16& objectId) override;
  Response getProperties(
      const String16& objectId, std::optional<bool> ownProperties,
      std::optional<bool> accessorPropertiesOnly,
      std::optional<bool> generatePreview,
      std::optional<bool> nonIndexedPropertiesOnly,
      std::unique_ptr<protocol::Array<protocol::Runtime::PropertyDescriptor>>*
          result,
      std::unique_ptr<
          protocol::Array<protocol::Runtime::InternalPropertyDescriptor>>*
          internalProperties,
      std::unique_ptr<
          protocol::Array<protocol::Runtime::PrivatePropertyDescriptor>>*
          privateProperties,
      std::unique_ptr<protocol::Runtime::ExceptionDetails>*) override;
  Response releaseObjectGroup(const String16& objectGroup) override;
  Response runIfWaitingForDebugger() override;
  Response setCustomObjectFormatterEnabled(bool) override;
  Response setMaxCallStackSizeToCapture(int) override;
  Response discardConsoleEntries() override;
  Response compileScript(
      const String16& expression, const String16& sourceURL, bool persistScript,
      std::optional<int> executionContextId, std::optional<String16>*,
      std::unique_ptr<protocol::Runtime::ExceptionDetails>*) override;
  void runScript(const String16&, std::optional<int> executionContextId,
                 std::optional<String16> objectGroup,
                 std::optional<bool> silent,
                 std::optional<bool> includeCommandLineAPI,
                 std::optional<bool> returnByValue,
                 std::optional<bool> generatePreview,
                 std::optional<bool> awaitPromise,
                 std::unique_ptr<RunScriptCallback>) override;
  Response queryObjects(
      const String16& prototypeObjectId, std::optional<String16> objectGroup,
      std::unique_ptr<protocol::Runtime::RemoteObject>* objects) override;
  Response globalLexicalScopeNames(
      std::optional<int> executionContextId,
      std::unique_ptr<protocol::Array<String16>>* outNames) override;
  Response getIsolateId(String16* outIsolateId) override;
  Response getHeapUsage(double* out_usedSize, double* out_totalSize,
                        double* out_embedderHeapUsedSize,
                        double* out_backingStorageSize) override;
  void terminateExecution(
      std::unique_ptr<TerminateExecutionCallback> callback) override;

  Response addBinding(const String16& name,
                      std::optional<int> executionContextId,
                      std::optional<String16> executionContextName) override;
  Response removeBinding(const String16& name) override;
  void addBindings(InspectedContext* context);
  Response getExceptionDetails(
      const String16& errorObjectId,
      std::unique_ptr<protocol::Runtime::ExceptionDetails>*
          out_exceptionDetails) override;

  void reset();
  void reportExecutionContextCreated(InspectedContext*);
  void reportExecutionContextDestroyed(InspectedContext*);
  void inspect(std::unique_ptr<protocol::Runtime::RemoteObject> objectToInspect,
               std::unique_ptr<protocol::DictionaryValue> hints,
               int executionContextId);
  void messageAdded(V8ConsoleMessage*);
  bool enabled() const { return m_enabled; }

 private:
  bool reportMessage(V8ConsoleMessage*, bool generatePreview);

  static void bindingCallback(const v8::FunctionCallbackInfo<v8::Value>& info);
  void bindingCalled(const String16& name, const String16& payload,
                     int executionContextId);
  void addBinding(InspectedContext* context, const String16& name);

  V8InspectorSessionImpl* m_session;
  protocol::DictionaryValue* m_state;
  protocol::Runtime::Frontend m_frontend;
  V8InspectorImpl* m_inspector;
  std::shared_ptr<V8DebuggerBarrier> m_debuggerBarrier;
  bool m_enabled;
  std::unordered_map<String16, std::unique_ptr<v8::Global<v8::Script>>>
      m_compiledScripts;
  // Binding name -> executionContextIds mapping.
  std::unordered_map<String16, std::unordered_set<int>> m_activeBindings;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_RUNTIME_AGENT_IMPL_H_
