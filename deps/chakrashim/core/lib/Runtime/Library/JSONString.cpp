//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    /* Generated using the following js program:
    function createEscapeMap(count)
    {
        var escapeMap = new Array(128);

        for(var i=0; i <  escapeMap.length; i++)
        {
        escapeMap[i] = count ? 0 : "L\'\\0\'";
        }
        for(var i=0; i <  ' '.charCodeAt(0); i++)
        {
        escapeMap[i] = count ? 5 : "L\'u\'";
        }
        escapeMap['\n'.charCodeAt(0)] = count ? 1 : "L\'n\'";
        escapeMap['\b'.charCodeAt(0)] = count ? 1 : "L\'b\'";
        escapeMap['\t'.charCodeAt(0)] = count ? 1 : "L\'t\'";
        escapeMap['\f'.charCodeAt(0)] = count ? 1 : "L\'f\'";
        escapeMap['\r'.charCodeAt(0)] = count ? 1 : "L\'r\'";
        escapeMap['\\'.charCodeAt(0)] = count ? 1 : "L\'\\\\\'";
        escapeMap['"'.charCodeAt(0)]  = count ? 1 : "L\'\"\'";
        WScript.Echo("{ " + escapeMap.join(", ") + " }");
    }
    createEscapeMap(false);
    createEscapeMap(true);
    */
    const WCHAR JSONString::escapeMap[] = {
        L'u', L'u', L'u', L'u', L'u', L'u', L'u', L'u', L'b', L't', L'n', L'u', L'f',
        L'r', L'u', L'u', L'u', L'u', L'u', L'u', L'u', L'u', L'u', L'u', L'u', L'u',
        L'u', L'u', L'u', L'u', L'u', L'u', L'\0', L'\0', L'"', L'\0', L'\0', L'\0',
        L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0',
        L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0',
        L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0',
        L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0',
        L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\\',
        L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0',
        L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0',
        L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0', L'\0',
        L'\0', L'\0' };

    const BYTE JSONString::escapeMapCount[] =
    { 5, 5, 5, 5, 5, 5, 5, 5, 1, 1, 1, 5, 1, 1, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 1, 0, 0, 0, 0, 0
    , 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    , 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    , 0, 0, 0, 0, 0, 0, 0, 0 };

    ArenaAllocator* JSONString::StringProxy::allocator(nullptr);

#ifndef IsJsDiag
    JSONString* JSONString::New(JavascriptString* originalString, charcount_t start, charcount_t extraChars)
    {
        Assert(extraChars > 0);
        charcount_t length = UInt32Math::Add(originalString->GetLength(), UInt32Math::Add(extraChars, /*quotes*/ 2));
        if (!IsValidCharCount(length))
        {
            Js::Throw::OutOfMemory();
        }
        JSONString* result = RecyclerNew(originalString->GetRecycler(), JSONString, originalString, start, length);
        return result;
    }

    JSONString::JSONString(Js::JavascriptString* originalString, charcount_t start, charcount_t length) :
        JavascriptString(originalString->GetScriptContext()->GetLibrary()->GetStringTypeStatic(), length, nullptr),
        m_originalString(originalString),
        m_start(start)
    {
        Assert(m_originalString->GetLength() < length);
    }

    const wchar_t* JSONString::GetSz()
    {
        Assert(!this->IsFinalized());
        charcount_t length = this->GetLength() + /*terminating null*/1;
        WCHAR* buffer = RecyclerNewArrayLeaf(this->GetRecycler(), WCHAR, length);
        this->SetBuffer(buffer);
        buffer[GetLength()] = '\0';
        WritableStringBuffer stringBuffer(buffer, length);
        JavascriptString* str = JSONString::Escape<EscapingOperation_Escape>(this->m_originalString, m_start, &stringBuffer);
        Assert(str == nullptr);
        Assert(buffer[GetLength()] == '\0');
        this->m_originalString = nullptr; // Remove the reference to the original string.
        VirtualTableInfo<LiteralString>::SetVirtualTable(this); // This will ensure GetSz does not get invoked again.
        return buffer;
    }

    void WritableStringBuffer::Append(const wchar_t * str, charcount_t countNeeded)
    {
        JavascriptString::CopyHelper(m_pszCurrentPtr, str, countNeeded);
        this->m_pszCurrentPtr += countNeeded;
        Assert(this->GetCount() <= m_length);
    }

    void WritableStringBuffer::Append(wchar_t c)
    {
        *m_pszCurrentPtr = c;
        this->m_pszCurrentPtr++;
        Assert(this->GetCount() <= m_length);
    }
    void WritableStringBuffer::AppendLarge(const wchar_t * str, charcount_t countNeeded)
    {
        js_memcpy_s(m_pszCurrentPtr, sizeof(WCHAR) * countNeeded, str, sizeof(WCHAR) * countNeeded);
        this->m_pszCurrentPtr += countNeeded;
        Assert(this->GetCount() <= m_length);
    }
#endif
}
