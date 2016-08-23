// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8StringUtil.h"

#include "platform/v8_inspector/V8InspectorImpl.h"
#include "platform/v8_inspector/V8InspectorSessionImpl.h"
#include "platform/v8_inspector/V8Regex.h"

namespace v8_inspector {

namespace {

String16 findMagicComment(const String16& content, const String16& name, bool multiline)
{
    DCHECK(name.find("=") == String16::kNotFound);
    unsigned length = content.length();
    unsigned nameLength = name.length();

    size_t pos = length;
    size_t equalSignPos = 0;
    size_t closingCommentPos = 0;
    while (true) {
        pos = content.reverseFind(name, pos);
        if (pos == String16::kNotFound)
            return String16();

        // Check for a /\/[\/*][@#][ \t]/ regexp (length of 4) before found name.
        if (pos < 4)
            return String16();
        pos -= 4;
        if (content[pos] != '/')
            continue;
        if ((content[pos + 1] != '/' || multiline)
            && (content[pos + 1] != '*' || !multiline))
            continue;
        if (content[pos + 2] != '#' && content[pos + 2] != '@')
            continue;
        if (content[pos + 3] != ' ' && content[pos + 3] != '\t')
            continue;
        equalSignPos = pos + 4 + nameLength;
        if (equalSignPos < length && content[equalSignPos] != '=')
            continue;
        if (multiline) {
            closingCommentPos = content.find("*/", equalSignPos + 1);
            if (closingCommentPos == String16::kNotFound)
                return String16();
        }

        break;
    }

    DCHECK(equalSignPos);
    DCHECK(!multiline || closingCommentPos);
    size_t urlPos = equalSignPos + 1;
    String16 match = multiline
        ? content.substring(urlPos, closingCommentPos - urlPos)
        : content.substring(urlPos);

    size_t newLine = match.find("\n");
    if (newLine != String16::kNotFound)
        match = match.substring(0, newLine);
    match = match.stripWhiteSpace();

    for (unsigned i = 0; i < match.length(); ++i) {
        UChar c = match[i];
        if (c == '"' || c == '\'' || c == ' ' || c == '\t')
            return "";
    }

    return match;
}

String16 createSearchRegexSource(const String16& text)
{
    String16Builder result;

    for (unsigned i = 0; i < text.length(); i++) {
        UChar c = text[i];
        if (c == '[' || c == ']' || c == '(' || c == ')' || c == '{' || c == '}'
            || c == '+' || c == '-' || c == '*' || c == '.' || c == ',' || c == '?'
            || c == '\\' || c == '^' || c == '$' || c == '|') {
            result.append('\\');
        }
        result.append(text[i]);
    }

    return result.toString();
}

std::unique_ptr<std::vector<unsigned>> lineEndings(const String16& text)
{
    std::unique_ptr<std::vector<unsigned>> result(new std::vector<unsigned>());

    const String16 lineEndString = "\n";
    unsigned start = 0;
    while (start < text.length()) {
        size_t lineEnd = text.find(lineEndString, start);
        if (lineEnd == String16::kNotFound)
            break;

        result->push_back(static_cast<unsigned>(lineEnd));
        start = lineEnd + 1;
    }
    result->push_back(text.length());

    return result;
}

std::vector<std::pair<int, String16>> scriptRegexpMatchesByLines(const V8Regex& regex, const String16& text)
{
    std::vector<std::pair<int, String16>> result;
    if (text.isEmpty())
        return result;

    std::unique_ptr<std::vector<unsigned>> endings(lineEndings(text));
    unsigned size = endings->size();
    unsigned start = 0;
    for (unsigned lineNumber = 0; lineNumber < size; ++lineNumber) {
        unsigned lineEnd = endings->at(lineNumber);
        String16 line = text.substring(start, lineEnd - start);
        if (line.length() && line[line.length() - 1] == '\r')
            line = line.substring(0, line.length() - 1);

        int matchLength;
        if (regex.match(line, 0, &matchLength) != -1)
            result.push_back(std::pair<int, String16>(lineNumber, line));

        start = lineEnd + 1;
    }
    return result;
}

std::unique_ptr<protocol::Debugger::SearchMatch> buildObjectForSearchMatch(int lineNumber, const String16& lineContent)
{
    return protocol::Debugger::SearchMatch::create()
        .setLineNumber(lineNumber)
        .setLineContent(lineContent)
        .build();
}

std::unique_ptr<V8Regex> createSearchRegex(V8InspectorImpl* inspector, const String16& query, bool caseSensitive, bool isRegex)
{
    String16 regexSource = isRegex ? query : createSearchRegexSource(query);
    return wrapUnique(new V8Regex(inspector, regexSource, caseSensitive));
}

} // namespace

v8::Local<v8::String> toV8String(v8::Isolate* isolate, const String16& string)
{
    if (string.isEmpty())
        return v8::String::Empty(isolate);
    return v8::String::NewFromTwoByte(isolate, reinterpret_cast<const uint16_t*>(string.characters16()), v8::NewStringType::kNormal, string.length()).ToLocalChecked();
}

v8::Local<v8::String> toV8StringInternalized(v8::Isolate* isolate, const String16& string)
{
    if (string.isEmpty())
        return v8::String::Empty(isolate);
    return v8::String::NewFromTwoByte(isolate, reinterpret_cast<const uint16_t*>(string.characters16()), v8::NewStringType::kInternalized, string.length()).ToLocalChecked();
}

v8::Local<v8::String> toV8StringInternalized(v8::Isolate* isolate, const char* str)
{
    return v8::String::NewFromUtf8(isolate, str, v8::NewStringType::kInternalized).ToLocalChecked();
}

String16 toProtocolString(v8::Local<v8::String> value)
{
    if (value.IsEmpty() || value->IsNull() || value->IsUndefined())
        return String16();
    std::unique_ptr<UChar[]> buffer(new UChar[value->Length()]);
    value->Write(reinterpret_cast<uint16_t*>(buffer.get()), 0, value->Length());
    return String16(buffer.get(), value->Length());
}

String16 toProtocolStringWithTypeCheck(v8::Local<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsString())
        return String16();
    return toProtocolString(value.As<v8::String>());
}

std::vector<std::unique_ptr<protocol::Debugger::SearchMatch>> searchInTextByLinesImpl(V8InspectorSession* session, const String16& text, const String16& query, const bool caseSensitive, const bool isRegex)
{
    std::unique_ptr<V8Regex> regex = createSearchRegex(static_cast<V8InspectorSessionImpl*>(session)->inspector(), query, caseSensitive, isRegex);
    std::vector<std::pair<int, String16>> matches = scriptRegexpMatchesByLines(*regex.get(), text);

    std::vector<std::unique_ptr<protocol::Debugger::SearchMatch>> result;
    for (const auto& match : matches)
        result.push_back(buildObjectForSearchMatch(match.first, match.second));
    return result;
}

String16 findSourceURL(const String16& content, bool multiline)
{
    return findMagicComment(content, "sourceURL", multiline);
}

String16 findSourceMapURL(const String16& content, bool multiline)
{
    return findMagicComment(content, "sourceMappingURL", multiline);
}

std::unique_ptr<protocol::Value> toProtocolValue(v8::Local<v8::Context> context, v8::Local<v8::Value> value, int maxDepth)
{
    if (value.IsEmpty()) {
        NOTREACHED();
        return nullptr;
    }

    if (!maxDepth)
        return nullptr;
    maxDepth--;

    if (value->IsNull() || value->IsUndefined())
        return protocol::Value::null();
    if (value->IsBoolean())
        return protocol::FundamentalValue::create(value.As<v8::Boolean>()->Value());
    if (value->IsNumber()) {
        double doubleValue = value.As<v8::Number>()->Value();
        int intValue = static_cast<int>(doubleValue);
        if (intValue == doubleValue)
            return protocol::FundamentalValue::create(intValue);
        return protocol::FundamentalValue::create(doubleValue);
    }
    if (value->IsString())
        return protocol::StringValue::create(toProtocolString(value.As<v8::String>()));
    if (value->IsArray()) {
        v8::Local<v8::Array> array = value.As<v8::Array>();
        std::unique_ptr<protocol::ListValue> inspectorArray = protocol::ListValue::create();
        uint32_t length = array->Length();
        for (uint32_t i = 0; i < length; i++) {
            v8::Local<v8::Value> value;
            if (!array->Get(context, i).ToLocal(&value))
                return nullptr;
            std::unique_ptr<protocol::Value> element = toProtocolValue(context, value, maxDepth);
            if (!element)
                return nullptr;
            inspectorArray->pushValue(std::move(element));
        }
        return std::move(inspectorArray);
    }
    if (value->IsObject()) {
        std::unique_ptr<protocol::DictionaryValue> jsonObject = protocol::DictionaryValue::create();
        v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
        v8::Local<v8::Array> propertyNames;
        if (!object->GetPropertyNames(context).ToLocal(&propertyNames))
            return nullptr;
        uint32_t length = propertyNames->Length();
        for (uint32_t i = 0; i < length; i++) {
            v8::Local<v8::Value> name;
            if (!propertyNames->Get(context, i).ToLocal(&name))
                return nullptr;
            // FIXME(yurys): v8::Object should support GetOwnPropertyNames
            if (name->IsString()) {
                v8::Maybe<bool> hasRealNamedProperty = object->HasRealNamedProperty(context, v8::Local<v8::String>::Cast(name));
                if (!hasRealNamedProperty.IsJust() || !hasRealNamedProperty.FromJust())
                    continue;
            }
            v8::Local<v8::String> propertyName;
            if (!name->ToString(context).ToLocal(&propertyName))
                continue;
            v8::Local<v8::Value> property;
            if (!object->Get(context, name).ToLocal(&property))
                return nullptr;
            std::unique_ptr<protocol::Value> propertyValue = toProtocolValue(context, property, maxDepth);
            if (!propertyValue)
                return nullptr;
            jsonObject->setValue(toProtocolString(propertyName), std::move(propertyValue));
        }
        return std::move(jsonObject);
    }
    NOTREACHED();
    return nullptr;
}

} // namespace v8_inspector
