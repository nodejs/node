//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

#if ENABLE_REGEX_CONFIG_OPTIONS

namespace UnifiedRegex
{
    const wchar_t* const DebugWriter::hex = L"0123456789abcdef";

    DebugWriter::DebugWriter() : indent(0), nlPending(false)
    {
    }

    void __cdecl DebugWriter::Print(const Char *form, ...)
    {
        va_list argptr;
        va_start(argptr, form);
        int len = _vsnwprintf_s(buf, bufLen, _TRUNCATE, form, argptr);
        if (len < 0)
            Output::Print(L"<not enough buffer space to format>");
        else
        {
            if (len > 0)
                CheckForNewline();
            Output::Print(L"%s", buf);
        }
    }

    void __cdecl DebugWriter::PrintEOL(const Char *form, ...)
    {
        va_list argptr;
        va_start(argptr, form);
        int len = _vsnwprintf_s(buf, bufLen, _TRUNCATE, form, argptr);
        Assert(len >= 0 && len < bufLen - 1);
        if (len > 0)
            CheckForNewline();
        Output::Print(L"%s", buf);
        EOL();
    }

    void DebugWriter::PrintEscapedString(const Char* str, CharCount len)
    {
        Assert(str != 0);
        CheckForNewline();

        const Char* pl = str + len;
        for (const Char* p = str; p < pl; p++)
        {
            if (*p == '"')
                Output::Print(L"\\\"");
            else
                PrintEscapedChar(*p);
        }
    }

    void DebugWriter::PrintQuotedString(const Char* str, CharCount len)
    {
        CheckForNewline();
        if (str == 0)
            Output::Print(L"null");
        else
        {
            Output::Print(L"\"");
            PrintEscapedString(str, len);
            Output::Print(L"\"");
        }
    }

    void DebugWriter::PrintEscapedChar(const Char c)
    {
        CheckForNewline();
        if (c > 0xff)
            Output::Print(L"\\u%lc%lc%lc%lc", hex[c >> 12], hex[(c >> 8) & 0xf], hex[(c >> 4) & 0xf], hex[c & 0xf]);
        else if (c < ' ' || c > '~')
            Output::Print(L"\\x%lc%lc", hex[c >> 4], hex[c & 0xf]);
        else
            Output::Print(L"%lc", c);
    }

    void DebugWriter::PrintQuotedChar(const Char c)
    {
        CheckForNewline();
        Output::Print(L"'");
        if (c == '\'')
            Output::Print(L"\\'");
        else
            PrintEscapedChar(c);
        Output::Print(L"'");
    }

    void DebugWriter::EOL()
    {
        CheckForNewline();
        nlPending = true;
    }

    void DebugWriter::Indent()
    {
        indent++;
    }

    void DebugWriter::Unindent()
    {
        indent--;
    }

    void DebugWriter::Flush()
    {
        Output::Print(L"\n");
        Output::Flush();
        nlPending = false;
    }

    void DebugWriter::BeginLine()
    {
        Output::Print(L"\n%*s", indent * 4, L"");
    }
}

#endif
