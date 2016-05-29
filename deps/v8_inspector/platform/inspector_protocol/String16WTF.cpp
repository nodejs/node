// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/inspector_protocol/String16WTF.h"

namespace blink {
namespace protocol {

String16::String16(const String16& other) : m_impl(other.m_impl) { }

String16::String16(const UChar* u, unsigned length) : m_impl(u, length) { }

String16::String16(const char* characters) : String16(characters, strlen(characters)) { }

String16::String16(const WTF::String& other)
{
    if (other.isNull())
        return;
    if (!other.is8Bit()) {
        m_impl = other;
        return;
    }

    UChar* data;
    const LChar* characters = other.characters8();
    size_t length = other.length();
    m_impl = String::createUninitialized(length, data);
    for (size_t i = 0; i < length; ++i)
        data[i] = characters[i];
}

String16::String16(const char* characters, size_t length)
{
    UChar* data;
    m_impl = String::createUninitialized(length, data);
    for (size_t i = 0; i < length; ++i)
        data[i] = characters[i];
}

String16 String16::createUninitialized(unsigned length, UChar*& data)
{
    return String::createUninitialized(length, data);
}

} // namespace protocol
} // namespace blink
