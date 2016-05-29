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

#include "platform/v8_inspector/V8RuntimeAgentImpl.h"

#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/InjectedScript.h"
#include "platform/v8_inspector/InspectedContext.h"
#include "platform/v8_inspector/RemoteObjectId.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8InspectorSessionImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"

namespace blink {

namespace V8RuntimeAgentImplState {
static const char customObjectFormatterEnabled[] = "customObjectFormatterEnabled";
static const char runtimeEnabled[] = "runtimeEnabled";
};

using protocol::Runtime::ExceptionDetails;
using protocol::Runtime::RemoteObject;

static bool hasInternalError(ErrorString* errorString, bool hasError)
{
    if (hasError)
        *errorString = "Internal error";
    return hasError;
}

V8RuntimeAgentImpl::V8RuntimeAgentImpl(V8InspectorSessionImpl* session)
    : m_session(session)
    , m_state(nullptr)
    , m_frontend(nullptr)
    , m_debugger(session->debugger())
    , m_enabled(false)
{
}

V8RuntimeAgentImpl::~V8RuntimeAgentImpl()
{
}

void V8RuntimeAgentImpl::evaluate(
    ErrorString* errorString,
    const String16& expression,
    const Maybe<String16>& objectGroup,
    const Maybe<bool>& includeCommandLineAPI,
    const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole,
    const Maybe<int>& executionContextId,
    const Maybe<bool>& returnByValue,
    const Maybe<bool>& generatePreview,
    const Maybe<bool>& userGesture,
    std::unique_ptr<RemoteObject>* result,
    Maybe<bool>* wasThrown,
    Maybe<ExceptionDetails>* exceptionDetails)
{
    int contextId;
    if (executionContextId.isJust()) {
        contextId = executionContextId.fromJust();
    } else {
        contextId = m_debugger->client()->ensureDefaultContextInGroup(m_session->contextGroupId());
        if (!contextId) {
            *errorString = "Cannot find default execution context";
            return;
        }
    }

    InjectedScript::ContextScope scope(errorString, m_debugger, m_session->contextGroupId(), contextId);
    if (!scope.initialize())
        return;

    if (doNotPauseOnExceptionsAndMuteConsole.fromMaybe(false))
        scope.ignoreExceptionsAndMuteConsole();
    if (userGesture.fromMaybe(false))
        scope.pretendUserGesture();

    if (includeCommandLineAPI.fromMaybe(false) && !scope.installCommandLineAPI())
        return;

    bool evalIsDisabled = !scope.context()->IsCodeGenerationFromStringsAllowed();
    // Temporarily enable allow evals for inspector.
    if (evalIsDisabled)
        scope.context()->AllowCodeGenerationFromStrings(true);

    v8::MaybeLocal<v8::Value> maybeResultValue;
    v8::Local<v8::Script> script = m_debugger->compileInternalScript(scope.context(), toV8String(m_debugger->isolate(), expression), String16());
    if (!script.IsEmpty())
        maybeResultValue = m_debugger->runCompiledScript(scope.context(), script);

    if (evalIsDisabled)
        scope.context()->AllowCodeGenerationFromStrings(false);

    // Re-initialize after running client's code, as it could have destroyed context or session.
    if (!scope.initialize())
        return;
    scope.injectedScript()->wrapEvaluateResult(errorString,
        maybeResultValue,
        scope.tryCatch(),
        objectGroup.fromMaybe(""),
        returnByValue.fromMaybe(false),
        generatePreview.fromMaybe(false),
        result,
        wasThrown,
        exceptionDetails);
}

void V8RuntimeAgentImpl::callFunctionOn(ErrorString* errorString,
    const String16& objectId,
    const String16& expression,
    const Maybe<protocol::Array<protocol::Runtime::CallArgument>>& optionalArguments,
    const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole,
    const Maybe<bool>& returnByValue,
    const Maybe<bool>& generatePreview,
    const Maybe<bool>& userGesture,
    std::unique_ptr<RemoteObject>* result,
    Maybe<bool>* wasThrown)
{
    InjectedScript::ObjectScope scope(errorString, m_debugger, m_session->contextGroupId(), objectId);
    if (!scope.initialize())
        return;

    std::unique_ptr<v8::Local<v8::Value>[]> argv = nullptr;
    int argc = 0;
    if (optionalArguments.isJust()) {
        protocol::Array<protocol::Runtime::CallArgument>* arguments = optionalArguments.fromJust();
        argc = arguments->length();
        argv.reset(new v8::Local<v8::Value>[argc]);
        for (int i = 0; i < argc; ++i) {
            v8::Local<v8::Value> argumentValue;
            if (!scope.injectedScript()->resolveCallArgument(errorString, arguments->get(i)).ToLocal(&argumentValue))
                return;
            argv[i] = argumentValue;
        }
    }

    if (doNotPauseOnExceptionsAndMuteConsole.fromMaybe(false))
        scope.ignoreExceptionsAndMuteConsole();
    if (userGesture.fromMaybe(false))
        scope.pretendUserGesture();

    v8::MaybeLocal<v8::Value> maybeFunctionValue = m_debugger->compileAndRunInternalScript(scope.context(), toV8String(m_debugger->isolate(), "(" + expression + ")"));
    // Re-initialize after running client's code, as it could have destroyed context or session.
    if (!scope.initialize())
        return;

    if (scope.tryCatch().HasCaught()) {
        scope.injectedScript()->wrapEvaluateResult(errorString, maybeFunctionValue, scope.tryCatch(), scope.objectGroupName(), false, false, result, wasThrown, nullptr);
        return;
    }

    v8::Local<v8::Value> functionValue;
    if (!maybeFunctionValue.ToLocal(&functionValue) || !functionValue->IsFunction()) {
        *errorString = "Given expression does not evaluate to a function";
        return;
    }

    v8::MaybeLocal<v8::Value> maybeResultValue = m_debugger->callFunction(functionValue.As<v8::Function>(), scope.context(), scope.object(), argc, argv.get());
    // Re-initialize after running client's code, as it could have destroyed context or session.
    if (!scope.initialize())
        return;

    scope.injectedScript()->wrapEvaluateResult(errorString, maybeResultValue, scope.tryCatch(), scope.objectGroupName(), returnByValue.fromMaybe(false), generatePreview.fromMaybe(false), result, wasThrown, nullptr);
}

void V8RuntimeAgentImpl::getProperties(
    ErrorString* errorString,
    const String16& objectId,
    const Maybe<bool>& ownProperties,
    const Maybe<bool>& accessorPropertiesOnly,
    const Maybe<bool>& generatePreview,
    std::unique_ptr<protocol::Array<protocol::Runtime::PropertyDescriptor>>* result,
    Maybe<protocol::Array<protocol::Runtime::InternalPropertyDescriptor>>* internalProperties,
    Maybe<ExceptionDetails>* exceptionDetails)
{
    using protocol::Runtime::InternalPropertyDescriptor;

    InjectedScript::ObjectScope scope(errorString, m_debugger, m_session->contextGroupId(), objectId);
    if (!scope.initialize())
        return;

    scope.ignoreExceptionsAndMuteConsole();
    if (!scope.object()->IsObject()) {
        *errorString = "Value with given id is not an object";
        return;
    }

    v8::Local<v8::Object> object = scope.object().As<v8::Object>();
    scope.injectedScript()->getProperties(errorString, object, scope.objectGroupName(), ownProperties.fromMaybe(false), accessorPropertiesOnly.fromMaybe(false), generatePreview.fromMaybe(false), result, exceptionDetails);
    if (!errorString->isEmpty() || exceptionDetails->isJust() || accessorPropertiesOnly.fromMaybe(false))
        return;
    v8::Local<v8::Array> propertiesArray;
    if (hasInternalError(errorString, !v8::Debug::GetInternalProperties(m_debugger->isolate(), scope.object()).ToLocal(&propertiesArray)))
        return;
    std::unique_ptr<protocol::Array<InternalPropertyDescriptor>> propertiesProtocolArray = protocol::Array<InternalPropertyDescriptor>::create();
    for (uint32_t i = 0; i < propertiesArray->Length(); i += 2) {
        v8::Local<v8::Value> name;
        if (hasInternalError(errorString, !propertiesArray->Get(scope.context(), i).ToLocal(&name)) || !name->IsString())
            return;
        v8::Local<v8::Value> value;
        if (hasInternalError(errorString, !propertiesArray->Get(scope.context(), i + 1).ToLocal(&value)))
            return;
        std::unique_ptr<RemoteObject> wrappedValue = scope.injectedScript()->wrapObject(errorString, value, scope.objectGroupName());
        if (!wrappedValue)
            return;
        propertiesProtocolArray->addItem(InternalPropertyDescriptor::create()
            .setName(toProtocolString(name.As<v8::String>()))
            .setValue(std::move(wrappedValue)).build());
    }
    if (!propertiesProtocolArray->length())
        return;
    *internalProperties = std::move(propertiesProtocolArray);
}

void V8RuntimeAgentImpl::releaseObject(ErrorString* errorString, const String16& objectId)
{
    InjectedScript::ObjectScope scope(errorString, m_debugger, m_session->contextGroupId(), objectId);
    if (!scope.initialize())
        return;
    scope.injectedScript()->releaseObject(objectId);
}

void V8RuntimeAgentImpl::releaseObjectGroup(ErrorString*, const String16& objectGroup)
{
    m_session->releaseObjectGroup(objectGroup);
}

void V8RuntimeAgentImpl::run(ErrorString* errorString)
{
    m_session->client()->resumeStartup();
}

void V8RuntimeAgentImpl::setCustomObjectFormatterEnabled(ErrorString*, bool enabled)
{
    m_state->setBoolean(V8RuntimeAgentImplState::customObjectFormatterEnabled, enabled);
    m_session->setCustomObjectFormatterEnabled(enabled);
}

void V8RuntimeAgentImpl::compileScript(ErrorString* errorString,
    const String16& expression,
    const String16& sourceURL,
    bool persistScript,
    int executionContextId,
    Maybe<String16>* scriptId,
    Maybe<ExceptionDetails>* exceptionDetails)
{
    if (!m_enabled) {
        *errorString = "Runtime agent is not enabled";
        return;
    }
    InjectedScript::ContextScope scope(errorString, m_debugger, m_session->contextGroupId(), executionContextId);
    if (!scope.initialize())
        return;

    v8::Local<v8::Script> script = m_debugger->compileInternalScript(scope.context(), toV8String(m_debugger->isolate(), expression), sourceURL);
    if (script.IsEmpty()) {
        v8::Local<v8::Message> message = scope.tryCatch().Message();
        if (!message.IsEmpty())
            *exceptionDetails = scope.injectedScript()->createExceptionDetails(message);
        else
            *errorString = "Script compilation failed";
        return;
    }

    if (!persistScript)
        return;

    String16 scriptValueId = String16::number(script->GetUnboundScript()->GetId());
    std::unique_ptr<v8::Global<v8::Script>> global(new v8::Global<v8::Script>(m_debugger->isolate(), script));
    m_compiledScripts.set(scriptValueId, std::move(global));
    *scriptId = scriptValueId;
}

void V8RuntimeAgentImpl::runScript(ErrorString* errorString,
    const String16& scriptId,
    int executionContextId,
    const Maybe<String16>& objectGroup,
    const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole,
    const Maybe<bool>& includeCommandLineAPI,
    std::unique_ptr<RemoteObject>* result,
    Maybe<ExceptionDetails>* exceptionDetails)
{
    if (!m_enabled) {
        *errorString = "Runtime agent is not enabled";
        return;
    }

    if (!m_compiledScripts.contains(scriptId)) {
        *errorString = "Script execution failed";
        return;
    }

    InjectedScript::ContextScope scope(errorString, m_debugger, m_session->contextGroupId(), executionContextId);
    if (!scope.initialize())
        return;

    if (doNotPauseOnExceptionsAndMuteConsole.fromMaybe(false))
        scope.ignoreExceptionsAndMuteConsole();

    std::unique_ptr<v8::Global<v8::Script>> scriptWrapper = m_compiledScripts.take(scriptId);
    v8::Local<v8::Script> script = scriptWrapper->Get(m_debugger->isolate());
    if (script.IsEmpty()) {
        *errorString = "Script execution failed";
        return;
    }

    if (includeCommandLineAPI.fromMaybe(false) && !scope.installCommandLineAPI())
        return;

    v8::MaybeLocal<v8::Value> maybeResultValue = m_debugger->runCompiledScript(scope.context(), script);

    // Re-initialize after running client's code, as it could have destroyed context or session.
    if (!scope.initialize())
        return;
    scope.injectedScript()->wrapEvaluateResult(errorString, maybeResultValue, scope.tryCatch(), objectGroup.fromMaybe(""), false, false, result, nullptr, exceptionDetails);
}

void V8RuntimeAgentImpl::setInspectorState(protocol::DictionaryValue* state)
{
    m_state = state;
}

void V8RuntimeAgentImpl::setFrontend(protocol::Frontend::Runtime* frontend)
{
    m_frontend = frontend;
}

void V8RuntimeAgentImpl::clearFrontend()
{
    ErrorString error;
    disable(&error);
    DCHECK(m_frontend);
    m_frontend = nullptr;
}

void V8RuntimeAgentImpl::restore()
{
    if (!m_state->booleanProperty(V8RuntimeAgentImplState::runtimeEnabled, false))
        return;
    m_frontend->executionContextsCleared();
    ErrorString error;
    enable(&error);
    if (m_state->booleanProperty(V8RuntimeAgentImplState::customObjectFormatterEnabled, false))
        m_session->setCustomObjectFormatterEnabled(true);
}

void V8RuntimeAgentImpl::enable(ErrorString* errorString)
{
    if (m_enabled)
        return;
    m_session->changeInstrumentationCounter(+1);
    m_enabled = true;
    m_state->setBoolean(V8RuntimeAgentImplState::runtimeEnabled, true);
    v8::HandleScope handles(m_debugger->isolate());
    m_session->reportAllContexts(this);
}

void V8RuntimeAgentImpl::disable(ErrorString* errorString)
{
    if (!m_enabled)
        return;
    m_enabled = false;
    m_state->setBoolean(V8RuntimeAgentImplState::runtimeEnabled, false);
    m_session->discardInjectedScripts();
    reset();
    m_session->changeInstrumentationCounter(-1);
}

void V8RuntimeAgentImpl::reset()
{
    m_compiledScripts.clear();
    if (m_enabled) {
        if (const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_session->contextGroupId())) {
            for (auto& idContext : *contexts)
                idContext.second->setReported(false);
        }
        m_frontend->executionContextsCleared();
    }
}

void V8RuntimeAgentImpl::reportExecutionContextCreated(InspectedContext* context)
{
    if (!m_enabled)
        return;
    context->setReported(true);
    std::unique_ptr<protocol::Runtime::ExecutionContextDescription> description = protocol::Runtime::ExecutionContextDescription::create()
        .setId(context->contextId())
        .setIsDefault(context->isDefault())
        .setName(context->humanReadableName())
        .setOrigin(context->origin())
        .setFrameId(context->frameId()).build();
    m_frontend->executionContextCreated(std::move(description));
}

void V8RuntimeAgentImpl::reportExecutionContextDestroyed(InspectedContext* context)
{
    if (m_enabled && context->isReported()) {
        context->setReported(false);
        m_frontend->executionContextDestroyed(context->contextId());
    }
}

void V8RuntimeAgentImpl::inspect(std::unique_ptr<protocol::Runtime::RemoteObject> objectToInspect, std::unique_ptr<protocol::DictionaryValue> hints)
{
    if (m_enabled)
        m_frontend->inspectRequested(std::move(objectToInspect), std::move(hints));
}

} // namespace blink
