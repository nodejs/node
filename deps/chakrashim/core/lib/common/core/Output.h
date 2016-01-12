//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
namespace Js
{
    enum Flag;
    enum Phase;
};

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS) && defined(BGJIT_STATS)
#define OUTPUT_TRACE(Phase, ...) Output::Trace((Phase), __VA_ARGS__)
#define OUTPUT_TRACE_2(Phase, ...) Output::Trace2((Phase), __VA_ARGS__)
#define OUTPUT_VERBOSE_TRACE(Phase, ...) \
    if(Js::Configuration::Global.flags.Verbose) \
    { \
        OUTPUT_TRACE((Phase), __VA_ARGS__); \
    }
#define OUTPUT_STATS(Phase, ...) Output::TraceStats((Phase), __VA_ARGS__)
#define OUTPUT_VERBOSE_STATS(Phase, ...) \
    if(Js::Configuration::Global.flags.Verbose) \
    { \
        OUTPUT_STATS((Phase), __VA_ARGS__); \
    }
#define OUTPUT_FLUSH() Output::Flush()
#else
#define OUTPUT_TRACE(Phase, ...)
#define OUTPUT_TRACE_2(Phase, ...)
#define OUTPUT_VERBOSE_TRACE(Phase, ...)
#define OUTPUT_STATS(Phase, ...)
#define OUTPUT_VERBOSE_STATS(Phase, ...)
#define OUTPUT_FLUSH()
#endif

#if DBG
#define OUTPUT_TRACE_DEBUGONLY_ENABLED 1
#define OUTPUT_TRACE_DEBUGONLY(Phase, ...) Output::TraceWithFlush((Phase), __VA_ARGS__)
#define OUTPUT_TRACE_DEBUGONLY(Flag, ...) Output::TraceWithFlush((Flag), __VA_ARGS__)
#else
#define OUTPUT_TRACE_DEBUGONLY(Phase, ...)
#define OUTPUT_TRACE_FLAG_DEBUGONLY(Flag, ...)
#endif

namespace Js
{
    // Logging interfaces:
    // decouple implementation so that in common.lib we don't have dependency on memory.lib
    struct ILogger
    {
        virtual void Write(const wchar_t* msg) = 0;
    };
    struct IStackTraceHelper
    {
        virtual size_t PrintStackTrace(ULONG framesToSkip, ULONG framesToCapture) = 0;  // Returns # of chars printed.
        virtual ULONG GetStackTrace(ULONG framesToSkip, ULONG framesToCapture, void** stackFrames) = 0; // Returns # of frames captured.
    };
} // namespace Js.


class Output
{
public:
    static size_t __cdecl VerboseNote(const wchar_t * format, ...);
#ifdef ENABLE_TRACE
    static size_t __cdecl Trace(Js::Phase phase, const wchar_t *form, ...);
    static size_t __cdecl Trace2(Js::Phase phase, const wchar_t *form, ...);
    static size_t __cdecl TraceWithPrefix(Js::Phase phase, const wchar_t prefix[], const wchar_t *form, ...);
    static size_t __cdecl TraceWithFlush(Js::Phase phase, const wchar_t *form, ...);
    static size_t __cdecl TraceWithFlush(Js::Flag flag, const wchar_t *form, ...);
    static size_t __cdecl TraceStats(Js::Phase phase, const wchar_t *form, ...);
    template<class Fn>
    static size_t __cdecl
    TraceWithCallback(Js::Phase phase, Fn callback, const wchar_t *form, ...)
    {
        size_t retValue = 0;

        if(Js::Configuration::Global.flags.Trace.IsEnabled(phase))
        {
            va_list argptr;
            va_start(argptr, form);
            retValue = Output::Print(L"%s:", Js::PhaseNames[static_cast<int>(phase)]);
            retValue += Output::VPrint(form, argptr);
            retValue += Output::Print(L"%s", callback());
        }

        return retValue;
    }
    static void     SetInMemoryLogger(Js::ILogger* logger);
    static void     SetStackTraceHelper(Js::IStackTraceHelper* helper);
#endif ENABLE_TRACE
    static size_t __cdecl Print(const wchar_t *form, ...);
    static size_t __cdecl Print(int column, const wchar_t *form, ...);
    static size_t __cdecl PrintBuffer(const wchar_t * buffer, size_t size);
    static size_t __cdecl VPrint(const wchar_t *form, va_list argptr);
    static void     SkipToColumn(size_t column);
    static FILE*    SetFile(FILE *);
    static FILE*    GetFile();
    static void     SetOutputFile(FILE *);
    static FILE*    GetOutputFile();
    static void     UseDebuggerWindow() { s_useDebuggerWindow = true; }
    static void     Flush();

    static WORD     SetConsoleForeground(WORD color);
    static void     CaptureStart();
    static wchar_t* CaptureEnd();

private:
    static void     DirectPrint(const wchar_t * string);

    static AutoFILE s_outputFile;
    static bool     s_useDebuggerWindow;
    static CriticalSection s_critsect;

#ifdef ENABLE_TRACE
    static Js::ILogger* s_inMemoryLogger;       // Used to trace into memory so that when process crashes, you can see tracing in crash dump file.
    static unsigned int s_traceEntryId;         // Sequential id of trace entry for rich output format.
#ifdef STACK_BACK_TRACE
    static Js::IStackTraceHelper* s_stackTraceHelper; // Used for capturing stack trace.
#endif
    static size_t VTrace(const wchar_t* shortPrefixFormat, const wchar_t* prefix, const wchar_t *form, va_list argptr);
#endif ENABLE_TRACE

#define THREAD_ST __declspec(thread)

    THREAD_ST static bool s_capture;
    THREAD_ST static FILE * s_file;
    THREAD_ST static wchar_t * buffer;
    THREAD_ST static size_t bufferFreeSize;
    THREAD_ST static size_t bufferAllocSize;
    THREAD_ST static size_t s_Column;
    THREAD_ST static WORD s_color;
    THREAD_ST static bool s_hasColor;
};
