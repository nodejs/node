//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include <strsafe.h>
#include "restrictederrorinfo.h"
#include "errstr.h"
#include "Library\JavascriptErrorDebug.h"

// Temporarily undefining "null" (defined in Common.h) to avoid compile errors when importing mscorlib.tlb
#import <mscorlib.tlb> raw_interfaces_only \
    rename("Assert","CLRAssert") rename("ReportEvent","CLRReportEvent") rename("Debugger","CLRDebugger")

namespace Js
{
    using namespace mscorlib;

    __declspec(thread)  wchar_t JavascriptErrorDebug::msgBuff[512];

    bool JavascriptErrorDebug::Is(Var aValue)
    {
        AssertMsg(aValue != NULL, "Error is NULL - did it come from an out of memory exception?");
        if (JavascriptOperators::GetTypeId(aValue) == TypeIds_Error)
        {
            JavascriptError * error = JavascriptError::FromVar(aValue);
            return error->HasDebugInfo();
        }
        return false;
    }

    // Info: Differentiates between JavascriptError and JavascriptErrorDebug
    bool JavascriptErrorDebug::HasDebugInfo()
    {
        return true;
    }

    // Info: Finalizer to release stored IErrorInfo
    void JavascriptErrorDebug::Finalize(bool isShutdown)
    {
        // Finalize can be called multiple times (from recycler and from ScriptSite::Close)
        // Check if that is the case and don't do anything
        if (isShutdown)
        {
            return;
        }
        if (pErrorInfo == nullptr)
        {
            return ;
        }
        pErrorInfo->Release();
        pErrorInfo = nullptr;
        if (restrictedStrings.restrictedErrStr != nullptr)
        {
            SysFreeString(restrictedStrings.restrictedErrStr);
            restrictedStrings.restrictedErrStr = nullptr;
        }
        if (restrictedStrings.referenceStr != nullptr)
        {
            SysFreeString(restrictedStrings.referenceStr);
            restrictedStrings.referenceStr = nullptr;
        }
        if (restrictedStrings.capabilitySid != nullptr)
        {
            SysFreeString(restrictedStrings.capabilitySid);
            restrictedStrings.capabilitySid = nullptr;
        }
    }

    void JavascriptErrorDebug::Dispose(bool isShutdown)
    {
    }

#ifdef ENABLE_PROJECTION
    void JavascriptErrorDebug::ClearErrorInfo(ScriptContext* scriptContext)
    {
        HRESULT hr = scriptContext->GetThreadContext()->GetWinRTErrorLibrary()->RoClearError();

        // If we fail to create the delay-load library or fail to find RoClearError in it, fallback to Win8 behavior.
        if (FAILED(hr))
        {
            IErrorInfo * perrinfo;
            if(NOERROR == (hr = GetErrorInfo(0L, &perrinfo)))
            {
                perrinfo->Release();
            }
        }
    }
#endif

    // Info:        Map and Throw a JavascriptError(Debug) containing restricted error information, if available
    // Parameters:  scriptContext - the script context
    //              hr - HRESULT of the error to be thrown
    void __declspec(noreturn) JavascriptErrorDebug::MapAndThrowErrorWithInfo(ScriptContext* scriptContext, HRESULT hr)
    {
        // Initialize error type to kjstWinRTError if WinRT is enabled, or to kjstError otherwise.
#ifdef ENABLE_PROJECTION
        ErrorTypeEnum errorType = scriptContext->GetConfig()->IsWinRTEnabled() ? kjstWinRTError : kjstError;
#else
        ErrorTypeEnum errorType = kjstError;
#endif
        // Get error type if hr is a runtime error.
        GetErrorTypeFromNumber(hr, &errorType);

        EXCEPINFO excepinfo;
        EXCEPINFO baseline;
        memset(&baseline, 0, sizeof(baseline));
        RestrictedErrorStrings roerrstr;
        IErrorInfo * perrinfo = nullptr;
        HRESULT result = GetExcepAndErrorInfo(scriptContext, hr, &excepinfo, &roerrstr, &perrinfo);
        // If there is WinRT specific information for this error, throw an error including it
        if (NOERROR == result)
        {
            MapAndThrowError(scriptContext, hr, errorType, &excepinfo, perrinfo, &roerrstr, true);
        }
        // If there is no restricted info, but unrestricted exception info was obtained,
        // throw an error with the exception info only
        else if(memcmp(&excepinfo, &baseline, sizeof(baseline)) != 0)
        {
            Assert(!perrinfo);
            MapAndThrowError(scriptContext, hr, errorType, &excepinfo, nullptr, nullptr, true);
        }
        // If no specialized info was obtained, throw an error by HRESULT and type only.
        else
        {
            MapAndThrowError(scriptContext, hr, errorType, nullptr, nullptr, nullptr, true);
        }
    }

    void JavascriptErrorDebug::SetErrorMessage(JavascriptError *pError, HRESULT hr, PCWSTR varDescription, ScriptContext* scriptContext)
    {
        Assert(FAILED(hr));

        wchar_t* allocatedString = nullptr;
        BSTR restrictedDescription = nullptr;
        BSTR message = nullptr;
        size_t length = 1; // +1 for null character

#ifdef ENABLE_PROJECTION
        // Get the restricted error string - but only if the WinRT is enabled and we are targeting WinBlue+.
        if (JavascriptErrorDebug::Is(pError)
            && scriptContext->GetConfig()->IsWinRTEnabled()
            && scriptContext->GetConfig()->GetProjectionConfig() != nullptr
            && scriptContext->GetConfig()->GetProjectionConfig()->IsTargetWindowsBlueOrLater())
        {
            restrictedDescription = JavascriptErrorDebug::FromVar(pError)->GetRestrictedErrorString();
            length += SysStringLen(restrictedDescription);
        }
#endif

        if (varDescription != nullptr)
        {
            length += wcslen(varDescription);
        }
        else
        {
            if (FACILITY_CONTROL == HRESULT_FACILITY(hr))
            {
                message = BstrGetResourceString(hr);
            }
            if (message == nullptr)
            {
                // Default argument array of empty strings, for CLR parity
                DWORD_PTR pArgs[] = { (DWORD_PTR)L"", (DWORD_PTR)L"", (DWORD_PTR)L"", (DWORD_PTR)L"", (DWORD_PTR)L"",
                    (DWORD_PTR)L"", (DWORD_PTR)L"", (DWORD_PTR)L"", (DWORD_PTR)L"", (DWORD_PTR)L"" };

                if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_ARGUMENT_ARRAY,
                    NULL,
                    hr,
                    0,
                    msgBuff,
                    _countof(msgBuff),
                    (va_list*)pArgs))
                {
                    message = SysAllocString(msgBuff);
                }
            }
            if (message == nullptr)
            {
                message = BstrGetResourceString(IDS_UNKNOWN_RUNTIME_ERROR);
            }
            if (message != nullptr)
            {
                length += SysStringLen(message);
            }
        }

        // If length == 1, we didn't find any description strings - leave allocatedString null.
        if (length > 1)
        {
             // If we have a restricted description, the error message format is ("%s\r\n%s", message, restrictedMessage).
             if (restrictedDescription != nullptr)
             {
                 // length should be longer by 2 characters to handle the \r\n
                 allocatedString = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), wchar_t, length + 2);
                 StringCchPrintfW(allocatedString, length + 2, L"%s\r\n%s", varDescription ? varDescription : message, restrictedDescription);
             }
             else
             {
                 allocatedString = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), wchar_t, length);
                 wcscpy_s(allocatedString, length, varDescription ? varDescription : message);
             }
        }

        if (restrictedDescription != nullptr)
        {
            SysFreeString(restrictedDescription);
            restrictedDescription = nullptr;
        }

        if (message != nullptr)
        {
            SysFreeString(message);
            message = nullptr;
        }

        JavascriptError::SetErrorMessageProperties(pError, hr, allocatedString, scriptContext);
    }

    // Info:        Gets both unrestricted and restricted error info
    // Parameters:  scriptContext - the script context
    //              pexcepinfo - the exception info of the error (unrestricted)
    //              proerrstr - the winrt specific error strings (restricted)
    //              pperrinfo - the IErrorInfo object
    // Returns:     Failed HRESULT - if GetErrorInfo or QI fail
    //              Success - otherwise
    HRESULT JavascriptErrorDebug::GetExcepAndErrorInfo(ScriptContext* scriptContext, HRESULT hrReturned, EXCEPINFO *pexcepinfo, RestrictedErrorStrings * proerrstr, IErrorInfo ** pperrinfo)
    {
        HRESULT hr;
        HRESULT hrResult;
        bool errorInfoMatch = false;

        memset(pexcepinfo, 0, sizeof(*pexcepinfo));
        memset(proerrstr, 0, sizeof(*proerrstr));
        *pperrinfo = nullptr;

        // GetErrorInfo returns S_FALSE if there is no rich error info
        // and S_OK if there is.
        IErrorInfo * perrinfo;
        if(NOERROR != (hr = GetErrorInfo(0L, &perrinfo)))
        {
            return hr;
        }
        // Fill Exception info
        perrinfo->GetSource(&pexcepinfo->bstrSource);
        perrinfo->GetDescription(&pexcepinfo->bstrDescription);
        perrinfo->GetHelpFile(&pexcepinfo->bstrHelpFile);
        perrinfo->GetHelpContext(&pexcepinfo->dwHelpContext);

        // Initialize restricted strings
        proerrstr->restrictedErrStr = nullptr;
        proerrstr->referenceStr = nullptr;
        proerrstr->capabilitySid = nullptr;

        BSTR bstrErr = nullptr;
        BSTR bstrRestrictedErr = nullptr;
        BSTR bstrCapabilitySid = nullptr;
        BSTR bstrReference = nullptr;

        IRestrictedErrorInfo * pROErrorInfo = nullptr;
        _Exception * pCLRException = nullptr;
        hr = perrinfo->QueryInterface(__uuidof(IRestrictedErrorInfo), reinterpret_cast<void**>(&pROErrorInfo));
        if (SUCCEEDED(hr))
        {
            // Get restricted error strings from IRestrictedErrorInfo
            HRESULT hrErr = S_OK;
            hrResult = pROErrorInfo->GetErrorDetails(&bstrErr, &hrErr, &bstrRestrictedErr, &bstrCapabilitySid);
            if (SUCCEEDED(hrResult))
            {
                errorInfoMatch = (hrErr == hrReturned);
                if (errorInfoMatch)
                {
                    if (nullptr != bstrRestrictedErr)
                    {
                        proerrstr->restrictedErrStr = bstrRestrictedErr;
                        bstrRestrictedErr = nullptr;
                    }
                    if (nullptr != bstrCapabilitySid)
                    {
                        proerrstr->capabilitySid = bstrCapabilitySid;
                        bstrCapabilitySid = nullptr;
                    }
                }
            }
            hrResult = pROErrorInfo->GetReference(&bstrReference);
            if (SUCCEEDED(hrResult) && errorInfoMatch)
            {
                if (nullptr != bstrReference)
                {
                    proerrstr->referenceStr = bstrReference;
                    bstrReference = nullptr;
                }
            }
        }
        else
        {
            hr = perrinfo->QueryInterface(__uuidof(_Exception), reinterpret_cast<void**>(&pCLRException));
            if(FAILED(hr))
            {
                perrinfo->Release();
                return hr;
            }
            hrResult = pCLRException->get_Message(&bstrRestrictedErr);
            if (SUCCEEDED(hrResult))
            {
                errorInfoMatch = true;
                if (nullptr != bstrRestrictedErr)
                {
                    proerrstr->restrictedErrStr = bstrRestrictedErr;
                    bstrRestrictedErr = nullptr;
                }
            }
        }
        if (nullptr != bstrErr)
        {
            SysFreeString(bstrErr);
            bstrErr = nullptr;
        }
        if (nullptr != bstrRestrictedErr)
        {
            SysFreeString(bstrRestrictedErr);
            bstrRestrictedErr = nullptr;
        }
        if (nullptr != bstrCapabilitySid)
        {
            SysFreeString(bstrCapabilitySid);
            bstrCapabilitySid = nullptr;
        }
        if (nullptr != bstrReference)
        {
            SysFreeString(bstrReference);
            bstrReference = nullptr;
        }

        if (!errorInfoMatch)
        {
            if (nullptr != pexcepinfo->bstrSource)
            {
                SysFreeString(pexcepinfo->bstrSource);
                pexcepinfo->bstrSource = nullptr;
            }
            if (nullptr != pexcepinfo->bstrDescription)
            {
                SysFreeString(pexcepinfo->bstrDescription);
                pexcepinfo->bstrDescription = nullptr;
            }
            if (nullptr != pexcepinfo->bstrHelpFile)
            {
                SysFreeString(pexcepinfo->bstrHelpFile);
                pexcepinfo->bstrHelpFile = nullptr;
            }
            perrinfo->Release();
            perrinfo = nullptr;
            hr = S_FALSE;
        }

        if (nullptr != pROErrorInfo)
        {
            pROErrorInfo->Release();
        }
        if (nullptr != pCLRException)
        {
            pCLRException->Release();
        }
        *pperrinfo = perrinfo;
        return hr;
    }

    void JavascriptErrorDebug::SetErrorInfo()
    {
        IErrorInfo * pErrorInfo = GetRestrictedErrorInfo();

        if (!pErrorInfo)
        {
            return;
        }

        ::SetErrorInfo(0L, pErrorInfo);
    }

    void JavascriptErrorDebug::GetErrorTypeFromNumber(HRESULT hr, ErrorTypeEnum * errorTypeOut)
    {
        if (errorTypeOut == nullptr)
        {
            return;
        }
        switch ((HRESULT)hr)
        {
#define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) \
        case name: \
        *errorTypeOut = jst; \
        break;
#define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource)
#include "rterrors.h"
#undef RT_PUBLICERROR_MSG
#undef RT_ERROR_MSG
        }
    }
}
