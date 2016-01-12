//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if DBG
#else // DBG
#pragma warning(disable: 4189)  // initialized but unused variable (e.g. variable that may only used by assert)
#endif

#define Unused(var) var;

#define IfFailedReturn(EXPR) do { hr = (EXPR); if (FAILED(hr)) { return hr; }} while(FALSE)
#define IfFailedGoLabel(expr, label) do { hr = (expr); if (FAILED(hr)) { goto label; } } while (FALSE)
#define IfFailGo(expr) IfFailedGoLabel(hr = (expr), Error)

#define IfJsrtErrorFail(expr, ret) do { if ((expr) != JsNoError) return ret; } while (0)
#define IfJsrtErrorHR(expr) do { if((expr) != JsNoError) { hr = E_FAIL; goto Error; } } while(0)
#define IfJsrtError(expr) do { if((expr) != JsNoError) { goto Error; } } while(0)
#define IfJsrtErrorSetGo(expr) do { errorCode = (expr); if(errorCode != JsNoError) { hr = E_FAIL; goto Error; } } while(0)
#define IfFalseGo(expr) do { if(!(expr)) { hr = E_FAIL; goto Error; } } while(0)

#define WIN32_LEAN_AND_MEAN 1

#define ENABLE_TEST_HOOKS 1
#include "CommonDefines.h"

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <io.h>

#if defined(_DEBUG)
#define _DEBUG_WAS_DEFINED
#undef _DEBUG
#endif
#include <map>
#include <atlbase.h>
#ifdef _DEBUG_WAS_DEFINED
#define _DEBUG
#undef _DEBUG_WAS_DEFINED
#endif

#ifdef Assert
#undef Assert
#endif

#ifdef AssertMsg
#undef AssertMsg
#endif

#if defined(DBG)

#define AssertMsg(exp, comment)   \
do { \
if (!(exp)) \
{ \
    fprintf(stderr, "ASSERTION (%s, line %d) %s %s\n", __FILE__, __LINE__, _CRT_STRINGIZE(exp), comment); \
    fflush(stderr); \
    DebugBreak(); \
} \
} while (0)
#else
#define AssertMsg(exp, comment) ((void)0)
#endif //defined(DBG)

#define Assert(exp)             AssertMsg(exp, #exp)
#define _JSRT_
#include "chakracommon.h"
#include "TestHooksRt.h"

typedef void * Var;

#include "TestHooks.h"
#include "chakrartinterface.h"
#include "Helpers.h"
#include "HostConfigFlags.h"
#include "MessageQueue.h"
#include "WScriptJsrt.h"
