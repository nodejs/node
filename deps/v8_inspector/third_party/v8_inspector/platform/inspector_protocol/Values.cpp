// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/inspector_protocol/Values.h"

#include "platform/inspector_protocol/Parser.h"
#include "platform/inspector_protocol/String16.h"

#include <algorithm>
#include <cmath>

namespace blink {
namespace protocol {

namespace {

const char* const nullString = "null";
const char* const trueString = "true";
const char* const falseString = "false";

inline bool escapeChar(UChar c, String16Builder* dst)
{
    switch (c) {
    case '\b': dst->append("\\b"); break;
    case '\f': dst->append("\\f"); break;
    case '\n': dst->append("\\n"); break;
    case '\r': dst->append("\\r"); break;
    case '\t': dst->append("\\t"); break;
    case '\\': dst->append("\\\\"); break;
    case '"': dst->append("\\\""); break;
    default:
        return false;
    }
    return true;
}

const LChar hexDigits[17] = "0123456789ABCDEF";

void appendUnsignedAsHex(UChar number, String16Builder* dst)
{
    dst->append("\\u");
    for (size_t i = 0; i < 4; ++i) {
        dst->append(hexDigits[(number & 0xF000) >> 12]);
        number <<= 4;
    }
}

void escapeStringForJSON(const String16& str, String16Builder* dst)
{
    for (unsigned i = 0; i < str.length(); ++i) {
        UChar c = str[i];
        if (!escapeChar(c, dst)) {
            if (c < 32 || c > 126 || c == '<' || c == '>') {
                // 1. Escaping <, > to prevent script execution.
                // 2. Technically, we could also pass through c > 126 as UTF8, but this
                //    is also optional. It would also be a pain to implement here.
                appendUnsignedAsHex(c, dst);
            } else {
                dst->append(c);
            }
        }
    }
}

void doubleQuoteStringForJSON(const String16& str, String16Builder* dst)
{
    dst->append('"');
    escapeStringForJSON(str, dst);
    dst->append('"');
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

bool Value::asString(String16*) const
{
    return false;
}

bool Value::asSerialized(String16*) const
{
    return false;
}

String16 Value::toJSONString() const
{
    String16Builder result;
    result.reserveCapacity(512);
    writeJSON(&result);
    return result.toString();
}

void Value::writeJSON(String16Builder* output) const
{
    DCHECK(m_type == TypeNull);
    output->append(nullString, 4);
}

std::unique_ptr<Value> Value::clone() const
{
    return Value::null();
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

void FundamentalValue::writeJSON(String16Builder* output) const
{
    DCHECK(type() == TypeBoolean || type() == TypeInteger || type() == TypeDouble);
    if (type() == TypeBoolean) {
        if (m_boolValue)
            output->append(trueString, 4);
        else
            output->append(falseString, 5);
    } else if (type() == TypeDouble) {
        if (!std::isfinite(m_doubleValue)) {
            output->append(nullString, 4);
            return;
        }
        output->append(String16::fromDouble(m_doubleValue));
    } else if (type() == TypeInteger) {
        output->append(String16::fromInteger(m_integerValue));
    }
}

std::unique_ptr<Value> FundamentalValue::clone() const
{
    switch (type()) {
    case TypeDouble: return FundamentalValue::create(m_doubleValue);
    case TypeInteger: return FundamentalValue::create(m_integerValue);
    case TypeBoolean: return FundamentalValue::create(m_boolValue);
    default:
        NOTREACHED();
    }
    return nullptr;
}

bool StringValue::asString(String16* output) const
{
    *output = m_stringValue;
    return true;
}

void StringValue::writeJSON(String16Builder* output) const
{
    DCHECK(type() == TypeString);
    doubleQuoteStringForJSON(m_stringValue, output);
}

std::unique_ptr<Value> StringValue::clone() const
{
    return StringValue::create(m_stringValue);
}

bool SerializedValue::asSerialized(String16* output) const
{
    *output = m_serializedValue;
    return true;
}

void SerializedValue::writeJSON(String16Builder* output) const
{
    DCHECK(type() == TypeSerialized);
    output->append(m_serializedValue);
}

std::unique_ptr<Value> SerializedValue::clone() const
{
    return SerializedValue::create(m_serializedValue);
}

DictionaryValue::~DictionaryValue()
{
}

void DictionaryValue::setBoolean(const String16& name, bool value)
{
    setValue(name, FundamentalValue::create(value));
}

void DictionaryValue::setInteger(const String16& name, int value)
{
    setValue(name, FundamentalValue::create(value));
}

void DictionaryValue::setDouble(const String16& name, double value)
{
    setValue(name, FundamentalValue::create(value));
}

void DictionaryValue::setString(const String16& name, const String16& value)
{
    setValue(name, StringValue::create(value));
}

void DictionaryValue::setValue(const String16& name, std::unique_ptr<Value> value)
{
    set(name, value);
}

void DictionaryValue::setObject(const String16& name, std::unique_ptr<DictionaryValue> value)
{
    set(name, value);
}

void DictionaryValue::setArray(const String16& name, std::unique_ptr<ListValue> value)
{
    set(name, value);
}

bool DictionaryValue::getBoolean(const String16& name, bool* output) const
{
    protocol::Value* value = get(name);
    if (!value)
        return false;
    return value->asBoolean(output);
}

bool DictionaryValue::getInteger(const String16& name, int* output) const
{
    Value* value = get(name);
    if (!value)
        return false;
    return value->asInteger(output);
}

bool DictionaryValue::getDouble(const String16& name, double* output) const
{
    Value* value = get(name);
    if (!value)
        return false;
    return value->asDouble(output);
}

bool DictionaryValue::getString(const String16& name, String16* output) const
{
    protocol::Value* value = get(name);
    if (!value)
        return false;
    return value->asString(output);
}

DictionaryValue* DictionaryValue::getObject(const String16& name) const
{
    return DictionaryValue::cast(get(name));
}

protocol::ListValue* DictionaryValue::getArray(const String16& name) const
{
    return ListValue::cast(get(name));
}

protocol::Value* DictionaryValue::get(const String16& name) const
{
    Dictionary::const_iterator it = m_data.find(name);
    if (it == m_data.end())
        return nullptr;
    return it->second.get();
}

DictionaryValue::Entry DictionaryValue::at(size_t index) const
{
    const String16 key = m_order[index];
    return std::make_pair(key, m_data.find(key)->second.get());
}

bool DictionaryValue::booleanProperty(const String16& name, bool defaultValue) const
{
    bool result = defaultValue;
    getBoolean(name, &result);
    return result;
}

int DictionaryValue::integerProperty(const String16& name, int defaultValue) const
{
    int result = defaultValue;
    getInteger(name, &result);
    return result;
}

double DictionaryValue::doubleProperty(const String16& name, double defaultValue) const
{
    double result = defaultValue;
    getDouble(name, &result);
    return result;
}

void DictionaryValue::remove(const String16& name)
{
    m_data.erase(name);
    m_order.erase(std::remove(m_order.begin(), m_order.end(), name), m_order.end());
}

void DictionaryValue::writeJSON(String16Builder* output) const
{
    output->append('{');
    for (size_t i = 0; i < m_order.size(); ++i) {
        Dictionary::const_iterator it = m_data.find(m_order[i]);
        CHECK(it != m_data.end());
        if (i)
            output->append(',');
        doubleQuoteStringForJSON(it->first, output);
        output->append(':');
        it->second->writeJSON(output);
    }
    output->append('}');
}

std::unique_ptr<Value> DictionaryValue::clone() const
{
    std::unique_ptr<DictionaryValue> result = DictionaryValue::create();
    for (size_t i = 0; i < m_order.size(); ++i) {
        String16 key = m_order[i];
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

void ListValue::writeJSON(String16Builder* output) const
{
    output->append('[');
    bool first = true;
    for (const std::unique_ptr<protocol::Value>& value : m_data) {
        if (!first)
            output->append(',');
        value->writeJSON(output);
        first = false;
    }
    output->append(']');
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

} // namespace protocol
} // namespace blink
