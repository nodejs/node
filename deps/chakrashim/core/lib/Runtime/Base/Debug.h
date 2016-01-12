//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if DBG_DUMP
WCHAR* DumpCallStackFull(uint frameCount = -1, bool print = true);
WCHAR* DumpCallStack(uint frameCount = -1);
#endif

#if DBG_DUMP
#define OUTPUT_TRACE_WITH_STACK(Phase, ...) OUTPUT_TRACE_WITH_STACK_COUNT(Phase, /*frameCount*/ 3, __VA_ARGS__)
#define OUTPUT_TRACE_WITH_STACK_COUNT(Phase, frameCount, ...)  \
    if(Js::Configuration::Global.flags.Verbose) \
        { \
        Output::TraceWithCallback( Phase, [] () { \
            return DumpCallStackFull(frameCount, /*print*/false); \
    }, __VA_ARGS__); \
                                }
#else
#define OUTPUT_TRACE_WITH_STACK(Phase, ...)
#define OUTPUT_TRACE_WITH_STACK_COUNT(Phase, frameCount, ...)
#endif

#if ENABLE_DEBUG_CONFIG_OPTIONS
#define OUTPUT_PRINT(FunctionBody) \
    Output::Print(L"Function %s (#%d.%u, #%u) ", (FunctionBody)->GetDisplayName(), \
            (int)(FunctionBody)->GetSourceContextId(), (FunctionBody)->GetLocalFunctionId(), (FunctionBody)->GetFunctionNumber());

#define OUTPUT_TRACE2(Phase, FunctionBody, ...) \
   if(Js::Configuration::Global.flags.Trace.IsEnabled((Phase))) \
   { \
        WCHAR prefixValue[512]; \
        swprintf_s(prefixValue, L"Function %s (#%d.%u, #%u)", (FunctionBody)->GetDisplayName(), \
            (int)(FunctionBody)->GetSourceContextId(), (FunctionBody)->GetLocalFunctionId(), (FunctionBody)->GetFunctionNumber()); \
        Output::TraceWithPrefix((Phase), prefixValue, __VA_ARGS__); \
   }
#define OUTPUT_TRACE_FUNC(Phase, Func, ...) \
   if(PHASE_TRACE((Phase), (Func))) \
      { \
        WCHAR workItem[256]; \
        Func->m_workItem->GetDisplayName(workItem, _countof(workItem)); \
        WCHAR prefixValue[512]; \
        swprintf_s(prefixValue, L"%s (#%d.%u, #%u)", workItem, \
           (int)(Func)->GetJnFunction()->GetSourceContextId(), (Func)->GetJnFunction()->GetLocalFunctionId(), (Func)->GetJnFunction()->GetFunctionNumber()); \
        Output::TraceWithPrefix((Phase), prefixValue, __VA_ARGS__); \
      }
#define OUTPUT_VERBOSE_TRACE2(Phase, FunctionBody, ...) \
    if(Js::Configuration::Global.flags.Verbose) \
    { \
        OUTPUT_TRACE2((Phase), (FunctionBody), __VA_ARGS__); \
    }
#define OUTPUT_VERBOSE_TRACE_FUNC(Phase, Func, ...) \
     if(Js::Configuration::Global.flags.Verbose) \
     { \
        OUTPUT_TRACE_FUNC((Phase), (Func), __VA_ARGS__); \
       }
#else
#define OUTPUT_PRINT(FunctionBody)
#define OUTPUT_TRACE2(Phase, FunctionBody, ...)
#define OUTPUT_VERBOSE_TRACE2(Phase, FunctionBody, ...)
#define OUTPUT_TRACE_FUNC(Phase, Func, ...)
#define OUTPUT_VERBOSE_TRACE_FUNC(Phase, Func, ...)
#endif

