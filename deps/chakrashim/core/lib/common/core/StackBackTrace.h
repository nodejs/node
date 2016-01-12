//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef STACK_BACK_TRACE
class StackBackTrace
{
public:
    static const ULONG DefaultFramesToCapture = 30;
    template <typename TAllocator> __declspec(noinline)
        static StackBackTrace * Capture(TAllocator * alloc, ULONG framesToSkip = 0, ULONG framesToCapture = DefaultFramesToCapture);

    template <typename TAllocator> __declspec(noinline)
        static StackBackTrace * Create(TAllocator * alloc, ULONG framesToCaptureLater = DefaultFramesToCapture);
    size_t Print();
    template<typename Fn>void Map(Fn fn);   // The Fn is expected to be: void Fn(void*).
    ULONG Capture(ULONG framesToSkip);
    ULONG GetRequestedFrameCount() { return this->requestedFramesToCapture; }
    template <typename TAllocator>
    void Delete(TAllocator * alloc);
private:
    // We want to skip at lease the StackBackTrace::Capture and the constructor frames
    static const ULONG BaseFramesToSkip = 2;

    __declspec(noinline) StackBackTrace(ULONG framesToSkip, ULONG framesToCapture);
    __declspec(noinline) StackBackTrace(ULONG framesToCapture);

    ULONG requestedFramesToCapture;
    ULONG framesCount;
    PVOID stackBackTrace[];
};

template <typename TAllocator>
StackBackTrace *
StackBackTrace::Capture(TAllocator * alloc, ULONG framesToSkip, ULONG framesToCapture)
{
    return AllocatorNewPlusZ(TAllocator, alloc, sizeof(PVOID) * framesToCapture, StackBackTrace, framesToSkip, framesToCapture);
}

template <typename TAllocator>
StackBackTrace* StackBackTrace::Create(TAllocator * alloc, ULONG framesToCaptureLater)
{
    return AllocatorNewPlusZ(TAllocator, alloc, sizeof(PVOID)* framesToCaptureLater, StackBackTrace, framesToCaptureLater);
}

template <typename TAllocator>
void StackBackTrace::Delete(TAllocator * alloc)
{
    AllocatorDeletePlus(TAllocator, alloc, sizeof(PVOID)* requestedFramesToCapture, this);
}

template <typename Fn>
void StackBackTrace::Map(Fn fn)
{
    for (ULONG i = 0; i < this->framesCount; ++i)
    {
        fn(this->stackBackTrace[i]);
    }
}

class StackBackTraceNode
{
public:
    template <typename TAllocator>
    static void Prepend(TAllocator * allocator, StackBackTraceNode *& head, StackBackTrace * stackBackTrace)
    {
        head = AllocatorNew(TAllocator, allocator, StackBackTraceNode, stackBackTrace, head);
    }

    template <typename TAllocator>
    static void DeleteAll(TAllocator * allocator, StackBackTraceNode *& head)
    {
        StackBackTraceNode * curr = head;
        while (curr != nullptr)
        {
            StackBackTraceNode * next = curr->next;
            curr->stackBackTrace->Delete(allocator);
            AllocatorDelete(TAllocator, allocator, curr);
            curr = next;
        }
        head = nullptr;
    }

    static void PrintAll(StackBackTraceNode * head)
    {
        // We want to print them tail first because that is the first stack trace we added

        // Reverse the list
        StackBackTraceNode * curr = head;
        StackBackTraceNode * prev = nullptr;
        while (curr != nullptr)
        {
            StackBackTraceNode * next = curr->next;
            curr->next = prev;
            prev = curr;
            curr = next;
        }

        // print and reverse again.
        curr = prev;
        prev = nullptr;
        while (curr != nullptr)
        {
            curr->stackBackTrace->Print();
            StackBackTraceNode * next = curr->next;
            curr->next = prev;
            prev = curr;
            curr = next;
        }

        Assert(prev == head);
    }
private:
    StackBackTraceNode(StackBackTrace * stackBackTrace, StackBackTraceNode * next) : stackBackTrace(stackBackTrace), next(next) {};
    StackBackTrace * stackBackTrace;
    StackBackTraceNode * next;
};

//
// Capture multiple call stack traces using a in-memory ring buffer. Useful for instrumenting source
// code to track calls.
//
//  BUFFERS:    Number of stack traces to keep. When all the buffers are filled up, capture will start
//              over from the beginning and overwrite older traces.
//  HEADER:     Number of pointer-sized data reserved in the header of each trace. You can save runtime
//              data in the header of each trace to record runtime state of the stack trace.
//  FRAMES:     Number of stack frames for each trace.
//  SKIPFRAMES: Top frames to skip for each capture. e.g., at least StackBackTraceRing::Capture frame is useless.
//
//  Usage: Following captures the last 100 stacks that changes scriptContext->debuggerMode:
//      Declare an instance:                            StackBackTraceRing<100> s_debuggerMode;
//      Call at every debuggerMode change point:        s_debugModeTrace.Capture(scriptContext, debuggerMode);
//
//      x86:
//      Debug a dump in windbg:     ?? chakra!Js::s_debuggerMode
//      Inspect trace 0:            dds [buf]
//      Inspect trace N:            dds [buf]+0n32*4*N
//      Inspect last trace:         dds [buf]+0n32*4*[cur-1]
//
template <ULONG BUFFERS, ULONG HEADER = 2, ULONG FRAMES = 30, ULONG SKIPFRAMES = 1>
class StackBackTraceRing
{
    static const ULONG ONE_TRACE = HEADER + FRAMES;

private:
    LPVOID* buf;
    ULONG cur;

public:
    StackBackTraceRing()
    {
        buf = new LPVOID[ONE_TRACE * BUFFERS];
        cur = 0;
    }

    ~StackBackTraceRing()
    {
        delete[] buf;
    }

    template <class HeaderFunc>
    void CaptureWithHeader(HeaderFunc writeHeader)
    {
        LPVOID* buffer = &buf[ONE_TRACE * cur++];
        cur = cur % BUFFERS;

        memset(buffer, 0, sizeof(LPVOID) * ONE_TRACE);
        writeHeader(buffer);

        LPVOID* frames = &buffer[HEADER];
        CaptureStackBackTrace(SKIPFRAMES, FRAMES, frames, nullptr);
    }

    // Capture a stack trace
    void Capture()
    {
        CaptureWithHeader([](LPVOID* buffer)
        {
        });
    }

    // Capture a stack trace and save data0 in header
    template <class T0>
    void Capture(T0 data0)
    {
        C_ASSERT(HEADER >= 1);

        CaptureWithHeader([=](_Out_writes_(HEADER) LPVOID* buffer)
        {
            buffer[0] = reinterpret_cast<LPVOID>(data0);
        });
    }

    // Capture a stack trace and save data0 and data1 in header
    template <class T0, class T1>
    void Capture(T0 data0, T1 data1)
    {
        C_ASSERT(HEADER >= 2);

        CaptureWithHeader([=](_Out_writes_(HEADER) LPVOID* buffer)
        {
            buffer[0] = reinterpret_cast<LPVOID>(data0);
            buffer[1] = reinterpret_cast<LPVOID>(data1);
        });
    }
};

#endif
