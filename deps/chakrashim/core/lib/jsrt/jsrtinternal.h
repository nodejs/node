//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "JsrtExceptionBase.h"
#include "Exceptions\EvalDisabledException.h"

#define PARAM_NOT_NULL(p) \
    if (p == nullptr) \
    { \
        return JsErrorNullArgument; \
    }

#define VALIDATE_JSREF(p) \
    if (p == JS_INVALID_REFERENCE) \
    { \
        return JsErrorInvalidArgument; \
    } \

#define MARSHAL_OBJECT(p, scriptContext) \
        Js::RecyclableObject* __obj = Js::RecyclableObject::FromVar(p); \
        if (__obj->GetScriptContext() != scriptContext) \
        { \
            if(__obj->GetScriptContext()->GetThreadContext() != scriptContext->GetThreadContext()) \
            {  \
                return JsErrorWrongRuntime;  \
            }  \
            p = Js::CrossSite::MarshalVar(scriptContext, __obj); \
        }

#define VALIDATE_INCOMING_RUNTIME_HANDLE(p) \
        { \
            if (p == JS_INVALID_RUNTIME_HANDLE) \
            { \
                return JsErrorInvalidArgument; \
            } \
        }

#define VALIDATE_INCOMING_PROPERTYID(p) \
          { \
            if (p == JS_INVALID_REFERENCE || \
                    Js::IsInternalPropertyId(((Js::PropertyRecord *)p)->GetPropertyId())) \
            { \
                return JsErrorInvalidArgument; \
            } \
        }

#define VALIDATE_INCOMING_REFERENCE(p, scriptContext) \
        { \
            VALIDATE_JSREF(p); \
            if (Js::RecyclableObject::Is(p)) \
            { \
                MARSHAL_OBJECT(p, scriptContext)   \
            } \
        }

#define VALIDATE_INCOMING_OBJECT(p, scriptContext) \
        { \
            VALIDATE_JSREF(p); \
            if (!Js::JavascriptOperators::IsObject(p)) \
            { \
                return JsErrorArgumentNotObject; \
            } \
            MARSHAL_OBJECT(p, scriptContext)   \
        }

#define VALIDATE_INCOMING_OBJECT_OR_NULL(p, scriptContext) \
        { \
            VALIDATE_JSREF(p); \
            if (!Js::JavascriptOperators::IsObjectOrNull(p)) \
            { \
                return JsErrorArgumentNotObject; \
            } \
            MARSHAL_OBJECT(p, scriptContext)   \
        }

#define VALIDATE_INCOMING_FUNCTION(p, scriptContext) \
        { \
            VALIDATE_JSREF(p); \
            if (!Js::JavascriptFunction::Is(p)) \
            { \
                return JsErrorInvalidArgument; \
            } \
            MARSHAL_OBJECT(p, scriptContext)   \
        }

template <class Fn>
JsErrorCode GlobalAPIWrapper(Fn fn)
{
    JsErrorCode errCode = JsNoError;
    try
    {
        // For now, treat this like an out of memory; consider if we should do something else here.

        AUTO_NESTED_HANDLED_EXCEPTION_TYPE((ExceptionType)(ExceptionType_OutOfMemory | ExceptionType_StackOverflow));

        errCode = fn();

        // These are error codes that should only be produced by the wrappers and should never
        // be produced by the internal calls.
        Assert(errCode != JsErrorFatal &&
            errCode != JsErrorNoCurrentContext &&
            errCode != JsErrorInExceptionState &&
            errCode != JsErrorInDisabledState &&
            errCode != JsErrorOutOfMemory &&
            errCode != JsErrorScriptException &&
            errCode != JsErrorScriptTerminated);
    }
    CATCH_STATIC_JAVASCRIPT_EXCEPTION_OBJECT

    CATCH_OTHER_EXCEPTIONS

    return errCode;
}

JsErrorCode CheckContext(JsrtContext *currentContext, bool verifyRuntimeState, bool allowInObjectBeforeCollectCallback = false);

template <bool verifyRuntimeState, class Fn>
JsErrorCode ContextAPIWrapper(Fn fn)
{
    JsrtContext *currentContext = JsrtContext::GetCurrent();
    JsErrorCode errCode = CheckContext(currentContext, verifyRuntimeState);

    if (errCode != JsNoError)
    {
        return errCode;
    }

    Js::ScriptContext *scriptContext = currentContext->GetScriptContext();
    try
    {
        AUTO_NESTED_HANDLED_EXCEPTION_TYPE((ExceptionType)ExceptionType_JavascriptException);

        // Enter script
        BEGIN_ENTER_SCRIPT(scriptContext, true, true, true)
        {
            errCode = fn(scriptContext);
        }
        END_ENTER_SCRIPT

            // These are error codes that should only be produced by the wrappers and should never
            // be produced by the internal calls.
            Assert(errCode != JsErrorFatal &&
            errCode != JsErrorNoCurrentContext &&
            errCode != JsErrorInExceptionState &&
            errCode != JsErrorInDisabledState &&
            errCode != JsErrorOutOfMemory &&
            errCode != JsErrorScriptException &&
            errCode != JsErrorScriptTerminated);
    }
    catch (Js::JavascriptExceptionObject *  exceptionObject)
    {
        scriptContext->GetThreadContext()->SetRecordedException(exceptionObject);
        return JsErrorScriptException;
    }
    catch (Js::ScriptAbortException)
    {
        Assert(scriptContext->GetThreadContext()->GetRecordedException() == nullptr);
        scriptContext->GetThreadContext()->SetRecordedException(scriptContext->GetThreadContext()->GetPendingTerminatedErrorObject());
        return JsErrorScriptTerminated;
    }
    catch (Js::EvalDisabledException)
    {
        return JsErrorScriptEvalDisabled;
    }

    CATCH_OTHER_EXCEPTIONS

    return errCode;
}

// allowInObjectBeforeCollectCallback only when current API is guaranteed not to do recycler allocation.
template <class Fn>
JsErrorCode ContextAPINoScriptWrapper(Fn fn, bool allowInObjectBeforeCollectCallback = false)
{
    JsrtContext *currentContext = JsrtContext::GetCurrent();
    JsErrorCode errCode = CheckContext(currentContext, /*verifyRuntimeState*/true, allowInObjectBeforeCollectCallback);

    if (errCode != JsNoError)
    {
        return errCode;
    }

    Js::ScriptContext *scriptContext = currentContext->GetScriptContext();

    try
    {
        // For now, treat this like an out of memory; consider if we should do something else here.

        AUTO_NESTED_HANDLED_EXCEPTION_TYPE((ExceptionType)(ExceptionType_OutOfMemory | ExceptionType_StackOverflow));

        errCode = fn(scriptContext);

        // These are error codes that should only be produced by the wrappers and should never
        // be produced by the internal calls.
        Assert(errCode != JsErrorFatal &&
            errCode != JsErrorNoCurrentContext &&
            errCode != JsErrorInExceptionState &&
            errCode != JsErrorInDisabledState &&
            errCode != JsErrorOutOfMemory &&
            errCode != JsErrorScriptException &&
            errCode != JsErrorScriptTerminated);
    }
    CATCH_STATIC_JAVASCRIPT_EXCEPTION_OBJECT

    catch (Js::JavascriptExceptionObject *  exceptionObject)
    {
        AssertMsg(false, "Should never get JavascriptExceptionObject for ContextAPINoScriptWrapper.");
        scriptContext->GetThreadContext()->SetRecordedException(exceptionObject);
        return JsErrorScriptException;
    }

    catch (Js::ScriptAbortException)
    {
        Assert(scriptContext->GetThreadContext()->GetRecordedException() == nullptr);
        scriptContext->GetThreadContext()->SetRecordedException(scriptContext->GetThreadContext()->GetPendingTerminatedErrorObject());
        return JsErrorScriptTerminated;
    }

    CATCH_OTHER_EXCEPTIONS

    return errCode;
}

void HandleScriptCompileError(Js::ScriptContext * scriptContext, CompileScriptException * se);


#if DBG
#define _PREPARE_RETURN_NO_EXCEPTION __debugCheckNoException.hasException = false;
#else
#define _PREPARE_RETURN_NO_EXCEPTION
#endif

#define BEGIN_JSRT_NO_EXCEPTION  BEGIN_NO_EXCEPTION
#define END_JSRT_NO_EXCEPTION    END_NO_EXCEPTION return JsNoError;
#define RETURN_NO_EXCEPTION(x)   _PREPARE_RETURN_NO_EXCEPTION return x
