// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef String16STL_h
#define String16STL_h

#include <cctype>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
#include <string>
#include <vector>

using UChar = uint16_t;

namespace blink {
namespace protocol {

class String16 : public String16Base<String16, UChar> {
public:
    static const size_t kNotFound = static_cast<size_t>(-1);

    String16() { }
    String16(const String16& other) : m_impl(other.m_impl) { }
    String16(const UChar* characters, size_t size) : m_impl(characters, size) { }
    String16(const UChar* characters) : m_impl(characters) { }
    String16(const char* characters) : String16(characters, std::strlen(characters)) { }
    String16(const char* characters, size_t size)
    {
        m_impl.resize(size);
        for (size_t i = 0; i < size; ++i)
            m_impl[i] = characters[i];
    }

    String16 isolatedCopy() const { return String16(m_impl); }
    const UChar* characters16() const { return m_impl.c_str(); }
    size_t length() const { return m_impl.length(); }
    bool isEmpty() const { return !m_impl.length(); }
    UChar operator[](unsigned index) const { return m_impl[index]; }
    String16 substring(unsigned pos, unsigned len = UINT_MAX) const { return String16(m_impl.substr(pos, len)); }
    size_t find(const String16& str, unsigned start = 0) const { return m_impl.find(str.m_impl, start); }
    size_t reverseFind(const String16& str, unsigned start = UINT_MAX) const { return m_impl.rfind(str.m_impl, start); }

    // Convenience methods.
    std::string utf8() const;
    static String16 fromUTF8(const char* stringStart, size_t length);

    const std::basic_string<UChar>& impl() const { return m_impl; }
    explicit String16(const std::basic_string<UChar>& impl) : m_impl(impl) { }

    std::size_t hash() const
    {
        if (!has_hash) {
            size_t hash = 0;
            for (size_t i = 0; i < length(); ++i)
                hash = 31 * hash + m_impl[i];
            hash_code = hash;
            has_hash = true;
        }
        return hash_code;
    }

private:
    std::basic_string<UChar> m_impl;
    mutable bool has_hash = false;
    mutable std::size_t hash_code = 0;
};

inline bool operator==(const String16& a, const String16& b) { return a.impl() == b.impl(); }
inline bool operator<(const String16& a, const String16& b) { return a.impl() < b.impl(); }
inline bool operator!=(const String16& a, const String16& b) { return a.impl() != b.impl(); }
inline bool operator==(const String16& a, const char* b) { return a.impl() == String16(b).impl(); }
inline String16 operator+(const String16& a, const char* b) { return String16(a.impl() + String16(b).impl()); }
inline String16 operator+(const char* a, const String16& b) { return String16(String16(a).impl() + b.impl()); }
inline String16 operator+(const String16& a, const String16& b) { return String16(a.impl() + b.impl()); }

} // namespace protocol
} // namespace blink

#if !defined(__APPLE__) || defined(_LIBCPP_VERSION)

namespace std {
template<> struct hash<blink::protocol::String16> {
    std::size_t operator()(const blink::protocol::String16& string) const
    {
        return string.hash();
    }
};

} // namespace std

#endif // !defined(__APPLE__) || defined(_LIBCPP_VERSION)

class InspectorProtocolConvenienceStringType {
public:
    // This class should not be ever instantiated, so we don't implement constructors.
    InspectorProtocolConvenienceStringType();
    InspectorProtocolConvenienceStringType(const blink::protocol::String16& other);
    operator blink::protocol::String16() const { return blink::protocol::String16(); };
};

#endif // !defined(String16STL_h)
