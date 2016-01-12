//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#ifdef _CONTROL_FLOW_GUARD
#if !defined(DELAYLOAD_SET_CFG_TARGET)
extern "C"
WINBASEAPI
BOOL
WINAPI
SetProcessValidCallTargets(
    _In_ HANDLE hProcess,
    _In_ PVOID VirtualAddress,
    _In_ SIZE_T RegionSize,
    _In_ ULONG NumberOfOffets,
    _In_reads_(NumberOfOffets) PCFG_CALL_TARGET_INFO OffsetInformation
    );
#endif
#endif

namespace Js
{
    HRESULT DelayLoadWinRtString::WindowsCreateString(_In_reads_opt_(length) const WCHAR * sourceString, UINT32 length, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string)
    {
        if (m_hModule)
        {
            if (m_pfnWindowsCreateString == nullptr)
            {
                m_pfnWindowsCreateString = (PFNCWindowsCreateString)GetFunction("WindowsCreateString");
                if (m_pfnWindowsCreateString == nullptr)
                {
                    *string = nullptr;
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnWindowsCreateString != nullptr);
            return m_pfnWindowsCreateString(sourceString, length, string);
        }

        *string = nullptr;
        return E_NOTIMPL;
    }

    HRESULT DelayLoadWinRtString::WindowsCreateStringReference(_In_reads_opt_(length + 1) const WCHAR *sourceString, UINT32 length, _Out_ HSTRING_HEADER *hstringHeader, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string)
    {
        if (m_hModule)
        {
            if (m_pfnWindowsCreateStringReference == nullptr)
            {
                m_pfnWindowsCreateStringReference = (PFNCWindowsCreateStringReference)GetFunction("WindowsCreateStringReference");
                if (m_pfnWindowsCreateStringReference == nullptr)
                {
                    *string = nullptr;
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnWindowsCreateStringReference != nullptr);
            return m_pfnWindowsCreateStringReference(sourceString, length, hstringHeader, string);
        }

        *string = nullptr;
        return E_NOTIMPL;
    }

    HRESULT DelayLoadWinRtString::WindowsDeleteString(_In_opt_ HSTRING string)
    {
        if (m_hModule)
        {
            if (m_pfnWindowsDeleteString == nullptr)
            {
                m_pfnWindowsDeleteString = (PFNCWindowsDeleteString)GetFunction("WindowsDeleteString");
                if (m_pfnWindowsDeleteString == nullptr)
                {
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnWindowsDeleteString != nullptr);
            HRESULT hr = m_pfnWindowsDeleteString(string);
            Assert(SUCCEEDED(hr));
            return hr;
        }

        return E_NOTIMPL;
    }

    PCWSTR DelayLoadWinRtString::WindowsGetStringRawBuffer(_In_opt_ HSTRING string, _Out_opt_ UINT32 * length)
    {
        if (m_hModule)
        {
            if (m_pfWindowsGetStringRawBuffer == nullptr)
            {
                m_pfWindowsGetStringRawBuffer = (PFNCWindowsGetStringRawBuffer)GetFunction("WindowsGetStringRawBuffer");
                if (m_pfWindowsGetStringRawBuffer == nullptr)
                {
                    if (length)
                    {
                        *length = 0;
                    }
                    return L"\0";
                }
            }

            Assert(m_pfWindowsGetStringRawBuffer != nullptr);
            return m_pfWindowsGetStringRawBuffer(string, length);
        }

        if (length)
        {
            *length = 0;
        }
        return L"\0";
    }

    HRESULT DelayLoadWinRtString::WindowsCompareStringOrdinal(_In_opt_ HSTRING string1, _In_opt_ HSTRING string2, _Out_ INT32 * result)
    {
        if (m_hModule)
        {
            if (m_pfnWindowsCompareStringOrdinal == nullptr)
            {
                m_pfnWindowsCompareStringOrdinal = (PFNCWindowsCompareStringOrdinal)GetFunction("WindowsCompareStringOrdinal");
                if (m_pfnWindowsCompareStringOrdinal == nullptr)
                {
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnWindowsCompareStringOrdinal != nullptr);
            return m_pfnWindowsCompareStringOrdinal(string1,string2,result);
        }

        return E_NOTIMPL;
    }
    HRESULT DelayLoadWinRtString::WindowsDuplicateString(_In_opt_ HSTRING original, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING *newString)
    {
        if(m_hModule)
        {
            if(m_pfnWindowsDuplicateString == nullptr)
            {
                m_pfnWindowsDuplicateString = (PFNCWindowsDuplicateString)GetFunction("WindowsDuplicateString");
                if(m_pfnWindowsDuplicateString == nullptr)
                {
                    *newString = nullptr;
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnWindowsDuplicateString != nullptr);
            return m_pfnWindowsDuplicateString(original, newString);
        }
        *newString = nullptr;
        return E_NOTIMPL;
    }

    HRESULT DelayLoadWinRtTypeResolution::RoParseTypeName(__in HSTRING typeName, __out DWORD *partsCount, __RPC__deref_out_ecount_full_opt(*partsCount) HSTRING **typeNameParts)
    {
        if (m_hModule)
        {
            if (m_pfnRoParseTypeName == nullptr)
            {
                m_pfnRoParseTypeName = (PFNCWRoParseTypeName)GetFunction("RoParseTypeName");
                if (m_pfnRoParseTypeName == nullptr)
                {
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnRoParseTypeName != nullptr);
            return m_pfnRoParseTypeName(typeName, partsCount, typeNameParts);
        }

        return E_NOTIMPL;
    }

    HRESULT DelayLoadWindowsGlobalization::DllGetActivationFactory(
        __in HSTRING activatibleClassId,
        __out IActivationFactory** factory)
    {
        if (m_hModule)
        {
            if (m_pfnFNCWDllGetActivationFactory == nullptr)
            {
                m_pfnFNCWDllGetActivationFactory = (PFNCWDllGetActivationFactory)GetFunction("DllGetActivationFactory");
                if (m_pfnFNCWDllGetActivationFactory == nullptr)
                {
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnFNCWDllGetActivationFactory != nullptr);
            return m_pfnFNCWDllGetActivationFactory(activatibleClassId, factory);
        }

        return E_NOTIMPL;
    }

    HRESULT DelayLoadWinRtFoundation::RoGetActivationFactory(
        __in HSTRING activatibleClassId,
        __in REFIID iid,
        __out IActivationFactory** factory)
    {
        if (m_hModule)
        {
            if (m_pfnFNCWRoGetActivationFactory == nullptr)
            {
                m_pfnFNCWRoGetActivationFactory = (PFNCWRoGetActivationFactory)GetFunction("RoGetActivationFactory");
                if (m_pfnFNCWRoGetActivationFactory == nullptr)
                {
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnFNCWRoGetActivationFactory != nullptr);
            return m_pfnFNCWRoGetActivationFactory(activatibleClassId, iid, factory);
        }

        return E_NOTIMPL;
    }

    HRESULT DelayLoadWinRtTypeResolution::RoResolveNamespace(
        __in_opt const HSTRING namespaceName,
        __in_opt const HSTRING windowsMetaDataPath,
        __in const DWORD packageGraphPathsCount,
        __in_opt const HSTRING *packageGraphPaths,
        __out DWORD *metaDataFilePathsCount,
        HSTRING **metaDataFilePaths,
        __out DWORD *subNamespacesCount,
        HSTRING **subNamespaces)
    {
        if (m_hModule)
        {
            if (m_pfnRoResolveNamespace == nullptr)
            {
                m_pfnRoResolveNamespace = (PFNCRoResolveNamespace)GetFunction("RoResolveNamespace");
                if (m_pfnRoResolveNamespace == nullptr)
                {
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnRoResolveNamespace != nullptr);
            return m_pfnRoResolveNamespace(namespaceName, windowsMetaDataPath, packageGraphPathsCount, packageGraphPaths,
                metaDataFilePathsCount, metaDataFilePaths, subNamespacesCount, subNamespaces);
        }

        return E_NOTIMPL;
    }

    void DelayLoadWindowsGlobalization::Ensure(Js::DelayLoadWinRtString *winRTStringLibrary)
    {
        if (!this->m_isInit)
        {
            DelayLoadLibrary::EnsureFromSystemDirOnly();

#if DBG
            // This unused variable is to allow one to see the value of lastError in case both LoadLibrary (DelayLoadLibrary::Ensure has one) fail.
            // As the issue might be with the first one, as opposed to the second
            DWORD errorWhenLoadingBluePlus = GetLastError();
            Unused(errorWhenLoadingBluePlus);
#endif
            //Perform a check to see if Windows.Globalization.dll was loaded; if not try loading jsIntl.dll as we are on Win7.
            if (m_hModule == nullptr)
            {
                m_hModule = LoadLibraryEx(GetWin7LibraryName(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            }

            this->winRTStringLibrary = winRTStringLibrary;
            this->winRTStringsPresent = GetFunction("WindowsDuplicateString") != nullptr;
        }
    }

    HRESULT DelayLoadWindowsGlobalization::WindowsCreateString(_In_reads_opt_(length) const WCHAR * sourceString, UINT32 length, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string)
    {
        //If winRtStringLibrary isn't nullptr, that means it is available and we are on Win8+
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {
            return winRTStringLibrary->WindowsCreateString(sourceString, length, string);
        }

        return DelayLoadWinRtString::WindowsCreateString(sourceString, length, string);
    }
    HRESULT DelayLoadWindowsGlobalization::WindowsCreateStringReference(_In_reads_opt_(length + 1) const WCHAR * sourceString, UINT32 length, _Out_ HSTRING_HEADER * header, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string)
    {
        //First, we attempt to use the WinStringRT api encapsulated in the globalization dll; if it is available then it is a downlevel dll.
        //Otherwise; we might run into an error where we are using the Win8 (because testing is being done for instance) with the downlevel dll, and that would cause errors.
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {
            return winRTStringLibrary->WindowsCreateStringReference(sourceString, length, header, string);
        }
        return DelayLoadWinRtString::WindowsCreateStringReference(sourceString, length, header, string);
    }
    HRESULT DelayLoadWindowsGlobalization::WindowsDeleteString(_In_opt_ HSTRING string)
    {
        //First, we attempt to use the WinStringRT api encapsulated in the globalization dll; if it is available then it is a downlevel dll.
        //Otherwise; we might run into an error where we are using the Win8 (because testing is being done for instance) with the downlevel dll, and that would cause errors.
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {
            return winRTStringLibrary->WindowsDeleteString(string);
        }
        return DelayLoadWinRtString::WindowsDeleteString(string);
    }
    PCWSTR DelayLoadWindowsGlobalization::WindowsGetStringRawBuffer(_In_opt_ HSTRING string, _Out_opt_ UINT32 * length)
    {
        //First, we attempt to use the WinStringRT api encapsulated in the globalization dll; if it is available then it is a downlevel dll.
        //Otherwise; we might run into an error where we are using the Win8 (because testing is being done for instance) with the downlevel dll, and that would cause errors.
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {
            return winRTStringLibrary->WindowsGetStringRawBuffer(string, length);
        }
        return DelayLoadWinRtString::WindowsGetStringRawBuffer(string, length);
    }
    HRESULT DelayLoadWindowsGlobalization::WindowsCompareStringOrdinal(_In_opt_ HSTRING string1, _In_opt_ HSTRING string2, _Out_ INT32 * result)
    {
        //First, we attempt to use the WinStringRT api encapsulated in the globalization dll; if it is available then it is a downlevel dll.
        //Otherwise; we might run into an error where we are using the Win8 (because testing is being done for instance) with the downlevel dll, and that would cause errors.
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {
            return winRTStringLibrary->WindowsCompareStringOrdinal(string1, string2, result);
        }
        return DelayLoadWinRtString::WindowsCompareStringOrdinal(string1, string2, result);
    }

    HRESULT DelayLoadWindowsGlobalization::WindowsDuplicateString(_In_opt_ HSTRING original, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING *newString)
    {
        //First, we attempt to use the WinStringRT api encapsulated in the globalization dll; if it is available then it is a downlevel dll.
        //Otherwise; we might run into an error where we are using the Win8 (because testing is being done for instance) with the downlevel dll, and that would cause errors.
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {
            return winRTStringLibrary->WindowsDuplicateString(original, newString);
        }
        return DelayLoadWinRtString::WindowsDuplicateString(original, newString);
    }

#ifdef ENABLE_PROJECTION
    HRESULT DelayLoadWinRtError::RoClearError()
    {
        if (m_hModule)
        {
            if (m_pfnRoClearError == nullptr)
            {
                m_pfnRoClearError = (PFNCRoClearError)GetFunction("RoClearError");
                if (m_pfnRoClearError == nullptr)
                {
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnRoClearError != nullptr);
            m_pfnRoClearError();

            return S_OK;
        }

        return E_NOTIMPL;
    }

    BOOL DelayLoadWinRtError::RoOriginateLanguageException(__in HRESULT error, __in_opt HSTRING message, __in IUnknown * languageException)
    {
        if (m_hModule)
        {
            if (m_pfnRoOriginateLanguageException == nullptr)
            {
                m_pfnRoOriginateLanguageException = (PFNCRoOriginateLanguageException)GetFunction("RoOriginateLanguageException");
                if (m_pfnRoOriginateLanguageException == nullptr)
                {
                    return FALSE;
                }
            }

            Assert(m_pfnRoOriginateLanguageException != nullptr);
            return m_pfnRoOriginateLanguageException(error, message, languageException);
        }

        return FALSE;
    }
#endif

#ifdef _CONTROL_FLOW_GUARD
// Note. __declspec(guard(nocf)) causes the CFG check to be removed
// inside this function, and is needed only for test binaries (chk and FRETEST)
#if defined(DELAYLOAD_SET_CFG_TARGET)
    DECLSPEC_GUARDNOCF
#endif
    BOOL DelayLoadWinCoreMemory::SetProcessCallTargets(_In_ HANDLE hProcess,
        _In_ PVOID VirtualAddress,
        _In_ SIZE_T RegionSize,
        _In_ ULONG NumberOfOffets,
        _In_reads_(NumberOfOffets) PCFG_CALL_TARGET_INFO OffsetInformation)
    {

#if defined(DELAYLOAD_SET_CFG_TARGET)
        if (m_hModule)
        {
            if (m_pfnSetProcessValidCallTargets == nullptr)
            {
                m_pfnSetProcessValidCallTargets = (PFNCSetProcessValidCallTargets) GetFunction("SetProcessValidCallTargets");
                if (m_pfnSetProcessValidCallTargets == nullptr)
                {
                    return FALSE;
                }
            }

            Assert(m_pfnSetProcessValidCallTargets != nullptr);
            return m_pfnSetProcessValidCallTargets(hProcess, VirtualAddress, RegionSize, NumberOfOffets, OffsetInformation);
        }

        return FALSE;
#else
        return SetProcessValidCallTargets(hProcess, VirtualAddress, RegionSize, NumberOfOffets, OffsetInformation);
#endif
    }
#endif

    BOOL DelayLoadWinCoreProcessThreads::GetMitigationPolicyForProcess(
        __in HANDLE hProcess,
        __in PROCESS_MITIGATION_POLICY MitigationPolicy,
        __out_bcount(nLength) PVOID lpBuffer,
        __in SIZE_T nLength
        )
    {
#if defined(DELAYLOAD_SET_CFG_TARGET)
        if (m_hModule)
        {
            if (m_pfnGetProcessMitigationPolicy == nullptr)
            {
                m_pfnGetProcessMitigationPolicy = (PFNCGetMitigationPolicyForProcess) GetFunction("GetProcessMitigationPolicy");
                if (m_pfnGetProcessMitigationPolicy == nullptr)
                {
                    return FALSE;
                }
            }

            Assert(m_pfnGetProcessMitigationPolicy != nullptr);
            return m_pfnGetProcessMitigationPolicy(hProcess, MitigationPolicy, lpBuffer, nLength);
        }
        return FALSE;
#else
        return BinaryFeatureControl::GetMitigationPolicyForProcess(hProcess, MitigationPolicy, lpBuffer, nLength);
#endif // ENABLE_DEBUG_CONFIG_OPTIONS
    }

    BOOL DelayLoadWinCoreProcessThreads::GetProcessInformation(
        __in HANDLE hProcess,
        __in PROCESS_INFORMATION_CLASS ProcessInformationClass,
        __out_bcount(nLength) PVOID lpBuffer,
        __in SIZE_T nLength
        )
    {
#if defined(DELAYLOAD_SET_CFG_TARGET) || defined(_M_ARM)
        if (m_hModule)
        {
            if (m_pfnGetProcessInformation == nullptr)
            {
                m_pfnGetProcessInformation = (PFNCGetProcessInformation) GetFunction("GetProcessInformation");
                if (m_pfnGetProcessInformation == nullptr)
                {
                    return FALSE;
                }
            }

            Assert(m_pfnGetProcessInformation != nullptr);
            return m_pfnGetProcessInformation(hProcess, ProcessInformationClass, lpBuffer, nLength);
        }
#endif
        return FALSE;
    }

    // Implement this function inlined so that WinRT.lib can be used without the runtime.
    HRESULT DelayLoadWinType::RoGetMetaDataFile(
        _In_ const HSTRING name,
        _In_opt_ IMetaDataDispenserEx *metaDataDispenser,
        _Out_opt_ HSTRING *metaDataFilePath,
        _Outptr_opt_ IMetaDataImport2 **metaDataImport,
        _Out_opt_ mdTypeDef *typeDefToken)
    {
        if (m_hModule)
        {
            if (m_pfnRoGetMetadataFile == nullptr)
            {
                m_pfnRoGetMetadataFile = (PFNCWRoGetMettadataFile)GetFunction("RoGetMetaDataFile");
                if (m_pfnRoGetMetadataFile == nullptr)
                {
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnRoGetMetadataFile != nullptr);
            return m_pfnRoGetMetadataFile(name, metaDataDispenser, metaDataFilePath, metaDataImport, typeDefToken);
        }

        return E_NOTIMPL;
    }
}
