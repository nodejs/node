//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"

#ifdef DBGHELP_SYMBOL_MANAGER
#include "core\DbgHelpSymbolManager.h"

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
#pragma init_seg(".CRT$XCAP")

DbgHelpSymbolManager DbgHelpSymbolManager::Instance;

void
DbgHelpSymbolManager::Initialize()
{
    wchar_t *wszSearchPath = nullptr;
    wchar_t *wszModuleDrive = nullptr;
    wchar_t *wszModuleDir = nullptr;
    wchar_t *wszOldSearchPath = nullptr;
    wchar_t *wszNewSearchPath = nullptr;
    wchar_t *wszModuleName = nullptr;

    const size_t ceModuleName = _MAX_PATH;
    const size_t ceOldSearchPath = 32767;
    const size_t ceNewSearchPath = ceOldSearchPath + _MAX_PATH + 1;

    if (isInitialized)
    {
        return;
    }

    AutoCriticalSection autocs(&cs);
    if (isInitialized)
    {
        goto end;
    }
    isInitialized = true;
    hProcess = GetCurrentProcess();

    // Let's make sure the directory where chakra.dll is, is on the symbol path.

    wchar_t const * wszModule = AutoSystemInfo::GetJscriptDllFileName();
    wszModuleName = NoCheckHeapNewArray(wchar_t, ceModuleName);
    if (wszModuleName == nullptr)
    {
        goto end;
    }

    if (wcscmp(wszModule, L"") == 0)
    {
        if (GetModuleFileName(NULL, wszModuleName, static_cast<DWORD>(ceModuleName)))
        {
            wszModule = wszModuleName;
        }
        else
        {
            wszModule = nullptr;
        }
    }

    if (wszModule != nullptr)
    {
        wszModuleDrive = NoCheckHeapNewArray(wchar_t, _MAX_DRIVE);
        if (wszModuleDrive == nullptr)
        {
            goto end;
        }

        wszModuleDir = NoCheckHeapNewArray(wchar_t, _MAX_DIR);
        if (wszModuleDir == nullptr)
        {
            goto end;
        }

        _wsplitpath_s(wszModule, wszModuleDrive, _MAX_DRIVE, wszModuleDir, _MAX_DIR, NULL, 0, NULL, 0);
        _wmakepath_s(wszModuleName, ceModuleName, wszModuleDrive, wszModuleDir, NULL, NULL);

        wszOldSearchPath = NoCheckHeapNewArray(wchar_t, ceOldSearchPath);
        if (wszOldSearchPath == nullptr)
        {
            goto end;
        }

        wszNewSearchPath = NoCheckHeapNewArray(wchar_t, ceNewSearchPath);
        if (wszNewSearchPath == nullptr)
        {
            goto end;
        }

        if (GetEnvironmentVariable(L"_NT_SYMBOL_PATH", wszOldSearchPath, static_cast<DWORD>(ceOldSearchPath)) != 0)
        {
            swprintf_s(wszNewSearchPath, ceNewSearchPath, L"%s;%s", wszOldSearchPath, wszModuleName);
            wszSearchPath = wszNewSearchPath;
        }
        else
        {
            wszSearchPath = wszModuleName;
        }
    }

    hDbgHelpModule = LoadLibraryEx(L"dbghelp.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hDbgHelpModule == nullptr)
    {
        goto end;
    }

    typedef BOOL(__stdcall *PfnSymInitialize)(HANDLE, PCWSTR, BOOL);
    PfnSymInitialize pfnSymInitialize = (PfnSymInitialize)GetProcAddress(hDbgHelpModule, "SymInitializeW");
    if (pfnSymInitialize)
    {
        pfnSymInitialize(hProcess, wszSearchPath, TRUE);
        pfnSymFromAddrW = (PfnSymFromAddrW)GetProcAddress(hDbgHelpModule, "SymFromAddrW");
        pfnSymGetLineFromAddr64W = (PfnSymGetLineFromAddr64W)GetProcAddress(hDbgHelpModule, "SymGetLineFromAddrW64");

        // load line information
        typedef DWORD(__stdcall *PfnSymGetOptions)();
        typedef VOID(__stdcall *PfnSymSetOptions)(DWORD);
        PfnSymGetOptions pfnSymGetOptions = (PfnSymGetOptions)GetProcAddress(hDbgHelpModule, "SymGetOptions");
        PfnSymSetOptions pfnSymSetOptions = (PfnSymSetOptions)GetProcAddress(hDbgHelpModule, "SymSetOptions");

        DWORD options = pfnSymGetOptions();
        options |= SYMOPT_LOAD_LINES;
        pfnSymSetOptions(options);
    }

end:
    if (wszModuleName != nullptr)
    {
        NoCheckHeapDeleteArray(ceModuleName, wszModuleName);
        wszModuleName = nullptr;
    }

    if (wszModuleDrive != nullptr)
    {
        NoCheckHeapDeleteArray(_MAX_DRIVE, wszModuleDrive);
        wszModuleDrive = nullptr;
    }

    if (wszModuleDir != nullptr)
    {
        NoCheckHeapDeleteArray(_MAX_DIR, wszModuleDir);
        wszModuleDir = nullptr;
    }

    if (wszOldSearchPath != nullptr)
    {
        NoCheckHeapDeleteArray(ceOldSearchPath, wszOldSearchPath);
        wszOldSearchPath = nullptr;
    }

    if (wszNewSearchPath != nullptr)
    {
        NoCheckHeapDeleteArray(ceNewSearchPath, wszNewSearchPath);
        wszNewSearchPath = nullptr;
    }
}

DbgHelpSymbolManager::~DbgHelpSymbolManager()
{
    if (hDbgHelpModule)
    {
        typedef BOOL(__stdcall *PfnSymCleanup)(HANDLE);
        PfnSymCleanup pfnSymCleanup = (PfnSymCleanup)GetProcAddress(hDbgHelpModule, "SymCleanup");
        if (pfnSymCleanup)
        {
            pfnSymCleanup(hProcess);
        }

        FreeLibrary(hDbgHelpModule);
    }
}

BOOL
DbgHelpSymbolManager::SymFromAddr(PVOID address, DWORD64 * dwDisplacement, PSYMBOL_INFO pSymbol)
{
    if (Instance.pfnSymFromAddrW)
    {
        return Instance.pfnSymFromAddrW(Instance.hProcess, (DWORD64)address, dwDisplacement, pSymbol);
    }
    return FALSE;
}

BOOL
DbgHelpSymbolManager::SymGetLineFromAddr64(_In_ PVOID address, _Out_ PDWORD pdwDisplacement, _Out_ PIMAGEHLP_LINEW64 pLine)
{
    if (pdwDisplacement != nullptr)
    {
        *pdwDisplacement = 0;
    }

    if (pLine != nullptr)
    {
        ZeroMemory(pLine, sizeof(IMAGEHLP_LINEW64));
        pLine->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    }

    if (Instance.pfnSymGetLineFromAddr64W)
    {
        return Instance.pfnSymGetLineFromAddr64W(Instance.hProcess, (DWORD64)address, pdwDisplacement, pLine);
    }
    return FALSE;
}

size_t DbgHelpSymbolManager::PrintSymbol(PVOID address)
{
    size_t retValue = 0;
    DWORD64  dwDisplacement = 0;
    char buffer[sizeof(SYMBOL_INFO)+MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    IMAGEHLP_LINE64 lineInfo;
    lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    if (DbgHelpSymbolManager::SymFromAddr(address, &dwDisplacement, pSymbol))
    {
        DWORD dwDisplacementDWord = static_cast<DWORD>(dwDisplacement);
        if (DbgHelpSymbolManager::SymGetLineFromAddr64(address, &dwDisplacementDWord, &lineInfo))
        {
            retValue += Output::Print(L"0x%p %s+0x%llx (%s:%d)", address, pSymbol->Name, dwDisplacement, lineInfo.FileName, lineInfo.LineNumber);
        }
        else
        {
            // SymGetLineFromAddr64 failed
            retValue += Output::Print(L"0x%p %s+0x%llx", address, pSymbol->Name, dwDisplacement);
        }
    }
    else
    {
        // SymFromAddr failed
        retValue += Output::Print(L"0x%p", address);
    }
    return retValue;
}
#endif
