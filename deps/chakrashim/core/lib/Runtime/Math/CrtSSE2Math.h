//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if _M_IX86
// This CRT routines skip some special condition checks for FPU state
// These routines are not expected to be called outside of the CRT, so using these should
// be re-evaluated when upgrading toolsets.
// Mark them explicitly as dllimport to workaround VC bug (Dev11:909888)
#ifdef USE_STATIC_RUNTIMELIB
#define _DLLIMPORT
#else
#define _DLLIMPORT __declspec(dllimport)
#endif
extern "C" double _DLLIMPORT __cdecl __libm_sse2_acos(double);
extern "C" double _DLLIMPORT __cdecl __libm_sse2_asin(double);
extern "C" double _DLLIMPORT __cdecl __libm_sse2_atan(double);
extern "C" double _DLLIMPORT __cdecl __libm_sse2_atan2(double,double);
extern "C" double _DLLIMPORT __cdecl __libm_sse2_cos(double);
extern "C" double _DLLIMPORT __cdecl __libm_sse2_exp(double);
extern "C" double _DLLIMPORT __cdecl __libm_sse2_pow(double,double);
extern "C" double _DLLIMPORT __cdecl __libm_sse2_log(double);
extern "C" double _DLLIMPORT __cdecl __libm_sse2_log10(double);
extern "C" double _DLLIMPORT __cdecl __libm_sse2_sin(double);
extern "C" double _DLLIMPORT __cdecl __libm_sse2_tan(double);
#undef _DLLIMPORT
#endif
