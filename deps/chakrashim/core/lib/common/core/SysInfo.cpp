//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"
#include <psapi.h>
#include <Wincrypt.h>
#include <VersionHelpers.h>

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
#pragma init_seg(".CRT$XCAB")

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

AutoSystemInfo AutoSystemInfo::Data;

void
AutoSystemInfo::Initialize()
{
    Assert(!initialized);
    processHandle = GetCurrentProcess();
    GetSystemInfo(this);

    // Make the page size constant so calculation are faster.
    Assert(this->dwPageSize == AutoSystemInfo::PageSize);
#if defined(_M_IX86) || defined(_M_X64)
    __cpuid(CPUInfo, 1);
    isAtom = CheckForAtom();
#endif
#if defined(_M_ARM32_OR_ARM64)
    armDivAvailable = IsProcessorFeaturePresent(PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE) ? true : false;
#endif
    allocationGranularityPageCount = dwAllocationGranularity / dwPageSize;

    isWindows8OrGreater = IsWindows8OrGreater();

    binaryName[0] = L'\0';

    dllLoadAddress = (UINT_PTR)&__ImageBase;
    dllHighAddress = (UINT_PTR)&__ImageBase +
        ((PIMAGE_NT_HEADERS)(((char *)&__ImageBase) + __ImageBase.e_lfanew))->OptionalHeader.SizeOfImage;

    InitPhysicalProcessorCount();
#if DBG
    initialized = true;
#endif
    WCHAR DisableDebugScopeCaptureFlag[MAX_PATH];
    if (::GetEnvironmentVariable(L"JS_DEBUG_SCOPE", DisableDebugScopeCaptureFlag, _countof(DisableDebugScopeCaptureFlag)) != 0)
    {
        disableDebugScopeCapture = true;
    }
    else
    {
        disableDebugScopeCapture = false;
    }

    this->shouldQCMoreFrequently = false;
    this->supportsOnlyMultiThreadedCOM = false;
    this->isLowMemoryDevice = false;

    // 0 indicates we haven't retrieved the available commit. We get it lazily.
    this->availableCommit = 0;

    ::ChakraBinaryAutoSystemInfoInit(this);
}


bool
AutoSystemInfo::InitPhysicalProcessorCount()
{
    DWORD size = 0;
    DWORD countPhysicalProcessor = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBufferCurrent;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBufferStart;
    BOOL bResult;

    Assert(!this->initialized);

    // Initialize physical processor to number of logical processors.
    // If anything below fails, we still need an approximate value

    this->dwNumberOfPhyscialProcessors = this->dwNumberOfProcessors;

    bResult = GetLogicalProcessorInformation(NULL, &size);

    if (bResult || GetLastError() != ERROR_INSUFFICIENT_BUFFER || !size)
    {
        return false;
    }

    DWORD count = (size) / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

    if (size != count * sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION))
    {
        Assert(false);
        return false;
    }

    pBufferCurrent = pBufferStart = NoCheckHeapNewArray(SYSTEM_LOGICAL_PROCESSOR_INFORMATION, (size_t)count);
    if (!pBufferCurrent)
    {
        return false;
    }

    bResult = GetLogicalProcessorInformation(pBufferCurrent, &size);
    if (!bResult)
    {
        return false;
    }

    while (pBufferCurrent < (pBufferStart + count))
    {
        if (pBufferCurrent->Relationship == RelationProcessorCore)
        {
            countPhysicalProcessor++;
        }
        pBufferCurrent++;
    }

    NoCheckHeapDeleteArray(count, pBufferStart);

    this->dwNumberOfPhyscialProcessors = countPhysicalProcessor;
    return true;
}

bool
AutoSystemInfo::IsJscriptModulePointer(void * ptr)
{
    return ((UINT_PTR)ptr >= Data.dllLoadAddress && (UINT_PTR)ptr < Data.dllHighAddress);
}


uint
AutoSystemInfo::GetAllocationGranularityPageCount() const
{
    Assert(initialized);
    return allocationGranularityPageCount;
}

uint
AutoSystemInfo::GetAllocationGranularityPageSize() const
{
    Assert(initialized);
    return allocationGranularityPageCount * PageSize;
}

#if defined(_M_IX86) || defined(_M_X64)
bool
AutoSystemInfo::VirtualSseAvailable(const int sseLevel) const
{
    #ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        return CONFIG_FLAG(Sse) < 0 || CONFIG_FLAG(Sse) >= sseLevel;
    #else
        return true;
    #endif
}
#endif

BOOL
AutoSystemInfo::SSE2Available() const
{
    Assert(initialized);

#if defined(_M_X64) || defined(_M_ARM32_OR_ARM64)
    return true;
#elif defined(_M_IX86)
    return VirtualSseAvailable(2) && (CPUInfo[3] & (1 << 26));
#else
    #error Unsupported platform.
#endif
}

#if defined(_M_IX86) || defined(_M_X64)
BOOL
AutoSystemInfo::SSE3Available() const
{
    Assert(initialized);
    return VirtualSseAvailable(3) && (CPUInfo[2] & 0x1);
}

BOOL
AutoSystemInfo::SSE4_1Available() const
{
    Assert(initialized);
    return VirtualSseAvailable(4) && (CPUInfo[2] & (0x1 << 19));
}

BOOL
AutoSystemInfo::PopCntAvailable() const
{
    Assert(initialized);
    return VirtualSseAvailable(4) && (CPUInfo[2] & (1 << 23));
}

BOOL
AutoSystemInfo::LZCntAvailable() const
{
    Assert(initialized);
    int CPUInfo[4];
    __cpuid(CPUInfo, 0x80000001);

    return VirtualSseAvailable(4) && (CPUInfo[2] & (1 << 5));
}

bool
AutoSystemInfo::IsAtomPlatform() const
{
    return isAtom;
}

bool
AutoSystemInfo::CheckForAtom() const
{
    int CPUInfo[4];
    const int GENUINE_INTEL_0 = 0x756e6547,
              GENUINE_INTEL_1 = 0x49656e69,
              GENUINE_INTEL_2 = 0x6c65746e;
    const int PLATFORM_MASK = 0x0fff3ff0;
    const int ATOM_PLATFORM_A = 0x0106c0, /* bonnell - extended model 1c, type 0, family code 6 */
              ATOM_PLATFORM_B = 0x020660, /* lincroft - extended model 26, type 0, family code 6 */
              ATOM_PLATFORM_C = 0x020670, /* saltwell - extended model 27, type 0, family code 6 */
              ATOM_PLATFORM_D = 0x030650, /* tbd - extended model 35, type 0, family code 6 */
              ATOM_PLATFORM_E = 0x030660, /* tbd - extended model 36, type 0, family code 6 */
              ATOM_PLATFORM_F = 0x030670; /* tbd - extended model 37, type 0, family code 6 */
    int platformSignature;

    __cpuid(CPUInfo, 0);

    // See if CPU is ATOM HW. First check if CPU is genuine Intel.
    if( CPUInfo[1]==GENUINE_INTEL_0 &&
        CPUInfo[3]==GENUINE_INTEL_1 &&
        CPUInfo[2]==GENUINE_INTEL_2)
    {
        __cpuid(CPUInfo, 1);
        // get platform signature
        platformSignature = CPUInfo[0];
        if((( PLATFORM_MASK & platformSignature) == ATOM_PLATFORM_A) ||
            ((PLATFORM_MASK & platformSignature) == ATOM_PLATFORM_B) ||
            ((PLATFORM_MASK & platformSignature) == ATOM_PLATFORM_C) ||
            ((PLATFORM_MASK & platformSignature) == ATOM_PLATFORM_D) ||
            ((PLATFORM_MASK & platformSignature) == ATOM_PLATFORM_E) ||
            ((PLATFORM_MASK & platformSignature) == ATOM_PLATFORM_F))
        {
            return true;
        }

    }
    return false;
}
#endif

bool
AutoSystemInfo::IsCFGEnabled()
{
#if defined(_CONTROL_FLOW_GUARD)
    return true
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        && IsWinThresholdOrLater() && !PHASE_OFF1(Js::CFGPhase)
#endif //ENABLE_DEBUG_CONFIG_OPTIONS
        ;
#else
    return false;
#endif //_CONTROL_FLOW_GUARD
}

bool
AutoSystemInfo::IsWin8OrLater()
{
    return isWindows8OrGreater;
}

#if defined(_CONTROL_FLOW_GUARD)
bool
AutoSystemInfo::IsWinThresholdOrLater()
{
    return IsWindowsThresholdOrGreater();
}
#endif

DWORD AutoSystemInfo::SaveModuleFileName(HANDLE hMod)
{
    return ::GetModuleFileNameW((HMODULE)hMod, Data.binaryName, MAX_PATH);
}

LPCWSTR AutoSystemInfo::GetJscriptDllFileName()
{
    return (LPCWSTR)Data.binaryName;
}

bool AutoSystemInfo::IsLowMemoryProcess()
{
    ULONG64 commit = ULONG64(-1);
    this->GetAvailableCommit(&commit);
    return commit <= CONFIG_FLAG(LowMemoryCap);
}

BOOL AutoSystemInfo::GetAvailableCommit(ULONG64 *pCommit)
{
    Assert(initialized);

    // Non-zero value indicates we've been here before.
    if (this->availableCommit == 0)
    {
        return false;
    }

    *pCommit = this->availableCommit;
    return true;
}

void AutoSystemInfo::SetAvailableCommit(ULONG64 commit)
{
    ::InterlockedCompareExchange64((volatile LONG64 *)&this->availableCommit, commit, 0);
}

//
// Returns the major and minor version of the loaded binary. If the version info has been fetched once, it will be cached
// and returned without any system calls to find the version number.
//
HRESULT AutoSystemInfo::GetJscriptFileVersion(DWORD* majorVersion, DWORD* minorVersion, DWORD *buildDateHash, DWORD *buildTimeHash)
{
    HRESULT hr = E_FAIL;
    if(AutoSystemInfo::Data.majorVersion == 0 && AutoSystemInfo::Data.minorVersion == 0)
    {
        // uninitialized state  - call the system API to get the version info.
        LPCWSTR jscriptDllName = GetJscriptDllFileName();
        hr = GetVersionInfo(jscriptDllName, majorVersion, minorVersion);

        AutoSystemInfo::Data.majorVersion = *majorVersion;
        AutoSystemInfo::Data.minorVersion = *minorVersion;
    }
    else if(AutoSystemInfo::Data.majorVersion != INVALID_VERSION)
    {
        // if the cached copy is valid, use it and return S_OK.
        *majorVersion = AutoSystemInfo::Data.majorVersion;
        *minorVersion = AutoSystemInfo::Data.minorVersion;
        hr = S_OK;
    }

    if (buildDateHash)
    {
        *buildDateHash = AutoSystemInfo::Data.buildDateHash;
    }

    if (buildTimeHash)
    {
        *buildTimeHash = AutoSystemInfo::Data.buildTimeHash;
    }
    return hr;
}

//
// Returns the major and minor version of the binary passed as argument.
//
HRESULT AutoSystemInfo::GetVersionInfo(__in LPCWSTR pszPath, DWORD* majorVersion, DWORD* minorVersion)
{
    DWORD   dwTemp;
    DWORD   cbVersionSz;
    HRESULT hr = E_FAIL;
    BYTE*    pVerBuffer = NULL;
    VS_FIXEDFILEINFO* pFileInfo = NULL;
    cbVersionSz = GetFileVersionInfoSizeEx(FILE_VER_GET_LOCALISED, pszPath, &dwTemp);
    if(cbVersionSz > 0)
    {
        pVerBuffer = NoCheckHeapNewArray(BYTE, cbVersionSz);
        if(pVerBuffer)
        {
            if(GetFileVersionInfoEx(FILE_VER_GET_LOCALISED|FILE_VER_GET_NEUTRAL, pszPath, 0, cbVersionSz, pVerBuffer))
            {
                UINT    uiSz = sizeof(VS_FIXEDFILEINFO);
                if(!VerQueryValue(pVerBuffer, L"\\", (LPVOID*)&pFileInfo, &uiSz))
                {
                    hr = HRESULT_FROM_WIN32(GetLastError());
                }
                else
                {
                    hr = S_OK;
                }
            }
            else
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
            }
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }
    }

    if(SUCCEEDED(hr))
    {
        *majorVersion = pFileInfo->dwFileVersionMS;
        *minorVersion = pFileInfo->dwFileVersionLS;
    }
    else
    {
        *majorVersion = INVALID_VERSION;
        *minorVersion = INVALID_VERSION;
    }
    if(pVerBuffer)
    {
        NoCheckHeapDeleteArray(cbVersionSz, pVerBuffer);
    }
    return hr;
}
