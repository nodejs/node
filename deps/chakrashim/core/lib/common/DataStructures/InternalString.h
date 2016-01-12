//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class InternalString
    {
        charcount_t m_charLength;
        unsigned char m_offset;
        const wchar_t* m_content;

    public:
        InternalString() : m_charLength(0), m_content(NULL), m_offset(0) { };
        InternalString(const wchar_t* content, charcount_t charLength, unsigned char offset = 0);
        static InternalString* New(ArenaAllocator* alloc, const wchar_t* content, charcount_t length);
        static InternalString* New(Recycler* recycler, const wchar_t* content, charcount_t length);
        static InternalString* NewNoCopy(ArenaAllocator* alloc, const wchar_t* content, charcount_t length);

        inline charcount_t GetLength() const
        {
            return m_charLength;
        }

        inline const wchar_t* GetBuffer() const
        {
            return m_content + m_offset;
        }
    };

    struct InternalStringComparer
    {
        __inline static bool Equals(InternalString const& str1, InternalString const& str2)
        {
            return str1.GetLength() == str2.GetLength() &&
                JsUtil::CharacterBuffer<WCHAR>::StaticEquals(str1.GetBuffer(), str2.GetBuffer(), str1.GetLength());
        }

        __inline static hash_t GetHashCode(InternalString const& str)
        {
            return JsUtil::CharacterBuffer<WCHAR>::StaticGetHashCode(str.GetBuffer(), str.GetLength());
        }
    };
}

template<>
struct DefaultComparer<Js::InternalString> : public Js::InternalStringComparer {};
