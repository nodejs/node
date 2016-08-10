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

#include "platform/inspector_protocol/Parser.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/InjectedScript.h"
#include "platform/v8_inspector/InspectedContext.h"
#include "platform/v8_inspector/RemoteObjectId.h"
#include "platform/v8_inspector/V8Compat.h"
#include "platform/v8_inspector/V8ConsoleMessage.h"
#include "platform/v8_inspector/V8Debugger.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8InspectorImpl.h"
#include "platform/v8_inspector/V8InspectorSessionImpl.h"
#include "platform/v8_inspector/V8StackTraceImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8InspectorClient.h"

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

namespace {

template<typename Callback>
class ProtocolPromiseHandler {
public:
    static void add(V8InspectorImpl* inspector, v8::Local<v8::Context> context, v8::MaybeLocal<v8::Value> value, const String16& notPromiseError, int contextGroupId, int executionContextId, const String16& objectGroup, bool returnByValue, bool generatePreview, std::unique_ptr<Callback> callback)
    {
        if (value.IsEmpty()) {
            callback->sendFailure("Internal error");
            return;
        }
        if (!value.ToLocalChecked()->IsPromise()) {
            callback->sendFailure(notPromiseError);
            return;
        }
        v8::Local<v8::Promise> promise = v8::Local<v8::Promise>::Cast(value.ToLocalChecked());
        Callback* rawCallback = callback.get();
        ProtocolPromiseHandler<Callback>* handler = new ProtocolPromiseHandler(inspector, contextGroupId, executionContextId, objectGroup, returnByValue, generatePreview, std::move(callback));
        v8::Local<v8::Value> wrapper = handler->m_wrapper.Get(inspector->isolate());

        v8::Local<v8::Function> thenCallbackFunction = V8_FUNCTION_NEW_REMOVE_PROTOTYPE(context, thenCallback, wrapper, 0).ToLocalChecked();
        if (promise->Then(context, thenCallbackFunction).IsEmpty()) {
            rawCallback->sendFailure("Internal error");
            return;
        }
        v8::Local<v8::Function> catchCallbackFunction = V8_FUNCTION_NEW_REMOVE_PROTOTYPE(context, catchCallback, wrapper, 0).ToLocalChecked();
        if (promise->Catch(context, catchCallbackFunction).IsEmpty()) {
            rawCallback->sendFailure("Internal error");
            return;
        }
    }
private:
    static void thenCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
    {
        ProtocolPromiseHandler<Callback>* handler = static_cast<ProtocolPromiseHandler<Callback>*>(info.Data().As<v8::External>()->Value());
        DCHECK(handler);
        v8::Local<v8::Value> value = info.Length() > 0 ? info[0] : v8::Local<v8::Value>::Cast(v8::Undefined(info.GetIsolate()));
        handler->m_callback->sendSuccess(handler->wrapObject(value), Maybe<bool>(), Maybe<protocol::Runtime::ExceptionDetails>());
    }

    static void catchCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
    {
        ProtocolPromiseHandler<Callback>* handler = static_cast<ProtocolPromiseHandler<Callback>*>(info.Data().As<v8::External>()->Value());
        DCHECK(handler);
        v8::Local<v8::Value> value = info.Length() > 0 ? info[0] : v8::Local<v8::Value>::Cast(v8::Undefined(info.GetIsolate()));

        std::unique_ptr<protocol::Runtime::ExceptionDetails> exceptionDetails;
        std::unique_ptr<V8StackTraceImpl> stack = handler->m_inspector->debugger()->captureStackTrace(true);
        if (stack) {
            exceptionDetails = protocol::Runtime::ExceptionDetails::create()
                .setText("Promise was rejected")
                .setLineNumber(!stack->isEmpty() ? stack->topLineNumber() : 0)
                .setColumnNumber(!stack->isEmpty() ? stack->topColumnNumber() : 0)
                .setScriptId(!stack->isEmpty() ? stack->topScriptId() : String16())
                .setStackTrace(stack->buildInspectorObjectImpl())
                .build();
        }
        handler->m_callback->sendSuccess(handler->wrapObject(value), true, std::move(exceptionDetails));
    }

    ProtocolPromiseHandler(V8InspectorImpl* inspector, int contextGroupId, int executionContextId, const String16& objectGroup, bool returnByValue, bool generatePreview, std::unique_ptr<Callback> callback)
        : m_inspector(inspector)
        , m_contextGroupId(contextGroupId)
        , m_executionContextId(executionContextId)
        , m_objectGroup(objectGroup)
        , m_returnByValue(returnByValue)
        , m_generatePreview(generatePreview)
        , m_callback(std::move(callback))
        , m_wrapper(inspector->isolate(), v8::External::New(inspector->isolate(), this))
    {
        m_wrapper.SetWeak(this, cleanup, v8::WeakCallbackType::kParameter);
    }

    static void cleanup(const v8::WeakCallbackInfo<ProtocolPromiseHandler<Callback>>& data)
    {
        if (!data.GetParameter()->m_wrapper.IsEmpty()) {
            data.GetParameter()->m_wrapper.Reset();
            data.SetSecondPassCallback(cleanup);
        } else {
            data.GetParameter()->m_callback->sendFailure("Promise was collected");
            delete data.GetParameter();
        }
    }

    std::unique_ptr<protocol::Runtime::RemoteObject> wrapObject(v8::Local<v8::Value> value)
    {
        ErrorString errorString;
        InjectedScript::ContextScope scope(&errorString, m_inspector, m_contextGroupId, m_executionContextId);
        if (!scope.initialize()) {
            m_callback->sendFailure(errorString);
            return nullptr;
        }
        std::unique_ptr<protocol::Runtime::RemoteObject> wrappedValue = scope.injectedScript()->wrapObject(&errorString, value, m_objectGroup, m_returnByValue, m_generatePreview);
        if (!wrappedValue) {
            m_callback->sendFailure(errorString);
            return nullptr;
        }
        return wrappedValue;
    }

    V8InspectorImpl* m_inspector;
    int m_contextGroupId;
    int m_executionContextId;
    String16 m_objectGroup;
    bool m_returnByValue;
    bool m_generatePreview;
    std::unique_ptr<Callback> m_callback;
    v8::Global<v8::External> m_wrapper;
};

template<typename Callback>
bool wrapEvaluateResultAsync(InjectedScript* injectedScript, v8::MaybeLocal<v8::Value> maybeResultValue, const v8::TryCatch& tryCatch, const String16& objectGroup, bool returnByValue, bool generatePreview, Callback* callback)
{
    std::unique_ptr<RemoteObject> result;
    Maybe<bool> wasThrown;
    Maybe<protocol::Runtime::ExceptionDetails> exceptionDetails;

    ErrorString errorString;
    injectedScript->wrapEvaluateResult(&errorString,
        maybeResultValue,
        tryCatch,
        objectGroup,
        returnByValue,
        generatePreview,
        &result,
        &wasThrown,
        &exceptionDetails);
    if (errorString.isEmpty()) {
        callback->sendSuccess(std::move(result), wasThrown, exceptionDetails);
        return true;
    }
    callback->sendFailure(errorString);
    return false;
}

int ensureContext(ErrorString* errorString, V8InspectorImpl* inspector, int contextGroupId, const Maybe<int>& executionContextId)
{
    int contextId;
    if (executionContextId.isJust()) {
        contextId = executionContextId.fromJust();
    } else {
        v8::HandleScope handles(inspector->isolate());
        v8::Local<v8::Context> defaultContext = inspector->client()->ensureDefaultContextInGroup(contextGroupId);
        if (defaultContext.IsEmpty()) {
            *errorString = "Cannot find default execution context";
            return 0;
        }
        contextId = V8Debugger::contextId(defaultContext);
    }
    return contextId;
}

} // namespace

V8RuntimeAgentImpl::V8RuntimeAgentImpl(V8InspectorSessionImpl* session, protocol::FrontendChannel* FrontendChannel, protocol::DictionaryValue* state)
    : m_session(session)
    , m_state(state)
    , m_frontend(FrontendChannel)
    , m_inspector(session->inspector())
    , m_enabled(false)
{
}

V8RuntimeAgentImpl::~V8RuntimeAgentImpl()
{
}

void V8RuntimeAgentImpl::evaluate(
    const String16& expression,
    const Maybe<String16>& objectGroup,
    const Maybe<bool>& includeCommandLineAPI,
    const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole,
    const Maybe<int>& executionContextId,
    const Maybe<bool>& returnByValue,
    const Maybe<bool>& generatePreview,
    const Maybe<bool>& userGesture,
    const Maybe<bool>& awaitPromise,
    std::unique_ptr<EvaluateCallback> callback)
{
    ErrorString errorString;
    int contextId = ensureContext(&errorString, m_inspector, m_session->contextGroupId(), executionContextId);
    if (!errorString.isEmpty()) {
        callback->sendFailure(errorString);
        return;
    }

    InjectedScript::ContextScope scope(&errorString, m_inspector, m_session->contextGroupId(), contextId);
    if (!scope.initialize()) {
        callback->sendFailure(errorString);
        return;
    }

    if (doNotPauseOnExceptionsAndMuteConsole.fromMaybe(false))
        scope.ignoreExceptionsAndMuteConsole();
    if (userGesture.fromMaybe(false))
        scope.pretendUserGesture();

    if (includeCommandLineAPI.fromMaybe(false) && !scope.installCommandLineAPI()) {
        callback->sendFailure(errorString);
        return;
    }

    bool evalIsDisabled = !scope.context()->IsCodeGenerationFromStringsAllowed();
    // Temporarily enable allow evals for inspector.
    if (evalIsDisabled)
        scope.context()->AllowCodeGenerationFromStrings(true);

    v8::MaybeLocal<v8::Value> maybeResultValue;
    v8::Local<v8::Script> script = m_inspector->compileScript(scope.context(), toV8String(m_inspector->isolate(), expression), String16(), false);
    if (!script.IsEmpty())
        maybeResultValue = m_inspector->runCompiledScript(scope.context(), script);

    if (evalIsDisabled)
        scope.context()->AllowCodeGenerationFromStrings(false);

    // Re-initialize after running client's code, as it could have destroyed context or session.
    if (!scope.initialize()) {
        callback->sendFailure(errorString);
        return;
    }

    if (!awaitPromise.fromMaybe(false) || scope.tryCatch().HasCaught()) {
        wrapEvaluateResultAsync(scope.injectedScript(), maybeResultValue, scope.tryCatch(), objectGroup.fromMaybe(""), returnByValue.fromMaybe(false), generatePreview.fromMaybe(false), callback.get());
        return;
    }
    ProtocolPromiseHandler<EvaluateCallback>::add(
        m_inspector,
        scope.context(),
        maybeResultValue,
        "Result of the evaluation is not a promise",
        m_session->contextGroupId(),
        scope.injectedScript()->context()->contextId(),
        objectGroup.fromMaybe(""),
        returnByValue.fromMaybe(false),
        generatePreview.fromMaybe(false),
        std::move(callback));
}

void V8RuntimeAgentImpl::awaitPromise(
    const String16& promiseObjectId,
    const Maybe<bool>& returnByValue,
    const Maybe<bool>& generatePreview,
    std::unique_ptr<AwaitPromiseCallback> callback)
{
    ErrorString errorString;
    InjectedScript::ObjectScope scope(&errorString, m_inspector, m_session->contextGroupId(), promiseObjectId);
    if (!scope.initialize()) {
        callback->sendFailure(errorString);
        return;
    }
    ProtocolPromiseHandler<AwaitPromiseCallback>::add(
        m_inspector,
        scope.context(),
        scope.object(),
        "Could not find promise with given id",
        m_session->contextGroupId(),
        scope.injectedScript()->context()->contextId(),
        scope.objectGroupName(),
        returnByValue.fromMaybe(false),
        generatePreview.fromMaybe(false),
        std::move(callback));
}

void V8RuntimeAgentImpl::callFunctionOn(
    const String16& objectId,
    const String16& expression,
    const Maybe<protocol::Array<protocol::Runtime::CallArgument>>& optionalArguments,
    const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole,
    const Maybe<bool>& returnByValue,
    const Maybe<bool>& generatePreview,
    const Maybe<bool>& userGesture,
    const Maybe<bool>& awaitPromise,
    std::unique_ptr<CallFunctionOnCallback> callback)
{
    ErrorString errorString;
    InjectedScript::ObjectScope scope(&errorString, m_inspector, m_session->contextGroupId(), objectId);
    if (!scope.initialize()) {
        callback->sendFailure(errorString);
        return;
    }

    std::unique_ptr<v8::Local<v8::Value>[]> argv = nullptr;
    int argc = 0;
    if (optionalArguments.isJust()) {
        protocol::Array<protocol::Runtime::CallArgument>* arguments = optionalArguments.fromJust();
        argc = arguments->length();
        argv.reset(new v8::Local<v8::Value>[argc]);
        for (int i = 0; i < argc; ++i) {
            v8::Local<v8::Value> argumentValue;
            if (!scope.injectedScript()->resolveCallArgument(&errorString, arguments->get(i)).ToLocal(&argumentValue)) {
                callback->sendFailure(errorString);
                return;
            }
            argv[i] = argumentValue;
        }
    }

    if (doNotPauseOnExceptionsAndMuteConsole.fromMaybe(false))
        scope.ignoreExceptionsAndMuteConsole();
    if (userGesture.fromMaybe(false))
        scope.pretendUserGesture();

    v8::MaybeLocal<v8::Value> maybeFunctionValue = m_inspector->compileAndRunInternalScript(scope.context(), toV8String(m_inspector->isolate(), "(" + expression + ")"));
    // Re-initialize after running client's code, as it could have destroyed context or session.
    if (!scope.initialize()) {
        callback->sendFailure(errorString);
        return;
    }

    if (scope.tryCatch().HasCaught()) {
        wrapEvaluateResultAsync(scope.injectedScript(), maybeFunctionValue, scope.tryCatch(), scope.objectGroupName(), false, false, callback.get());
        return;
    }

    v8::Local<v8::Value> functionValue;
    if (!maybeFunctionValue.ToLocal(&functionValue) || !functionValue->IsFunction()) {
        callback->sendFailure("Given expression does not evaluate to a function");
        return;
    }

    v8::MaybeLocal<v8::Value> maybeResultValue = m_inspector->callFunction(functionValue.As<v8::Function>(), scope.context(), scope.object(), argc, argv.get());
    // Re-initialize after running client's code, as it could have destroyed context or session.
    if (!scope.initialize()) {
        callback->sendFailure(errorString);
        return;
    }

    if (!awaitPromise.fromMaybe(false) || scope.tryCatch().HasCaught()) {
        wrapEvaluateResultAsync(scope.injectedScript(), maybeResultValue, scope.tryCatch(), scope.objectGroupName(), returnByValue.fromMaybe(false), generatePreview.fromMaybe(false), callback.get());
        return;
    }

    ProtocolPromiseHandler<CallFunctionOnCallback>::add(
        m_inspector,
        scope.context(),
        maybeResultValue,
        "Result of the function call is not a promise",
        m_session->contextGroupId(),
        scope.injectedScript()->context()->contextId(),
        scope.objectGroupName(),
        returnByValue.fromMaybe(false),
        generatePreview.fromMaybe(false),
        std::move(callback));
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

    InjectedScript::ObjectScope scope(errorString, m_inspector, m_session->contextGroupId(), objectId);
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
    if (hasInternalError(errorString, !m_inspector->debugger()->internalProperties(scope.context(), scope.object()).ToLocal(&propertiesArray)))
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
    InjectedScript::ObjectScope scope(errorString, m_inspector, m_session->contextGroupId(), objectId);
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
    m_inspector->client()->resumeStartup(m_session->contextGroupId());
}

void V8RuntimeAgentImpl::setCustomObjectFormatterEnabled(ErrorString*, bool enabled)
{
    m_state->setBoolean(V8RuntimeAgentImplState::customObjectFormatterEnabled, enabled);
    m_session->setCustomObjectFormatterEnabled(enabled);
}

void V8RuntimeAgentImpl::discardConsoleEntries(ErrorString*)
{
    V8ConsoleMessageStorage* storage = m_inspector->ensureConsoleMessageStorage(m_session->contextGroupId());
    storage->clear();
}

void V8RuntimeAgentImpl::compileScript(ErrorString* errorString,
    const String16& expression,
    const String16& sourceURL,
    bool persistScript,
    const Maybe<int>& executionContextId,
    Maybe<String16>* scriptId,
    Maybe<ExceptionDetails>* exceptionDetails)
{
    if (!m_enabled) {
        *errorString = "Runtime agent is not enabled";
        return;
    }
    int contextId = ensureContext(errorString, m_inspector, m_session->contextGroupId(), executionContextId);
    if (!errorString->isEmpty())
        return;
    InjectedScript::ContextScope scope(errorString, m_inspector, m_session->contextGroupId(), contextId);
    if (!scope.initialize())
        return;

    if (!persistScript)
        m_inspector->debugger()->muteScriptParsedEvents();
    v8::Local<v8::Script> script = m_inspector->compileScript(scope.context(), toV8String(m_inspector->isolate(), expression), sourceURL, false);
    if (!persistScript)
        m_inspector->debugger()->unmuteScriptParsedEvents();
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

    String16 scriptValueId = String16::fromInteger(script->GetUnboundScript()->GetId());
    std::unique_ptr<v8::Global<v8::Script>> global(new v8::Global<v8::Script>(m_inspector->isolate(), script));
    m_compiledScripts[scriptValueId] = std::move(global);
    *scriptId = scriptValueId;
}

void V8RuntimeAgentImpl::runScript(
    const String16& scriptId,
    const Maybe<int>& executionContextId,
    const Maybe<String16>& objectGroup,
    const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole,
    const Maybe<bool>& includeCommandLineAPI,
    const Maybe<bool>& returnByValue,
    const Maybe<bool>& generatePreview,
    const Maybe<bool>& awaitPromise,
    std::unique_ptr<RunScriptCallback> callback)
{
    if (!m_enabled) {
        callback->sendFailure("Runtime agent is not enabled");
        return;
    }

    auto it = m_compiledScripts.find(scriptId);
    if (it == m_compiledScripts.end()) {
        callback->sendFailure("No script with given id");
        return;
    }

    ErrorString errorString;
    int contextId = ensureContext(&errorString, m_inspector, m_session->contextGroupId(), executionContextId);
    if (!errorString.isEmpty()) {
        callback->sendFailure(errorString);
        return;
    }

    InjectedScript::ContextScope scope(&errorString, m_inspector, m_session->contextGroupId(), contextId);
    if (!scope.initialize()) {
        callback->sendFailure(errorString);
        return;
    }

    if (doNotPauseOnExceptionsAndMuteConsole.fromMaybe(false))
        scope.ignoreExceptionsAndMuteConsole();

    std::unique_ptr<v8::Global<v8::Script>> scriptWrapper = std::move(it->second);
    m_compiledScripts.erase(it);
    v8::Local<v8::Script> script = scriptWrapper->Get(m_inspector->isolate());
    if (script.IsEmpty()) {
        callback->sendFailure("Script execution failed");
        return;
    }

    if (includeCommandLineAPI.fromMaybe(false) && !scope.installCommandLineAPI())
        return;

    v8::MaybeLocal<v8::Value> maybeResultValue = m_inspector->runCompiledScript(scope.context(), script);

    // Re-initialize after running client's code, as it could have destroyed context or session.
    if (!scope.initialize())
        return;

    if (!awaitPromise.fromMaybe(false) || scope.tryCatch().HasCaught()) {
        wrapEvaluateResultAsync(scope.injectedScript(), maybeResultValue, scope.tryCatch(), objectGroup.fromMaybe(""), returnByValue.fromMaybe(false), generatePreview.fromMaybe(false), callback.get());
        return;
    }
    ProtocolPromiseHandler<RunScriptCallback>::add(
        m_inspector,
        scope.context(),
        maybeResultValue.ToLocalChecked(),
        "Result of the script execution is not a promise",
        m_session->contextGroupId(),
        scope.injectedScript()->context()->contextId(),
        objectGroup.fromMaybe(""),
        returnByValue.fromMaybe(false),
        generatePreview.fromMaybe(false),
        std::move(callback));
}

void V8RuntimeAgentImpl::restore()
{
    if (!m_state->booleanProperty(V8RuntimeAgentImplState::runtimeEnabled, false))
        return;
    m_frontend.executionContextsCleared();
    ErrorString error;
    enable(&error);
    if (m_state->booleanProperty(V8RuntimeAgentImplState::customObjectFormatterEnabled, false))
        m_session->setCustomObjectFormatterEnabled(true);
}

void V8RuntimeAgentImpl::enable(ErrorString* errorString)
{
    if (m_enabled)
        return;
    m_inspector->client()->beginEnsureAllContextsInGroup(m_session->contextGroupId());
    m_enabled = true;
    m_state->setBoolean(V8RuntimeAgentImplState::runtimeEnabled, true);
    m_inspector->enableStackCapturingIfNeeded();
    m_session->reportAllContexts(this);
    V8ConsoleMessageStorage* storage = m_inspector->ensureConsoleMessageStorage(m_session->contextGroupId());
    for (const auto& message : storage->messages())
        reportMessage(message.get(), false);
}

void V8RuntimeAgentImpl::disable(ErrorString* errorString)
{
    if (!m_enabled)
        return;
    m_enabled = false;
    m_state->setBoolean(V8RuntimeAgentImplState::runtimeEnabled, false);
    m_inspector->disableStackCapturingIfNeeded();
    m_session->discardInjectedScripts();
    reset();
    m_inspector->client()->endEnsureAllContextsInGroup(m_session->contextGroupId());
}

void V8RuntimeAgentImpl::reset()
{
    m_compiledScripts.clear();
    if (m_enabled) {
        if (const V8InspectorImpl::ContextByIdMap* contexts = m_inspector->contextGroup(m_session->contextGroupId())) {
            for (auto& idContext : *contexts)
                idContext.second->setReported(false);
        }
        m_frontend.executionContextsCleared();
    }
}

void V8RuntimeAgentImpl::reportExecutionContextCreated(InspectedContext* context)
{
    if (!m_enabled)
        return;
    context->setReported(true);
    std::unique_ptr<protocol::Runtime::ExecutionContextDescription> description = protocol::Runtime::ExecutionContextDescription::create()
        .setId(context->contextId())
        .setName(context->humanReadableName())
        .setOrigin(context->origin()).build();
    if (!context->auxData().isEmpty())
        description->setAuxData(protocol::DictionaryValue::cast(parseJSON(context->auxData())));
    m_frontend.executionContextCreated(std::move(description));
}

void V8RuntimeAgentImpl::reportExecutionContextDestroyed(InspectedContext* context)
{
    if (m_enabled && context->isReported()) {
        context->setReported(false);
        m_frontend.executionContextDestroyed(context->contextId());
    }
}

void V8RuntimeAgentImpl::inspect(std::unique_ptr<protocol::Runtime::RemoteObject> objectToInspect, std::unique_ptr<protocol::DictionaryValue> hints)
{
    if (m_enabled)
        m_frontend.inspectRequested(std::move(objectToInspect), std::move(hints));
}

void V8RuntimeAgentImpl::messageAdded(V8ConsoleMessage* message)
{
    if (m_enabled)
        reportMessage(message, true);
}

void V8RuntimeAgentImpl::reportMessage(V8ConsoleMessage* message, bool generatePreview)
{
    message->reportToFrontend(&m_frontend, m_session, generatePreview);
    m_frontend.flush();
}

} // namespace blink
