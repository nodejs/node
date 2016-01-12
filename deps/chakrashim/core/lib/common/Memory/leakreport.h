//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{

#ifdef LEAK_REPORT

class LeakReport
{
public:
    class UrlRecord
    {
    public:
        void * scriptEngine;
    private:
        wchar_t const * url;
        __time64_t time;
        DWORD tid;
        UrlRecord * next;

        void * globalObject;
        friend class LeakReport;
    };

    static void StartRedirectOutput();
    static void EndRedirectOutput();
    static void StartSection(wchar_t const * msg, ...);
    static void StartSection(wchar_t const * msg, va_list argptr);
    static void EndSection();
    static void Print(wchar_t const * msg, ...);

    static UrlRecord * LogUrl(wchar_t const * url, void * globalObject);
    static void DumpUrl(DWORD tid);
private:
    static CriticalSection s_cs;
    static AutoFILE file;
    static bool openReportFileFailed;
    static DWORD nestedSectionCount;
    static DWORD nestedRedirectOutputCount;

    static UrlRecord * urlRecordHead;
    static UrlRecord * urlRecordTail;

    static bool EnsureLeakReportFile();
};

class AutoLeakReportSection
{
public:
    AutoLeakReportSection(Js::ConfigFlagsTable& flags, wchar_t const * msg, ...);
    ~AutoLeakReportSection();

private:
    Js::ConfigFlagsTable& m_flags;
};

#define STRINGIFY2(x,y) x ## y
#define STRINGIFY(x,y) STRINGIFY2(x,y)
#define LEAK_REPORT_PRINT(msg, ...) if (Js::Configuration::Global.flags.IsEnabled(Js::LeakReportFlag)) LeakReport::Print(msg, __VA_ARGS__)
#define AUTO_LEAK_REPORT_SECTION(flags, msg, ...) AutoLeakReportSection STRINGIFY(__autoLeakReportSection, __COUNTER__)(flags, msg, __VA_ARGS__)
#else
#define LEAK_REPORT_PRINT(msg, ...)
#define AUTO_LEAK_REPORT_SECTION(msg, ...)
#endif
}
