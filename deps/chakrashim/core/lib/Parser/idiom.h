//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifndef __IDIOM_H__
#define __IDIOM_H__

// cleanup if needed, and set to (null)

#ifndef DELETEARR
#define DELETEARR(arr) do {if (arr){ delete [] (arr); (arr) = NULL; }} while (0)
#endif

#ifndef DELETEPTR
#define DELETEPTR(p) do {if (p){ delete (p); (p) = NULL; }} while (0)
#endif

#ifndef FREEPTR
#define FREEPTR(p) do {if (p){ free(p); (p) = NULL; }} while (0)
#endif

#ifndef SYSFREE
#define SYSFREE(p) do {if (p){ ::SysFreeString(p); (p) = NULL; }} while (0)
#endif

#ifndef RELEASEPTR
#define RELEASEPTR(p) do {if (p){ (p)->Release(); (p) = NULL; }} while (0)
#endif

#ifndef UNADVISERELEASE
#define UNADVISERELEASE(p, dwCookie) do {if (p){ (p)->Unadvise(dwCookie); (p)->Release(); (p) = NULL; }} while (0)
#endif

#ifndef RELEASETYPEINFOATTR
#define RELEASETYPEINFOATTR(pinfo, pattr) do { if (NULL != (pinfo)) { if (NULL != (pattr)) { (pinfo)->ReleaseTypeAttr(pattr); (pattr) = NULL; } (pinfo)->Release(); (pinfo) = NULL; } } while (0)
#endif

#ifndef REGCLOSE
#define REGCLOSE(hkey) do {if (NULL != (hkey)){ RegCloseKey(hkey); (hkey) = NULL; }} while (0)
#endif

#ifndef CLOSEPTR
#define CLOSEPTR(p) do {if (NULL != (p)) { (p)->Close(); (p) = 0; }} while (0)
#endif
// check result, cleanup if failed

#ifndef IFNULLMEMGOLABEL
#define IFNULLMEMGOLABEL(p, label) do {if (NULL == (p)){ hr = E_OUTOFMEMORY; goto label; }} while (0)
#endif

#ifndef IFNULLMEMGO
#define IFNULLMEMGO(p) IFNULLMEMGOLABEL(p, LReturn)
#endif

#ifndef IFNULLMEMRET
#define IFNULLMEMRET(p) do {if (!(p)) return E_OUTOFMEMORY; } while (0)
#endif

#ifndef IFFAILGOLABEL
#define IFFAILGOLABEL(expr, label) do {if (FAILED(hr = (expr))) goto label; } while (0)
#endif

#ifndef IFFAILGO
#define IFFAILGO(expr) IFFAILGOLABEL(expr, LReturn)
#endif

// If (expr) failed, go to LReturn with (code)
#ifndef IFFAILGORET
#define IFFAILGORET(expr, code) do {if (FAILED(hr = (expr))) { hr = (code); goto LReturn; }} while (0)
#endif

#ifndef FAILGO
#define FAILGO(hresult) do { hr = (hresult); goto LReturn; } while (0)
#endif

#ifndef IFFAILWINERRGO
#define IFFAILWINERRGO(expr) do { if (FAILED(hr = HRESULT_FROM_WIN32(expr))) goto LReturn; } while (0)
#endif

#ifndef FAILWINERRGO
#define FAILWINERRGO(expr) do { hr = HRESULT_FROM_WIN32(expr); goto LReturn; } while (0)
#endif

#ifndef IFFAILRET
#define IFFAILRET(expr) do {if (FAILED(hr = (expr))) return hr; } while (0)
#endif

#ifndef IFFAILLEAVE
#define IFFAILLEAVE(expr) do {if (FAILED(hr = (expr))) __leave; } while (0)
#endif

#ifndef FAILLEAVE
#define FAILLEAVE(expr) do { hr = (expr); __leave; } while (0)
#endif

// set optional return value

#ifndef SETRETVAL
#define SETRETVAL(ptr, val) do { if (ptr) *(ptr) = (val); } while (0)
#endif

#ifndef CHECK_POINTER
#define CHECK_POINTER(p) do { if (NULL == (p)) return E_POINTER; } while (0)
#endif

#ifndef EXPECT_POINTER
#define EXPECT_POINTER(p) do { if (NULL == (p)) return E_UNEXPECTED; } while (0)
#endif

#ifndef ARG_POINTER
#define ARG_POINTER(p) do { if (NULL == (p)) return E_INVALIDARG; } while (0)
#endif

#endif __IDIOM_H__
