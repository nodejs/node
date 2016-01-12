//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"
#include "errstr.h"

// scaffolding - get a g_hInstance from scrbgase.cpp
HANDLE g_hInstance;

// Used as a prefix to generate the resource dll name.
const wchar_t g_wszPrefix[] = L"js";

static BOOL FGetStringFromLibrary(HMODULE hlib, int istring, __out_ecount(cchMax) WCHAR * psz, int cchMax)
{
    // NOTE - istring is expected to be HRESULT

    Assert(0 < cchMax);
    AssertArrMem(psz, cchMax);

    HGLOBAL hgl = NULL;
    WCHAR * pchRes = NULL;
    HRSRC hrsrc;
    WCHAR * pchCur;
    int cch;
    int cstring;
    DWORD cbRes;
    int itable = ((WORD)istring >> 4) + 1;
    istring &= 0x0F;
    BOOL fRet = FALSE;

    psz[0] = '\0';

    if (NULL == hlib)
        goto LError;

    hrsrc = FindResourceEx((HMODULE)hlib, RT_STRING, MAKEINTRESOURCE(itable), 0);
    if (NULL == hrsrc)
        goto LError;

    hgl = LoadResource((HMODULE)hlib, hrsrc);
    if (NULL == hgl)
        goto LError;

    pchRes = (WCHAR *)LockResource(hgl);
    if (NULL == pchRes)
        goto LError;

    cbRes = SizeofResource((HMODULE)hlib, hrsrc);

    if (cbRes < sizeof(WORD))
        goto LError;

    pchCur = pchRes;
    for (cstring = istring; cstring-- > 0;)
    {
        if (cbRes - sizeof(WORD) < sizeof(WCHAR) * (pchCur - pchRes))
            goto LError;

        cch = (*(WORD *) pchCur) + 1;

        if (cch <= 0)
            goto LError;

        if (cbRes < sizeof(WCHAR) * cch)
            goto LError;

        if (cbRes - sizeof(WCHAR) * cch < sizeof(WCHAR) * (pchCur - pchRes))
            goto LError;

        pchCur += cch;
    }

    if (cbRes - sizeof(WORD) < sizeof(WCHAR) * (pchCur - pchRes))
        goto LError;
    cch = * (WORD *) pchCur;

    if (cch <= 0)
        goto LError;

    if (cbRes < sizeof(WCHAR) * (cch + 1))
        goto LError;

    if (cbRes - sizeof(WCHAR) * (cch + 1) < sizeof(WCHAR) * (pchCur - pchRes))
        goto LError;

    if (cch > cchMax - 1)
        cch = cchMax - 1;

    js_memcpy_s(psz, cchMax * sizeof(WCHAR), pchCur + 1, cch * sizeof(WCHAR));
    psz[cch] = '\0';
    fRet = TRUE;

LError:

#if !_WIN32 && !_WIN64

    //
    // Unlock/FreeResource non-essential on win32/64.
    //
    if (NULL != hgl)
    {
        if (NULL != pchRes)
            UnlockResource(hgl);
        FreeResource(hgl);
    }

#endif

    return fRet;
}


BOOL FGetResourceString(long isz, __out_ecount(cchMax) OLECHAR *psz, int cchMax)
{
    return FGetStringFromLibrary((HINSTANCE)g_hInstance, isz, psz, cchMax);
}

// Get a bstr version of the error string
__declspec(noinline) // Don't inline. This function needs 2KB stack.
BSTR BstrGetResourceString(long isz)
{
    // NOTE - isz is expected to be HRESULT

    OLECHAR szT[1024];

    if (!FGetResourceString(isz, szT,
        sizeof(szT) / sizeof(szT[0]) - 1))
    {
        return NULL;
    }

    return SysAllocString(szT);
}
