// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef String16WTF_h
#define String16WTF_h

#include "platform/Decimal.h"
#include "public/platform/WebString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringConcatenate.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/StringToNumber.h"
#include "wtf/text/StringView.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace protocol {

class PLATFORM_EXPORT String16 : public String16Base<String16, UChar> {
public:
    static const size_t kNotFound = WTF::kNotFound;

    String16() { }
    String16(const String16& other) : m_impl(other.m_impl) { }
    String16(const UChar* characters, unsigned length) : m_impl(characters, length) { }
    String16(const char* characters) : String16(characters, strlen(characters)) { }
    String16(const char* characters, size_t length)
    {
        UChar* data;
        m_impl = WTF::String::createUninitialized(length, data);
        for (size_t i = 0; i < length; ++i)
            data[i] = characters[i];
    }

    ~String16() { }

    String16 isolatedCopy() const { return String16(m_impl.isolatedCopy()); }
    const UChar* characters16() const { return m_impl.isEmpty() ? nullptr : m_impl.characters16(); }
    size_t length() const { return m_impl.length(); }
    bool isEmpty() const { return m_impl.isEmpty(); }
    UChar operator[](unsigned index) const { return m_impl[index]; }
    String16 substring(unsigned pos, unsigned len = UINT_MAX) const { return m_impl.substring(pos, len); }
    size_t find(const String16& str, unsigned start = 0) const { return m_impl.find(str.impl(), start); }
    size_t reverseFind(const String16& str, unsigned start = UINT_MAX) const { return m_impl.reverseFind(str.impl(), start); }

    // WTF convenience constructors and helper methods.
    String16(const WebString& other) : String16(String(other)) { }
    template<typename StringType1, typename StringType2>
    String16(const WTF::StringAppend<StringType1, StringType2>& impl) : String16(String(impl)) { }
    String16(const WTF::AtomicString& impl) : String16(String(impl)) { }
    String16(const WTF::String& impl) : m_impl(impl) { m_impl.ensure16Bit(); }
    String16(WTF::HashTableDeletedValueType) : m_impl(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_impl.isHashTableDeletedValue(); }
    operator WTF::String() const { return m_impl; }
    operator WTF::StringView() const { return StringView(m_impl); }
    operator WebString() { return m_impl; }
    const WTF::String& impl() const { return m_impl; }

private:
    WTF::String m_impl;
};

inline bool operator==(const String16& a, const String16& b) { return a.impl() == b.impl(); }
inline bool operator!=(const String16& a, const String16& b) { return a.impl() != b.impl(); }
inline bool operator==(const String16& a, const char* b) { return a.impl() == b; }
inline String16 operator+(const String16& a, const char* b) { return String16(a.impl() + b); }
inline String16 operator+(const char* a, const String16& b) { return String16(a + b.impl()); }
inline String16 operator+(const String16& a, const String16& b) { return String16(a.impl() + b.impl()); }

} // namespace protocol
} // namespace blink

namespace std {
template<> struct hash<blink::protocol::String16> {
    std::size_t operator()(const blink::protocol::String16& string) const
    {
        return StringHash::hash(string.impl());
    }
};
} // namespace std

using InspectorProtocolConvenienceStringType = WTF::String;

// WTF helpers below this line.

namespace WTF {

struct String16Hash {
    static unsigned hash(const blink::protocol::String16& key) { return StringHash::hash(key.impl()); }
    static bool equal(const blink::protocol::String16& a, const blink::protocol::String16& b)
    {
        return StringHash::equal(a.impl(), b.impl());
    }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<blink::protocol::String16> {
    typedef String16Hash Hash;
};

template<>
struct HashTraits<blink::protocol::String16> : SimpleClassHashTraits<blink::protocol::String16> {
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const blink::protocol::String16& a) { return a.impl().isNull(); }
};

} // namespace WTF

#endif // !defined(String16WTF_h)
