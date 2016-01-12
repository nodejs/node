//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"
#include "MemoryLogger.h"

#ifdef ENABLE_TRACE
namespace Js
{
    MemoryLogger::MemoryLogger(TAllocator* alloc, ULONG count) : m_alloc(alloc), m_capacity(count), m_current(0)
    {
        m_log = AnewArrayZ(m_alloc, wchar_t*, m_capacity);
    }

    MemoryLogger::~MemoryLogger()
    {
        Adelete(m_alloc, m_log);
    }

    void MemoryLogger::Write(const wchar_t* msg)
    {
#ifdef EXCEPTION_CHECK
        // In most cases this will be called at runtime when we have exception check enabled.
        AutoNestedHandledExceptionType autoNestedHandledExceptionType(ExceptionType_DisableCheck);
#endif
        AutoCriticalSection autocs(&m_criticalSection); // TODO: with circular buffer now we can use much granular lock.

        // Create a copy of the message.
        size_t len = wcslen(msg);
        wchar_t* buf = AnewArray(m_alloc, wchar_t, len + 1);
        js_wmemcpy_s(buf, len + 1, msg, len + 1);   // Copy with the NULL-terminator.

        // m_current is the next position to write to.
        if (m_log[m_current])
        {
            Adelete(m_alloc, m_log[m_current]);
        }
        m_log[m_current] = buf;
        m_current = (m_current + 1) % m_capacity;
    }

    MemoryLogger* MemoryLogger::Create(TAllocator* alloc, ULONG count)
    {
#ifdef EXCEPTION_CHECK
        AutoNestedHandledExceptionType autoNestedHandledExceptionType(ExceptionType_DisableCheck);
#endif
        return Anew(alloc, MemoryLogger, alloc, count);
    }

#ifdef STACK_BACK_TRACE
    StackTraceHelper* StackTraceHelper::Create(TAllocator* alloc)
    {
#ifdef EXCEPTION_CHECK
        AutoNestedHandledExceptionType autoNestedHandledExceptionType(ExceptionType_DisableCheck);
#endif
        return Anew(alloc, StackTraceHelper, alloc);
    }

    // Capture and fill buffer.
    ULONG StackTraceHelper::GetStackTrace(ULONG framesToSkip, ULONG framesToCapture, void** stackFrames)
    {
#ifdef EXCEPTION_CHECK
        AutoNestedHandledExceptionType autoNestedHandledExceptionType(ExceptionType_DisableCheck);
#endif
        StackBackTrace* stackTrace = this->GetStackBackTrace(framesToCapture);
        ULONG capturedCount = stackTrace->Capture(framesToSkip);
        Assert(capturedCount <= framesToCapture);
        ULONG i = 0;
        stackTrace->Map([&i, framesToCapture, stackFrames](void* funcAddr){
            Assert(i <= framesToCapture);
            stackFrames[i++] = funcAddr;
        });
        return capturedCount;
    }

    // Capture and print.
    size_t StackTraceHelper::PrintStackTrace(ULONG framesToSkip, ULONG framesToCapture)
    {
#ifdef EXCEPTION_CHECK
        AutoNestedHandledExceptionType autoNestedHandledExceptionType(ExceptionType_DisableCheck);
#endif
        StackBackTrace* stackTrace = this->GetStackBackTrace(framesToCapture);
        stackTrace->Capture(framesToSkip);
        return stackTrace->Print();
    }

    StackBackTrace* StackTraceHelper::GetStackBackTrace(ULONG frameCount)
    {
        // Create, if we haven't already for calling thread.
        if (!s_stackBackTrace)
        {
            s_stackBackTrace = StackBackTrace::Create(m_alloc, frameCount);
        }

        Assert(s_stackBackTrace->GetRequestedFrameCount() >= frameCount);

        return s_stackBackTrace;
    }

    __declspec(thread) StackBackTrace* StackTraceHelper::s_stackBackTrace = nullptr;
#endif STACK_BACK_TRACE
}
#endif ENABLE_TRACE
