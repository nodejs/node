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

#ifndef V8RuntimeAgentImpl_h
#define V8RuntimeAgentImpl_h

#include "platform/inspector_protocol/InspectorProtocol.h"
#include "platform/v8_inspector/protocol/Runtime.h"

#include <v8.h>

namespace v8_inspector {

class InjectedScript;
class InspectedContext;
class RemoteObjectIdBase;
class V8ConsoleMessage;
class V8InspectorImpl;
class V8InspectorSessionImpl;

namespace protocol = blink::protocol;
using blink::protocol::Maybe;

class V8RuntimeAgentImpl : public protocol::Runtime::Backend {
    PROTOCOL_DISALLOW_COPY(V8RuntimeAgentImpl);
public:
    V8RuntimeAgentImpl(V8InspectorSessionImpl*, protocol::FrontendChannel*, protocol::DictionaryValue* state);
    ~V8RuntimeAgentImpl() override;
    void restore();

    // Part of the protocol.
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void evaluate(
        const String16& expression,
        const Maybe<String16>& objectGroup,
        const Maybe<bool>& includeCommandLineAPI,
        const Maybe<bool>& silent,
        const Maybe<int>& executionContextId,
        const Maybe<bool>& returnByValue,
        const Maybe<bool>& generatePreview,
        const Maybe<bool>& userGesture,
        const Maybe<bool>& awaitPromise,
        std::unique_ptr<EvaluateCallback>) override;
    void awaitPromise(
        const String16& promiseObjectId,
        const Maybe<bool>& returnByValue,
        const Maybe<bool>& generatePreview,
        std::unique_ptr<AwaitPromiseCallback>) override;
    void callFunctionOn(
        const String16& objectId,
        const String16& expression,
        const Maybe<protocol::Array<protocol::Runtime::CallArgument>>& optionalArguments,
        const Maybe<bool>& silent,
        const Maybe<bool>& returnByValue,
        const Maybe<bool>& generatePreview,
        const Maybe<bool>& userGesture,
        const Maybe<bool>& awaitPromise,
        std::unique_ptr<CallFunctionOnCallback>) override;
    void releaseObject(ErrorString*, const String16& objectId) override;
    void getProperties(ErrorString*,
        const String16& objectId,
        const Maybe<bool>& ownProperties,
        const Maybe<bool>& accessorPropertiesOnly,
        const Maybe<bool>& generatePreview,
        std::unique_ptr<protocol::Array<protocol::Runtime::PropertyDescriptor>>* result,
        Maybe<protocol::Array<protocol::Runtime::InternalPropertyDescriptor>>* internalProperties,
        Maybe<protocol::Runtime::ExceptionDetails>*) override;
    void releaseObjectGroup(ErrorString*, const String16& objectGroup) override;
    void runIfWaitingForDebugger(ErrorString*) override;
    void setCustomObjectFormatterEnabled(ErrorString*, bool) override;
    void discardConsoleEntries(ErrorString*) override;
    void compileScript(ErrorString*,
        const String16& expression,
        const String16& sourceURL,
        bool persistScript,
        const Maybe<int>& executionContextId,
        Maybe<String16>*,
        Maybe<protocol::Runtime::ExceptionDetails>*) override;
    void runScript(
        const String16&,
        const Maybe<int>& executionContextId,
        const Maybe<String16>& objectGroup,
        const Maybe<bool>& silent,
        const Maybe<bool>& includeCommandLineAPI,
        const Maybe<bool>& returnByValue,
        const Maybe<bool>& generatePreview,
        const Maybe<bool>& awaitPromise,
        std::unique_ptr<RunScriptCallback>) override;

    void reset();
    void reportExecutionContextCreated(InspectedContext*);
    void reportExecutionContextDestroyed(InspectedContext*);
    void inspect(std::unique_ptr<protocol::Runtime::RemoteObject> objectToInspect, std::unique_ptr<protocol::DictionaryValue> hints);
    void messageAdded(V8ConsoleMessage*);
    bool enabled() const { return m_enabled; }

private:
    void reportMessage(V8ConsoleMessage*, bool generatePreview);

    V8InspectorSessionImpl* m_session;
    protocol::DictionaryValue* m_state;
    protocol::Runtime::Frontend m_frontend;
    V8InspectorImpl* m_inspector;
    bool m_enabled;
    protocol::HashMap<String16, std::unique_ptr<v8::Global<v8::Script>>> m_compiledScripts;
};

} // namespace v8_inspector

#endif // V8RuntimeAgentImpl_h
