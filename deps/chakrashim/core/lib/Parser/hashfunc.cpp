//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
/*
 * IMPORTANT:
 *  This file does not compile stand alone. It was required so that
 *  the same code could be built into a utility program comphash.exe as well
 *  as the scripting dll's. This file is included in core\comphash.cpp
 *  to be used by comphash.exe. It is included in core\scrutil.cpp where to
 *  be used by jscript.dll and vbscript.dll.
 *
 *  comphash.exe is a utility used in the build to generate a source code file
 *  containing a table of hash values associated with strings needed by
 *  jscript and vbscript. It is highly desirable to have a single definition
 *  of the hash function so things don't go out of sync.
 */

// scaffolding - define ULONG
typedef unsigned long ULONG;

ULONG CaseSensitiveComputeHashCch(LPCOLESTR prgch, long cch)
{
    ULONG luHash = 0;

    while (cch-- > 0)
        luHash = 17 * luHash + *(wchar_t *)prgch++;
    return luHash;
}

ULONG CaseSensitiveComputeHashCch(LPCUTF8 prgch, long cch)
{
    utf8::DecodeOptions options = utf8::doAllowThreeByteSurrogates;
    ULONG luHash = 0;

    while (cch-- > 0)
        luHash = 17 * luHash + utf8::Decode(prgch, prgch + 4, options); // WARNING: Assume cch correct, suppress end-of-buffer checking
    return luHash;
}

ULONG CaseSensitiveComputeHashCch(char const * prgch, long cch)
{
    ULONG luHash = 0;

    while (cch-- > 0)
    {
        Assert(utf8::IsStartByte(*prgch) && !utf8::IsLeadByte(*prgch));
        luHash = 17 * luHash + *prgch++;
    }
    return luHash;
}

ULONG CaseInsensitiveComputeHash(LPCOLESTR posz)
{
    ULONG luHash = 0;
    wchar_t ch;
    while (0 != (ch = *(wchar_t *)posz++))
    {
        if (ch <= 'Z' && ch >= 'A')
            ch += 'a' - 'A';
        luHash = 17 * luHash + ch;
    }
    return luHash;
}
