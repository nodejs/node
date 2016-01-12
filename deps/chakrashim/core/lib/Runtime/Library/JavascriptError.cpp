//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "errstr.h"
#include "Library\JavascriptErrorDebug.h"

#ifdef ERROR_TRACE
#define TRACE_ERROR(...) { Trace(__VA_ARGS__); }
#else
#define TRACE_ERROR(...)
#endif

namespace Js
{
    DWORD JavascriptError::GetAdjustedResourceStringHr(DWORD hr, bool isFormatString)
    {
        AssertMsg(FACILITY_CONTROL == HRESULT_FACILITY(hr) || FACILITY_JSCRIPT == HRESULT_FACILITY(hr), "Chakra hr are either FACILITY_CONTROL (for private HRs) or FACILITY_JSCRIPT (for public HRs)");
        WORD scodeIncr = isFormatString ? RTERROR_STRINGFORMAT_OFFSET : 0; // default for FACILITY_CONTROL == HRESULT_FACILITY(hr)
        if (FACILITY_JSCRIPT == HRESULT_FACILITY(hr))
        {
            scodeIncr += RTERROR_PUBLIC_RESOURCEOFFSET;
        }

        hr += scodeIncr;

        return hr;
    }

    bool JavascriptError::Is(Var aValue)
    {
        AssertMsg(aValue != NULL, "Error is NULL - did it come from an out of memory exception?");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Error;
    }

    bool JavascriptError::IsRemoteError(Var aValue)
    {
        // IJscriptInfo is not remotable (we don't register the proxy),
        // so we can't query for actual remote object
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_HostDispatch;
    }

    bool JavascriptError::HasDebugInfo()
    {
        return false;
    }

    void JavascriptError::SetNotEnumerable(PropertyId propertyId)
    {
        // Not all the properties of Error objects (like description, stack, number etc.) are in the spec.
        // Other browsers have all the properties as not-enumerable.
        SetEnumerable(propertyId, false);
    }

    Var JavascriptError::NewInstance(RecyclableObject* function, JavascriptError* pError, CallInfo callInfo, Arguments args)
    {
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && (callInfo.Flags & (CallFlags_Value|CallFlags_NewTarget)) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        JavascriptString* messageString = nullptr;

        if (args.Info.Count >= 2 && !JavascriptOperators::GetTypeId(args[1]) == TypeIds_Undefined)
        {
            messageString = JavascriptConversion::ToString(args[1], scriptContext);
        }

        if (messageString)
        {
            JavascriptOperators::SetProperty(pError, pError, PropertyIds::message, messageString, scriptContext);
            pError->SetNotEnumerable(PropertyIds::message);
        }

        JavascriptExceptionContext exceptionContext;
        JavascriptExceptionOperators::WalkStackForExceptionContext(*scriptContext, exceptionContext, pError,
            JavascriptExceptionOperators::StackCrawlLimitOnThrow(pError, *scriptContext), /*returnAddress=*/ nullptr, /*isThrownException=*/ false, /*resetSatck=*/ false);
        JavascriptExceptionOperators::AddStackTraceToObject(pError, exceptionContext.GetStackTrace(), *scriptContext, /*isThrownException=*/ false, /*resetSatck=*/ false);

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), pError, nullptr, scriptContext) :
            pError;
    }

    Var JavascriptError::NewErrorInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptError* pError = scriptContext->GetLibrary()->CreateError();

        // Process the arguments for IE specific behaviors for numbers and description

        JavascriptString* descriptionString = nullptr;
        bool hasNumber = false;
        double number = 0;
        if (args.Info.Count >= 3)
        {
            hasNumber = true;
            number = JavascriptConversion::ToNumber(args[1], scriptContext);

            // Get rid of this arg.  NewInstance only expects a message arg.
            args.Values++;
            args.Info.Count--;

            descriptionString = JavascriptConversion::ToString(args[1], scriptContext);
        }
        else if (args.Info.Count == 2)
        {
            if (!hasNumber)
            {
                descriptionString = JavascriptConversion::ToString(args[1], scriptContext);
            }
        }
        else
        {
            hasNumber = true;
            descriptionString = scriptContext->GetLibrary()->GetEmptyString();
        }

        Assert(descriptionString != nullptr);
        if (hasNumber)
        {
            JavascriptOperators::InitProperty(pError, PropertyIds::number, JavascriptNumber::ToVarNoCheck(number, scriptContext));
            pError->SetNotEnumerable(PropertyIds::number);
        }
        JavascriptOperators::SetProperty(pError, pError, PropertyIds::description, descriptionString, scriptContext);
        pError->SetNotEnumerable(PropertyIds::description);

        return JavascriptError::NewInstance(function, pError, callInfo, args);
    }

    Var JavascriptError::NewEvalErrorInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptError* pError = scriptContext->GetLibrary()->CreateEvalError();

        return JavascriptError::NewInstance(function, pError, callInfo, args);
    }

    Var JavascriptError::NewRangeErrorInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptError* pError = scriptContext->GetLibrary()->CreateRangeError();

        return JavascriptError::NewInstance(function, pError, callInfo, args);
    }

    Var JavascriptError::NewReferenceErrorInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptError* pError = scriptContext->GetLibrary()->CreateReferenceError();

        return JavascriptError::NewInstance(function, pError, callInfo, args);
    }

    Var JavascriptError::NewSyntaxErrorInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptError* pError = scriptContext->GetLibrary()->CreateSyntaxError();

        return JavascriptError::NewInstance(function, pError, callInfo, args);
    }

    Var JavascriptError::NewTypeErrorInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptError* pError = scriptContext->GetLibrary()->CreateTypeError();

        return JavascriptError::NewInstance(function, pError, callInfo, args);
    }

    Var JavascriptError::NewURIErrorInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptError* pError = scriptContext->GetLibrary()->CreateURIError();

        return JavascriptError::NewInstance(function, pError, callInfo, args);
    }

#ifdef ENABLE_PROJECTION
    Var JavascriptError::NewWinRTErrorInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptError* pError = scriptContext->GetHostScriptContext()->CreateWinRTError(nullptr, nullptr);

        return JavascriptError::NewInstance(function, pError, callInfo, args);
    }
#endif

    Var JavascriptError::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args[0] == 0 || !JavascriptOperators::IsObject(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, L"Error.prototype.toString");
        }

        RecyclableObject * thisError = RecyclableObject::FromVar(args[0]);
        Var value = NULL;
        JavascriptString *outputStr, *message;

        // get error.name
        BOOL hasName = JavascriptOperators::GetProperty(thisError, PropertyIds::name, &value, scriptContext, NULL) &&
            JavascriptOperators::GetTypeId(value) != TypeIds_Undefined;

        if (hasName)
        {
            outputStr = JavascriptConversion::ToString(value, scriptContext);
        }
        else
        {
            outputStr = scriptContext->GetLibrary()->CreateStringFromCppLiteral(L"Error");
        }

        // get error.message
        if (JavascriptOperators::GetProperty(thisError, PropertyIds::message, &value, scriptContext, NULL)
            && JavascriptOperators::GetTypeId(value) != TypeIds_Undefined)
        {
            message = JavascriptConversion::ToString(value, scriptContext);
        }
        else
        {
            message = scriptContext->GetLibrary()->GetEmptyString();
        }

        charcount_t nameLen = outputStr->GetLength();
        charcount_t msgLen = message->GetLength();

        if (nameLen > 0 && msgLen > 0)
        {
           outputStr = JavascriptString::Concat(outputStr, scriptContext->GetLibrary()->CreateStringFromCppLiteral(L": "));
           outputStr = JavascriptString::Concat(outputStr, message);
        }
        else if (msgLen > 0)
        {
            outputStr = message;
        }

        return outputStr;
    }

    void __declspec(noreturn) JavascriptError::MapAndThrowError(ScriptContext* scriptContext, HRESULT hr)
    {
        ErrorTypeEnum errorType;
        hr = MapHr(hr, &errorType);

        JavascriptError::MapAndThrowError(scriptContext, hr, errorType, nullptr);
    }

    void __declspec(noreturn) JavascriptError::MapAndThrowError(ScriptContext* scriptContext, HRESULT hr, ErrorTypeEnum errorType, EXCEPINFO* pei, IErrorInfo * perrinfo, RestrictedErrorStrings * proerrstr, bool useErrInfoDescription)
    {
        JavascriptError* pError = MapError(scriptContext, errorType, perrinfo, proerrstr);
        MapAndThrowError(scriptContext, pError, hr, pei, useErrInfoDescription);
    }

    void __declspec(noreturn) JavascriptError::MapAndThrowError(ScriptContext* scriptContext, JavascriptError *pError, long hCode, EXCEPINFO* pei, bool useErrInfoDescription/* = false*/)
    {
        Assert(pError != nullptr);

        PCWSTR varName = (pei ? pei->bstrDescription : nullptr);
        if (useErrInfoDescription)
        {
            JavascriptErrorDebug::SetErrorMessage(pError, hCode, varName, scriptContext);
        }
        else
        {
            JavascriptError::SetErrorMessage(pError, hCode, varName, scriptContext);
        }
        if (pei) FreeExcepInfo(pei);
        JavascriptExceptionOperators::Throw(pError, scriptContext);
    }

#define THROW_ERROR_IMPL(err_method, create_method, get_type_method, err_type) \
    static JavascriptError* create_method(ScriptContext* scriptContext, IErrorInfo* perrinfo, RestrictedErrorStrings * proerrstr) \
    { \
        JavascriptLibrary *library = scriptContext->GetLibrary(); \
        JavascriptError *pError = nullptr; \
        if (perrinfo) \
        { \
            JavascriptErrorDebug *pErrorDebug = RecyclerNewFinalized(library->GetRecycler(), JavascriptErrorDebug, perrinfo, library->get_type_method()); \
            JavascriptError::SetErrorType(pErrorDebug, err_type); \
            if (proerrstr) \
            { \
                pErrorDebug->SetRestrictedErrorStrings(proerrstr); \
            } \
            pError = static_cast<JavascriptError*>(pErrorDebug); \
        } \
        else \
        { \
            pError = library->create_method(); \
        } \
        return pError; \
    } \
    \
    void __declspec(noreturn) JavascriptError::err_method(ScriptContext* scriptContext, long hCode, EXCEPINFO* pei, IErrorInfo* perrinfo, RestrictedErrorStrings * proerrstr, bool useErrInfoDescription) \
    { \
        JavascriptError *pError = create_method(scriptContext, perrinfo, proerrstr); \
        MapAndThrowError(scriptContext, pError, hCode, pei, useErrInfoDescription); \
    } \
    \
    void __declspec(noreturn) JavascriptError::err_method(ScriptContext* scriptContext, long hCode, PCWSTR varName) \
    { \
        JavascriptLibrary *library = scriptContext->GetLibrary(); \
        JavascriptError *pError = library->create_method(); \
        JavascriptError::SetErrorMessage(pError, hCode, varName, scriptContext); \
        JavascriptExceptionOperators::Throw(pError, scriptContext); \
    } \
    \
    void __declspec(noreturn) JavascriptError::err_method(ScriptContext* scriptContext, long hCode, JavascriptString* varName) \
    { \
        JavascriptLibrary *library = scriptContext->GetLibrary(); \
        JavascriptError *pError = library->create_method(); \
        JavascriptError::SetErrorMessage(pError, hCode, varName->GetSz(), scriptContext); \
        JavascriptExceptionOperators::Throw(pError, scriptContext); \
    } \
    \
    void __declspec(noreturn) JavascriptError::err_method##Var(ScriptContext* scriptContext, long hCode, ...) \
    { \
        JavascriptLibrary *library = scriptContext->GetLibrary(); \
        JavascriptError *pError = library->create_method(); \
        va_list argList; \
        va_start(argList, hCode); \
        JavascriptError::SetErrorMessage(pError, hCode, scriptContext, argList); \
        va_end(argList); \
        JavascriptExceptionOperators::Throw(pError, scriptContext); \
    }

    THROW_ERROR_IMPL(ThrowError, CreateError, GetErrorType, kjstError)
    THROW_ERROR_IMPL(ThrowEvalError, CreateEvalError, GetEvalErrorType, kjstEvalError)
    THROW_ERROR_IMPL(ThrowRangeError, CreateRangeError, GetRangeErrorType, kjstRangeError)
    THROW_ERROR_IMPL(ThrowReferenceError, CreateReferenceError, GetReferenceErrorType, kjstReferenceError)
    THROW_ERROR_IMPL(ThrowSyntaxError, CreateSyntaxError, GetSyntaxErrorType, kjstSyntaxError)
    THROW_ERROR_IMPL(ThrowTypeError, CreateTypeError, GetTypeErrorType, kjstTypeError)
    THROW_ERROR_IMPL(ThrowURIError, CreateURIError, GetURIErrorType, kjstURIError)
#undef THROW_ERROR_IMPL

    JavascriptError* JavascriptError::MapError(ScriptContext* scriptContext, ErrorTypeEnum errorType, IErrorInfo * perrinfo /*= nullptr*/, RestrictedErrorStrings * proerrstr /*= nullptr*/)
    {
        switch (errorType)
        {
        case kjstError:
          return CreateError(scriptContext, perrinfo, proerrstr);
        case kjstTypeError:
          return CreateTypeError(scriptContext, perrinfo, proerrstr);
        case kjstRangeError:
          return CreateRangeError(scriptContext, perrinfo, proerrstr);
        case kjstSyntaxError:
          return CreateSyntaxError(scriptContext, perrinfo, proerrstr);
        case kjstReferenceError:
          return CreateReferenceError(scriptContext, perrinfo, proerrstr);
        case kjstURIError:
          return CreateURIError(scriptContext, perrinfo, proerrstr);
#ifdef ENABLE_PROJECTION
        case kjstWinRTError:
          if (scriptContext->GetConfig()->IsWinRTEnabled())
          {
              return scriptContext->GetHostScriptContext()->CreateWinRTError(perrinfo, proerrstr);
          }
          else
          {
              return CreateError(scriptContext, perrinfo, proerrstr);
          }
          break;
#endif
        default:
            AssertMsg(FALSE, "Invalid error type");
            __assume(false);
        };
    }

    void __declspec(noreturn) JavascriptError::ThrowDispatchError(ScriptContext* scriptContext, HRESULT hCode, PCWSTR message)
    {
        JavascriptError *pError = scriptContext->GetLibrary()->CreateError();
        JavascriptError::SetErrorMessageProperties(pError, hCode, message, scriptContext);
        JavascriptExceptionOperators::Throw(pError, scriptContext);
    }

    void JavascriptError::SetErrorMessageProperties(JavascriptError *pError, HRESULT hr, PCWSTR message, ScriptContext* scriptContext)
    {
        JavascriptString * messageString;
        if (message != nullptr)
        {
            // Save the runtime error message to be reported to IE.
            pError->originalRuntimeErrorMessage = message;
            messageString = Js::JavascriptString::NewWithSz(message, scriptContext);
        }
        else
        {
            messageString = scriptContext->GetLibrary()->GetEmptyString();
            // Set an empty string so we will return it as an runtime message with the error code to IE
            pError->originalRuntimeErrorMessage = L"";
        }

        JavascriptOperators::InitProperty(pError, PropertyIds::message, messageString);
        pError->SetNotEnumerable(PropertyIds::message);

        JavascriptOperators::InitProperty(pError, PropertyIds::description, messageString);
        pError->SetNotEnumerable(PropertyIds::description);

        hr = JavascriptError::GetErrorNumberFromResourceID(hr);
        JavascriptOperators::InitProperty(pError, PropertyIds::number, JavascriptNumber::ToVar(hr, scriptContext));
        pError->SetNotEnumerable(PropertyIds::number);
    }

    void JavascriptError::SetErrorMessage(JavascriptError *pError, HRESULT hr, ScriptContext* scriptContext, va_list argList)
    {
        Assert(FAILED(hr));
        wchar_t * allocatedString = nullptr;

        if (FACILITY_CONTROL == HRESULT_FACILITY(hr) || FACILITY_JSCRIPT == HRESULT_FACILITY(hr))
        {
            if (argList != nullptr)
            {
                HRESULT hrAdjusted = GetAdjustedResourceStringHr(hr, /* isFormatString */ true);

                BSTR message = BstrGetResourceString(hrAdjusted);
                if (message != nullptr)
                {
                    size_t len = _vscwprintf(message, argList);
                    Assert(len > 0);
                    len = AllocSizeMath::Add(len, 1);
                    allocatedString = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), wchar_t, len);

#pragma prefast(push)
#pragma prefast(disable:26014, "allocatedString allocated size more than msglen")
#pragma prefast(disable:26000, "allocatedString allocated size more than msglen")
                    len = vswprintf_s(allocatedString, len, message, argList);
                    Assert(len > 0);
#pragma prefast(pop)

                    SysFreeString(message);
                }
            }
            if (allocatedString == nullptr)
            {
                HRESULT hrAdjusted = GetAdjustedResourceStringHr(hr, /* isFormatString */ false);

                BSTR message = BstrGetResourceString(hrAdjusted);
                if (message == nullptr)
                {
                    message = BstrGetResourceString(IDS_UNKNOWN_RUNTIME_ERROR);
                }
                if (message != nullptr)
                {
                    uint32 len = SysStringLen(message) +1;
                    allocatedString = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), wchar_t, len);
                    wcscpy_s(allocatedString, len, message);
                    SysFreeString(message);
                }
            }
        }
        JavascriptError::SetErrorMessageProperties(pError, hr, allocatedString, scriptContext);
    }

    void JavascriptError::SetErrorMessage(JavascriptError *pError, HRESULT hr, PCWSTR varName, ScriptContext* scriptContext)
    {
        Assert(FAILED(hr));
        wchar_t * allocatedString = nullptr;

        if (FACILITY_CONTROL == HRESULT_FACILITY(hr) || FACILITY_JSCRIPT == HRESULT_FACILITY(hr))
        {
            if (varName != nullptr)
            {
                HRESULT hrAdjusted = GetAdjustedResourceStringHr(hr, /* isFormatString */ true);

                BSTR message = BstrGetResourceString(hrAdjusted);
                if (message != nullptr)
                {
                    uint32 msglen = SysStringLen(message);
                    size_t varlen = wcslen(varName);
                    size_t len = AllocSizeMath::Add(msglen, varlen);
                    allocatedString = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), wchar_t, len);
                    size_t outputIndex = 0;
                    for (size_t i = 0; i < msglen; i++)
                    {
                        Assert(outputIndex < len);
                        if (message[i] == L'%' && i + 1 < msglen && message[i+1] == L's')
                        {
                            Assert(len - outputIndex >= varlen);
                            wcscpy_s(allocatedString + outputIndex, len - outputIndex, varName);
                            outputIndex += varlen;
                            wcscpy_s(allocatedString + outputIndex, len - outputIndex, message + i + 2);
                            outputIndex += (msglen - i);
                            break;
                        }
#pragma prefast(push)
#pragma prefast(disable:26014, "allocatedString allocated size more than msglen")
#pragma prefast(disable:26000, "allocatedString allocated size more than msglen")
                        allocatedString[outputIndex++] = message[i];
#pragma prefast(pop)
                    }
                    SysFreeString(message);
                    if (outputIndex != len)
                    {
                        allocatedString = nullptr;
                    }
                }
            }
            if (allocatedString == nullptr)
            {
                HRESULT hrAdjusted = GetAdjustedResourceStringHr(hr, /* isFormatString */ false);

                BSTR message = BstrGetResourceString(hrAdjusted);
                if (message == nullptr)
                {
                    message = BstrGetResourceString(IDS_UNKNOWN_RUNTIME_ERROR);
                }
                if (message != nullptr)
                {
                    uint32 len = SysStringLen(message) +1;
                    allocatedString = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), wchar_t, len);
                    wcscpy_s(allocatedString, len, message);
                    SysFreeString(message);
                }
            }
        }
        JavascriptError::SetErrorMessageProperties(pError, hr, allocatedString, scriptContext);
    }

    void JavascriptError::SetErrorType(JavascriptError *pError, ErrorTypeEnum errorType)
    {
        pError->m_errorType = errorType;
    }

    BOOL JavascriptError::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        wchar_t const *pszMessage = nullptr;
        GetRuntimeErrorWithScriptEnter(this, &pszMessage);

        if (pszMessage)
        {
            stringBuilder->AppendSz(pszMessage);
            return TRUE;
        }

        return TRUE; // Return true to display an empty string
    }

    BOOL JavascriptError::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"Error");
        return TRUE;
    }

    HRESULT JavascriptError::GetRuntimeError(RecyclableObject* errorObject, __out_opt LPCWSTR * pMessage)
    {
        // Only report the error number if it is a runtime error
        HRESULT hr = JSERR_UncaughtException;
        ScriptContext* scriptContext = errorObject->GetScriptContext();

        // This version needs to be called in script.
        Assert(scriptContext->GetThreadContext()->IsScriptActive());

        Var number = JavascriptOperators::GetProperty(errorObject, Js::PropertyIds::number, scriptContext, NULL);
        if (TaggedInt::Is(number))
        {
            hr = TaggedInt::ToInt32(number);
        }
        else if (JavascriptNumber::Is_NoTaggedIntCheck(number))
        {
            hr = (HRESULT)JavascriptNumber::GetValue(number);
        }
        if (!FAILED(hr))
        {
            hr = E_FAIL;
        }

        if (pMessage != NULL)
        {
            *pMessage = L"";  // default to have IE load the error message, by returning empty-string

            // The description property always overrides any error message
            Var description = Js::JavascriptOperators::GetProperty(errorObject, Js::PropertyIds::description, scriptContext, NULL);
            if (JavascriptString::Is(description))
            {
                // Always report the description to IE if it is a string, even if the user sets it
                JavascriptString * messageString = JavascriptString::FromVar(description);
                *pMessage = messageString->GetSz();
            }
            else if (Js::JavascriptError::Is(errorObject) && Js::JavascriptError::FromVar(errorObject)->originalRuntimeErrorMessage != nullptr)
            {
                // use the runtime error message
                *pMessage = Js::JavascriptError::FromVar(errorObject)->originalRuntimeErrorMessage;
            }
            else if (FACILITY_CONTROL == HRESULT_FACILITY(hr))
            {
                // User might have create it's own Error object with JS error code, try to load the
                // resource string from the HResult by returning null;
                *pMessage = nullptr;
            }
        }

        // If neither the description or original runtime error message is set, and there are no error message.
        // Then just return false and we will report Uncaught exception to IE.
        return hr;
    }

    HRESULT JavascriptError::GetRuntimeErrorWithScriptEnter(RecyclableObject* errorObject, __out_opt LPCWSTR * pMessage)
    {
        ScriptContext* scriptContext = errorObject->GetScriptContext();
        Assert(!scriptContext->GetThreadContext()->IsScriptActive());

        // Use _NOT_SCRIPT. We enter runtime to get error info, likely inside a catch.
        BEGIN_JS_RUNTIME_CALL_NOT_SCRIPT(scriptContext)
        {
            return GetRuntimeError(errorObject, pMessage);
        }
        END_JS_RUNTIME_CALL(scriptContext);
    }

    void __declspec(noreturn) JavascriptError::ThrowOutOfMemoryError(ScriptContext *scriptContext)
    {
        JavascriptExceptionOperators::ThrowOutOfMemory(scriptContext);
    }

    void __declspec(noreturn) JavascriptError::ThrowStackOverflowError(ScriptContext *scriptContext, PVOID returnAddress)
    {
        JavascriptExceptionOperators::ThrowStackOverflow(scriptContext, returnAddress);
    }

    void __declspec(noreturn) JavascriptError::ThrowParserError(ScriptContext* scriptContext, HRESULT hrParser, CompileScriptException* se)
    {
        Assert(FAILED(hrParser));

        hrParser = SCRIPT_E_RECORDED;
        EXCEPINFO ei;
        se->GetError(&hrParser, &ei);

        JavascriptError* pError = MapParseError(scriptContext, ei.scode);
        JavascriptError::MapAndThrowError(scriptContext, pError, ei.scode, &ei);
    }

    ErrorTypeEnum JavascriptError::MapParseError(long hCode)
    {
        switch (hCode)
        {
#define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) \
        case name: \
            return jst; \
        break;
#define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource)
#include "rterrors.h"
#undef RT_PUBLICERROR_MSG
#undef RT_ERROR_MSG
        default:
            return kjstSyntaxError;
        }
    }

    JavascriptError* JavascriptError::MapParseError(ScriptContext* scriptContext, long hCode)
    {
        ErrorTypeEnum errorType = JavascriptError::MapParseError(hCode);
        return MapError(scriptContext, errorType);
    }

    bool JavascriptError::ThrowCantAssign(PropertyOperationFlags flags, ScriptContext* scriptContext, PropertyId propertyId)
    {
        if (flags == PropertyOperation_ThrowIfNonWritable)
        {
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotWritable, scriptContext->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowCantAssign(PropertyOperationFlags flags, ScriptContext* scriptContext, uint32 index)
    {
        if (flags == PropertyOperation_ThrowIfNonWritable)
        {
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotWritable, JavascriptConversion::ToString(JavascriptNumber::ToVar(index, scriptContext), scriptContext)->GetSz());
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowCantAssignIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext)
    {
        if (flags & PropertyOperation_StrictMode)
        {
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_CantAssignToReadOnly);
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowCantExtendIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext)
    {
        if (flags & PropertyOperation_StrictMode)
        {
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NonExtensibleObject);
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowCantDeleteIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext, PCWSTR varName)
    {
        if (flags & PropertyOperation_StrictMode)
        {
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_CantDeleteExpr, varName);
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowIfStrictModeUndefinedSetter(
        PropertyOperationFlags flags, Var setterValue, ScriptContext* scriptContext)
    {
        if ((flags & PropertyOperation_StrictMode)
            && JavascriptOperators::IsUndefinedAccessor(setterValue, scriptContext))
        {
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_CantAssignToReadOnly);
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowIfNotExtensibleUndefinedSetter(PropertyOperationFlags flags, Var setterValue, ScriptContext* scriptContext)
    {
        if ((flags & PropertyOperation_ThrowIfNotExtensible)
            && JavascriptOperators::IsUndefinedAccessor(setterValue, scriptContext))
        {
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotExtensible);
            }
            return true;
        }
        return false;
    }

    // Gets the error number associated with the resource ID for an error message.
    // When 'errorNumSource' is 0 (the default case), the resource ID is used as the error number.
    long JavascriptError::GetErrorNumberFromResourceID(long resourceId)
    {
        long result;
        switch (resourceId)
        {
    #define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) \
            case name: \
                result = (errorNumSource == 0) ? name : errorNumSource; \
                break;
    #define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource)
    #include "rterrors.h"
    #undef RT_PUBLICERROR_MSG
    #undef RT_ERROR_MSG
            default:
                result = resourceId;
        }

        return result;
    }

    JavascriptError* JavascriptError::CreateNewErrorOfSameType(JavascriptLibrary* targetJavascriptLibrary)
    {
        JavascriptError* jsNewError = nullptr;
        switch (this->GetErrorType())
        {
        case kjstError:
            jsNewError = targetJavascriptLibrary->CreateError();
            break;
        case kjstEvalError:
            jsNewError = targetJavascriptLibrary->CreateEvalError();
            break;
        case kjstRangeError:
            jsNewError = targetJavascriptLibrary->CreateRangeError();
            break;
        case kjstReferenceError:
            jsNewError = targetJavascriptLibrary->CreateReferenceError();
            break;
        case kjstSyntaxError:
            jsNewError = targetJavascriptLibrary->CreateSyntaxError();
            break;
        case kjstTypeError:
            jsNewError = targetJavascriptLibrary->CreateTypeError();
            break;
        case kjstURIError:
            jsNewError = targetJavascriptLibrary->CreateURIError();
            break;
#ifdef ENABLE_PROJECTION
        case kjstWinRTError:
            jsNewError = targetJavascriptLibrary->GetScriptContext()->GetHostScriptContext()->CreateWinRTError(nullptr, nullptr);
            break;
#endif

        case kjstCustomError:
        default:
            AssertMsg(false, "Unhandled error type?");
            break;
        }
        return jsNewError;
    }

    JavascriptError* JavascriptError::CloneErrorMsgAndNumber(JavascriptLibrary* targetJavascriptLibrary)
    {
        JavascriptError* jsNewError = this->CreateNewErrorOfSameType(targetJavascriptLibrary);
        if (jsNewError)
        {
            LPCWSTR msg = nullptr;
            HRESULT hr = JavascriptError::GetRuntimeError(this, &msg);
            jsNewError->SetErrorMessageProperties(jsNewError, hr, msg, targetJavascriptLibrary->GetScriptContext());
        }
        return jsNewError;
    }

    void JavascriptError::TryThrowTypeError(ScriptContext * checkScriptContext, ScriptContext * scriptContext, long hCode, PCWSTR varName)
    {
        // Don't throw if implicit exceptions are disabled
        if (checkScriptContext->GetThreadContext()->RecordImplicitException())
        {
            ThrowTypeError(scriptContext, hCode, varName);
        }
    }
}
