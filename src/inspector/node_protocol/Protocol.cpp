// This file is generated.

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "inspector/node_protocol/Protocol.h"

#include <algorithm>
#include <cmath>

#include <cstring>


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#include "ErrorSupport.h"

namespace node {
namespace inspector {
namespace protocol {

ErrorSupport::ErrorSupport() { }
ErrorSupport::~ErrorSupport() { }

void ErrorSupport::setName(const char* name)
{
    setName(String(name));
}

void ErrorSupport::setName(const String& name)
{
    DCHECK(m_path.size());
    m_path[m_path.size() - 1] = name;
}

void ErrorSupport::push()
{
    m_path.push_back(String());
}

void ErrorSupport::pop()
{
    m_path.pop_back();
}

void ErrorSupport::addError(const char* error)
{
    addError(String(error));
}

void ErrorSupport::addError(const String& error)
{
    StringBuilder builder;
    for (size_t i = 0; i < m_path.size(); ++i) {
        if (i)
            StringUtil::builderAppend(builder, '.');
        StringUtil::builderAppend(builder, m_path[i]);
    }
    StringUtil::builderAppend(builder, ": ");
    StringUtil::builderAppend(builder, error);
    m_errors.push_back(StringUtil::builderToString(builder));
}

bool ErrorSupport::hasErrors()
{
    return !!m_errors.size();
}

String ErrorSupport::errors()
{
    StringBuilder builder;
    for (size_t i = 0; i < m_errors.size(); ++i) {
        if (i)
            StringUtil::builderAppend(builder, "; ");
        StringUtil::builderAppend(builder, m_errors[i]);
    }
    return StringUtil::builderToString(builder);
}

} // namespace node
} // namespace inspector
} // namespace protocol


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#include "Values.h"

namespace node {
namespace inspector {
namespace protocol {

namespace {

const char* const nullValueString = "null";
const char* const trueValueString = "true";
const char* const falseValueString = "false";

inline bool escapeChar(uint16_t c, StringBuilder* dst)
{
    switch (c) {
    case '\b': StringUtil::builderAppend(*dst, "\\b"); break;
    case '\f': StringUtil::builderAppend(*dst, "\\f"); break;
    case '\n': StringUtil::builderAppend(*dst, "\\n"); break;
    case '\r': StringUtil::builderAppend(*dst, "\\r"); break;
    case '\t': StringUtil::builderAppend(*dst, "\\t"); break;
    case '\\': StringUtil::builderAppend(*dst, "\\\\"); break;
    case '"': StringUtil::builderAppend(*dst, "\\\""); break;
    default:
        return false;
    }
    return true;
}

const char hexDigits[17] = "0123456789ABCDEF";

void appendUnsignedAsHex(uint16_t number, StringBuilder* dst)
{
    StringUtil::builderAppend(*dst, "\\u");
    for (size_t i = 0; i < 4; ++i) {
        uint16_t c = hexDigits[(number & 0xF000) >> 12];
        StringUtil::builderAppend(*dst, c);
        number <<= 4;
    }
}

template <typename Char>
void escapeStringForJSONInternal(const Char* str, unsigned len,
                                 StringBuilder* dst)
{
    for (unsigned i = 0; i < len; ++i) {
        Char c = str[i];
        if (escapeChar(c, dst))
            continue;
        if (c < 32 || c > 126) {
            appendUnsignedAsHex(c, dst);
        } else {
            StringUtil::builderAppend(*dst, c);
        }
    }
}

} // anonymous namespace

bool Value::asBoolean(bool*) const
{
    return false;
}

bool Value::asDouble(double*) const
{
    return false;
}

bool Value::asInteger(int*) const
{
    return false;
}

bool Value::asString(String*) const
{
    return false;
}

bool Value::asSerialized(String*) const
{
    return false;
}

void Value::writeJSON(StringBuilder* output) const
{
    DCHECK(m_type == TypeNull);
    StringUtil::builderAppend(*output, nullValueString, 4);
}

std::unique_ptr<Value> Value::clone() const
{
    return Value::null();
}

String Value::serialize()
{
    StringBuilder result;
    StringUtil::builderReserve(result, 512);
    writeJSON(&result);
    return StringUtil::builderToString(result);
}

bool FundamentalValue::asBoolean(bool* output) const
{
    if (type() != TypeBoolean)
        return false;
    *output = m_boolValue;
    return true;
}

bool FundamentalValue::asDouble(double* output) const
{
    if (type() == TypeDouble) {
        *output = m_doubleValue;
        return true;
    }
    if (type() == TypeInteger) {
        *output = m_integerValue;
        return true;
    }
    return false;
}

bool FundamentalValue::asInteger(int* output) const
{
    if (type() != TypeInteger)
        return false;
    *output = m_integerValue;
    return true;
}

void FundamentalValue::writeJSON(StringBuilder* output) const
{
    DCHECK(type() == TypeBoolean || type() == TypeInteger || type() == TypeDouble);
    if (type() == TypeBoolean) {
        if (m_boolValue)
            StringUtil::builderAppend(*output, trueValueString, 4);
        else
            StringUtil::builderAppend(*output, falseValueString, 5);
    } else if (type() == TypeDouble) {
        if (!std::isfinite(m_doubleValue)) {
            StringUtil::builderAppend(*output, nullValueString, 4);
            return;
        }
        StringUtil::builderAppend(*output, StringUtil::fromDouble(m_doubleValue));
    } else if (type() == TypeInteger) {
        StringUtil::builderAppend(*output, StringUtil::fromInteger(m_integerValue));
    }
}

std::unique_ptr<Value> FundamentalValue::clone() const
{
    switch (type()) {
    case TypeDouble: return FundamentalValue::create(m_doubleValue);
    case TypeInteger: return FundamentalValue::create(m_integerValue);
    case TypeBoolean: return FundamentalValue::create(m_boolValue);
    default:
        DCHECK(false);
    }
    return nullptr;
}

bool StringValue::asString(String* output) const
{
    *output = m_stringValue;
    return true;
}

void StringValue::writeJSON(StringBuilder* output) const
{
    DCHECK(type() == TypeString);
    StringUtil::builderAppendQuotedString(*output, m_stringValue);
}

std::unique_ptr<Value> StringValue::clone() const
{
    return StringValue::create(m_stringValue);
}

bool SerializedValue::asSerialized(String* output) const
{
    *output = m_serializedValue;
    return true;
}

void SerializedValue::writeJSON(StringBuilder* output) const
{
    DCHECK(type() == TypeSerialized);
    StringUtil::builderAppend(*output, m_serializedValue);
}

std::unique_ptr<Value> SerializedValue::clone() const
{
    return SerializedValue::create(m_serializedValue);
}

DictionaryValue::~DictionaryValue()
{
}

void DictionaryValue::setBoolean(const String& name, bool value)
{
    setValue(name, FundamentalValue::create(value));
}

void DictionaryValue::setInteger(const String& name, int value)
{
    setValue(name, FundamentalValue::create(value));
}

void DictionaryValue::setDouble(const String& name, double value)
{
    setValue(name, FundamentalValue::create(value));
}

void DictionaryValue::setString(const String& name, const String& value)
{
    setValue(name, StringValue::create(value));
}

void DictionaryValue::setValue(const String& name, std::unique_ptr<Value> value)
{
    set(name, value);
}

void DictionaryValue::setObject(const String& name, std::unique_ptr<DictionaryValue> value)
{
    set(name, value);
}

void DictionaryValue::setArray(const String& name, std::unique_ptr<ListValue> value)
{
    set(name, value);
}

bool DictionaryValue::getBoolean(const String& name, bool* output) const
{
    protocol::Value* value = get(name);
    if (!value)
        return false;
    return value->asBoolean(output);
}

bool DictionaryValue::getInteger(const String& name, int* output) const
{
    Value* value = get(name);
    if (!value)
        return false;
    return value->asInteger(output);
}

bool DictionaryValue::getDouble(const String& name, double* output) const
{
    Value* value = get(name);
    if (!value)
        return false;
    return value->asDouble(output);
}

bool DictionaryValue::getString(const String& name, String* output) const
{
    protocol::Value* value = get(name);
    if (!value)
        return false;
    return value->asString(output);
}

DictionaryValue* DictionaryValue::getObject(const String& name) const
{
    return DictionaryValue::cast(get(name));
}

protocol::ListValue* DictionaryValue::getArray(const String& name) const
{
    return ListValue::cast(get(name));
}

protocol::Value* DictionaryValue::get(const String& name) const
{
    Dictionary::const_iterator it = m_data.find(name);
    if (it == m_data.end())
        return nullptr;
    return it->second.get();
}

DictionaryValue::Entry DictionaryValue::at(size_t index) const
{
    const String key = m_order[index];
    return std::make_pair(key, m_data.find(key)->second.get());
}

bool DictionaryValue::booleanProperty(const String& name, bool defaultValue) const
{
    bool result = defaultValue;
    getBoolean(name, &result);
    return result;
}

int DictionaryValue::integerProperty(const String& name, int defaultValue) const
{
    int result = defaultValue;
    getInteger(name, &result);
    return result;
}

double DictionaryValue::doubleProperty(const String& name, double defaultValue) const
{
    double result = defaultValue;
    getDouble(name, &result);
    return result;
}

void DictionaryValue::remove(const String& name)
{
    m_data.erase(name);
    m_order.erase(std::remove(m_order.begin(), m_order.end(), name), m_order.end());
}

void DictionaryValue::writeJSON(StringBuilder* output) const
{
    StringUtil::builderAppend(*output, '{');
    for (size_t i = 0; i < m_order.size(); ++i) {
        Dictionary::const_iterator it = m_data.find(m_order[i]);
        CHECK(it != m_data.end());
        if (i)
            StringUtil::builderAppend(*output, ',');
        StringUtil::builderAppendQuotedString(*output, it->first);
        StringUtil::builderAppend(*output, ':');
        it->second->writeJSON(output);
    }
    StringUtil::builderAppend(*output, '}');
}

std::unique_ptr<Value> DictionaryValue::clone() const
{
    std::unique_ptr<DictionaryValue> result = DictionaryValue::create();
    for (size_t i = 0; i < m_order.size(); ++i) {
        String key = m_order[i];
        Dictionary::const_iterator value = m_data.find(key);
        DCHECK(value != m_data.cend() && value->second);
        result->setValue(key, value->second->clone());
    }
    return std::move(result);
}

DictionaryValue::DictionaryValue()
    : Value(TypeObject)
{
}

ListValue::~ListValue()
{
}

void ListValue::writeJSON(StringBuilder* output) const
{
    StringUtil::builderAppend(*output, '[');
    bool first = true;
    for (const std::unique_ptr<protocol::Value>& value : m_data) {
        if (!first)
            StringUtil::builderAppend(*output, ',');
        value->writeJSON(output);
        first = false;
    }
    StringUtil::builderAppend(*output, ']');
}

std::unique_ptr<Value> ListValue::clone() const
{
    std::unique_ptr<ListValue> result = ListValue::create();
    for (const std::unique_ptr<protocol::Value>& value : m_data)
        result->pushValue(value->clone());
    return std::move(result);
}

ListValue::ListValue()
    : Value(TypeArray)
{
}

void ListValue::pushValue(std::unique_ptr<protocol::Value> value)
{
    DCHECK(value);
    m_data.push_back(std::move(value));
}

protocol::Value* ListValue::at(size_t index)
{
    DCHECK_LT(index, m_data.size());
    return m_data[index].get();
}

void escapeLatinStringForJSON(const uint8_t* str, unsigned len, StringBuilder* dst)
{
    escapeStringForJSONInternal<uint8_t>(str, len, dst);
}

void escapeWideStringForJSON(const uint16_t* str, unsigned len, StringBuilder* dst)
{
    escapeStringForJSONInternal<uint16_t>(str, len, dst);
}

} // namespace node
} // namespace inspector
} // namespace protocol


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#include "Object.h"

namespace node {
namespace inspector {
namespace protocol {

std::unique_ptr<Object> Object::fromValue(protocol::Value* value, ErrorSupport* errors)
{
    protocol::DictionaryValue* dictionary = DictionaryValue::cast(value);
    if (!dictionary) {
        errors->addError("object expected");
        return nullptr;
    }
    dictionary = static_cast<protocol::DictionaryValue*>(dictionary->clone().release());
    return std::unique_ptr<Object>(new Object(std::unique_ptr<DictionaryValue>(dictionary)));
}

std::unique_ptr<protocol::DictionaryValue> Object::toValue() const
{
    return DictionaryValue::cast(m_object->clone());
}

std::unique_ptr<Object> Object::clone() const
{
    return std::unique_ptr<Object>(new Object(DictionaryValue::cast(m_object->clone())));
}

Object::Object(std::unique_ptr<protocol::DictionaryValue> object) : m_object(std::move(object)) { }

Object::~Object() { }

} // namespace node
} // namespace inspector
} // namespace protocol


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#include "DispatcherBase.h"
//#include "Parser.h"

namespace node {
namespace inspector {
namespace protocol {

// static
DispatchResponse DispatchResponse::OK()
{
    DispatchResponse result;
    result.m_status = kSuccess;
    result.m_errorCode = kParseError;
    return result;
}

// static
DispatchResponse DispatchResponse::Error(const String& error)
{
    DispatchResponse result;
    result.m_status = kError;
    result.m_errorCode = kServerError;
    result.m_errorMessage = error;
    return result;
}

// static
DispatchResponse DispatchResponse::InternalError()
{
    DispatchResponse result;
    result.m_status = kError;
    result.m_errorCode = kInternalError;
    result.m_errorMessage = "Internal error";
    return result;
}

// static
DispatchResponse DispatchResponse::InvalidParams(const String& error)
{
    DispatchResponse result;
    result.m_status = kError;
    result.m_errorCode = kInvalidParams;
    result.m_errorMessage = error;
    return result;
}

// static
DispatchResponse DispatchResponse::FallThrough()
{
    DispatchResponse result;
    result.m_status = kFallThrough;
    result.m_errorCode = kParseError;
    return result;
}

// static
const char DispatcherBase::kInvalidParamsString[] = "Invalid parameters";

DispatcherBase::WeakPtr::WeakPtr(DispatcherBase* dispatcher) : m_dispatcher(dispatcher) { }

DispatcherBase::WeakPtr::~WeakPtr()
{
    if (m_dispatcher)
        m_dispatcher->m_weakPtrs.erase(this);
}

DispatcherBase::Callback::Callback(std::unique_ptr<DispatcherBase::WeakPtr> backendImpl, int callId, int callbackId)
    : m_backendImpl(std::move(backendImpl))
    , m_callId(callId)
    , m_callbackId(callbackId) { }

DispatcherBase::Callback::~Callback() = default;

void DispatcherBase::Callback::dispose()
{
    m_backendImpl = nullptr;
}

void DispatcherBase::Callback::sendIfActive(std::unique_ptr<protocol::DictionaryValue> partialMessage, const DispatchResponse& response)
{
    if (!m_backendImpl || !m_backendImpl->get())
        return;
    m_backendImpl->get()->sendResponse(m_callId, response, std::move(partialMessage));
    m_backendImpl = nullptr;
}

void DispatcherBase::Callback::fallThroughIfActive()
{
    if (!m_backendImpl || !m_backendImpl->get())
        return;
    m_backendImpl->get()->markFallThrough(m_callbackId);
    m_backendImpl = nullptr;
}

DispatcherBase::DispatcherBase(FrontendChannel* frontendChannel)
    : m_frontendChannel(frontendChannel)
    , m_lastCallbackId(0)
    , m_lastCallbackFallThrough(false) { }

DispatcherBase::~DispatcherBase()
{
    clearFrontend();
}

int DispatcherBase::nextCallbackId()
{
    m_lastCallbackFallThrough = false;
    return ++m_lastCallbackId;
}

void DispatcherBase::markFallThrough(int callbackId)
{
    DCHECK(callbackId == m_lastCallbackId);
    m_lastCallbackFallThrough = true;
}

void DispatcherBase::sendResponse(int callId, const DispatchResponse& response, std::unique_ptr<protocol::DictionaryValue> result)
{
    if (!m_frontendChannel)
        return;
    if (response.status() == DispatchResponse::kError) {
        reportProtocolError(callId, response.errorCode(), response.errorMessage(), nullptr);
        return;
    }
    m_frontendChannel->sendProtocolResponse(callId, InternalResponse::createResponse(callId, std::move(result)));
}

void DispatcherBase::sendResponse(int callId, const DispatchResponse& response)
{
    sendResponse(callId, response, DictionaryValue::create());
}

namespace {

class ProtocolError : public Serializable {
public:
    static std::unique_ptr<ProtocolError> createErrorResponse(int callId, DispatchResponse::ErrorCode code, const String& errorMessage, ErrorSupport* errors)
    {
        std::unique_ptr<ProtocolError> protocolError(new ProtocolError(code, errorMessage));
        protocolError->m_callId = callId;
        protocolError->m_hasCallId = true;
        if (errors && errors->hasErrors())
            protocolError->m_data = errors->errors();
        return protocolError;
    }

    static std::unique_ptr<ProtocolError> createErrorNotification(DispatchResponse::ErrorCode code, const String& errorMessage)
    {
        return std::unique_ptr<ProtocolError>(new ProtocolError(code, errorMessage));
    }

    String serialize() override
    {
        std::unique_ptr<protocol::DictionaryValue> error = DictionaryValue::create();
        error->setInteger("code", m_code);
        error->setString("message", m_errorMessage);
        if (m_data.length())
            error->setString("data", m_data);
        std::unique_ptr<protocol::DictionaryValue> message = DictionaryValue::create();
        message->setObject("error", std::move(error));
        if (m_hasCallId)
            message->setInteger("id", m_callId);
        return message->serialize();
    }

    ~ProtocolError() override {}

private:
    ProtocolError(DispatchResponse::ErrorCode code, const String& errorMessage)
        : m_code(code)
        , m_errorMessage(errorMessage)
    {
    }

    DispatchResponse::ErrorCode m_code;
    String m_errorMessage;
    String m_data;
    int m_callId = 0;
    bool m_hasCallId = false;
};

} // namespace

static void reportProtocolErrorTo(FrontendChannel* frontendChannel, int callId, DispatchResponse::ErrorCode code, const String& errorMessage, ErrorSupport* errors)
{
    if (frontendChannel)
        frontendChannel->sendProtocolResponse(callId, ProtocolError::createErrorResponse(callId, code, errorMessage, errors));
}

static void reportProtocolErrorTo(FrontendChannel* frontendChannel, DispatchResponse::ErrorCode code, const String& errorMessage)
{
    if (frontendChannel)
        frontendChannel->sendProtocolNotification(ProtocolError::createErrorNotification(code, errorMessage));
}

void DispatcherBase::reportProtocolError(int callId, DispatchResponse::ErrorCode code, const String& errorMessage, ErrorSupport* errors)
{
    reportProtocolErrorTo(m_frontendChannel, callId, code, errorMessage, errors);
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
    : m_frontendChannel(frontendChannel)
    , m_fallThroughForNotFound(false) { }

void UberDispatcher::setFallThroughForNotFound(bool fallThroughForNotFound)
{
    m_fallThroughForNotFound = fallThroughForNotFound;
}

void UberDispatcher::registerBackend(const String& name, std::unique_ptr<protocol::DispatcherBase> dispatcher)
{
    m_dispatchers[name] = std::move(dispatcher);
}

void UberDispatcher::setupRedirects(const HashMap<String, String>& redirects)
{
    for (const auto& pair : redirects)
        m_redirects[pair.first] = pair.second;
}

DispatchResponse::Status UberDispatcher::dispatch(std::unique_ptr<Value> parsedMessage, int* outCallId, String* outMethod)
{
    if (!parsedMessage) {
        reportProtocolErrorTo(m_frontendChannel, DispatchResponse::kParseError, "Message must be a valid JSON");
        return DispatchResponse::kError;
    }
    std::unique_ptr<protocol::DictionaryValue> messageObject = DictionaryValue::cast(std::move(parsedMessage));
    if (!messageObject) {
        reportProtocolErrorTo(m_frontendChannel, DispatchResponse::kInvalidRequest, "Message must be an object");
        return DispatchResponse::kError;
    }

    int callId = 0;
    protocol::Value* callIdValue = messageObject->get("id");
    bool success = callIdValue && callIdValue->asInteger(&callId);
    if (outCallId)
        *outCallId = callId;
    if (!success) {
        reportProtocolErrorTo(m_frontendChannel, DispatchResponse::kInvalidRequest, "Message must have integer 'id' property");
        return DispatchResponse::kError;
    }

    protocol::Value* methodValue = messageObject->get("method");
    String method;
    success = methodValue && methodValue->asString(&method);
    if (outMethod)
        *outMethod = method;
    if (!success) {
        reportProtocolErrorTo(m_frontendChannel, callId, DispatchResponse::kInvalidRequest, "Message must have string 'method' property", nullptr);
        return DispatchResponse::kError;
    }

    HashMap<String, String>::iterator redirectIt = m_redirects.find(method);
    if (redirectIt != m_redirects.end())
        method = redirectIt->second;

    size_t dotIndex = StringUtil::find(method, ".");
    if (dotIndex == StringUtil::kNotFound) {
        if (m_fallThroughForNotFound)
            return DispatchResponse::kFallThrough;
        reportProtocolErrorTo(m_frontendChannel, callId, DispatchResponse::kMethodNotFound, "'" + method + "' wasn't found", nullptr);
        return DispatchResponse::kError;
    }
    String domain = StringUtil::substring(method, 0, dotIndex);
    auto it = m_dispatchers.find(domain);
    if (it == m_dispatchers.end()) {
        if (m_fallThroughForNotFound)
            return DispatchResponse::kFallThrough;
        reportProtocolErrorTo(m_frontendChannel, callId, DispatchResponse::kMethodNotFound, "'" + method + "' wasn't found", nullptr);
        return DispatchResponse::kError;
    }
    return it->second->dispatch(callId, method, std::move(messageObject));
}

bool UberDispatcher::getCommandName(const String& message, String* method, std::unique_ptr<protocol::DictionaryValue>* parsedMessage)
{
    std::unique_ptr<protocol::Value> value = StringUtil::parseJSON(message);
    if (!value) {
        reportProtocolErrorTo(m_frontendChannel, DispatchResponse::kParseError, "Message must be a valid JSON");
        return false;
   }

    protocol::DictionaryValue* object = DictionaryValue::cast(value.get());
    if (!object) {
        reportProtocolErrorTo(m_frontendChannel, DispatchResponse::kInvalidRequest, "Message must be an object");
        return false;
    }

    if (!object->getString("method", method)) {
        reportProtocolErrorTo(m_frontendChannel, DispatchResponse::kInvalidRequest, "Message must have string 'method' property");
        return false;
    }

    parsedMessage->reset(DictionaryValue::cast(value.release()));
    return true;
}

UberDispatcher::~UberDispatcher() = default;

// static
std::unique_ptr<InternalResponse> InternalResponse::createResponse(int callId, std::unique_ptr<Serializable> params)
{
    return std::unique_ptr<InternalResponse>(new InternalResponse(callId, String(), std::move(params)));
}

// static
std::unique_ptr<InternalResponse> InternalResponse::createNotification(const String& notification, std::unique_ptr<Serializable> params)
{
    return std::unique_ptr<InternalResponse>(new InternalResponse(0, notification, std::move(params)));
}

String InternalResponse::serialize()
{
    std::unique_ptr<DictionaryValue> result = DictionaryValue::create();
    std::unique_ptr<Serializable> params(m_params ? std::move(m_params) : DictionaryValue::create());
    if (m_notification.length()) {
        result->setString("method", m_notification);
        result->setValue("params", SerializedValue::create(params->serialize()));
    } else {
        result->setInteger("id", m_callId);
        result->setValue("result", SerializedValue::create(params->serialize()));
    }
    return result->serialize();
}

InternalResponse::InternalResponse(int callId, const String& notification, std::unique_ptr<Serializable> params)
    : m_callId(callId)
    , m_notification(notification)
    , m_params(params ? std::move(params) : nullptr)
{
}

} // namespace node
} // namespace inspector
} // namespace protocol


// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace node {
namespace inspector {
namespace protocol {

namespace {

const int stackLimit = 1000;

enum Token {
    ObjectBegin,
    ObjectEnd,
    ArrayBegin,
    ArrayEnd,
    StringLiteral,
    Number,
    BoolTrue,
    BoolFalse,
    NullToken,
    ListSeparator,
    ObjectPairSeparator,
    InvalidToken,
};

const char* const nullString = "null";
const char* const trueString = "true";
const char* const falseString = "false";

bool isASCII(uint16_t c)
{
    return !(c & ~0x7F);
}

bool isSpaceOrNewLine(uint16_t c)
{
    return isASCII(c) && c <= ' ' && (c == ' ' || (c <= 0xD && c >= 0x9));
}

double charactersToDouble(const uint16_t* characters, size_t length, bool* ok)
{
    std::vector<char> buffer;
    buffer.reserve(length + 1);
    for (size_t i = 0; i < length; ++i) {
        if (!isASCII(characters[i])) {
            *ok = false;
            return 0;
        }
        buffer.push_back(static_cast<char>(characters[i]));
    }
    buffer.push_back('\0');
    return StringUtil::toDouble(buffer.data(), length, ok);
}

double charactersToDouble(const uint8_t* characters, size_t length, bool* ok)
{
    std::string buffer(reinterpret_cast<const char*>(characters), length);
    return StringUtil::toDouble(buffer.data(), length, ok);
}

template<typename Char>
bool parseConstToken(const Char* start, const Char* end, const Char** tokenEnd, const char* token)
{
    while (start < end && *token != '\0' && *start++ == *token++) { }
    if (*token != '\0')
        return false;
    *tokenEnd = start;
    return true;
}

template<typename Char>
bool readInt(const Char* start, const Char* end, const Char** tokenEnd, bool canHaveLeadingZeros)
{
    if (start == end)
        return false;
    bool haveLeadingZero = '0' == *start;
    int length = 0;
    while (start < end && '0' <= *start && *start <= '9') {
        ++start;
        ++length;
    }
    if (!length)
        return false;
    if (!canHaveLeadingZeros && length > 1 && haveLeadingZero)
        return false;
    *tokenEnd = start;
    return true;
}

template<typename Char>
bool parseNumberToken(const Char* start, const Char* end, const Char** tokenEnd)
{
    // We just grab the number here. We validate the size in DecodeNumber.
    // According to RFC4627, a valid number is: [minus] int [frac] [exp]
    if (start == end)
        return false;
    Char c = *start;
    if ('-' == c)
        ++start;

    if (!readInt(start, end, &start, false))
        return false;
    if (start == end) {
        *tokenEnd = start;
        return true;
    }

    // Optional fraction part
    c = *start;
    if ('.' == c) {
        ++start;
        if (!readInt(start, end, &start, true))
            return false;
        if (start == end) {
            *tokenEnd = start;
            return true;
        }
        c = *start;
    }

    // Optional exponent part
    if ('e' == c || 'E' == c) {
        ++start;
        if (start == end)
            return false;
        c = *start;
        if ('-' == c || '+' == c) {
            ++start;
            if (start == end)
                return false;
        }
        if (!readInt(start, end, &start, true))
            return false;
    }

    *tokenEnd = start;
    return true;
}

template<typename Char>
bool readHexDigits(const Char* start, const Char* end, const Char** tokenEnd, int digits)
{
    if (end - start < digits)
        return false;
    for (int i = 0; i < digits; ++i) {
        Char c = *start++;
        if (!(('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F')))
            return false;
    }
    *tokenEnd = start;
    return true;
}

template<typename Char>
bool parseStringToken(const Char* start, const Char* end, const Char** tokenEnd)
{
    while (start < end) {
        Char c = *start++;
        if ('\\' == c) {
	    if (start == end)
	        return false;
            c = *start++;
            // Make sure the escaped char is valid.
            switch (c) {
            case 'x':
                if (!readHexDigits(start, end, &start, 2))
                    return false;
                break;
            case 'u':
                if (!readHexDigits(start, end, &start, 4))
                    return false;
                break;
            case '\\':
            case '/':
            case 'b':
            case 'f':
            case 'n':
            case 'r':
            case 't':
            case 'v':
            case '"':
                break;
            default:
                return false;
            }
        } else if ('"' == c) {
            *tokenEnd = start;
            return true;
        }
    }
    return false;
}

template<typename Char>
bool skipComment(const Char* start, const Char* end, const Char** commentEnd)
{
    if (start == end)
        return false;

    if (*start != '/' || start + 1 >= end)
        return false;
    ++start;

    if (*start == '/') {
        // Single line comment, read to newline.
        for (++start; start < end; ++start) {
            if (*start == '\n' || *start == '\r') {
                *commentEnd = start + 1;
                return true;
            }
        }
        *commentEnd = end;
        // Comment reaches end-of-input, which is fine.
        return true;
    }

    if (*start == '*') {
        Char previous = '\0';
        // Block comment, read until end marker.
        for (++start; start < end; previous = *start++) {
            if (previous == '*' && *start == '/') {
                *commentEnd = start + 1;
                return true;
            }
        }
        // Block comment must close before end-of-input.
        return false;
    }

    return false;
}

template<typename Char>
void skipWhitespaceAndComments(const Char* start, const Char* end, const Char** whitespaceEnd)
{
    while (start < end) {
        if (isSpaceOrNewLine(*start)) {
            ++start;
        } else if (*start == '/') {
            const Char* commentEnd;
            if (!skipComment(start, end, &commentEnd))
                break;
            start = commentEnd;
        } else {
            break;
        }
    }
    *whitespaceEnd = start;
}

template<typename Char>
Token parseToken(const Char* start, const Char* end, const Char** tokenStart, const Char** tokenEnd)
{
    skipWhitespaceAndComments(start, end, tokenStart);
    start = *tokenStart;

    if (start == end)
        return InvalidToken;

    switch (*start) {
    case 'n':
        if (parseConstToken(start, end, tokenEnd, nullString))
            return NullToken;
        break;
    case 't':
        if (parseConstToken(start, end, tokenEnd, trueString))
            return BoolTrue;
        break;
    case 'f':
        if (parseConstToken(start, end, tokenEnd, falseString))
            return BoolFalse;
        break;
    case '[':
        *tokenEnd = start + 1;
        return ArrayBegin;
    case ']':
        *tokenEnd = start + 1;
        return ArrayEnd;
    case ',':
        *tokenEnd = start + 1;
        return ListSeparator;
    case '{':
        *tokenEnd = start + 1;
        return ObjectBegin;
    case '}':
        *tokenEnd = start + 1;
        return ObjectEnd;
    case ':':
        *tokenEnd = start + 1;
        return ObjectPairSeparator;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
        if (parseNumberToken(start, end, tokenEnd))
            return Number;
        break;
    case '"':
        if (parseStringToken(start + 1, end, tokenEnd))
            return StringLiteral;
        break;
    }
    return InvalidToken;
}

template<typename Char>
int hexToInt(Char c)
{
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('A' <= c && c <= 'F')
        return c - 'A' + 10;
    if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    DCHECK(false);
    return 0;
}

template<typename Char>
bool decodeString(const Char* start, const Char* end, StringBuilder* output)
{
    while (start < end) {
        uint16_t c = *start++;
        if ('\\' != c) {
            StringUtil::builderAppend(*output, c);
            continue;
        }
	if (start == end)
	    return false;
        c = *start++;

        if (c == 'x') {
            // \x is not supported.
            return false;
        }

        switch (c) {
        case '"':
        case '/':
        case '\\':
            break;
        case 'b':
            c = '\b';
            break;
        case 'f':
            c = '\f';
            break;
        case 'n':
            c = '\n';
            break;
        case 'r':
            c = '\r';
            break;
        case 't':
            c = '\t';
            break;
        case 'v':
            c = '\v';
            break;
        case 'u':
            c = (hexToInt(*start) << 12) +
                (hexToInt(*(start + 1)) << 8) +
                (hexToInt(*(start + 2)) << 4) +
                hexToInt(*(start + 3));
            start += 4;
            break;
        default:
            return false;
        }
        StringUtil::builderAppend(*output, c);
    }
    return true;
}

template<typename Char>
bool decodeString(const Char* start, const Char* end, String* output)
{
    if (start == end) {
        *output = "";
        return true;
    }
    if (start > end)
        return false;
    StringBuilder buffer;
    StringUtil::builderReserve(buffer, end - start);
    if (!decodeString(start, end, &buffer))
        return false;
    *output = StringUtil::builderToString(buffer);
    return true;
}

template<typename Char>
std::unique_ptr<Value> buildValue(const Char* start, const Char* end, const Char** valueTokenEnd, int depth)
{
    if (depth > stackLimit)
        return nullptr;

    std::unique_ptr<Value> result;
    const Char* tokenStart;
    const Char* tokenEnd;
    Token token = parseToken(start, end, &tokenStart, &tokenEnd);
    switch (token) {
    case InvalidToken:
        return nullptr;
    case NullToken:
        result = Value::null();
        break;
    case BoolTrue:
        result = FundamentalValue::create(true);
        break;
    case BoolFalse:
        result = FundamentalValue::create(false);
        break;
    case Number: {
        bool ok;
        double value = charactersToDouble(tokenStart, tokenEnd - tokenStart, &ok);
        if (!ok)
            return nullptr;
        int number = static_cast<int>(value);
        if (number == value)
            result = FundamentalValue::create(number);
        else
            result = FundamentalValue::create(value);
        break;
    }
    case StringLiteral: {
        String value;
        bool ok = decodeString(tokenStart + 1, tokenEnd - 1, &value);
        if (!ok)
            return nullptr;
        result = StringValue::create(value);
        break;
    }
    case ArrayBegin: {
        std::unique_ptr<ListValue> array = ListValue::create();
        start = tokenEnd;
        token = parseToken(start, end, &tokenStart, &tokenEnd);
        while (token != ArrayEnd) {
            std::unique_ptr<Value> arrayNode = buildValue(start, end, &tokenEnd, depth + 1);
            if (!arrayNode)
                return nullptr;
            array->pushValue(std::move(arrayNode));

            // After a list value, we expect a comma or the end of the list.
            start = tokenEnd;
            token = parseToken(start, end, &tokenStart, &tokenEnd);
            if (token == ListSeparator) {
                start = tokenEnd;
                token = parseToken(start, end, &tokenStart, &tokenEnd);
                if (token == ArrayEnd)
                    return nullptr;
            } else if (token != ArrayEnd) {
                // Unexpected value after list value. Bail out.
                return nullptr;
            }
        }
        if (token != ArrayEnd)
            return nullptr;
        result = std::move(array);
        break;
    }
    case ObjectBegin: {
        std::unique_ptr<DictionaryValue> object = DictionaryValue::create();
        start = tokenEnd;
        token = parseToken(start, end, &tokenStart, &tokenEnd);
        while (token != ObjectEnd) {
            if (token != StringLiteral)
                return nullptr;
            String key;
            if (!decodeString(tokenStart + 1, tokenEnd - 1, &key))
                return nullptr;
            start = tokenEnd;

            token = parseToken(start, end, &tokenStart, &tokenEnd);
            if (token != ObjectPairSeparator)
                return nullptr;
            start = tokenEnd;

            std::unique_ptr<Value> value = buildValue(start, end, &tokenEnd, depth + 1);
            if (!value)
                return nullptr;
            object->setValue(key, std::move(value));
            start = tokenEnd;

            // After a key/value pair, we expect a comma or the end of the
            // object.
            token = parseToken(start, end, &tokenStart, &tokenEnd);
            if (token == ListSeparator) {
                start = tokenEnd;
                token = parseToken(start, end, &tokenStart, &tokenEnd);
                if (token == ObjectEnd)
                    return nullptr;
            } else if (token != ObjectEnd) {
                // Unexpected value after last object value. Bail out.
                return nullptr;
            }
        }
        if (token != ObjectEnd)
            return nullptr;
        result = std::move(object);
        break;
    }

    default:
        // We got a token that's not a value.
        return nullptr;
    }

    skipWhitespaceAndComments(tokenEnd, end, valueTokenEnd);
    return result;
}

template<typename Char>
std::unique_ptr<Value> parseJSONInternal(const Char* start, unsigned length)
{
    const Char* end = start + length;
    const Char *tokenEnd;
    std::unique_ptr<Value> value = buildValue(start, end, &tokenEnd, 0);
    if (!value || tokenEnd != end)
        return nullptr;
    return value;
}

} // anonymous namespace

std::unique_ptr<Value> parseJSONCharacters(const uint16_t* characters, unsigned length)
{
    return parseJSONInternal<uint16_t>(characters, length);
}

std::unique_ptr<Value> parseJSONCharacters(const uint8_t* characters, unsigned length)
{
    return parseJSONInternal<uint8_t>(characters, length);
}

} // namespace node
} // namespace inspector
} // namespace protocol
