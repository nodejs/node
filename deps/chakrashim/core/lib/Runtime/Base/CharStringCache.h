//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class CharStringCache
    {
    public:
        CharStringCache();
        JavascriptString* GetStringForCharA(char c);    // ASCII 7-bit
        JavascriptString* GetStringForCharW(wchar_t c); // Unicode
        JavascriptString* GetStringForChar(wchar_t c);  // Either
        JavascriptString* GetStringForCharSP(codepoint_t c); // Supplementary char

        // For JIT
        static const wchar_t CharStringCacheSize = 0x80; /*range of ASCII 7-bit chars*/
        static DWORD GetCharStringCacheAOffset() { return offsetof(CharStringCache, charStringCacheA); }
        const PropertyString * const * GetCharStringCacheA() const { return charStringCacheA; }

        static JavascriptString* GetStringForChar(CharStringCache *charStringCache, wchar_t c) { return charStringCache->GetStringForChar(c); }
        static JavascriptString* GetStringForCharCodePoint(CharStringCache *charStringCache, codepoint_t c)
        {
            return (c >= 0x10000 ? charStringCache->GetStringForCharSP(c) : charStringCache->GetStringForCharW((wchar_t)c));
        }

    private:
        PropertyString * charStringCacheA[CharStringCacheSize];
        typedef JsUtil::BaseDictionary<wchar_t, JavascriptString *, Recycler, PowerOf2SizePolicy> CharStringCacheMap;
        CharStringCacheMap * charStringCache;

        friend class CharStringCacheValidator;
    };

    class CharStringCacheValidator
    {
        // Lower assert that charStringCacheA is at the 0 offset
        CompileAssert(offsetof(CharStringCache, charStringCacheA) == 0);
    };
};
