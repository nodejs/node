//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#ifdef LEAK_REPORT
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
#pragma init_seg(".CRT$XCAR")

CriticalSection LeakReport::s_cs;
DWORD LeakReport::nestedSectionCount = 0;
DWORD LeakReport::nestedRedirectOutputCount = 0;
AutoFILE LeakReport::file;
FILE * oldFile = nullptr;
bool LeakReport::openReportFileFailed = false;
LeakReport::UrlRecord * LeakReport::urlRecordHead = nullptr;
LeakReport::UrlRecord * LeakReport::urlRecordTail = nullptr;

void
LeakReport::StartRedirectOutput()
{
    if (!EnsureLeakReportFile())
    {
        return;
    }
    s_cs.Enter();
    if (nestedRedirectOutputCount == 0)
    {
        Assert(oldFile == nullptr);
        oldFile = Output::SetFile(file);
    }
    nestedRedirectOutputCount++;
}

void
LeakReport::EndRedirectOutput()
{
    if (nestedRedirectOutputCount == 0)
    {
        return;
    }
    Assert(file != nullptr);
    nestedRedirectOutputCount--;

    if (nestedRedirectOutputCount == 0)
    {
        fflush(file);
        FILE * tmpFile = Output::SetFile(oldFile);
        Assert(tmpFile == file);
        oldFile = nullptr;
    }
    s_cs.Leave();
}

void
LeakReport::StartSection(wchar_t const * msg, ...)
{
    va_list argptr;
    va_start(argptr, msg);
    StartSection(msg, argptr);
    va_end(argptr);
}

void
LeakReport::StartSection(wchar_t const * msg, va_list argptr)
{
    s_cs.Enter();
    if (!EnsureLeakReportFile())
    {
        return;
    }
    nestedSectionCount++;


    Print(L"--------------------------------------------------------------------------------\n");
    vfwprintf(file, msg, argptr);
    Print(L"\n");
    Print(L"--------------------------------------------------------------------------------\n");
}

void
LeakReport::EndSection()
{
    s_cs.Leave();
    if (file == nullptr)
    {
        return;
    }
    nestedSectionCount--;
}

void
LeakReport::Print(wchar_t const * msg, ...)
{
    AutoCriticalSection autocs(&s_cs);
    if (!EnsureLeakReportFile())
    {
        return;
    }

    va_list argptr;
    va_start(argptr, msg);
    vfwprintf(file, msg, argptr);
    va_end(argptr);
}

bool
LeakReport::EnsureLeakReportFile()
{
    AutoCriticalSection autocs(&s_cs);
    if (openReportFileFailed)
    {
        return false;
    }
    if (file != nullptr)
    {
        return true;
    }

    wchar_t const * filename = Js::Configuration::Global.flags.LeakReport;
    wchar_t const * openMode = L"w+";
    wchar_t defaultFilename[_MAX_PATH];
    if (filename == nullptr)
    {
        swprintf_s(defaultFilename, L"jsleakreport-%u.txt", ::GetCurrentProcessId());
        filename = defaultFilename;
        openMode = L"a+";   // append mode
    }
    if (_wfopen_s(&file, filename, openMode) != 0)
    {
        openReportFileFailed = true;
        return false;
    }
    Print(L"================================================================================\n");
    Print(L"Chakra Leak Report - PID: %d\n", ::GetCurrentProcessId());
    __time64_t time_value = _time64(NULL);
    wchar_t time_string[26];
    struct tm local_time;
    _localtime64_s(&local_time, &time_value);
    _wasctime_s(time_string, &local_time);
    Print(time_string);
    Print(L"\n");
    return true;
}

LeakReport::UrlRecord *
LeakReport::LogUrl(wchar_t const * url, void * globalObject)
{
    UrlRecord * record = NoCheckHeapNewStruct(UrlRecord);

    size_t length = wcslen(url) + 1; // Add 1 for the NULL.
    wchar_t* urlCopy = NoCheckHeapNewArray(wchar_t, length);
    js_memcpy_s(urlCopy, (length - 1) * sizeof(wchar_t), url, (length - 1) * sizeof(wchar_t));
    urlCopy[length - 1] = L'\0';

    record->url = urlCopy;
    record->time = _time64(NULL);
    record->tid = ::GetCurrentThreadId();
    record->next = nullptr;
    record->scriptEngine = nullptr;
    record->globalObject = globalObject;

    AutoCriticalSection autocs(&s_cs);
    if (LeakReport::urlRecordHead == nullptr)
    {
        Assert(LeakReport::urlRecordTail == nullptr);
        LeakReport::urlRecordHead = record;
        LeakReport::urlRecordTail = record;
    }
    else
    {
        LeakReport::urlRecordTail->next = record;
        LeakReport::urlRecordTail = record;
    }

    return record;
}

void
LeakReport::DumpUrl(DWORD tid)
{
    AutoCriticalSection autocs(&s_cs);
    if (!EnsureLeakReportFile())
    {
        return;
    }

    UrlRecord * prev = nullptr;
    UrlRecord ** pprev = &LeakReport::urlRecordHead;
    UrlRecord * curr = *pprev;
    while (curr != nullptr)
    {
        if (curr->tid == tid)
        {
            wchar_t timeStr[26];
            struct tm local_time;
            _localtime64_s(&local_time, &curr->time);
            _wasctime_s(timeStr, &local_time);
            timeStr[wcslen(timeStr) - 1] = 0;
            Print(L"%s - (%p, %p) %s\n", timeStr, curr->scriptEngine, curr->globalObject, curr->url);
            *pprev = curr->next;
            NoCheckHeapDeleteArray(wcslen(curr->url) + 1, curr->url);
            NoCheckHeapDelete(curr);
        }
        else
        {
            pprev = &curr->next;
            prev = curr;
        }
        curr = *pprev;
    }

    if (prev == nullptr)
    {
        LeakReport::urlRecordTail = nullptr;
    }
    else if (prev->next == nullptr)
    {
        LeakReport::urlRecordTail = prev;
    }
}

AutoLeakReportSection::AutoLeakReportSection(Js::ConfigFlagsTable& flags, wchar_t const * msg, ...):
    m_flags(flags)
{
    if (flags.IsEnabled(Js::LeakReportFlag))
    {
        va_list argptr;
        va_start(argptr, msg);
        LeakReport::StartSection(msg, argptr);
        va_end(argptr);
    }
}

AutoLeakReportSection::~AutoLeakReportSection()
{
    if (m_flags.IsEnabled(Js::LeakReportFlag))
    {
        LeakReport::EndSection();
    }
}
#endif
