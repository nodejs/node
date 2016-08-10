// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/inspector_protocol/DispatcherBase.h"

#include "platform/inspector_protocol/FrontendChannel.h"
#include "platform/inspector_protocol/Parser.h"

namespace blink {
namespace protocol {

// static
const char DispatcherBase::kInvalidRequest[] = "Invalid request";

DispatcherBase::WeakPtr::WeakPtr(DispatcherBase* dispatcher) : m_dispatcher(dispatcher) { }

DispatcherBase::WeakPtr::~WeakPtr()
{
    if (m_dispatcher)
        m_dispatcher->m_weakPtrs.erase(this);
}

DispatcherBase::Callback::Callback(std::unique_ptr<DispatcherBase::WeakPtr> backendImpl, int callId)
    : m_backendImpl(std::move(backendImpl))
    , m_callId(callId) { }

DispatcherBase::Callback::~Callback() = default;

void DispatcherBase::Callback::dispose()
{
    m_backendImpl = nullptr;
}

void DispatcherBase::Callback::sendIfActive(std::unique_ptr<protocol::DictionaryValue> partialMessage, const ErrorString& invocationError)
{
    if (!m_backendImpl || !m_backendImpl->get())
        return;
    m_backendImpl->get()->sendResponse(m_callId, invocationError, nullptr, std::move(partialMessage));
    m_backendImpl = nullptr;
}

DispatcherBase::DispatcherBase(FrontendChannel* frontendChannel)
    : m_frontendChannel(frontendChannel) { }

DispatcherBase::~DispatcherBase()
{
    clearFrontend();
}

// static
bool DispatcherBase::getCommandName(const String16& message, String16* result)
{
    std::unique_ptr<protocol::Value> value = parseJSON(message);
    if (!value)
        return false;

    protocol::DictionaryValue* object = DictionaryValue::cast(value.get());
    if (!object)
        return false;

    if (!object->getString("method", result))
        return false;

    return true;
}

void DispatcherBase::sendResponse(int callId, const ErrorString& invocationError, ErrorSupport* errors, std::unique_ptr<protocol::DictionaryValue> result)
{
    if (invocationError.length() || (errors && errors->hasErrors())) {
        reportProtocolError(callId, ServerError, invocationError, errors);
        return;
    }

    std::unique_ptr<protocol::DictionaryValue> responseMessage = DictionaryValue::create();
    responseMessage->setInteger("id", callId);
    responseMessage->setObject("result", std::move(result));
    if (m_frontendChannel)
        m_frontendChannel->sendProtocolResponse(callId, responseMessage->toJSONString());
}

void DispatcherBase::sendResponse(int callId, const ErrorString& invocationError, std::unique_ptr<protocol::DictionaryValue> result)
{
    sendResponse(callId, invocationError, nullptr, std::move(result));
}

void DispatcherBase::sendResponse(int callId, const ErrorString& invocationError)
{
    sendResponse(callId, invocationError, nullptr, DictionaryValue::create());
}

static void reportProtocolError(FrontendChannel* frontendChannel, int callId, DispatcherBase::CommonErrorCode code, const String16& errorMessage, ErrorSupport* errors)
{
    std::unique_ptr<protocol::DictionaryValue> error = DictionaryValue::create();
    error->setInteger("code", code);
    error->setString("message", errorMessage);
    DCHECK(error);
    if (errors && errors->hasErrors())
        error->setString("data", errors->errors());
    std::unique_ptr<protocol::DictionaryValue> message = DictionaryValue::create();
    message->setObject("error", std::move(error));
    message->setInteger("id", callId);
    frontendChannel->sendProtocolResponse(callId, message->toJSONString());
}

void DispatcherBase::reportProtocolError(int callId, CommonErrorCode code, const String16& errorMessage, ErrorSupport* errors)
{
    if (m_frontendChannel)
        ::blink::protocol::reportProtocolError(m_frontendChannel, callId, code, errorMessage, errors);
}

void DispatcherBase::clearFrontend()
{
    m_frontendChannel = nullptr;
    for (auto& weak : m_weakPtrs)
        weak->dispose();
    m_weakPtrs.clear();
}

std::unique_ptr<DispatcherBase::WeakPtr> DispatcherBase::weakPtr()
{
    std::unique_ptr<DispatcherBase::WeakPtr> weak(new DispatcherBase::WeakPtr(this));
    m_weakPtrs.insert(weak.get());
    return weak;
}

UberDispatcher::UberDispatcher(FrontendChannel* frontendChannel)
    : m_frontendChannel(frontendChannel) { }

void UberDispatcher::registerBackend(const String16& name, std::unique_ptr<protocol::DispatcherBase> dispatcher)
{
    m_dispatchers[name] = std::move(dispatcher);
}

void UberDispatcher::dispatch(const String16& message)
{
    std::unique_ptr<protocol::Value> parsedMessage = parseJSON(message);
    if (!parsedMessage)
        return;
    std::unique_ptr<protocol::DictionaryValue> messageObject = DictionaryValue::cast(std::move(parsedMessage));
    if (!messageObject)
        return;

    int callId = 0;
    protocol::Value* callIdValue = messageObject->get("id");
    bool success = callIdValue->asInteger(&callId);
    if (!success)
        return;

    protocol::Value* methodValue = messageObject->get("method");
    String16 method;
    success = methodValue && methodValue->asString(&method);
    if (!success)
        return;

    size_t dotIndex = method.find(".");
    if (dotIndex == kNotFound) {
        reportProtocolError(m_frontendChannel, callId, DispatcherBase::MethodNotFound, "'" + method + "' wasn't found", nullptr);
        return;
    }
    String16 domain = method.substring(0, dotIndex);
    auto it = m_dispatchers.find(domain);
    if (it == m_dispatchers.end()) {
        reportProtocolError(m_frontendChannel, callId, DispatcherBase::MethodNotFound, "'" + method + "' wasn't found", nullptr);
        return;
    }
    it->second->dispatch(callId, method, std::move(messageObject));
}

UberDispatcher::~UberDispatcher() = default;

} // namespace protocol
} // namespace blink
