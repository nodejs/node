//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#include <wchar.h>

namespace JSON
{
    class JSONParser;

    // Small scanner for exclusive JSON purpose. The general
    // JScript scanner is not appropriate here because of the JSON restricted lexical grammar
    // token enums and structures are shared although the token semantics is slightly different.
    class JSONScanner
    {
    public:
        JSONScanner();
        tokens Scan();
        void Init(const wchar_t* input, uint len, Token* pOutToken,
            ::Js::ScriptContext* sc, const wchar_t* current, ArenaAllocator* allocator);

        void Finalizer();
        wchar_t* GetCurrentString(){return currentString;}
        uint GetCurrentStringLen(){return currentIndex;}


    private:

        // Data structure for unescaping strings
        struct RangeCharacterPair {
        public:
            uint m_rangeStart;
            uint m_rangeLength;
            wchar_t m_char;
            RangeCharacterPair() {}
            RangeCharacterPair(uint rangeStart, uint rangeLength, wchar_t ch) : m_rangeStart(rangeStart), m_rangeLength(rangeLength), m_char(ch) {}
        };

        typedef JsUtil::List<RangeCharacterPair, ArenaAllocator> RangeCharacterPairList;

        RangeCharacterPairList* currentRangeCharacterPairList;

        Js::TempGuestArenaAllocatorObject* allocatorObject;
        ArenaAllocator* allocator;
        void BuildUnescapedString(bool shouldSkipLastCharacter);

        RangeCharacterPairList* GetCurrentRangeCharacterPairList(void);

        __inline wchar_t ReadNextChar(void)
        {
            return *currentChar++;
        }

        __inline wchar_t PeekNextChar(void)
        {
            return *currentChar;
        }

        tokens ScanString();
        bool IsJSONNumber();

        const wchar_t* inputText;
        uint    inputLen;
        const wchar_t* currentChar;
        const wchar_t* pTokenString;

        Token*   pToken;
        ::Js::ScriptContext* scriptContext;

        uint     currentIndex;
        wchar_t* currentString;
        __field_ecount(stringBufferLength) wchar_t* stringBuffer;
        int      stringBufferLength;

        friend class JSONParser;
    };
} // namespace JSON
