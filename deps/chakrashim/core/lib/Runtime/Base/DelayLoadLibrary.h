//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//#include <roapi.h>
#include "activation.h"
#include <winstring.h>

// cor.h includes corhdr.h which is not clean with warning 4091
#pragma warning(push)
#pragma warning(disable: 4091) /* warning C4091: 'typedef ': ignored on left of '' when no variable is declared */
#include <cor.h>
#pragma warning(pop)

#include "RoParameterizedIID.h"

namespace Js
{
    class DelayLoadWinRtString : public DelayLoadLibrary
    {
    private:
        // WinRTString specific functions
        typedef HRESULT FNCWindowsCreateString(const WCHAR *, UINT32, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING *);
        typedef FNCWindowsCreateString* PFNCWindowsCreateString;
        PFNCWindowsCreateString m_pfnWindowsCreateString;

        typedef HRESULT FNCWindowsCreateStringReference(const WCHAR *, UINT32, HSTRING_HEADER *, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING *);
        typedef FNCWindowsCreateStringReference* PFNCWindowsCreateStringReference;
        PFNCWindowsCreateStringReference m_pfnWindowsCreateStringReference;

        typedef PCWSTR FNCWindowsGetStringRawBuffer(HSTRING, UINT32*);
        typedef FNCWindowsGetStringRawBuffer* PFNCWindowsGetStringRawBuffer;
        PFNCWindowsGetStringRawBuffer m_pfWindowsGetStringRawBuffer;

        typedef HRESULT FNCWindowsDeleteString(HSTRING);
        typedef FNCWindowsDeleteString* PFNCWindowsDeleteString;
        PFNCWindowsDeleteString m_pfnWindowsDeleteString;

        typedef HRESULT FNCWindowsCompareStringOrdinal(HSTRING,HSTRING,INT32*);
        typedef FNCWindowsCompareStringOrdinal* PFNCWindowsCompareStringOrdinal;
        PFNCWindowsCompareStringOrdinal m_pfnWindowsCompareStringOrdinal;

        typedef HRESULT FNCWindowsDuplicateString(HSTRING, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING*);
        typedef FNCWindowsDuplicateString* PFNCWindowsDuplicateString;
        PFNCWindowsDuplicateString m_pfnWindowsDuplicateString;

    public:
        DelayLoadWinRtString() : DelayLoadLibrary(),
            m_pfnWindowsCreateString(NULL),
            m_pfWindowsGetStringRawBuffer(NULL),
            m_pfnWindowsDeleteString(NULL),
            m_pfnWindowsCreateStringReference(NULL),
            m_pfnWindowsDuplicateString(NULL),
            m_pfnWindowsCompareStringOrdinal(NULL) { }

        virtual ~DelayLoadWinRtString() { }

        LPCTSTR GetLibraryName() const { return L"api-ms-win-core-winrt-string-l1-1-0.dll"; }

        virtual HRESULT WindowsCreateString(_In_reads_opt_(length) const WCHAR * sourceString, UINT32 length, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string);
        virtual HRESULT WindowsCreateStringReference(_In_reads_opt_(length + 1) const WCHAR * sourceString, UINT32 length, _Out_ HSTRING_HEADER * header, _Outptr_result_maybenull_ _Result_nullonfailure_  HSTRING * string);
        virtual HRESULT WindowsDeleteString(_In_opt_ HSTRING string);
        virtual PCWSTR WindowsGetStringRawBuffer(_In_opt_ HSTRING string, _Out_opt_ UINT32 * length);
        virtual HRESULT WindowsCompareStringOrdinal(_In_opt_ HSTRING string1, _In_opt_ HSTRING string2, _Out_ INT32 * result);
        virtual HRESULT WindowsDuplicateString(_In_opt_ HSTRING original, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * newString);
    };

    class DelayLoadWinRtTypeResolution sealed : public DelayLoadLibrary
    {
    private:
        // WinRtTypeResolution specific functions
        typedef HRESULT FNCWRoParseTypeName(HSTRING, DWORD *, HSTRING **);
        typedef FNCWRoParseTypeName* PFNCWRoParseTypeName;
        PFNCWRoParseTypeName m_pfnRoParseTypeName;

        typedef HRESULT FNCRoResolveNamespace(const HSTRING, const HSTRING, const DWORD, const HSTRING*, DWORD*, HSTRING**, DWORD*, HSTRING**);
        typedef FNCRoResolveNamespace* PFNCRoResolveNamespace;
        PFNCRoResolveNamespace m_pfnRoResolveNamespace;

    public:
        DelayLoadWinRtTypeResolution() : DelayLoadLibrary(),
            m_pfnRoParseTypeName(nullptr) { }

        virtual ~DelayLoadWinRtTypeResolution() { }

        LPCTSTR GetLibraryName() const { return L"api-ms-win-ro-typeresolution-l1-1-0.dll"; }

        HRESULT RoParseTypeName(__in HSTRING typeName, __out DWORD *partsCount, __RPC__deref_out_ecount_full_opt(*partsCount) HSTRING **typeNameParts);

        HRESULT RoResolveNamespace(
            __in_opt const HSTRING namespaceName,
            __in_opt const HSTRING windowsMetaDataPath,
            __in const DWORD packageGraphPathsCount,
            __in_opt const HSTRING *packageGraphPaths,
            __out DWORD *metaDataFilePathsCount,
            HSTRING **metaDataFilePaths,
            __out DWORD *subNamespacesCount,
            HSTRING **subNamespaces);

    };

    class DelayLoadWinType sealed : public DelayLoadLibrary
    {
    private:
        // WinRtTypeResolution specific functions
        typedef HRESULT FNCWRoGetMetaDataFile(
            _In_ const HSTRING name,
            _In_opt_ IMetaDataDispenserEx *metaDataDispenser,
            _Out_opt_ HSTRING *metaDataFilePath,
            _Outptr_opt_ IMetaDataImport2 **metaDataImport,
            _Out_opt_ mdTypeDef *typeDefToken);

        typedef FNCWRoGetMetaDataFile* PFNCWRoGetMettadataFile;
        PFNCWRoGetMettadataFile m_pfnRoGetMetadataFile;


    public:
        DelayLoadWinType() : DelayLoadLibrary(),
            m_pfnRoGetMetadataFile(nullptr) { }

        virtual ~DelayLoadWinType() { }

        LPCTSTR GetLibraryName() const { return L"wintypes.dll"; }

        HRESULT WINAPI RoGetMetaDataFile(
            _In_ const HSTRING name,
            _In_opt_ IMetaDataDispenserEx *metaDataDispenser,
            _Out_opt_ HSTRING *metaDataFilePath,
            _Outptr_opt_ IMetaDataImport2 **metaDataImport,
            _Out_opt_ mdTypeDef *typeDefToken);
    };

    class DelayLoadWinRtRoParameterizedIID sealed : public DelayLoadLibrary
    {
    private:
        // WinRtRoParameterizedIID specific functions
        typedef HRESULT FNCWRoGetParameterizedTypeInstanceIID(UINT32, PCWSTR*, const IRoMetaDataLocator&, GUID*, ROPARAMIIDHANDLE*);

        typedef FNCWRoGetParameterizedTypeInstanceIID* PFNCWRoGetParameterizedTypeInstanceIID;
        PFNCWRoGetParameterizedTypeInstanceIID m_pfnRoGetParameterizedTypeInstanceIID;

    public:
        DelayLoadWinRtRoParameterizedIID() : DelayLoadLibrary(),
            m_pfnRoGetParameterizedTypeInstanceIID(nullptr) { }

        virtual ~DelayLoadWinRtRoParameterizedIID() { }

        LPCTSTR GetLibraryName() const { return L"api-ms-win-core-winrt-roparameterizediid-l1-1-0.dll"; }

        HRESULT RoGetParameterizedTypeInstanceIID(
            __in UINT32 nameElementCount,
            __in_ecount(nameElementCount) PCWSTR*   nameElements,
            __in const IRoMetaDataLocator&          metaDataLocator,
            __out GUID*                             iid,
            __deref_opt_out ROPARAMIIDHANDLE*       pExtra = nullptr);
    };

    class DelayLoadWindowsGlobalization sealed : public DelayLoadWinRtString
    {
    private:
        // DelayLoadWindowsGlobalization specific functions
        typedef HRESULT FNCWDllGetActivationFactory(HSTRING clsid, IActivationFactory** factory);
        typedef FNCWDllGetActivationFactory* PFNCWDllGetActivationFactory;
        PFNCWDllGetActivationFactory m_pfnFNCWDllGetActivationFactory;

        Js::DelayLoadWinRtString *winRTStringLibrary;
        BOOL winRTStringsPresent;

    public:
        DelayLoadWindowsGlobalization() : DelayLoadWinRtString(),
            m_pfnFNCWDllGetActivationFactory(nullptr),
            winRTStringLibrary(nullptr),
            winRTStringsPresent(false) { }

        virtual ~DelayLoadWindowsGlobalization() { }

        LPCTSTR GetLibraryName() const
        {
            return L"windows.globalization.dll";
        }
        LPCTSTR GetWin7LibraryName() const
        {
            return L"jsIntl.dll";
        }
        void Ensure(Js::DelayLoadWinRtString *winRTStringLibrary);

        HRESULT DllGetActivationFactory(__in HSTRING activatibleClassId, __out IActivationFactory** factory);

        HRESULT WindowsCreateString(_In_reads_opt_(length) const WCHAR * sourceString, UINT32 length, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string) override;
        HRESULT WindowsCreateStringReference(_In_reads_opt_(length+1) const WCHAR * sourceString, UINT32 length, _Out_ HSTRING_HEADER * header, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string) override;
        HRESULT WindowsDeleteString(_In_opt_ HSTRING string) override;
        PCWSTR WindowsGetStringRawBuffer(_In_opt_ HSTRING string, _Out_opt_ UINT32 * length) override;
        HRESULT WindowsCompareStringOrdinal(_In_opt_ HSTRING string1, _In_opt_ HSTRING string2, _Out_ INT32 * result) override;
        HRESULT WindowsDuplicateString(_In_opt_ HSTRING original, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING *newString) override;
    };

    class DelayLoadWinRtFoundation sealed : public DelayLoadLibrary
    {
    private:

        // DelayLoadWindowsFoundation specific functions
        typedef HRESULT FNCWRoGetActivationFactory(HSTRING clsid, REFIID iid, IActivationFactory** factory);

        typedef FNCWRoGetActivationFactory* PFNCWRoGetActivationFactory;
        PFNCWRoGetActivationFactory m_pfnFNCWRoGetActivationFactory;

    public:
        DelayLoadWinRtFoundation() : DelayLoadLibrary(),
            m_pfnFNCWRoGetActivationFactory(nullptr) { }

        virtual ~DelayLoadWinRtFoundation() { }

        LPCTSTR GetLibraryName() const { return L"api-ms-win-core-winrt-l1-1-0.dll"; }

        HRESULT RoGetActivationFactory(
            __in HSTRING activatibleClassId,
            __in REFIID iid,
            __out IActivationFactory** factory);
    };

#ifdef ENABLE_PROJECTION
    class DelayLoadWinRtError sealed : public DelayLoadLibrary
    {
    private:
        // DelayLoadWinRtError specific functions
        typedef void FNCRoClearError();
        typedef FNCRoClearError* PFNCRoClearError;
        PFNCRoClearError m_pfnRoClearError;

        typedef BOOL FNCRoOriginateLanguageException(HRESULT, HSTRING, IUnknown *);
        typedef FNCRoOriginateLanguageException* PFNCRoOriginateLanguageException;
        PFNCRoOriginateLanguageException m_pfnRoOriginateLanguageException;

    public:
        DelayLoadWinRtError() : DelayLoadLibrary(),
            m_pfnRoClearError(nullptr),
            m_pfnRoOriginateLanguageException(nullptr) { }

        virtual ~DelayLoadWinRtError() { }

        LPCTSTR GetLibraryName() const { return L"api-ms-win-core-winrt-error-l1-1-1.dll"; }

        HRESULT RoClearError();
        BOOL RoOriginateLanguageException(__in HRESULT error, __in_opt HSTRING message, __in IUnknown * languageException);
    };
#endif

#if defined(_CONTROL_FLOW_GUARD)
    class DelayLoadWinCoreMemory sealed : public DelayLoadLibrary
    {
    private:
        // LoadWinCoreMemory specific functions
        typedef BOOL FNCSetProcessValidCallTargets(HANDLE, PVOID, SIZE_T, ULONG, PCFG_CALL_TARGET_INFO);
        typedef FNCSetProcessValidCallTargets* PFNCSetProcessValidCallTargets;
        PFNCSetProcessValidCallTargets m_pfnSetProcessValidCallTargets;

    public:
        DelayLoadWinCoreMemory() : DelayLoadLibrary(),
            m_pfnSetProcessValidCallTargets(nullptr) { }

        LPCTSTR GetLibraryName() const { return L"api-ms-win-core-memory-l1-1-3.dll"; }

        BOOL SetProcessCallTargets(
            _In_ HANDLE hProcess,
            _In_ PVOID VirtualAddress,
            _In_ SIZE_T RegionSize,
            _In_ ULONG NumberOfOffets,
            _In_reads_(NumberOfOffets) PCFG_CALL_TARGET_INFO OffsetInformation
            );
    };
#endif

    class DelayLoadWinCoreProcessThreads sealed : public DelayLoadLibrary
    {
    private:
        // LoadWinCoreMemory specific functions
        typedef BOOL FNCGetMitigationPolicyForProcess(HANDLE, PROCESS_MITIGATION_POLICY, PVOID, SIZE_T);
        typedef FNCGetMitigationPolicyForProcess* PFNCGetMitigationPolicyForProcess;
        PFNCGetMitigationPolicyForProcess m_pfnGetProcessMitigationPolicy;

        typedef BOOL FNCGetProcessInformation(HANDLE, PROCESS_INFORMATION_CLASS, PVOID, SIZE_T);
        typedef FNCGetProcessInformation* PFNCGetProcessInformation;
        PFNCGetProcessInformation m_pfnGetProcessInformation;

    public:
        DelayLoadWinCoreProcessThreads() :
            DelayLoadLibrary(),
            m_pfnGetProcessMitigationPolicy(nullptr),
            m_pfnGetProcessInformation(nullptr)
            {
            }

        LPCTSTR GetLibraryName() const { return L"api-ms-win-core-processthreads-l1-1-3.dll"; }

        BOOL GetMitigationPolicyForProcess(
            __in HANDLE hProcess,
            __in PROCESS_MITIGATION_POLICY MitigationPolicy,
            __out_bcount(nLength) PVOID lpBuffer,
            __in SIZE_T nLength
            );

        BOOL GetProcessInformation(
            __in HANDLE hProcess,
            __in PROCESS_INFORMATION_CLASS ProcessInformationClass,
            __out_bcount(nLength) PVOID lpBuffer,
            __in SIZE_T nLength
            );
    };

    // Implement this function inlined so that WinRT.lib can be used without the runtime.
    inline HRESULT DelayLoadWinRtRoParameterizedIID::RoGetParameterizedTypeInstanceIID(
            __in UINT32 nameElementCount,
            __in_ecount(nameElementCount) PCWSTR*   nameElements,
            __in const IRoMetaDataLocator&          metaDataLocator,
            __out GUID*                             iid,
            __deref_opt_out ROPARAMIIDHANDLE*       pExtra)
    {
        if (m_hModule)
        {
            if (m_pfnRoGetParameterizedTypeInstanceIID == NULL)
            {
                m_pfnRoGetParameterizedTypeInstanceIID = (PFNCWRoGetParameterizedTypeInstanceIID)GetFunction("RoGetParameterizedTypeInstanceIID");
                if (m_pfnRoGetParameterizedTypeInstanceIID == NULL)
                {
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnRoGetParameterizedTypeInstanceIID != NULL);
            return m_pfnRoGetParameterizedTypeInstanceIID(nameElementCount, nameElements, metaDataLocator, iid, pExtra);
        }

        return E_NOTIMPL;
    }
 }
