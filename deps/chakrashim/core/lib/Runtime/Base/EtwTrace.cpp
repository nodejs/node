//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#ifdef ENABLE_JS_ETW
#include "core\EtwTraceCore.h"
#include "Base\EtwTrace.h"

#ifdef VTUNE_PROFILING
#ifdef CDECL
#define ORIGINAL_CDECL CDECL
#undef CDECL
#endif
// Not enabled in ChakraCore
#include "jitProfiling.h"
#ifdef ORIGINAL_CDECL
#undef CDECL
#endif
#define CDECL ORIGINAL_CDECL
#endif

using namespace Js;

//
// This C style callback is invoked by ETW when a trace session is started/stopped
// by an ETW controller for the Jscript and MSHTML providers.
//

static const char LoopStr[] = "Loop";
static const wchar_t LoopWStr[] = L"Loop";

void EtwCallbackApi::OnSessionChange(ULONG controlCode, PVOID callbackContext)
{
    PMCGEN_TRACE_CONTEXT context = (PMCGEN_TRACE_CONTEXT)callbackContext;

    // A manifest based provider can be enabled to multiple event tracing sessions
    // As long as there is at least 1 enabled session, isEnabled will be TRUE
    // We only care about Jscript events.
    if(context->RegistrationHandle == Microsoft_JScriptHandle)
    {
        switch(controlCode)
        {
        case EVENT_CONTROL_CODE_ENABLE_PROVIDER:
        case EVENT_CONTROL_CODE_CAPTURE_STATE:
            if(McGenLevelKeywordEnabled(context,
                TRACE_LEVEL_INFORMATION,
                JSCRIPT_RUNDOWNSTART_KEYWORD))
            {
                EtwTrace::PerformRundown(/*start*/ true);
            }

            if(McGenLevelKeywordEnabled(context,
                TRACE_LEVEL_INFORMATION,
                JSCRIPT_RUNDOWNEND_KEYWORD))
            {
                EtwTrace::PerformRundown(/*start*/ false);
            }
            break;
        case EVENT_CONTROL_CODE_DISABLE_PROVIDER:
            break; // Do Nothing
        }
    }
}

#ifdef VTUNE_PROFILING
const utf8char_t EtwTrace::DynamicCode[] = "Dynamic code";
bool EtwTrace::isJitProfilingActive = false;
#endif

//
// Registers the ETW provider - this is usually done on Jscript DLL load
// After registration, we will receive callbacks when ETW tracing is enabled/disabled.
//
void EtwTrace::Register()
{
    EtwTraceCore::Register();

#ifdef TEST_ETW_EVENTS
    TestEtwEventSink::Load();
#endif

#ifdef VTUNE_PROFILING
    isJitProfilingActive = (iJIT_IsProfilingActive() == iJIT_SAMPLING_ON);
#endif
}

//
// Unregister to ensure we do not get callbacks.
//
void EtwTrace::UnRegister()
{
    EtwTraceCore::UnRegister();

#ifdef TEST_ETW_EVENTS
    TestEtwEventSink::Unload();
#endif

#ifdef VTUNE_PROFILING
    if(isJitProfilingActive)
    {
        iJIT_NotifyEvent(iJVM_EVENT_TYPE_SHUTDOWN, NULL);
    }
#endif
}

//
// Enumerate through all the script contexts in the process and log events
// for each function loaded. Depending on the argument, start or end events are logged.
// In particular, a rundown is needed for the 'Attach' scenario of profiling.
//
void EtwTrace::PerformRundown(bool start)
{
    // Lock threadContext list during etw rundown
    AutoCriticalSection autoThreadContextCs(ThreadContext::GetCriticalSection());

    ThreadContext * threadContext = ThreadContext::GetThreadContextList();
    if(start)
    {
        JS_ETW(EventWriteDCStartInit());
    }
    else
    {
        JS_ETW(EventWriteDCEndInit());
    }

    while(threadContext != nullptr)
    {
        // Take etw rundown lock on this thread context
        AutoCriticalSection autoEtwRundownCs(threadContext->GetEtwRundownCriticalSection());

        ScriptContext* scriptContext = threadContext->GetScriptContextList();
        while(scriptContext != NULL)
        {
            if(scriptContext->IsClosed())
            {
                scriptContext = scriptContext->next;
                continue;
            }
            if(start)
            {
                JS_ETW(EventWriteScriptContextDCStart(scriptContext));

                if(scriptContext->GetSourceContextInfoMap() != nullptr)
                {
                    scriptContext->GetSourceContextInfoMap()->Map( [=] (DWORD_PTR sourceContext, SourceContextInfo * sourceContextInfo)
                    {
                        if (sourceContext != Constants::NoHostSourceContext)
                        {
                            JS_ETW(LogSourceEvent(EventWriteSourceDCStart,
                                sourceContext,
                                scriptContext,
                                /* sourceFlags*/ 0,
                                sourceContextInfo->url));
                        }
                    });
                }
            }
            else
            {
                JS_ETW(EventWriteScriptContextDCEnd(scriptContext));

                if(scriptContext->GetSourceContextInfoMap() != nullptr)
                {
                    scriptContext->GetSourceContextInfoMap()->Map( [=] (DWORD_PTR sourceContext, SourceContextInfo * sourceContextInfo)
                    {
                        if (sourceContext != Constants::NoHostSourceContext)
                        {
                            JS_ETW(LogSourceEvent(EventWriteSourceDCEnd,
                                sourceContext,
                                scriptContext,
                                /* sourceFlags*/ 0,
                                sourceContextInfo->url));
                        }
                    });
                }
            }

            scriptContext->MapFunction([&start] (FunctionBody* body)
            {
                if(body->HasInterpreterThunkGenerated())
                {
                    if(start)
                    {
                        LogMethodInterpretedThunkEvent(EventWriteMethodDCStart, body);
                    }
                    else
                    {
                        LogMethodInterpretedThunkEvent(EventWriteMethodDCEnd, body);
                    }
                }

                body->MapEntryPoints([&](int index, FunctionEntryPointInfo * entryPoint)
                {
                    if(entryPoint->IsCodeGenDone())
                    {
                        if (start)
                        {
                            LogMethodNativeEvent(EventWriteMethodDCStart, body, entryPoint);
                        }
                        else
                        {
                            LogMethodNativeEvent(EventWriteMethodDCEnd, body, entryPoint);
                        }
                    }
                });

                body->MapLoopHeaders([&](uint loopNumber, LoopHeader* header)
                {
                    header->MapEntryPoints([&](int index, LoopEntryPointInfo * entryPoint)
                    {
                        if(entryPoint->IsCodeGenDone())
                        {
                            if(start)
                            {
                                LogLoopBodyEvent(EventWriteMethodDCStart, body, header, entryPoint);
                            }
                            else
                            {
                                LogLoopBodyEvent(EventWriteMethodDCEnd, body, header, entryPoint);
                            }
                        }
                    });
                });
            });

            scriptContext = scriptContext->next;
        }
        if (EventEnabledJSCRIPT_HOSTING_CEO_START())
        {
            threadContext->EtwLogPropertyIdList();
        }

        threadContext = threadContext->Next();
    }
    if(start)
    {
        JS_ETW(EventWriteDCStartComplete());
    }
    else
    {
        JS_ETW(EventWriteDCEndComplete());
    }
}

//
// Returns an ID for the source file of the function.
//
DWORD_PTR EtwTrace::GetSourceId(FunctionBody* body)
{
    DWORD_PTR sourceId = body->GetHostSourceContext();

    // For dynamic scripts - use fixed source ID of -1.
    // TODO: Find a way to generate unique ID for dynamic scripts.
    if(sourceId == Js::Constants::NoHostSourceContext)
    {
        sourceId = (DWORD_PTR)-1;
    }
    return sourceId;
}

//
// Returns an ID to identify the function.
//
uint EtwTrace::GetFunctionId(FunctionProxy* body)
{
    return body->GetFunctionNumber();
}

void EtwTrace::LogSourceUnloadEvents(ScriptContext* scriptContext)
{
    if(scriptContext->GetSourceContextInfoMap() != nullptr)
    {
        scriptContext->GetSourceContextInfoMap()->Map( [&] (DWORD_PTR sourceContext, SourceContextInfo * sourceContextInfo)
        {
            if(sourceContext != Constants::NoHostSourceContext)
            {
                JS_ETW(LogSourceEvent(EventWriteSourceUnload,
                    sourceContext,
                    scriptContext,
                    /* sourceFlags*/ 0,
                    sourceContextInfo->url));
            }
        });
    }

    JS_ETW(EventWriteScriptContextUnload(scriptContext));
}

void EtwTrace::LogMethodInterpreterThunkLoadEvent(FunctionBody* body)
{
    LogMethodInterpretedThunkEvent(EventWriteMethodLoad, body);
}

void EtwTrace::LogMethodNativeLoadEvent(FunctionBody* body, FunctionEntryPointInfo* entryPoint)
{
    LogMethodNativeEvent(EventWriteMethodLoad, body, entryPoint);

#ifdef VTUNE_PROFILING
    if(isJitProfilingActive)
    {
        iJIT_Method_Load methodInfo;
        memset(&methodInfo, 0, sizeof(iJIT_Method_Load));
        const wchar_t* methodName = body->GetExternalDisplayName();
        // Append function line number info to method name so that VTune can distinguish between polymorphic methods
        wchar_t methodNameBuffer[_MAX_PATH];
        ULONG lineNumber = body->GetLineNumber();
        wchar_t numberBuffer[20];
        _ltow_s(lineNumber, numberBuffer, 10);
        wcscpy_s(methodNameBuffer, methodName);
        if(entryPoint->GetJitMode() == ExecutionMode::SimpleJit)
        {
            wcscat_s(methodNameBuffer, L" Simple");
        }
        wcscat_s(methodNameBuffer, L" {line:");
        wcscat_s(methodNameBuffer, numberBuffer);
        wcscat_s(methodNameBuffer, L"}");

        size_t methodLength = wcslen(methodNameBuffer);
        Assert(methodLength < _MAX_PATH);
        size_t length = methodLength * 3 + 1;
        utf8char_t* utf8MethodName = HeapNewNoThrowArray(utf8char_t, length);
        if(utf8MethodName)
        {
            methodInfo.method_id = iJIT_GetNewMethodID();
            utf8::EncodeIntoAndNullTerminate(utf8MethodName, methodNameBuffer, (charcount_t)methodLength);
            methodInfo.method_name = (char*)utf8MethodName;
            methodInfo.method_load_address = (void*)entryPoint->GetNativeAddress();
            methodInfo.method_size = (uint)entryPoint->GetCodeSize();        // Size in memory - Must be exact

            LineNumberInfo numberInfo[1];

            uint lineCount = (entryPoint->GetNativeOffsetMapCount()) * 2 + 1; // may need to record both .begin and .end for all elements
            LineNumberInfo* pLineInfo = HeapNewNoThrowArray(LineNumberInfo, lineCount);

            if (pLineInfo == NULL || Js::Configuration::Global.flags.DisableVTuneSourceLineInfo)
            {
                // resort to original implementation, attribute all samples to first line
                numberInfo[0].LineNumber = lineNumber;
                numberInfo[0].Offset = 0;
                methodInfo.line_number_size = 1;
                methodInfo.line_number_table = numberInfo;
            }
            else
            {
                int size = entryPoint->PopulateLineInfo(pLineInfo, body);
                methodInfo.line_number_size = size;
                methodInfo.line_number_table = pLineInfo;
            }

            size_t urlLength  = 0;
            utf8char_t* utf8Url = GetUrl(body, &urlLength);
            methodInfo.source_file_name = (char*)utf8Url;
            methodInfo.env = iJDE_JittingAPI;
            OUTPUT_TRACE(Js::ProfilerPhase, L"Method load event: %s\n", methodNameBuffer);
            iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, &methodInfo);

            HeapDeleteArray(lineCount, pLineInfo);

            if(urlLength > 0)
            {
                HeapDeleteArray(urlLength, utf8Url);
            }

            HeapDeleteArray(length, utf8MethodName);
        }
    }
#endif
}

void EtwTrace::LogLoopBodyLoadEvent(FunctionBody* body, LoopHeader* loopHeader, LoopEntryPointInfo* entryPoint)
{
    LogLoopBodyEvent(EventWriteMethodLoad, body, loopHeader, entryPoint);

#ifdef VTUNE_PROFILING
    if(isJitProfilingActive)
    {
        iJIT_Method_Load methodInfo;
        memset(&methodInfo, 0, sizeof(iJIT_Method_Load));
        const wchar_t* methodName = body->GetExternalDisplayName();
        size_t methodLength = wcslen(methodName);
        methodLength = min(methodLength, (size_t)UINT_MAX); // Just truncate if it is too big
        size_t length = methodLength * 3 + /* spaces */ 2 + _countof(LoopStr) + /*size of loop number*/ 10 + /*NULL*/ 1;
        utf8char_t* utf8MethodName = HeapNewNoThrowArray(utf8char_t, length);
        if(utf8MethodName)
        {
            methodInfo.method_id = iJIT_GetNewMethodID();
            size_t len = utf8::EncodeInto(utf8MethodName, methodName, (charcount_t)methodLength);
            uint loopNumber = body->GetLoopNumber(loopHeader) + 1;
            sprintf_s((char*)(utf8MethodName + len), length - len," %s %d", LoopStr, loopNumber);
            methodInfo.method_name = (char*)utf8MethodName;
            methodInfo.method_load_address = (void*)entryPoint->GetNativeAddress();
            methodInfo.method_size = (uint)entryPoint->GetCodeSize();        // Size in memory - Must be exact

            size_t urlLength  = 0;
            utf8char_t* utf8Url = GetUrl(body, &urlLength);
            methodInfo.source_file_name = (char*)utf8Url;
            methodInfo.env = iJDE_JittingAPI;

            iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, &methodInfo);
            OUTPUT_TRACE(Js::ProfilerPhase, L"Loop body load event: %s Loop %d\n", methodName, loopNumber);

            if(urlLength > 0)
            {
                HeapDeleteArray(urlLength, utf8Url);
            }
            HeapDeleteArray(length, utf8MethodName);
        }
    }
#endif
}

void EtwTrace::LogMethodInterpreterThunkUnloadEvent(FunctionBody* body)
{
    LogMethodInterpretedThunkEvent(EventWriteMethodUnload, body);
}


void EtwTrace::LogMethodNativeUnloadEvent(FunctionBody* body, FunctionEntryPointInfo* entryPoint)
{
    LogMethodNativeEvent(EventWriteMethodUnload, body, entryPoint);
}

void EtwTrace::LogLoopBodyUnloadEvent(FunctionBody* body, LoopHeader* loopHeader, LoopEntryPointInfo* entryPoint)
{
    LogLoopBodyEvent(EventWriteMethodUnload, body, loopHeader, entryPoint);
}


//
// Logs the runtime script context load event
//
void EtwTrace::LogScriptContextLoadEvent(ScriptContext* scriptContext)
{
    JS_ETW(EventWriteScriptContextLoad(
        scriptContext));
}

//
// Logs the runtime source module load event.
//
void EtwTrace::LogSourceModuleLoadEvent(ScriptContext* scriptContext, DWORD_PTR sourceContext, _In_z_ const wchar_t* url)
{
    AssertMsg(sourceContext != Constants::NoHostSourceContext, "We should not be logged this if there is no source code available");

    JS_ETW(LogSourceEvent(EventWriteSourceLoad,
        sourceContext,
        scriptContext,
        /* sourceFlags*/ 0,
        url));
}

//
// This emulates the logic used by the F12 profiler to give names to functions
//
const wchar_t* EtwTrace::GetFunctionName(FunctionBody* body)
{
    return body->GetExternalDisplayName();
}

size_t EtwTrace::GetLoopBodyName(_In_ FunctionBody* body, _In_ LoopHeader* loopHeader, _Out_writes_opt_z_(size) wchar_t* nameBuffer, _In_ size_t size)
{
    return body->GetLoopBodyName(body->GetLoopNumber(loopHeader), nameBuffer, size);
}

_Success_(return == 0)
size_t EtwTrace::GetSimpleJitFunctionName(
    Js::FunctionBody *const body,
    _Out_writes_opt_z_(nameCharCapacity) wchar_t *const name,
    const size_t nameCharCapacity)
{
    Assert(body);
    Assert(name);
    Assert(nameCharCapacity != 0);

    const wchar_t *const suffix = L"Simple";
    const size_t suffixCharLength = _countof(L"Simple") - 1;

    const wchar_t *const functionName = GetFunctionName(body);
    const size_t functionNameCharLength = wcslen(functionName);
    const size_t requiredCharCapacity = functionNameCharLength + suffixCharLength + 1;
    if(requiredCharCapacity > nameCharCapacity)
    {
        return requiredCharCapacity;
    }

    wcscpy_s(name, nameCharCapacity, functionName);
    wcscpy_s(&name[functionNameCharLength], nameCharCapacity - functionNameCharLength, suffix);
    return 0;
}

#ifdef VTUNE_PROFILING
utf8char_t* EtwTrace::GetUrl( FunctionBody* body, size_t* urlBufferLength )
{
    utf8char_t* utf8Url = NULL;
    if(!body->GetSourceContextInfo()->IsDynamic())
    {
        const wchar* url = body->GetSourceContextInfo()->url;
        if(url)
        {
            size_t urlCharLength = wcslen(url);
            urlCharLength = min(urlCharLength, (size_t)UINT_MAX);       // Just truncate if it is too big

            *urlBufferLength = urlCharLength * 3 + 1;
            utf8Url = HeapNewNoThrowArray(utf8char_t, *urlBufferLength);
            if (utf8Url)
            {
                utf8::EncodeIntoAndNullTerminate(utf8Url, url, (charcount_t)urlCharLength);
            }
        }
    }
    else
    {
        utf8Url = (utf8char_t*)EtwTrace::DynamicCode;
    }
    return utf8Url;
}
#endif

#endif
