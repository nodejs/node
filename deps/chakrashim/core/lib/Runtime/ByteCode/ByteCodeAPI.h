//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

HRESULT GenerateByteCode(__in ParseNode *pnode, __in ulong grfscr, __in Js::ScriptContext* scriptContext, __inout Js::ParseableFunctionInfo ** ppRootFunc,
                         __in uint sourceIndex, __in bool forceNoNative, __in Parser* parser, __in CompileScriptException *pse, Js::ScopeInfo* parentScopeInfo = nullptr,
                         Js::ScriptFunction ** functionRef = nullptr);

//
// Output for -Trace:ByteCode
//
#if DBG_DUMP
#define TRACE_BYTECODE(fmt, ...) \
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::ByteCodePhase)) \
    { \
        Output::Print(fmt, __VA_ARGS__); \
    }
#else
#define TRACE_BYTECODE(fmt, ...)
#endif
