//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#if DBG_DUMP
#include <strsafe.h>
#include "Language\JavascriptStackWalker.h"

WCHAR* DumpCallStack(uint frameCount) { return DumpCallStackFull(frameCount, /*print*/ true); }

WCHAR* DumpCallStackFull(uint frameCount, bool print)
{
    Js::ScriptContext* scriptContext = ThreadContext::GetContextForCurrentThread()->GetScriptContextList();
    Js::JavascriptStackWalker walker(scriptContext);

    WCHAR buffer[512];
    Js::StringBuilder<ArenaAllocator> sb(scriptContext->GeneralAllocator());
    uint fc = 0;
    while (walker.Walk())
    {
        void * codeAddr = walker.GetCurrentCodeAddr();
        if (walker.IsJavascriptFrame())
        {
            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            StringCchPrintf(buffer, _countof(buffer), L"0x%p  ", codeAddr);
            sb.AppendSz(buffer);
            // Found a JavascriptFunction.  Dump its name and parameters.
            Js::JavascriptFunction *jsFunc = walker.GetCurrentFunction();

            Js::FunctionBody * jsBody = jsFunc->GetFunctionBody();
            Js::CallInfo const * callInfo = walker.GetCallInfo();
            const WCHAR* sourceFileName = L"NULL";
            ULONG line = 0; LONG column = 0;
            walker.GetSourcePosition(&sourceFileName, &line, &column);

            StringCchPrintf(buffer, _countof(buffer), L"%s [%s] (0x%p, Args=%u", jsBody->GetDisplayName(), jsBody->GetDebugNumberSet(debugStringBuffer), jsFunc,
                callInfo->Count);
            sb.AppendSz(buffer);

            for (uint i = 0; i < callInfo->Count; i++)
            {
                StringCchPrintf(buffer, _countof(buffer), L", 0x%p", walker.GetJavascriptArgs()[i]);
                sb.AppendSz(buffer);
            }
            StringCchPrintf(buffer, _countof(buffer), L")[%s (%u, %d)]\n", sourceFileName, line + 1, column + 1);
            sb.AppendSz(buffer);
            fc++;
            if(fc >= frameCount)
            {
                break;
            }
       }
    }
    sb.AppendCppLiteral(L"----------------------------------------------------------------------\n");
    WCHAR* stack = sb.Detach();
    if(print)
    {
        Output::Print(stack);
    }
    return stack;
}
#endif
