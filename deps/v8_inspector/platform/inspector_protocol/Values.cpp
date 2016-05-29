// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/inspector_protocol/Values.h"

#include "platform/inspector_protocol/Parser.h"
#include "platform/inspector_protocol/String16.h"
#include "wtf/Assertions.h"
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

bool Value::asNumber(double*) const
{
    return false;
}

bool Value::asNumber(int*) const
{
    return false;
}

bool Value::asString(String16*) const
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

bool FundamentalValue::asNumber(double* output) const
{
    if (type() != TypeNumber)
        return false;
    *output = m_doubleValue;
    return true;
}

bool FundamentalValue::asNumber(int* output) const
{
    if (type() != TypeNumber)
        return false;
    *output = static_cast<int>(m_doubleValue);
    return true;
}

void FundamentalValue::writeJSON(String16Builder* output) const
{
    DCHECK(type() == TypeBoolean || type() == TypeNumber);
    if (type() == TypeBoolean) {
        if (m_boolValue)
            output->append(trueString, 4);
        else
            output->append(falseString, 5);
    } else if (type() == TypeNumber) {
        if (!std::isfinite(m_doubleValue)) {
            output->append(nullString, 4);
            return;
        }
        output->append(String16::fromDouble(m_doubleValue));
    }
}

std::unique_ptr<Value> FundamentalValue::clone() const
{
    return type() == TypeNumber ? FundamentalValue::create(m_doubleValue) : FundamentalValue::create(m_boolValue);
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

DictionaryValue::~DictionaryValue()
{
}

void DictionaryValue::setBoolean(const String16& name, bool value)
{
    setValue(name, FundamentalValue::create(value));
}

void DictionaryValue::setNumber(const String16& name, double value)
{
    setValue(name, FundamentalValue::create(value));
}

void DictionaryValue::setString(const String16& name, const String16& value)
{
    setValue(name, StringValue::create(value));
}

void DictionaryValue::setValue(const String16& name, std::unique_ptr<Value> value)
{
    DCHECK(value);
    if (m_data.set(name, std::move(value)))
        m_order.append(name);
}

void DictionaryValue::setObject(const String16& name, std::unique_ptr<DictionaryValue> value)
{
    DCHECK(value);
    if (m_data.set(name, std::move(value)))
        m_order.append(name);
}

void DictionaryValue::setArray(const String16& name, std::unique_ptr<ListValue> value)
{
    DCHECK(value);
    if (m_data.set(name, std::move(value)))
        m_order.append(name);
}

bool DictionaryValue::getBoolean(const String16& name, bool* output) const
{
    protocol::Value* value = get(name);
    if (!value)
        return false;
    return value->asBoolean(output);
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
    return it->second;
}

DictionaryValue::Entry DictionaryValue::at(size_t index) const
{
    String16 key = m_order[index];
    return std::make_pair(key, m_data.get(key));
}

bool DictionaryValue::booleanProperty(const String16& name, bool defaultValue) const
{
    bool result = defaultValue;
    getBoolean(name, &result);
    return result;
}

double DictionaryValue::numberProperty(const String16& name, double defaultValue) const
{
    double result = defaultValue;
    getNumber(name, &result);
    return result;
}

void DictionaryValue::remove(const String16& name)
{
    m_data.remove(name);
    for (size_t i = 0; i < m_order.size(); ++i) {
        if (m_order[i] == name) {
            m_order.remove(i);
            break;
        }
    }
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
        Value* value = m_data.get(key);
        DCHECK(value);
        result->setValue(key, value->clone());
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
    for (Vector<std::unique_ptr<protocol::Value>>::const_iterator it = m_data.begin(); it != m_data.end(); ++it) {
        if (it != m_data.begin())
            output->append(',');
        (*it)->writeJSON(output);
    }
    output->append(']');
}

std::unique_ptr<Value> ListValue::clone() const
{
    std::unique_ptr<ListValue> result = ListValue::create();
    for (Vector<std::unique_ptr<protocol::Value>>::const_iterator it = m_data.begin(); it != m_data.end(); ++it)
        result->pushValue((*it)->clone());
    return std::move(result);
}

ListValue::ListValue()
    : Value(TypeArray)
{
}

void ListValue::pushValue(std::unique_ptr<protocol::Value> value)
{
    DCHECK(value);
    m_data.append(std::move(value));
}

protocol::Value* ListValue::at(size_t index)
{
    CHECK(index < m_data.size());
    return m_data[index];
}

} // namespace protocol
} // namespace blink
