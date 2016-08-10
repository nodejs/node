// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8DebuggerScript.h"

#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/Platform.h"
#include "platform/v8_inspector/V8StringUtil.h"

namespace blink {

static const LChar hexDigits[17] = "0123456789ABCDEF";

static void appendUnsignedAsHex(unsigned number, String16Builder* destination)
{
    for (size_t i = 0; i < 8; ++i) {
        destination->append(hexDigits[number & 0xF]);
        number >>= 4;
    }
}

// Hash algorithm for substrings is described in "Über die Komplexität der Multiplikation in
// eingeschränkten Branchingprogrammmodellen" by Woelfe.
// http://opendatastructures.org/versions/edition-0.1d/ods-java/node33.html#SECTION00832000000000000000
static String16 calculateHash(const String16& str)
{
    static uint64_t prime[] = { 0x3FB75161, 0xAB1F4E4F, 0x82675BC5, 0xCD924D35, 0x81ABE279 };
    static uint64_t random[] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    static uint32_t randomOdd[] = { 0xB4663807, 0xCC322BF5, 0xD4F91BBD, 0xA7BEA11D, 0x8F462907 };

    uint64_t hashes[] = { 0, 0, 0, 0, 0 };
    uint64_t zi[] = { 1, 1, 1, 1, 1 };

    const size_t hashesSize = PROTOCOL_ARRAY_LENGTH(hashes);

    size_t current = 0;
    const uint32_t* data = nullptr;
    data = reinterpret_cast<const uint32_t*>(str.characters16());
    for (size_t i = 0; i < str.sizeInBytes() / 4; i += 4) {
        uint32_t v = data[i];
        uint64_t xi = v * randomOdd[current] & 0x7FFFFFFF;
        hashes[current] = (hashes[current] + zi[current] * xi) % prime[current];
        zi[current] = (zi[current] * random[current]) % prime[current];
        current = current == hashesSize - 1 ? 0 : current + 1;
    }
    if (str.sizeInBytes() % 4) {
        uint32_t v = 0;
        for (size_t i = str.sizeInBytes() - str.sizeInBytes() % 4; i < str.sizeInBytes(); ++i) {
            v <<= 8;
            v |= reinterpret_cast<const uint8_t*>(data)[i];
        }
        uint64_t xi = v * randomOdd[current] & 0x7FFFFFFF;
        hashes[current] = (hashes[current] + zi[current] * xi) % prime[current];
        zi[current] = (zi[current] * random[current]) % prime[current];
        current = current == hashesSize - 1 ? 0 : current + 1;
    }

    for (size_t i = 0; i < hashesSize; ++i)
        hashes[i] = (hashes[i] + zi[i] * (prime[i] - 1)) % prime[i];

    String16Builder hash;
    for (size_t i = 0; i < hashesSize; ++i)
        appendUnsignedAsHex(hashes[i], &hash);
    return hash.toString();
}

V8DebuggerScript::V8DebuggerScript(v8::Isolate* isolate, v8::Local<v8::Object> object, bool isLiveEdit)
{
    v8::Local<v8::Value> idValue = object->Get(toV8StringInternalized(isolate, "id"));
    DCHECK(!idValue.IsEmpty() && idValue->IsInt32());
    m_id = String16::fromInteger(idValue->Int32Value());

    m_url = toProtocolStringWithTypeCheck(object->Get(toV8StringInternalized(isolate, "name")));
    m_sourceURL = toProtocolStringWithTypeCheck(object->Get(toV8StringInternalized(isolate, "sourceURL")));
    m_sourceMappingURL = toProtocolStringWithTypeCheck(object->Get(toV8StringInternalized(isolate, "sourceMappingURL")));
    m_startLine = object->Get(toV8StringInternalized(isolate, "startLine"))->ToInteger(isolate)->Value();
    m_startColumn = object->Get(toV8StringInternalized(isolate, "startColumn"))->ToInteger(isolate)->Value();
    m_endLine = object->Get(toV8StringInternalized(isolate, "endLine"))->ToInteger(isolate)->Value();
    m_endColumn = object->Get(toV8StringInternalized(isolate, "endColumn"))->ToInteger(isolate)->Value();
    m_executionContextAuxData = toProtocolStringWithTypeCheck(object->Get(toV8StringInternalized(isolate, "executionContextAuxData")));
    m_isInternalScript = object->Get(toV8StringInternalized(isolate, "isInternalScript"))->ToBoolean(isolate)->Value();
    m_executionContextId = object->Get(toV8StringInternalized(isolate, "executionContextId"))->ToInteger(isolate)->Value();
    m_isLiveEdit = isLiveEdit;

    v8::Local<v8::Value> sourceValue = object->Get(toV8StringInternalized(isolate, "source"));
    if (!sourceValue.IsEmpty() && sourceValue->IsString())
        setSource(isolate, sourceValue.As<v8::String>());
}

V8DebuggerScript::~V8DebuggerScript()
{
}

const String16& V8DebuggerScript::sourceURL() const
{
    return m_sourceURL.isEmpty() ? m_url : m_sourceURL;
}

v8::Local<v8::String> V8DebuggerScript::source(v8::Isolate* isolate) const
{
    return m_source.Get(isolate);
}

void V8DebuggerScript::setSourceURL(const String16& sourceURL)
{
    m_sourceURL = sourceURL;
}

void V8DebuggerScript::setSourceMappingURL(const String16& sourceMappingURL)
{
    m_sourceMappingURL = sourceMappingURL;
}

void V8DebuggerScript::setSource(v8::Isolate* isolate, v8::Local<v8::String> source)
{
    m_source.Reset(isolate, source);
    m_hash = calculateHash(toProtocolString(source));
}

} // namespace blink
