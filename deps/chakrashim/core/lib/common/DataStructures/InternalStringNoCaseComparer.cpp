//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"
#include "DataStructures\CharacterBuffer.h"
#include "DataStructures\InternalStringNoCaseComparer.h"

namespace JsUtil
{
    bool NoCaseComparer<JsUtil::CharacterBuffer<WCHAR>>::Equals(JsUtil::CharacterBuffer<WCHAR> const& s1, JsUtil::CharacterBuffer<WCHAR> const& s2)
    {
        return (s1.GetLength() == s2.GetLength()) && (NoCaseComparer<JsUtil::CharacterBuffer<WCHAR>>::Compare(s1, s2)==0);
    }

    uint NoCaseComparer<JsUtil::CharacterBuffer<WCHAR>>::GetHashCode(JsUtil::CharacterBuffer<WCHAR> const& s1)
    {
        const wchar_t* s = s1.GetBuffer();
        size_t length = s1.GetLength();
        uint hash = 0;
        for (size_t i = 0; i < length; i++)
        {
            hash = _rotl(hash, 7);
            hash ^= tolower(s[i]);
        }
        return ((hash & 0x7fffffff) << 1) | 1;
    }

    int NoCaseComparer<JsUtil::CharacterBuffer<WCHAR>>::Compare(JsUtil::CharacterBuffer<WCHAR> const& s1, JsUtil::CharacterBuffer<WCHAR> const& s2)
    {
        if (s1.GetLength() != s2.GetLength()) return +1;
        int count = s1.GetLength();
        const wchar_t* buf1 = s1.GetBuffer();
        const wchar_t* buf2 = s2.GetBuffer();
        for (int i=0; i < count; i++)
        {
            if (tolower(buf1[i]) != tolower(buf2[i]))
            {
                return (buf1[i] < buf2[i] ? -1 : +1);
            }
        }
        return (0);
    }
}
