//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"

#include <string.h>
#include <stdarg.h>

// Initialization order
//  AB AutoSystemInfo
//  AD PerfCounter
//  AE PerfCounterSet
//  AM Output/Configuration
//  AN MemProtectHeap
//  AP DbgHelpSymbolManager
//  AQ CFGLogger
//  AR LeakReport
//  AS JavascriptDispatch/RecyclerObjectDumper
//  AT HeapAllocator/RecyclerHeuristic
//  AU RecyclerWriteBarrierManager
#pragma warning(disable:4075)       // initializers put in unrecognized initialization area on purpose
#pragma init_seg(".CRT$XCAM")

bool                Output::s_useDebuggerWindow = false;
CriticalSection     Output::s_critsect;
AutoFILE            Output::s_outputFile; // Create a separate output file that is not thread-local.
#ifdef ENABLE_TRACE
Js::ILogger*        Output::s_inMemoryLogger = nullptr;
Js::IStackTraceHelper* Output::s_stackTraceHelper = nullptr;
unsigned int Output::s_traceEntryId = 0;
#endif

THREAD_ST FILE*    Output::s_file = nullptr;
THREAD_ST wchar_t* Output::buffer = nullptr;
THREAD_ST size_t   Output::bufferAllocSize = 0;
THREAD_ST size_t   Output::bufferFreeSize = 0;
THREAD_ST size_t   Output::s_Column  = 0;
THREAD_ST WORD     Output::s_color = 0;
THREAD_ST bool     Output::s_hasColor = false;
THREAD_ST bool     Output::s_capture = false;

#define MAX_OUTPUT_BUFFER_SIZE 10 * 1024 * 1024  // 10 MB maximum before we force a flush

size_t __cdecl
Output::VerboseNote(const wchar_t * format, ...)
{
#ifdef ENABLE_TRACE
    if (Js::Configuration::Global.flags.Verbose)
    {
        AutoCriticalSection autocs(&s_critsect);
        va_list argptr;
        va_start(argptr, format);
        size_t size = vfwprintf(stdout, format, argptr);
        va_end(argptr);
        return size;
    }
#endif
    return 0;
}

#ifdef ENABLE_TRACE
size_t __cdecl
Output::Trace(Js::Phase phase, const wchar_t *form, ...)
{
    size_t retValue = 0;

    if(Js::Configuration::Global.flags.Trace.IsEnabled(phase))
    {
        va_list argptr;
        va_start(argptr, form);
        retValue += Output::VTrace(L"%s: ", Js::PhaseNames[static_cast<int>(phase)], form, argptr);
    }

    return retValue;
}

size_t __cdecl
Output::Trace2(Js::Phase phase, const wchar_t *form, ...)
{
    size_t retValue = 0;

    if (Js::Configuration::Global.flags.Trace.IsEnabled(phase))
    {
        va_list argptr;
        va_start(argptr, form);
        retValue += Output::VPrint(form, argptr);
    }

    return retValue;
}

size_t __cdecl
Output::TraceWithPrefix(Js::Phase phase, const wchar_t prefix[], const wchar_t *form, ...)
{
    size_t retValue = 0;

    if (Js::Configuration::Global.flags.Trace.IsEnabled(phase))
    {
        va_list argptr;
        va_start(argptr, form);
        WCHAR prefixValue[512];
        swprintf_s(prefixValue, L"%s: %s: ", Js::PhaseNames[static_cast<int>(phase)], prefix);
        retValue += Output::VTrace(L"%s", prefixValue, form, argptr);
    }

    return retValue;
}

size_t __cdecl
Output::TraceWithFlush(Js::Phase phase, const wchar_t *form, ...)
{
    size_t retValue = 0;

    if(Js::Configuration::Global.flags.Trace.IsEnabled(phase))
    {
        va_list argptr;
        va_start(argptr, form);
        retValue += Output::VTrace(L"%s:", Js::PhaseNames[static_cast<int>(phase)], form, argptr);
        Output::Flush();
    }

    return retValue;
}

size_t __cdecl
Output::TraceWithFlush(Js::Flag flag, const wchar_t *form, ...)
{
    size_t retValue = 0;

    if (Js::Configuration::Global.flags.IsEnabled(flag))
    {
        va_list argptr;
        va_start(argptr, form);
        retValue += Output::VTrace(L"[-%s]::", Js::FlagNames[static_cast<int>(flag)], form, argptr);
        Output::Flush();
    }

    return retValue;
}

size_t
Output::VTrace(const wchar_t* shortPrefixFormat, const wchar_t* prefix, const wchar_t *form, va_list argptr)
{
    size_t retValue = 0;

    if (CONFIG_FLAG(RichTraceFormat))
    {
        InterlockedIncrement(&s_traceEntryId);
        retValue += Output::Print(L"[%d ~%d %s] ", s_traceEntryId, ::GetCurrentThreadId(), prefix);
    }
    else
    {
        retValue += Output::Print(shortPrefixFormat, prefix);
    }
    retValue += Output::VPrint(form, argptr);

    // Print stack trace.
    if (s_stackTraceHelper)
    {
        const ULONG c_framesToSkip = 2; // Skip 2 frames -- Output::VTrace and Output::Trace.
        const ULONG c_frameCount = 10;  // TODO: make it configurable.
        const wchar_t callStackPrefix[] = L"call stack:";
        if (s_inMemoryLogger)
        {
            // Trace just addresses of functions, avoid symbol info as it takes too much memory.
            // One line for whole stack trace for easier parsing on the jd side.
            const size_t c_msgCharCount = _countof(callStackPrefix) + (1 + sizeof(void*) * 2) * c_frameCount; // 2 hexadecimal digits per byte + 1 for space.
            wchar_t callStackMsg[c_msgCharCount];
            void* frames[c_frameCount];
            size_t start = 0;
            size_t temp;

            temp = _snwprintf_s(callStackMsg, _countof(callStackMsg), _TRUNCATE, L"%s", callStackPrefix);
            Assert(temp != -1);
            start += temp;

            ULONG framesObtained = s_stackTraceHelper->GetStackTrace(c_framesToSkip, c_frameCount, frames);
            Assert(framesObtained <= c_frameCount);
            for (ULONG i = 0; i < framesObtained && i < c_frameCount; ++i)
            {
                Assert(_countof(callStackMsg) >= start);
                temp = _snwprintf_s(callStackMsg + start, _countof(callStackMsg) - start, _TRUNCATE, L" %p", frames[i]);
                Assert(temp != -1);
                start += temp;
            }

            retValue += Output::Print(L"%s\n", callStackMsg);
        }
        else
        {
            // Trace with full symbol info.
            retValue += Output::Print(L"%s\n", callStackPrefix);
            retValue += s_stackTraceHelper->PrintStackTrace(c_framesToSkip, c_frameCount);
        }
    }

    return retValue;
}

#ifdef BGJIT_STATS
size_t __cdecl
Output::TraceStats(Js::Phase phase, const wchar_t *form, ...)
{
    if(PHASE_STATS1(phase))
    {
        va_list argptr;
        va_start(argptr, form);
        return Output::VPrint(form, argptr);
    }
    return 0;
}
#endif
#endif ENABLE_TRACE

///----------------------------------------------------------------------------
///
/// Output::Print
///
///     Print the given format string.
///
///
///----------------------------------------------------------------------------

size_t __cdecl
Output::Print(const wchar_t *form, ...)
{
    va_list argptr;
    va_start(argptr, form);
    return Output::VPrint(form, argptr);
}

size_t __cdecl
Output::Print(int column, const wchar_t *form, ...)
{
    Output::SkipToColumn(column);
    va_list argptr;
    va_start(argptr, form);
    return Output::VPrint(form, argptr);
}

size_t __cdecl
Output::VPrint(const wchar_t *form, va_list argptr)
{
    wchar_t buf[2048];
    size_t size;

    size = _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, form, argptr);
    if(size == -1)
    {
        size = 2048;
    }
    return Output::PrintBuffer(buf, size);
}

size_t __cdecl
Output::PrintBuffer(const wchar_t * buf, size_t size)
{
    Output::s_Column += size;
    const wchar_t * endbuf = wcschr(buf, '\n');
    while (endbuf != nullptr)
    {
        Output::s_Column = size - (endbuf - buf) - 1;
        endbuf = wcschr(endbuf + 1, '\n');
    }

    bool useConsoleOrFile = true;
    if (!Output::s_capture)
    {
        if (Output::s_useDebuggerWindow)
        {
            OutputDebugStringW(buf);
            useConsoleOrFile = false;
        }
#ifdef ENABLE_TRACE
        if (Output::s_inMemoryLogger)
        {
            s_inMemoryLogger->Write(buf);
            useConsoleOrFile = false;
        }
#endif
    }

    if (useConsoleOrFile)
    {
        if (s_file == nullptr || Output::s_capture)
        {
            bool addToBuffer = true;
            if (Output::bufferFreeSize < size + 1)
            {
                if (Output::bufferAllocSize > MAX_OUTPUT_BUFFER_SIZE && !Output::s_capture)
                {
                    Output::Flush();
                    if (Output::bufferFreeSize < size + 1)
                    {
                        DirectPrint(buf);
                        addToBuffer = false;
                    }
                }
                else
                {
                    size_t oldBufferSize = bufferAllocSize - bufferFreeSize;
                    size_t newBufferAllocSize = (bufferAllocSize + size + 1) * 4 / 3;
                    wchar_t * newBuffer = (wchar_t *)realloc(buffer, (newBufferAllocSize * sizeof(wchar_t)));
                    if (newBuffer == nullptr)
                    {
                        // See if I can just flush it and print directly
                        Output::Flush();

                        // Reset the buffer
                        free(Output::buffer);
                        Output::buffer = nullptr;
                        Output::bufferAllocSize = 0;
                        Output::bufferFreeSize = 0;

                        // Print it directly
                        DirectPrint(buf);
                        addToBuffer = false;
                    }
                    else
                    {
                        bufferAllocSize = newBufferAllocSize;
                        buffer = newBuffer;
                        bufferFreeSize = bufferAllocSize - oldBufferSize;
                    }
                }
            }
            if (addToBuffer)
            {
                Assert(Output::bufferFreeSize >= size + 1);
                memcpy_s(Output::buffer + Output::bufferAllocSize - Output::bufferFreeSize, Output::bufferFreeSize * sizeof(wchar_t),
                    buf, (size + 1) * sizeof(wchar_t));
                bufferFreeSize -= size;
            }
        }
        else
        {
            fwprintf_s(Output::s_file, L"%s", buf);
        }

        if(s_outputFile != nullptr && !Output::s_capture)
        {
            fwprintf_s(s_outputFile, L"%s", buf);
        }
    }

    if (IsDebuggerPresent())
    {
        Output::Flush();
    }

    return size;
}

void Output::Flush()
{
    if (s_capture)
    {
        return;
    }
    if (bufferFreeSize != bufferAllocSize)
    {
        DirectPrint(Output::buffer);
        bufferFreeSize = bufferAllocSize;
    }
    if(s_outputFile != nullptr)
    {
        fflush(s_outputFile);
    }
    _flushall();
}

void Output::DirectPrint(wchar_t const * string)
{
    AutoCriticalSection autocs(&s_critsect);
    WORD oldValue = 0;
    BOOL restoreColor = FALSE;
    HANDLE hConsole = NULL;
    if (Output::s_hasColor)
    {
        _CONSOLE_SCREEN_BUFFER_INFO info;
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

        if (hConsole && GetConsoleScreenBufferInfo(hConsole, &info))
        {
            oldValue = info.wAttributes;
            restoreColor = SetConsoleTextAttribute(hConsole, Output::s_color);
        }
    }
    fwprintf(stdout, L"%s", string);

    if (restoreColor)
    {
        SetConsoleTextAttribute(hConsole, oldValue);
    }
}
///----------------------------------------------------------------------------
///
/// Output::SkipToColumn
///
///     Inserts spaces up to the column passed in.
///
///----------------------------------------------------------------------------

void
Output::SkipToColumn(size_t column)
{
    if (column <= Output::s_Column)
    {
        Output::Print(L" ");
        return;
    }

    // compute distance to our destination

    size_t dist = column - Output::s_Column;

    // Print at least one space
    while (dist > 0)
    {
        Output::Print(L" ");
        dist--;
    }
}

FILE*
Output::GetFile()
{
    return Output::s_file;
}

FILE*
Output::SetFile(FILE *file)
{
    Output::Flush();
    FILE *oldfile = Output::s_file;
    Output::s_file = file;
    return oldfile;
}

void
Output::SetOutputFile(FILE* file)
{
    if(s_outputFile != nullptr)
    {
        AssertMsg(false, "Output file is being set twice.");
    }
    else
    {
        s_outputFile = file;
    }
}

FILE*
Output::GetOutputFile()
{
    return s_outputFile;
}

#ifdef ENABLE_TRACE
void
Output::SetInMemoryLogger(Js::ILogger* logger)
{
    AssertMsg(s_inMemoryLogger == nullptr, "This cannot be called more than once.");
    s_inMemoryLogger = logger;
}

void
Output::SetStackTraceHelper(Js::IStackTraceHelper* helper)
{
    AssertMsg(s_stackTraceHelper == nullptr, "This cannot be called more than once.");
#ifndef STACK_BACK_TRACE
    AssertMsg("STACK_BACK_TRACE must be defined");
#endif
    s_stackTraceHelper = helper;
}
#endif ENABLE_TRACE

//
// Sets the foreground color and returns the old color. Returns 0 on failure
//

WORD
Output::SetConsoleForeground(WORD color)
{
    AutoCriticalSection autocs(&s_critsect);
    _CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    if (hConsole && GetConsoleScreenBufferInfo(hConsole, &info))
    {
        Output::Flush();
        Output::s_color = color | (info.wAttributes & ~15);
        Output::s_hasColor = Output::s_color != info.wAttributes;
        return info.wAttributes;
    }

    return 0;
}

void
Output::CaptureStart()
{
    Assert(!s_capture);
    Output::Flush();
    s_capture = true;
}

wchar_t *
Output::CaptureEnd()
{
    Assert(s_capture);
    s_capture = false;
    bufferFreeSize = 0;
    bufferAllocSize = 0;
    wchar_t * returnBuffer = buffer;
    buffer = nullptr;
    return returnBuffer;
}
