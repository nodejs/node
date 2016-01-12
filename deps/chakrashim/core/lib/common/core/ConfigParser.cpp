//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <strsafe.h>
#include "Memory\MemoryLogger.h"
#include "Memory\ForcedMemoryConstraints.h"
#include "core\ICustomConfigFlags.h"
#include "core\CmdParser.h"
#include "core\ConfigParser.h"

ConfigParser ConfigParser::s_moduleConfigParser(Js::Configuration::Global.flags);

#ifdef ENABLE_TRACE
class ArenaHost
{
    AllocationPolicyManager m_allocationPolicyManager;
    PageAllocator m_pageAllocator;
    ArenaAllocator m_allocator;

public:
    ArenaHost(__in_z wchar_t* arenaName) :
        m_allocationPolicyManager(/* needConcurrencySupport = */ true),
        m_pageAllocator(&m_allocationPolicyManager, Js::Configuration::Global.flags),
        m_allocator(arenaName, &m_pageAllocator, Js::Throw::OutOfMemory)
    {
    }
    ArenaAllocator* GetAllocator() { return &m_allocator; }
};

ArenaAllocator* GetOutputAllocator1()
{
    static ArenaHost s_arenaHost(L"For Output::Trace (1)");
    return s_arenaHost.GetAllocator();
}

ArenaAllocator* GetOutputAllocator2()
{
    static ArenaHost s_arenaHost(L"For Output::Trace (2)");
    return s_arenaHost.GetAllocator();
}
#endif

void ConfigParser::ParseOnModuleLoad(CmdLineArgsParser& parser, HANDLE hmod)
{
    Assert(!s_moduleConfigParser.HasReadConfig());

    s_moduleConfigParser.ParseRegistry(parser);
    s_moduleConfigParser.ParseConfig(hmod, parser);
    s_moduleConfigParser.ProcessConfiguration(hmod);
    // 'parser' destructor post-processes some configuration
}

void ConfigParser::ParseRegistry(CmdLineArgsParser &parser)
{
    HKEY hk;
    bool includeUserHive = true;

    if (NOERROR == RegOpenKeyExW(HKEY_LOCAL_MACHINE, JsUtil::ExternalApi::GetFeatureKeyName(), 0, KEY_READ, &hk))
    {
        DWORD dwValue;
        DWORD dwSize = sizeof(dwValue);

        ParseRegistryKey(hk, parser);

        // HKLM can prevent user config from being read.
        if (NOERROR == RegGetValueW(hk, nullptr, L"AllowUserConfig", RRF_RT_DWORD, nullptr, (LPBYTE)&dwValue, &dwSize) && dwValue == 0)
        {
            includeUserHive = false;
        }

        RegCloseKey(hk);
    }

    if (includeUserHive && NOERROR == RegOpenKeyExW(HKEY_CURRENT_USER, JsUtil::ExternalApi::GetFeatureKeyName(), 0, KEY_READ, &hk))
    {
        ParseRegistryKey(hk, parser);
        RegCloseKey(hk);
    }
}

void ConfigParser::ParseRegistryKey(HKEY hk, CmdLineArgsParser &parser)
{
    DWORD dwSize;
    DWORD dwValue;

#if ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t regBuffer[MaxRegSize];
    dwSize = sizeof(regBuffer);
    if (NOERROR == RegGetValueW(hk, nullptr, L"JScript9", RRF_RT_REG_SZ, nullptr, (LPBYTE)regBuffer, &dwSize))
    {
        LPWSTR regValue = regBuffer, nextValue = nullptr;
        regValue = wcstok_s(regBuffer, L" ", &nextValue);
        while (regValue != nullptr)
        {
            int err = 0;
            if ((err = parser.Parse(regValue)) != 0)
            {
                break;
            }
            regValue = wcstok_s(nullptr, L" ", &nextValue);
        }
    }
#endif

    // MemSpect - This setting controls whether MemSpect instrumentation is enabled.
    // The value is treated as a bit field with the following bits:
    //   0x01 - Track Arena memory
    //   0x02 - Track Recycler memory
    //   0x04 - Track Page allocations
    dwValue = 0;
    dwSize = sizeof(dwValue);
    if (NOERROR == ::RegGetValueW(hk, nullptr, L"MemSpect", RRF_RT_DWORD, nullptr, (LPBYTE)&dwValue, &dwSize))
    {
        if (dwValue & 0x01)
        {
            ArenaMemoryTracking::Activate();
        }
        if (dwValue & 0x02)
        {
            RecyclerMemoryTracking::Activate();
        }
        if (dwValue & 0x04)
        {
            PageTracking::Activate();
        }
    }

    // JScriptJIT - This setting controls the JIT/interpretation of Jscript code.
    // The legal values are as follows:
    //      1- Force JIT code to be generated for everything.
    //      2- Force interpretation without profiling (turn off JIT)
    //      3- Default
    //      4- Interpreter, simple JIT, and full JIT run a predetermined number of times. Requires >= 3 calls to functions.
    //      5- Interpreter, simple JIT, and full JIT run a predetermined number of times. Requires >= 4 calls to functions.
    //      6- Force interpretation with profiling
    //
    // This reg key is present in released builds.  The QA team's tests use these switches to
    // get reliable JIT coverage in servicing runs done by IE/Windows.  Because this reg key is
    // released, the number of possible values is limited to reduce surface area.
    dwValue = 0;
    dwSize = sizeof(dwValue);
    if (NOERROR == RegGetValueW(hk, nullptr, L"JScriptJIT", RRF_RT_DWORD, nullptr, (LPBYTE)&dwValue, &dwSize))
    {
        Js::ConfigFlagsTable &configFlags = Js::Configuration::Global.flags;
        switch (dwValue)
        {
        case 1:
            configFlags.Enable(Js::ForceNativeFlag);
            configFlags.ForceNative = true;
            break;

        case 6:
            configFlags.Enable(Js::ForceDynamicProfileFlag);
            configFlags.ForceDynamicProfile = true;
            // fall through

        case 2:
            configFlags.Enable(Js::NoNativeFlag);
            configFlags.NoNative = true;
            break;

        case 3:
            break;

        case 4:
            configFlags.Enable(Js::AutoProfilingInterpreter0LimitFlag);
            configFlags.Enable(Js::ProfilingInterpreter0LimitFlag);
            configFlags.Enable(Js::AutoProfilingInterpreter1LimitFlag);
            configFlags.Enable(Js::SimpleJitLimitFlag);
            configFlags.Enable(Js::ProfilingInterpreter1LimitFlag);
            configFlags.Enable(Js::EnforceExecutionModeLimitsFlag);

            configFlags.AutoProfilingInterpreter0Limit = 0;
            configFlags.AutoProfilingInterpreter1Limit = 0;
            if (
#if ENABLE_DEBUG_CONFIG_OPTIONS
                configFlags.NewSimpleJit
#else
                DEFAULT_CONFIG_NewSimpleJit
#endif
                )
            {
                configFlags.ProfilingInterpreter0Limit = 0;
                configFlags.SimpleJitLimit = 0;
                configFlags.ProfilingInterpreter1Limit = 2;
            }
            else
            {
                configFlags.ProfilingInterpreter0Limit = 1;
                configFlags.SimpleJitLimit = 1;
                configFlags.ProfilingInterpreter1Limit = 0;
            }
            configFlags.EnforceExecutionModeLimits = true;
            break;

        case 5:
            configFlags.Enable(Js::AutoProfilingInterpreter0LimitFlag);
            configFlags.Enable(Js::ProfilingInterpreter0LimitFlag);
            configFlags.Enable(Js::AutoProfilingInterpreter1LimitFlag);
            configFlags.Enable(Js::SimpleJitLimitFlag);
            configFlags.Enable(Js::ProfilingInterpreter1LimitFlag);
            configFlags.Enable(Js::EnforceExecutionModeLimitsFlag);

            configFlags.AutoProfilingInterpreter0Limit = 0;
            configFlags.ProfilingInterpreter0Limit = 0;
            configFlags.AutoProfilingInterpreter1Limit = 1;
            if (
#if ENABLE_DEBUG_CONFIG_OPTIONS
                configFlags.NewSimpleJit
#else
                DEFAULT_CONFIG_NewSimpleJit
#endif
                )
            {
                configFlags.SimpleJitLimit = 0;
                configFlags.ProfilingInterpreter1Limit = 2;
            }
            else
            {
                configFlags.SimpleJitLimit = 2;
                configFlags.ProfilingInterpreter1Limit = 0;
            }
            configFlags.EnforceExecutionModeLimits = true;
            break;
        }
    }

    // EnumerationCompat
    // This setting allows disabling a couple of changes to enumeration:
    //     - A change that causes deleted property indexes to be reused for new properties, thereby changing the order in which
    //       properties are enumerated
    //     - A change that creates a true snapshot of the type just before enumeration, and enumerating only those properties. A
    //       property that was deleted before enumeration and is added back during enumeration will not be enumerated.
    // Values:
    //     0 - Default
    //     1 - Compatibility mode for enumeration order (disable changes described above)
    // This FCK does not apply to WWAs. WWAs should use the RC compat mode to disable these changes.
    dwValue = 0;
    dwSize = sizeof(dwValue);
    if (NOERROR == RegGetValueW(hk, nullptr, L"EnumerationCompat", RRF_RT_DWORD, nullptr, (LPBYTE)&dwValue, &dwSize))
    {
        if(dwValue == 1)
        {
            Js::Configuration::Global.flags.EnumerationCompat = true;
        }
    }

#ifdef ENABLE_PROJECTION
    // FailFastIfDisconnectedDelegate
    // This setting allows enabling fail fast if the delegate invoked is disconnected
    //     0 - Default return the error RPC_E_DISCONNECTED if disconnected delegate is invoked
    //     1 - Fail fast if disconnected delegate
    dwValue = 0;
    dwSize = sizeof(dwValue);
    if (NOERROR == RegGetValueW(hk, nullptr, L"FailFastIfDisconnectedDelegate", RRF_RT_DWORD, nullptr, (LPBYTE)&dwValue, &dwSize))
    {
        if(dwValue == 1)
        {
            Js::Configuration::Global.flags.FailFastIfDisconnectedDelegate = true;
        }
    }
#endif

    // ES6 feature control
    // This setting allows enabling\disabling es6 features
    //     0 - Enable ES6 flag - Also default behavior
    //     1 - Disable ES6 flag
    dwValue = 0;
    dwSize = sizeof(dwValue);
    if (NOERROR == RegGetValueW(hk, nullptr, L"DisableES6", RRF_RT_DWORD, nullptr, (LPBYTE)&dwValue, &dwSize))
    {
        Js::ConfigFlagsTable &configFlags = Js::Configuration::Global.flags;
        if (dwValue == 1)
        {
            configFlags.Enable(Js::ES6Flag);
            configFlags.SetAsBoolean(Js::ES6Flag, false);
        }
    }

    // Asmjs feature control
    // This setting allows enabling\disabling asmjs compilation
    //     0 - Disable Asmjs phase - Also default behavior
    //     1 - Enable Asmjs phase
    dwValue = 0;
    dwSize = sizeof(dwValue);
    if (NOERROR == RegGetValueW(hk, nullptr, L"EnableAsmjs", RRF_RT_DWORD, nullptr, (LPBYTE)&dwValue, &dwSize))
    {
        if (dwValue == 1)
        {
            Js::Configuration::Global.flags.Asmjs = true;
        }
    }
}


void ConfigParser::ParseConfig(HANDLE hmod, CmdLineArgsParser &parser)
{
#if defined(ENABLE_DEBUG_CONFIG_OPTIONS) || defined(PARSE_CONFIG_FILE)
    Assert(!_hasReadConfig);
    _hasReadConfig = true;

    wchar_t configBuffer[MaxTokenSize];
    int err = 0;
    wchar_t modulename[_MAX_PATH];
    wchar_t filename[_MAX_PATH];

    GetModuleFileName((HMODULE)hmod, modulename, _MAX_PATH);
    wchar_t drive[_MAX_DRIVE];
    wchar_t dir[_MAX_DIR];

    _wsplitpath_s(modulename, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);
    _wmakepath_s(filename, drive, dir, _configFileName, L".config");

    FILE* configFile;
    if (_wfopen_s(&configFile, filename, L"r, ccs=UNICODE") != 0 || configFile == nullptr)
    {
        WCHAR configFileFullName[MAX_PATH];

        StringCchPrintf(configFileFullName, MAX_PATH, L"%s.config", _configFileName);

        // try the one in the current working directory (Desktop)
        if (_wfullpath(filename, configFileFullName, _MAX_PATH) == nullptr)
        {
            return;
        }
        if (_wfopen_s(&configFile, filename, L"r, ccs=UNICODE") != 0 || configFile == nullptr)
        {
            return;
        }
    }

    while (fwscanf_s(configFile, L"%s", configBuffer, MaxTokenSize) != FINISHED)
    {
        if ((err = parser.Parse(configBuffer)) != 0)
        {
            break;
        }
    }
    fclose(configFile);

    if (err !=0)
    {
        return;
    }
#endif
}

void ConfigParser::ProcessConfiguration(HANDLE hmod)
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    bool hasOutput = false;
    wchar_t modulename[_MAX_PATH];

    GetModuleFileName((HMODULE)hmod, modulename, _MAX_PATH);

    if (Js::Configuration::Global.flags.Console)
    {
        int fd;
        FILE *fp;

        // fail usually means there is an existing console. We don't really care.
        AllocConsole();

        fd = _open_osfhandle((intptr_t)GetStdHandle(STD_OUTPUT_HANDLE), O_TEXT);
        fp = _wfdopen(fd, L"w");

        *stdout = *fp;
        setvbuf(stdout, nullptr, _IONBF, 0);

        fd = _open_osfhandle((intptr_t)GetStdHandle(STD_ERROR_HANDLE), O_TEXT);
        fp = _wfdopen(fd, L"w");

        *stderr = *fp;
        setvbuf(stderr, nullptr, _IONBF, 0);

        wchar_t buffer[_MAX_PATH + 70];

        if (ConfigParserAPI::FillConsoleTitle(buffer, _MAX_PATH + 20, modulename))
        {
            SetConsoleTitle(buffer);
        }

        hasOutput = true;
    }

    if (Js::Configuration::Global.flags.IsEnabled(Js::OutputFileFlag)
        && Js::Configuration::Global.flags.OutputFile != nullptr)
    {
        SetOutputFile(Js::Configuration::Global.flags.OutputFile, Js::Configuration::Global.flags.OutputFileOpenMode);
        hasOutput = true;
    }

    if (Js::Configuration::Global.flags.DebugWindow)
    {
        Output::UseDebuggerWindow();
        hasOutput = true;
    }

#ifdef ENABLE_TRACE
    if (CONFIG_FLAG(InMemoryTrace))
    {
        Output::SetInMemoryLogger(
            Js::MemoryLogger::Create(::GetOutputAllocator1(),
            CONFIG_FLAG(InMemoryTraceBufferSize) * 3));   // With stack each trace is 3 entries (header, msg, stack).
        hasOutput = true;
    }

#ifdef STACK_BACK_TRACE
    if (CONFIG_FLAG(TraceWithStack))
    {
        Output::SetStackTraceHelper(Js::StackTraceHelper::Create(::GetOutputAllocator2()));
    }
#endif STACK_BACK_TRACE
#endif ENABLE_TRACE

    if (hasOutput)
    {
        ConfigParserAPI::DisplayInitialOutput(modulename);

        Output::Print(L"\n");

        Js::Configuration::Global.flags.VerboseDump();
        Output::Flush();
    }

    if (Js::Configuration::Global.flags.ForceSerialized)
    {
        // Can't generate or execute byte code under forced serialize
        Js::Configuration::Global.flags.GenerateByteCodeBufferReturnsCantGenerate = true;
        Js::Configuration::Global.flags.ExecuteByteCodeBufferReturnsInvalidByteCode = true;
    }

    ForcedMemoryConstraint::Apply();
#endif

#ifdef MEMSPECT_TRACKING
    bool all = false;
    if (Js::Configuration::Global.flags.Memspect.IsEnabled(Js::AllPhase))
    {
        all = true;
    }
    if (all || Js::Configuration::Global.flags.Memspect.IsEnabled(Js::RecyclerPhase))
    {
        RecyclerMemoryTracking::Activate();
    }
    if (all || Js::Configuration::Global.flags.Memspect.IsEnabled(Js::PageAllocatorPhase))
    {
        PageTracking::Activate();
    }
    if (all || Js::Configuration::Global.flags.Memspect.IsEnabled(Js::ArenaPhase))
    {
        ArenaMemoryTracking::Activate();
    }
#endif
}

HRESULT ConfigParser::SetOutputFile(const WCHAR* outputFile, const WCHAR* openMode)
{
    // If present, replace the {PID} token with the process ID
    const WCHAR* pidStr = nullptr;
    WCHAR buffer[_MAX_PATH];
    if ((pidStr = wcsstr(outputFile, L"{PID}")) != nullptr)
    {
        size_t pidStartPosition = pidStr - outputFile;

        WCHAR* pDest = buffer;
        size_t bufferLen = _MAX_PATH;

        // Copy the filename before the {PID} token
        wcsncpy_s(pDest, bufferLen, outputFile, pidStartPosition);
        pDest += pidStartPosition;
        bufferLen = bufferLen - pidStartPosition;

        // Copy the PID
        _ultow_s(GetCurrentProcessId(), pDest, /*bufferSize=*/_MAX_PATH - pidStartPosition, /*radix=*/10);
#pragma prefast(suppress: 26014, "ultow string length is smaller than 256")
        pDest += wcslen(pDest);
        bufferLen = bufferLen - wcslen(pDest);

        // Copy the rest of the string.
#pragma prefast(suppress: 26014, "Overwriting pDset's null terminator is intentional since the string being copied is null terminated")
        wcscpy_s(pDest, bufferLen, outputFile + pidStartPosition + /*length of {PID}*/ 5);

        outputFile = buffer;
    }

    wchar_t fileName[_MAX_PATH];
    wchar_t moduleName[_MAX_PATH];
    GetModuleFileName(0, moduleName, _MAX_PATH);
    _wsplitpath_s(moduleName, nullptr, 0, nullptr, 0, fileName, _MAX_PATH, nullptr, 0);
    if (_wcsicmp(fileName, L"WWAHost") == 0 || _wcsicmp(fileName, L"ByteCodeGenerator") == 0 ||
        _wcsicmp(fileName, L"spartan") == 0 || _wcsicmp(fileName, L"spartan_edge") == 0 ||
        _wcsicmp(fileName, L"MicrosoftEdge") == 0 || _wcsicmp(fileName, L"MicrosoftEdgeCP") == 0)
    {

        // we need to output to %temp% directory in wwa. we don't have permission otherwise.
        if (GetEnvironmentVariable(L"temp", fileName, _MAX_PATH) != 0)
        {
            wcscat_s(fileName, _MAX_PATH, L"\\");
            const wchar_t * fileNameOnly = wcsrchr(outputFile, L'\\');
            // if outputFile is full path we just need filename, discard the path
            wcscat_s(fileName, _MAX_PATH, fileNameOnly == nullptr ? outputFile : fileNameOnly);
        }
        else
        {
            AssertMsg(FALSE, "Get temp environment failed");
        }
        outputFile = fileName;
    }

    FILE *fp;
    if ((fp = _wfsopen(outputFile, openMode, _SH_DENYWR)) != nullptr)
    {
        Output::SetOutputFile(fp);
        return S_OK;
    }

    AssertMsg(false, "Could not open file for logging output.");
    return E_FAIL;
}
