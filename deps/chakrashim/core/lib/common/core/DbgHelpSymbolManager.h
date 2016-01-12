//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifdef DBGHELP_SYMBOL_MANAGER
#define DBGHELP_TRANSLATE_TCHAR

// dbghelp.h is not clean with warning 4091
#pragma warning(push)
#pragma warning(disable: 4091) /* warning C4091: 'typedef ': ignored on left of '' when no variable is declared */
#include <dbghelp.h>
#pragma warning(pop)

class DbgHelpSymbolManager
{
public:
    static void EnsureInitialized() { Instance.Initialize(); }
    static BOOL SymFromAddr(PVOID address, DWORD64 * dwDisplacement, PSYMBOL_INFO pSymbol);
    static BOOL SymGetLineFromAddr64(_In_ PVOID address, _Out_ PDWORD pdwDisplacement, _Out_ PIMAGEHLP_LINEW64 pLine);

    static size_t PrintSymbol(PVOID address);
private:
    DbgHelpSymbolManager() : isInitialized(false), hDbgHelpModule(nullptr), pfnSymFromAddrW(nullptr) {}
    ~DbgHelpSymbolManager();

    static DbgHelpSymbolManager Instance;
    void Initialize();

    bool isInitialized;
    CriticalSection cs;
    HANDLE hProcess;
    HMODULE hDbgHelpModule;

    typedef BOOL(__stdcall *PfnSymFromAddrW)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFOW);
    PfnSymFromAddrW pfnSymFromAddrW;

    typedef BOOL(__stdcall *PfnSymGetLineFromAddr64W)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINEW64);
    PfnSymGetLineFromAddr64W pfnSymGetLineFromAddr64W;
};
#endif
