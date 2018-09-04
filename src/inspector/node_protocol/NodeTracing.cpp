// This file is generated

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "inspector/node_protocol/NodeTracing.h"

#include "inspector/node_protocol/Protocol.h"

namespace node {
namespace inspector {
namespace protocol {
namespace NodeTracing {

// ------------- Enum values from types.

const char Metainfo::domainName[] = "NodeTracing";
const char Metainfo::commandPrefix[] = "NodeTracing.";
const char Metainfo::version[] = "1.0";

const char* TraceConfig::RecordModeEnum::RecordUntilFull = "recordUntilFull";
const char* TraceConfig::RecordModeEnum::RecordContinuously = "recordContinuously";
const char* TraceConfig::RecordModeEnum::RecordAsMuchAsPossible = "recordAsMuchAsPossible";

std::unique_ptr<TraceConfig> TraceConfig::fromValue(protocol::Value* value, ErrorSupport* errors)
{
    if (!value || value->type() != protocol::Value::TypeObject) {
        errors->addError("object expected");
        return nullptr;
    }

    std::unique_ptr<TraceConfig> result(new TraceConfig());
    protocol::DictionaryValue* object = DictionaryValue::cast(value);
    errors->push();
    protocol::Value* recordModeValue = object->get("recordMode");
    if (recordModeValue) {
        errors->setName("recordMode");
        result->m_recordMode = ValueConversions<String>::fromValue(recordModeValue, errors);
    }
    protocol::Value* includedCategoriesValue = object->get("includedCategories");
    errors->setName("includedCategories");
    result->m_includedCategories = ValueConversions<protocol::Array<String>>::fromValue(includedCategoriesValue, errors);
    errors->pop();
    if (errors->hasErrors())
        return nullptr;
    return result;
}

std::unique_ptr<protocol::DictionaryValue> TraceConfig::toValue() const
{
    std::unique_ptr<protocol::DictionaryValue> result = DictionaryValue::create();
    if (m_recordMode.isJust())
        result->setValue("recordMode", ValueConversions<String>::toValue(m_recordMode.fromJust()));
    result->setValue("includedCategories", ValueConversions<protocol::Array<String>>::toValue(m_includedCategories.get()));
    return result;
}

std::unique_ptr<TraceConfig> TraceConfig::clone() const
{
    ErrorSupport errors;
    return fromValue(toValue().get(), &errors);
}

std::unique_ptr<DataCollectedNotification> DataCollectedNotification::fromValue(protocol::Value* value, ErrorSupport* errors)
{
    if (!value || value->type() != protocol::Value::TypeObject) {
        errors->addError("object expected");
        return nullptr;
    }

    std::unique_ptr<DataCollectedNotification> result(new DataCollectedNotification());
    protocol::DictionaryValue* object = DictionaryValue::cast(value);
    errors->push();
    protocol::Value* valueValue = object->get("value");
    errors->setName("value");
    result->m_value = ValueConversions<protocol::Array<protocol::DictionaryValue>>::fromValue(valueValue, errors);
    errors->pop();
    if (errors->hasErrors())
        return nullptr;
    return result;
}

std::unique_ptr<protocol::DictionaryValue> DataCollectedNotification::toValue() const
{
    std::unique_ptr<protocol::DictionaryValue> result = DictionaryValue::create();
    result->setValue("value", ValueConversions<protocol::Array<protocol::DictionaryValue>>::toValue(m_value.get()));
    return result;
}

std::unique_ptr<DataCollectedNotification> DataCollectedNotification::clone() const
{
    ErrorSupport errors;
    return fromValue(toValue().get(), &errors);
}

// ------------- Enum values from params.


// ------------- Frontend notifications.

void Frontend::dataCollected(std::unique_ptr<protocol::Array<protocol::DictionaryValue>> value)
{
    if (!m_frontendChannel)
        return;
    std::unique_ptr<DataCollectedNotification> messageData = DataCollectedNotification::create()
        .setValue(std::move(value))
        .build();
    m_frontendChannel->sendProtocolNotification(InternalResponse::createNotification("NodeTracing.dataCollected", std::move(messageData)));
}

void Frontend::tracingComplete()
{
    if (!m_frontendChannel)
        return;
    m_frontendChannel->sendProtocolNotification(InternalResponse::createNotification("NodeTracing.tracingComplete"));
}

void Frontend::flush()
{
    m_frontendChannel->flushProtocolNotifications();
}

void Frontend::sendRawNotification(const String& notification)
{
    m_frontendChannel->sendProtocolNotification(InternalRawNotification::create(notification));
}

// --------------------- Dispatcher.

class DispatcherImpl : public protocol::DispatcherBase {
public:
    DispatcherImpl(FrontendChannel* frontendChannel, Backend* backend, bool fallThroughForNotFound)
        : DispatcherBase(frontendChannel)
        , m_backend(backend)
        , m_fallThroughForNotFound(fallThroughForNotFound) {
        m_dispatchMap["NodeTracing.getCategories"] = &DispatcherImpl::getCategories;
        m_dispatchMap["NodeTracing.start"] = &DispatcherImpl::start;
        m_dispatchMap["NodeTracing.stop"] = &DispatcherImpl::stop;
    }
    ~DispatcherImpl() override { }
    DispatchResponse::Status dispatch(int callId, const String& method, std::unique_ptr<protocol::DictionaryValue> messageObject) override;
    HashMap<String, String>& redirects() { return m_redirects; }

protected:
    using CallHandler = DispatchResponse::Status (DispatcherImpl::*)(int callId, std::unique_ptr<DictionaryValue> messageObject, ErrorSupport* errors);
    using DispatchMap = protocol::HashMap<String, CallHandler>;
    DispatchMap m_dispatchMap;
    HashMap<String, String> m_redirects;

    DispatchResponse::Status getCategories(int callId, std::unique_ptr<DictionaryValue> requestMessageObject, ErrorSupport*);
    DispatchResponse::Status start(int callId, std::unique_ptr<DictionaryValue> requestMessageObject, ErrorSupport*);
    DispatchResponse::Status stop(int callId, std::unique_ptr<DictionaryValue> requestMessageObject, ErrorSupport*);

    Backend* m_backend;
    bool m_fallThroughForNotFound;
};

DispatchResponse::Status DispatcherImpl::dispatch(int callId, const String& method, std::unique_ptr<protocol::DictionaryValue> messageObject)
{
    protocol::HashMap<String, CallHandler>::iterator it = m_dispatchMap.find(method);
    if (it == m_dispatchMap.end()) {
        if (m_fallThroughForNotFound)
            return DispatchResponse::kFallThrough;
        reportProtocolError(callId, DispatchResponse::kMethodNotFound, "'" + method + "' wasn't found", nullptr);
        return DispatchResponse::kError;
    }

    protocol::ErrorSupport errors;
    return (this->*(it->second))(callId, std::move(messageObject), &errors);
}


DispatchResponse::Status DispatcherImpl::getCategories(int callId, std::unique_ptr<DictionaryValue> requestMessageObject, ErrorSupport* errors)
{
    // Declare output parameters.
    std::unique_ptr<protocol::Array<String>> out_categories;

    std::unique_ptr<DispatcherBase::WeakPtr> weak = weakPtr();
    DispatchResponse response = m_backend->getCategories(&out_categories);
    if (response.status() == DispatchResponse::kFallThrough)
        return response.status();
    std::unique_ptr<protocol::DictionaryValue> result = DictionaryValue::create();
    if (response.status() == DispatchResponse::kSuccess) {
        result->setValue("categories", ValueConversions<protocol::Array<String>>::toValue(out_categories.get()));
    }
    if (weak->get())
        weak->get()->sendResponse(callId, response, std::move(result));
    return response.status();
}

DispatchResponse::Status DispatcherImpl::start(int callId, std::unique_ptr<DictionaryValue> requestMessageObject, ErrorSupport* errors)
{
    // Prepare input parameters.
    protocol::DictionaryValue* object = DictionaryValue::cast(requestMessageObject->get("params"));
    errors->push();
    protocol::Value* traceConfigValue = object ? object->get("traceConfig") : nullptr;
    errors->setName("traceConfig");
    std::unique_ptr<protocol::NodeTracing::TraceConfig> in_traceConfig = ValueConversions<protocol::NodeTracing::TraceConfig>::fromValue(traceConfigValue, errors);
    errors->pop();
    if (errors->hasErrors()) {
        reportProtocolError(callId, DispatchResponse::kInvalidParams, kInvalidParamsString, errors);
        return DispatchResponse::kError;
    }

    std::unique_ptr<DispatcherBase::WeakPtr> weak = weakPtr();
    DispatchResponse response = m_backend->start(std::move(in_traceConfig));
    if (response.status() == DispatchResponse::kFallThrough)
        return response.status();
    if (weak->get())
        weak->get()->sendResponse(callId, response);
    return response.status();
}

DispatchResponse::Status DispatcherImpl::stop(int callId, std::unique_ptr<DictionaryValue> requestMessageObject, ErrorSupport* errors)
{

    std::unique_ptr<DispatcherBase::WeakPtr> weak = weakPtr();
    DispatchResponse response = m_backend->stop();
    if (response.status() == DispatchResponse::kFallThrough)
        return response.status();
    if (weak->get())
        weak->get()->sendResponse(callId, response);
    return response.status();
}

// static
void Dispatcher::wire(UberDispatcher* uber, Backend* backend)
{
    std::unique_ptr<DispatcherImpl> dispatcher(new DispatcherImpl(uber->channel(), backend, uber->fallThroughForNotFound()));
    uber->setupRedirects(dispatcher->redirects());
    uber->registerBackend("NodeTracing", std::move(dispatcher));
}

} // NodeTracing
} // namespace node
} // namespace inspector
} // namespace protocol
